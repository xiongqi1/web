/*
 * This file contains the main server loop used for receiving and responding
 * HTTP connection requests in accordance with the TR069 specification.
 *
 * by Matthew Forgie, March 2012.
 * Copyright NetComm Wireless Limited.
 *
 */
#include "cwmp-crd.h"

//#define DEBUGS_ON

#ifdef DEBUGS_ON
	#define DEBUG(...) printf(__VA_ARGS__);fflush(stdout);
#else
	#define DEBUG(...)
#endif

struct context {
	pthread_t thread;
	int fd;
	struct context *next;
};

/*
 * This is a global list of thread contexts, used for tracking active threads.
 * Each entry is removed by its own thread once that thread is finished, so the
 * list is locked with a mutex to prevent race conditions.
 */
static struct context *context_list = NULL;
static pthread_mutex_t context_lock = PTHREAD_MUTEX_INITIALIZER;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr (struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void free_context(struct context *conn)
{
	struct context *ptr, *next_ptr;

	if (context_list == NULL) {
		return;
	}

	pthread_mutex_lock(&context_lock);
	ptr = context_list;
	if (ptr->fd == conn->fd) {
		context_list = context_list->next;
		free(ptr);
		pthread_mutex_unlock(&context_lock);
		return;
	}

	next_ptr = ptr->next;
	while (next_ptr != NULL) {
		if (next_ptr->fd == conn->fd) {
			ptr->next = next_ptr->next;
			free(next_ptr);
			pthread_mutex_unlock(&context_lock);
			return;
		}
		ptr = next_ptr;
		next_ptr = next_ptr->next;
	}
	//didn't find a context, so just return;
	pthread_mutex_unlock(&context_lock);
	return;
}

void *handle_connection(void *context)
{
	struct context *conn_context = (struct context *)context;
	int numbytes;
	char request[MAXREQUESTSIZE + 1];
	char reply[MAXREQUESTSIZE];


	if ((numbytes = recv(conn_context->fd, request, MAXREQUESTSIZE, 0)) 
	    == -1) {
		perror("recv");
		close(conn_context->fd);
		free_context(conn_context);
		pthread_exit(NULL);
	}
	if (numbytes > 0) {
		//nothing to do, so finish
		request[numbytes] = '\0';
		DEBUG("received message of %i bytes on %i:\n%s\n", numbytes,
		      conn_context->fd, request);
		parse_request(&request, &reply);
		if (send(conn_context->fd, reply, strlen(reply), 0) == -1) {
			perror("send");
		}
		DEBUG("Sent reply: %s\n", reply);
		DEBUG("processing finished.\n");
		DEBUG("--------------------\n\n");
	}
	close(conn_context->fd);

	// find the context in the linked list and free it
	free_context(conn_context);

// This cause process crash with new toolchain that have 'libgcc_s.so' as static libarry.
//	pthread_exit(NULL);
	return NULL;
}

void Usage(void)
{
	fprintf(stderr, "\nUsage: cwmp-crd [-6] -h port\n\n");
	fprintf(stderr, "cwmp-crd is a http server that has one purpose: to"
                    " listen for TR-069 connection request, and to check the"
                    " authorization of requests that arrive. Only one argument"
                    " is available, which specifies which port the http server"
                    " should listen on.\n\nThis command for internal use of "
                    "TR-069 only.\n"
                    "	-6: enable IPv4 and IPv6 binding\n");
	exit(1);
}

/*
 * The main function is primarily responsible for managing the socket
 * connections, and receiving and responding to HTTP requests.  The actual
 * work of parsing and authorization is done in the helper functions above.
 * Each incoming message is handled in a child process, which exits after it
 * is finished.  This prevents the main loop getting slowed down waiting for
 * a request to finish.
 */

int main (int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	int yes=1;
	int no=0;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char *port;
	int request_count = 0;
	time_t request_period = 0, time_now = 0;
	char reply[MAXREQUESTSIZE + 1];
	struct context *new_context = NULL;
	pthread_attr_t attr;
	int ret;
	FILE *fp;
	int status;
	char cmd[512];
	int opt, enable_ipv6 = 0;

	while ((opt = getopt(argc, argv, "6")) != -1) {
		switch (opt) {
			case '6':
				enable_ipv6 = 1;
				break;
			default: /* '?' */
				Usage();
				return -1;
		}
	}

	if (optind == argc) { // set to default.
		port = DEFAULT_PORT;
	} else if ((optind + 1)== argc) {
		port = argv[optind]; //need to sanity check this value
	} else {
		Usage();
		return -1;
	}

	if (rdb_open_db() < 0) {
		fprintf(stderr, "unable to access RDB\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	if (!enable_ipv6) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_INET6;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		/*
		 * System-wide global setting -> /proc/sys/net/ipv6/bindv6only
		 * 1 -> the socket is restricted to sending and receiving IPv6 packets only
		 * 0 -> the socket can be used to send and receive packets
		 *      to and from an IPv6 address or an IPv4-mapped IPv6 address.
		 */
		if (enable_ipv6 == 1 && p->ai_family == AF_INET6 &&
			setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		DEBUG("server: socket created on [%d]\n", p->ai_family);
		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	// worker thread attribute init
	if(pthread_attr_init(&attr)) {
		perror("pthread_attr_init");
		exit(1);
	}
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		perror("pthread_attr_setdetachstate");
		exit(1);
	}
	pthread_attr_getstacksize(&attr, &ret);
	DEBUG("default stack size: %d (min %d)\n", ret, PTHREAD_STACK_MIN);
	if(pthread_attr_setstacksize(&attr, WORKER_THREAD_STACKSIZE)) {
		perror("pthread_attr_setstacksize");
		exit(1);
	}

	DEBUG("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		// Start the request period if not already started
		time_now = time(NULL);
		if (request_period == 0) {
			request_period = time_now + REQUEST_PERIOD_LENGTH;
		}
		// check if we're receiving too many requests
		if (time_now <= request_period) {
			DEBUG("time now is %i, request period is %i\n",
				time_now, request_period);
			request_count++;
			if (request_count > MAX_REQUESTS_PER_PERIOD) {
				DEBUG("request_count is %i, max is %i\n",
					request_count, MAX_REQUESTS_PER_PERIOD);
				//return an error
				sprintf(reply, INVALID_REQUEST_503);
				if (send(new_fd, reply, strlen(reply), 0) == -1)
					perror("send");
				close(new_fd);
				continue;
			}
		} else {
			//period has expired, so reset counters
			request_count = 1;
			request_period = time_now + REQUEST_PERIOD_LENGTH;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		DEBUG("server: got connection from %s\n", s);

		sprintf(cmd, "/usr/lib/tr-069/scripts/checkConnReqWhitelist.lua %s 1>/dev/null 2>&1", s);
		fp = popen(cmd, "r");
		if (fp != NULL) {
			status = pclose(fp);
			// The pclose will return non-zero,
			// 1. if it receives Connection Request message from the ip address on the WhiteList.
			// 2. if WhiteList is empty.
			// 3. if checkConnReqWhitelist.lua does not exist on the system.
			// Otherwise(pclose returns 0), the Conneciton requestion messaged should be ignored.
			if (!status) {
				DEBUG("Connection request is not in whitelist. Ignore request.\n");
				continue;
			}
		}

		// create a thread to handle processing the request
		new_context = malloc(sizeof(struct context));
		if (new_context == NULL) {
			perror("context");
			close(new_fd);
			continue;
		}
		new_context->fd = new_fd;

		// push the next context onto the linked list
		pthread_mutex_lock(&context_lock);
		new_context->next = context_list;
		context_list = new_context;
		pthread_mutex_unlock(&context_lock);
		ret = pthread_create(&(new_context->thread), &attr,
				     handle_connection,
				     (void *)new_context);
		if (ret) {
			perror("pthread_create");
			free_context(new_context);
			close(new_fd);
			continue;
		}
	}

	return 0;
}
