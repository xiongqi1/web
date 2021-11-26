/*
 * cwmp-crd-testclient.c -- a pretend ACS server that sends a variety of
 * GET requests to the CR daemon.
 */

#include "cwmp-crd.h"

//#define DEBUGS_ON

#ifdef DEBUGS_ON
        #define DEBUG(...) printf(__VA_ARGS__);
#else
        #define DEBUG(...)
#endif

#define TEST_URI "kjhad917an"
#define TEST_USER "user"
#define BAD_URI "baduri1234"
#define TEST_NONCE "dcd98b7102dd2f0e8b11d0f600bfb0c093"
#define TEST_RESPONSE "6629fae49393a05397450978507c4ef1"
#define TEST_OPAQUE "5ccc069c403ebaf9f0171e9517f40e41"
#define TEST_REQUEST_1 "GET %s HTTP/1.1\r\nHost: %s:%s\r\nConnection:" \
			" close\r\nContent-Length: 0\r\n\r\n"
#define TEST_REQUEST_2 "POST %s HTTP/1.1\r\nHost: %s:%s\r\nConnection:" \
			" close\r\nContent-Length: 0\r\n\r\n"
#define TEST_REQUEST_3 "GET %s HTTP/1.1\r\nHost: %s:%s\r\n" \
				"Connection: close\r\n" \
				"Content-Length: 0\r\n" \
				"Authorization: Digest username=\"%s\"," \
				"realm=\"Casa Systems. Inc.\"," \
                     "nonce=\"%s\"," \
                     "uri=\"%s\"," \
                     "qop=auth," \
                     "nc=00000001," \
                     "cnonce=\"0a4f113b\"," \
                     "response=\"%s\"," \
                     "opaque=\"%s\"\r\n"
#define TEST_REQUEST_4 "GET %s HTTP/1.1\r\nHost: %s:%s\r\nConnection:" \
			" close\r\nContent-Length: 0\r\n\r\n"

char reply[MAXREQUESTSIZE];
char *local_ip = "127.0.0.1";

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int check_authenticate (char *buf)
{
	char *digest_challenge;
	char *token;
	struct auth_params incoming;
	char hash[EVP_MAX_MD_SIZE];

	if (strncmp(buf, "Digest", 6)) {
		DEBUG("Only Digest Authentication handled at present\n");
		return 1;
	}
	DEBUG("buf is %s\n", buf);
	digest_challenge = buf + 7;
	DEBUG("digest_challenge is %s\n", digest_challenge);
	
	DEBUG("Checking Authentication....\n");
	token = strtok(digest_challenge, ",");
	parse_auth_params(&incoming, token);
	while (token != NULL) {
		token = strtok(NULL, ",");
		parse_auth_params(&incoming, token);
	} 

	// generate a hash from local username password plus the nonce
	incoming.nc = "1";
	incoming.cnonce = "0a4f113b";
	generate_hash(&hash, &incoming);

	printf("The hash generated is: %s\n", hash);
	printf("nonce is: %s\n", incoming.nonce);
	printf("opaque is: %s\n", incoming.opaque);
	// use the reply to generate an authorisation request
	sprintf(reply, TEST_REQUEST_3, TEST_URI, local_ip, DEFAULT_PORT,
		TEST_USER, incoming.nonce, TEST_URI, &hash, incoming.opaque);

	return 0;
}


int parse_reply_status (char *request)
{
	char *http;
	char *code;
	char *message;
	int i;
	
	http = request;
	//remove leading whitespace
	while (isspace(*http)) {
		http++;
	}
	code = http;
	//move to the end of the http token
	while (!isspace(*code)) {
		code++;
	}
	*code = '\0';
	DEBUG("First token is %s\n", http);
	code++;
	//move past the next whitespace
	while (isspace(*code)) {
		code++;
	}
	message = code;
	// move to the end of the code token
	while (!isspace(*message)) {
		message++;
	}
	*message = '\0';
	DEBUG("code token is %s\n", code);
	message++;
	//move past the next whitespace
	while (isspace(*message)) {
		message++;
	}
	DEBUG("message is %s\n", message);

	if(strcmp(code, "401") == 0 ) {
		printf("401 received, now to find right option\n");	
		return 0;
	}
	printf("unexpected reply, stopping.\n");	
	return 1;
}

int parse_reply_option (char *buf)
{
	char *c = buf;
	int i, len = strlen(buf);
	char *key, *value;

	if (c == NULL) {
		return -1;
	}
	//Skip past non-alphanumeric characters
	while (isalnum(*c) == 0) {
		c++;
	}
	key = c;
	//Scan the string for the end of the key
	for ( i = 0; i < len; i++) {
		c = buf + i;
		if (*c == ':') {
			*c = '\0';
			c++;
			while (isspace(*c) != 0) {
				c++;
			}
			value = c;
			break;
		}
	}
	/* Next interpret the option line information.  We're only really
	 * interested in the Authorization header.
	 */
	DEBUG("Option line has key = %s, value = %s\n", key, value);
	if (!strcmp(key, "WWW-Authenticate")) {
		//parse authorization fields
		return check_authenticate(value);
	} else {
		//Currently we don't care about other fields
		DEBUG("Skipping option: %s\n", key);
	}
	return 1;
}



