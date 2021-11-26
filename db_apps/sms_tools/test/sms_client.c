#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <sys/ioctl.h> 
#include <unistd.h>
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <time.h>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h> 

#define OK 0x00

static char *server_addr, *server_port, *sms_dest, *sms_msg;
int sockfd;
struct addrinfo hints;
struct addrinfo *result;

#define BUF_SIZE	1024
unsigned char hbuf[BUF_SIZE]; /* main host data sen/recv buffer */
#define MAX_RETRY_LIMIT			5

static void create_command_packet(void)
{
	/* Format hbuf[] message */
	(void) memset(hbuf, 0x00, BUF_SIZE);
	sprintf(hbuf, "<Destination Number>%s</Destination Number>\n<Message Body>%s</Message Body>", sms_dest, sms_msg);
	printf("command packet: %s\n\n", hbuf);
}

static void printPacket(unsigned char* msg, int len)
{
	unsigned char buf[BUF_SIZE], buf2[BUF_SIZE];
	int i, j = len/8, k = len % 8;
	long long int sn = 0;
	(void) memset(buf, 0x00, BUF_SIZE);
	(void) memcpy(buf, msg, len);
	printf("---- raw packet ---\n");
	for (i = 0; i < j; i++)	{
		printf("%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
				buf[i*8],buf[i*8+1],buf[i*8+2],buf[i*8+3],buf[i*8+4],buf[i*8+5],buf[i*8+6],buf[i*8+7]);
	}
	j = i;
	(void) memset(buf2, 0x00, BUF_SIZE);
	for (i = 0; i < k; i++)
		sprintf(buf2, "%s%02x, ", buf2, buf[j*8+i]);
	printf("%s\n\n", buf2);
}

static int get_address_info(void)
{
	int retry = 0;
	int s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    	/* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; 	/* TCP socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          	/* Any protocol */

	printf("get addr. info for %s:%s\n", server_addr, server_port);
retry_getaddress:
    s = getaddrinfo(server_addr, server_port, &hints, &result);
    if (s != 0 && retry++ > MAX_RETRY_LIMIT) {
		printf("getaddrinfo error for %s:%s, %s\n",
				server_addr, server_port, gai_strerror(s));
		return -1;
    }
    else if (s != 0)
    {
		goto retry_getaddress;
    }
	return 0;
}

int main(int argc, char *argv[])
{
	int j;
	char t[5];
	int nb;
	ssize_t nn;

	struct sockaddr_in sms_server;
	struct hostent *host;
	int sockfd;
	int server_timeout, rd_cnt;

    if (argc < 4)
	{
		printf("SMS Client needs 4 arguments: server address/port destination msg\n");
		exit(1);
	}
	server_addr = argv[1];
	server_port = argv[2];
	sms_dest = argv[3];
	sms_msg = argv[4];
	
	printf("\n---------------------------------------------------------------------------\n");
	printf("SMS Client start...\n", server_addr, server_port);
	printf("      server address : %s\n", server_addr);
	printf("      server port    : %s\n", server_port);
	printf("      destination no : %s\n", sms_dest);
	printf("      message body   : %s\n", sms_msg);
	printf("---------------------------------------------------------------------------\n\n");
	
	if (get_address_info() < 0) {
		printf("Failed to get address info, exit...\n");
		exit(1);
	}
	if (strlen(sms_dest) <= 0) {
		printf("Wrong destination number, exit...\n");
		exit(1);
	}
	if (strlen(sms_msg) <= 0) {
		printf("Empty message body, exit...\n");
		exit(1);
	}

	/* ----------------------------------------------------- */
	if ((sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) < 0) {
		printf("Create socket Failed, exit...\n");
		freeaddrinfo(result);
		exit(1);
	}
	printf("Created socket ...\n");

	printf("Connecting to %s:%s...\n", server_addr, server_port);
	if(connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
		printf("Connection attempt failed\n");
		printf("Err = %d, %s\n",errno, strerror(errno));
		(void)close(sockfd);
		printf("Exit...\n");
		freeaddrinfo(result);
		exit(1);
	}
	printf("Connected\n\n");

	/* SEND packet*/
	/* ==== */
	create_command_packet();
   
	if((nn = write(sockfd,(const void *)&hbuf[0], strlen(hbuf)+1) < 0)) {
		printf("Socket send failed, exit...\n");
		(void)close(sockfd);
		freeaddrinfo(result);
		exit(1);
	}
	printf("Sent command request\n");
	nb =(int)nn;

	/* RECEIVE */
	/* ==== */
	#define SERVER_TIMEOUT_LIMIT	30
	do
	{
		fd_set fdr;
		int selected, wr_cnt;
		struct timeval timeout = { .tv_sec = SERVER_TIMEOUT_LIMIT, .tv_usec = 0 };
		int nfds = 1 + sockfd;

		FD_ZERO( &fdr );
		FD_SET( sockfd, &fdr );
		selected = select( nfds, &fdr, NULL, NULL, &timeout );

		if( selected < 0 ) { printf("select() failed error\n"); continue; }

		else if ( selected == 0 )
		{
			printf("Server disconnected! Stop SMS Client, exit...\n");
			(void)close(sockfd);
			freeaddrinfo(result);
			exit(1);
		}
		else {
			if( FD_ISSET( sockfd, &fdr ) )
			{
				//printf("Receiving result packet\n\n");
				rd_cnt = recv(sockfd, hbuf, BUF_SIZE, 0);
				if (rd_cnt <= 0) break;
				hbuf[rd_cnt] = 0;
				//printPacket(hbuf, rd_cnt);
				printf("Received result packet: %s\n\n", hbuf);
				server_timeout = 0;
			}
		}
	} while (1);
   
	/* Other stuff */

	(void)close(sockfd);
	printf("SMS CLient done, exit...\n");
	freeaddrinfo(result);
	exit(0);
}
