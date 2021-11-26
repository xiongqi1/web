/* agstatus.c                                                   */
/* ABurnell: Jul-2012, Access Gateway test utility.             */
/*           Test the AG_STATUS message.                        */
/* ------------------------------------------------------------ */
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

static char *meter_addr, *meter_port;
int sockfd;
struct addrinfo hints;
struct addrinfo *result;

#define BUF_SIZE	2048
unsigned char hbuf[BUF_SIZE]; /* main host data sen/recv buffer */
//unsigned char hbuf[2048];
#define MAX_RETRY_LIMIT			5

static int create_command_packet(void)
{
	int j;
	/* Format hbuf[] message */
#if 0
	(void) memset(hbuf, 0x00, BUF_SIZE);
	hbuf[0] = (unsigned char)0xAC;
	hbuf[1] = (unsigned char)0x00;
	hbuf[2] = (unsigned char)0x00;
	hbuf[3] = (unsigned char)0x74;
	hbuf[4] = (unsigned char)0xAA;
	return 5;
#endif
	(void)memset(hbuf, 0x00, 2048);
	strcat(hbuf, "--------------------------------------------\n\r");
	for (j = 97; j < 123; j++) {
		sprintf(hbuf, "%s%c1234567890b1234567890c1234567890d1234567890\n\r", hbuf, j);
	}
	return strlen(hbuf);
}

static void printPacket(unsigned char* msg, int len)
{
	unsigned char buf[BUF_SIZE], buf2[BUF_SIZE];
	int i, j = len/8, k = len % 8;
	long long int sn = 0;
	(void) memset(buf, 0x00, BUF_SIZE);
	(void) memcpy(buf, msg, len);
	printf("---- Metering Response raw packet ---\n");
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

	printf("get addr. info for %s:%s\n", meter_addr, meter_port);
retry_getaddress:
    s = getaddrinfo(meter_addr, meter_port, &hints, &result);
    if (s != 0 && retry++ > MAX_RETRY_LIMIT) {
		printf("getaddrinfo error for %s:%s, %s\n",
				meter_addr, meter_port, gai_strerror(s));
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
	int nb, bytes_to_send = 0, sent_bytes = 0;
	char *wp;
	ssize_t nn;

	struct sockaddr_in meter_server;
	struct hostent *host;
	int sockfd;
	int server_timeout, rd_cnt;

    if (argc < 2)
	{
		printf("Metering PDP emulator needs two arguments: meter ip address/port...\n");
		exit(1);
	}
	meter_addr = argv[1];
	meter_port = argv[2];
	
	printf("Metering PDP emulator start(%s,%s)...\n", meter_addr, meter_port);
	
	if (get_address_info() < 0) {
		printf("Failed to get address info, exit...\n");
		exit(1);
	}
	
	/* ----------------------------------------------------- */
	if ((sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) < 0) {
	//if((sockfd = socket(AF_INET,SOCK_STREAM, 0)) < 0) {
		printf("Create Socket Failed, exit...\n");
		freeaddrinfo(result);
		exit(1);
	}
	printf("Created Socket ...\n");

	//memset(&meter_server,0, sizeof(meter_server));
	//meter_server.sin_family      = AF_INET;
	//meter_server.sin_addr.s_addr = inet_addr(meter_addr);
	//meter_server.sin_port        = htons(atoi(meter_port));
	//bzero(&(meter_server.sin_zero),8);

	printf("Connecting to %s:%s...\n", meter_addr, meter_port);
	if(connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
	//if(connect(sockfd,(struct sockaddr *)&meter_server,sizeof(meter_server)) < 0) {
		printf("Connection attempt failed\n");
		printf("Err = %d, %s\n",errno, strerror(errno));
		(void)close(sockfd);
		printf("exit...\n");
		freeaddrinfo(result);
		exit(1);
	}
	printf("Connected\n");

	//sleep(1);
	
	/* SEND packet*/
	/* ==== */
	bytes_to_send = create_command_packet();
	
	/* send header */
	wp = &hbuf[0];
	bytes_to_send = strlen(wp);
	while (sent_bytes < bytes_to_send) {
		nn = write(sockfd,(const void *)wp, (size_t)bytes_to_send);
		if(nn < 0) {
			printf("Socket Send Failed, exit...\n");
			(void)close(sockfd);
			freeaddrinfo(result);
			exit(1);
		}
		nb =(int)nn;
		printf("Socket SENT, %d bytes\n",nb);
		sent_bytes += nb;
		bytes_to_send -= nb;
		wp += nb;
		sleep(1);
	}
	
	//goto quit;
	
	/* RECEIVE */
	/* ==== */
	#define SERVER_TIMEOUT_LIMIT	3 
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
			printf("Server disconnected! Stop Metering PDP Emul, exit...\n");
			(void)close(sockfd);
			freeaddrinfo(result);
			exit(1);
		}
		else {
			if( FD_ISSET( sockfd, &fdr ) )
			{
				printf("receiving packet\n");
				rd_cnt = recv(sockfd, hbuf, BUF_SIZE, 0);
				if (rd_cnt < 0) break;
				//else if (rd_cnt == 0) return -1;
				else if (rd_cnt == 0) continue;
				hbuf[rd_cnt] = 0;
				//printPacket(hbuf, rd_cnt);
				printf("\n%s\n", hbuf);
				server_timeout = 0;
			}
		}
	} while (1);
   
	/* Other stuff */
quit:
	(void)close(sockfd);
	printf("Metering PDP Emul done, exit...\n");
	freeaddrinfo(result);
	exit(0);
}