void parse_reply (char *request)
{
	int i, j = 0;
	char *c;
	char *request_line[MAXHEADERLINES];
	int len;
	char nonce[NONCE_LENGTH];
	char opaque[OPAQUE_LENGTH];

	request_line[j++] = request;
	len = strlen(request);
	for (i = 0; i < len; i++) {
		c = request + i;
		if ( *c == '\r' && *(c + 1) == '\n') {
			DEBUG("detected break at %i and %i\n", i, i + 1);
			//check if line is folded
			if ( *(c + 2) == ' ' || *(c + 2) == '\t') {
				// the line is folded so continue after break
				i += 2;
				DEBUG("detected folded line#####\n");
			} else {
				//otherwise put a break, and add a pointer
				*c = '\0';
				*(c + 1) = '\0';
				request_line[j++] = c + 2;
				DEBUG("add a pointer at index %i, total"
				" number of lines now is %i\n", i, j - 1);
			}
		}
	}
	// parse each line (now unfolded) that we have detected in the header
	if (parse_reply_status(request_line[0]) != 0) {
		DEBUG("skipping the option lines\n");
		return; // an invalid status line was received.
	}
	for (i = 1; i < j; i++) {
		DEBUG("parsing option line %i\n", i);
		if (request_line[i] == NULL) {
			break;
		}
		if (parse_reply_option(request_line[i]) == 0) {
			DEBUG("found the line we wanted, so stopping\n");
			return;
		}
	}
			
	return;
}

void setup_socket(struct addrinfo *servinfo, int *sockfd)
{
	struct addrinfo *p;
	char s[INET6_ADDRSTRLEN];

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((*sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(*sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

}


void test_query(int sockfd, struct addrinfo *servinfo, char *port, 
		char *request, char *uri, char *user, char *nonce,
		char *response, char *opaque)
{
	char buf[MAXREQUESTSIZE];
	char query[MAXREQUESTSIZE];
	int numbytes;
 

	setup_socket(servinfo, &sockfd);
	sprintf(query, request, uri, local_ip, port, user, nonce, uri,
		response, opaque);
	fprintf(stderr, "Sending Request:\n%s\n", query);
	if (send(sockfd, query, strlen(query), 0) == -1) {
		perror("send");
		exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXREQUESTSIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);
	parse_reply(buf);
	close(sockfd);
	printf("finished test_query\n");
	return;
}

int main(int argc, char *argv[])
{
	int sockfd;  
	struct addrinfo hints, *servinfo;
	int rv;
	char *port;
	char buf[MAXREQUESTSIZE];
	int numbytes;
	int i;

	if (argc < 2 || argc > 3) {
	    fprintf(stderr, "usage: client hostname [port]\n");
	    exit(1);
	}
	if (argc == 3) {
		port = argv[2]; //need to sanity check this value
	} else {
		port = DEFAULT_PORT;
	}

	if (rdb_open_db() < 0) {
		fprintf(stderr, "unable to access RDB\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	
	// TEST 1 - GET REQUEST w/ no authorization
	printf("############# TEST 1 ##########################\n");
	test_query(sockfd, servinfo, port, TEST_REQUEST_1, TEST_URI,
 		   TEST_USER, NULL, NULL, NULL);

	// TEST 2 - POST REQUEST w/ no authorization
	printf("############# TEST 2 ##########################\n");
	test_query(sockfd, servinfo, port, TEST_REQUEST_2, TEST_URI,
		   TEST_USER, NULL, NULL, NULL);

	// TEST 3 - GET REQUEST w/ wrong authorization
	printf("############# TEST 3 ##########################\n");
	test_query(sockfd, servinfo, port, TEST_REQUEST_3, TEST_URI,
		   TEST_USER, TEST_NONCE, TEST_RESPONSE, TEST_OPAQUE);

	// TEST 4 - GET REQUEST w/ wrong URI
	printf("############# TEST 4 ##########################\n");
	test_query(sockfd, servinfo, port, TEST_REQUEST_4, BAD_URI,
		   TEST_USER, NULL, NULL, NULL);

	// TEST 5 - GET REQUEST w/ correct authorization
	printf("############# TEST 5 ##########################\n");

	// send the initial query
	test_query(sockfd, servinfo, port, TEST_REQUEST_1, TEST_URI,
		   TEST_USER, TEST_NONCE, TEST_RESPONSE, TEST_OPAQUE);

	// the response is parsed within test_query, so at this point we're
	// ready to send a new request with authorization

	if (reply != NULL) {
		setup_socket(servinfo, &sockfd);
		if (send(sockfd, reply, strlen(reply), 0) == -1) {
			perror("send");
			exit(1);
		}

		if ((numbytes = recv(sockfd, buf, MAXREQUESTSIZE-1, 0)) == -1)
		{
		    perror("recv");
		    exit(1);
		}

		buf[numbytes] = '\0';

		printf("client: received '%s'\n", buf);
	}
	close(sockfd);

	// TEST 6 - Send MAX QUERIES + 1, verify the last fails
	printf("############# TEST 6 ##########################\n");
	for (i = 0; i <= MAX_REQUESTS_PER_PERIOD; i++) {
		// send the initial query
		test_query(sockfd, servinfo, port, TEST_REQUEST_1, TEST_URI,
			   TEST_USER, TEST_NONCE, TEST_RESPONSE, TEST_OPAQUE);

		// the response is parsed within test_query, so at this point
		// we're ready to send a new request with authorization

		if (reply != NULL) {
			setup_socket(servinfo, &sockfd);
			if (send(sockfd, reply, strlen(reply), 0) == -1) {
				perror("send");
			exit(1);
			}

			if ((numbytes = recv(sockfd, buf, MAXREQUESTSIZE-1, 0)) == -1)
			{
			    perror("recv");
			    exit(1);
			}

			buf[numbytes] = '\0';

			printf("client: received '%s'\n", buf);
		}
		close(sockfd);
	}
	
	printf("############# END OF TESTS ####################\n");
	freeaddrinfo(servinfo); // all done with this structure
	return 0;
}
