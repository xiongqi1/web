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

#include <string.h> 

#define OK 0x00
#define AG_STATUS_MSG   0x1051
#define AG_STATUS_REPLY 0x9051


unsigned char hbuf[8192]; /* main host data sen/recv buffer */
int  nhbuf;

/* Globals */
/* ------- */
unsigned char gout[8192]; /* General purpose diagnostics buffer */


unsigned int GetU16(unsigned char *p) 
{
	unsigned int x;

	x = (unsigned int)*(p+1) << 8;
	x = x + (unsigned int)*(p+0);
	return(x);
}

int ag_logger(char *s)
{
	printf("%s\n",s);
	return(0);
}

static void ag_setmsg(void)
{
	/* Fromats hbuf[] message */
	hbuf[0] = (unsigned char)0x51;
	hbuf[1] = (unsigned char)0x10;
}

static void printPacket(unsigned char* msg, int len)
{
	unsigned char buf[1024], buf2[256];
	int i, j = len/8, k = len % 8;
	long long int sn = 0;
	(void) memset(buf, 0x00, 1024);
	(void) memcpy(buf, msg, len);
	printf("---- AG Status Response raw packet ---\n");
	for (i = 0; i < j; i++)	{
		printf("%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x\n",
				buf[i*8],buf[i*8+1],buf[i*8+2],buf[i*8+3],buf[i*8+4],buf[i*8+5],buf[i*8+6],buf[i*8+7]);
	}
	j = i;
	(void) memset(buf2, 0x00, 256);
	for (i = 0; i < k; i++)
		sprintf(buf2, "%s%02x, ", buf2, buf[j*8+i]);
	printf("%s\n\n", buf2);
	printf("---- AG Status Response contents ---\n");
	printf("  Packet ID      : %02x%02x\n", buf[1], buf[0]);
	printf("  Result Code    : %02x%02x, %s\n", buf[3], buf[2],
		   (buf[2] == 0x01? "OK" : buf[2] == 0x08? "NOT READY FOR COMMAND" :
		    buf[2] == 0x0A? "SOFTWARE ERROR" : "UNKNOWN ERROR"));
	sn = (buf[4] | ((long long int)buf[5] << 8) | ((long long int)buf[6] << 16) |
	      ((long long int)buf[7] << 24) | ((long long int)buf[8] << 32) |
	      ((long long int)buf[9] << 40) | ((long long int)buf[10] << 48) |
	      ((long long int)buf[11] << 56));
	printf("  Device SN      : %lld\n", sn);
	printf("  Statue Date    : %c%c%c%c%c%c\n", buf[12],buf[13],buf[14],buf[15],buf[16],buf[17]);
	printf("  Statue Time    : %c%c%c%c\n", buf[18],buf[19],buf[20],buf[21]);
	printf("  System Mode    : %02x, %s\n", buf[22], 
		   (buf[22] == 0x00? "No Service" : buf[22] == 0x01? "GSM/GPRS mode" :
		    buf[22] == 0x02? "EDGE" : buf[22] == 0x03? "UMTS/HSPA/LTE mode" :
		    buf[22] == 0x04? "Wi-Fi mode" : "Unknown mode"));
	if (buf[23] >= 1 && buf[23] <= 30)
	printf("  Signal RSSI    : %d, %d dBm\n", buf[23], (-113 + buf[23]*2));
	else
	printf("  Signal RSSI    : %d, %s\n", buf[23], 
		   (buf[23] == 0? "-113 dBm or less" : buf[23] == 31? "-51 dBm or greater" :
		    buf[23] == 99? "not detectable or SYSTEM_MODE Wi-fi" : "Unknown RSSI"));
	printf("  Signal BER     : %d, %s\n", buf[24], 
		   (buf[24] == 0? "< 0.2 %" : buf[24] == 1? "0.2% - 0.4%" :
		    buf[24] == 2? "0.4% - 0.8%" : buf[24] == 3? "0.8% - 1.6%" :
		    buf[24] == 4? "1.6% - 3.2%" : buf[24] == 5? "3.2% - 6.4%" :
		    buf[24] == 6? "6.4% - 12.8%" : buf[24] == 7? "> 12.8%" :
		    buf[24] == 99? "not detectable or SYSTEM_MODE Wi-fi" : "Unknown BER"));
}

int main(int argc, char *argv[])
{
	int j;
	int t_event; /* Timer event (simulated) */
	char t[5];
	int flag;
	int nb;
	ssize_t nn;

	struct sockaddr_in auth_server;
	struct hostent *host;
	int sockfd;
	int tmpres;
	int rtry = 1;
	int server_timeout, rd_cnt;

	(void)ag_logger("AG Status Test start...");
	
	/* AG processing on 192.168.1.1:20010                    */ 
	/* ----------------------------------------------------- */
	(void)ag_logger("Contacting AG 192.168.1.1:20010");
	if((sockfd = socket(AF_INET,SOCK_STREAM, 0)) < 0) {
		(void)ag_logger("Create Socket Failed");
		(void)ag_logger("AG Status Test exit...");
		exit(1);
	}
	(void)ag_logger("Created Socket ...");

	memset(&auth_server,0, sizeof(auth_server));
	auth_server.sin_family      = AF_INET;
	auth_server.sin_addr.s_addr = inet_addr("192.168.1.1");
	auth_server.sin_port        = htons(20010);
	bzero(&(auth_server.sin_zero),8);

	(void)ag_logger("Connecting...");
	if(connect(sockfd,(struct sockaddr *)&auth_server,sizeof(auth_server)) < 0) {
		(void)ag_logger("Connection attempt failed");
		printf("Errno = %d\n",errno);
		(void)close(sockfd);
		(void)ag_logger("AG Status Test exit...");
		exit(1);
	}
	(void)ag_logger("Connected");

	/* SEND */
	/* ==== */
	ag_setmsg();
   
	if((nn = write(sockfd,(const void *)&hbuf[0], (size_t)2) < 0)) {
		(void)ag_logger("Socket Send Failed");
		(void)close(sockfd);
		(void)ag_logger("AG Status Test exit...");
		exit(1);
	}
	(void)ag_logger("Socket Send [AG Status Request] - OK");
	nb =(int)nn;
	printf("Socket SEND, nb=%d\n",nb);

	/* RECEIVE */
	/* ==== */
	#define SERVER_TIMEOUT_LIMIT	10 
	do
	{
		fd_set fdr;
		int selected, wr_cnt;
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
		int nfds = 1 + sockfd;

		FD_ZERO( &fdr );
		FD_SET( sockfd, &fdr );
		selected = select( nfds, &fdr, NULL, NULL, &timeout );

		if( selected < 0 ) { (void)ag_logger("select() failed error"); continue; }

		else if ( selected == 0 )
		{
			(void)ag_logger("Server disconnected! Stop AG Status Client");
			(void)close(sockfd);
			(void)ag_logger("AG Status Test exit...");
			exit(1);
		}
		else {
			if( FD_ISSET( sockfd, &fdr ) )
			{
				(void)ag_logger("receiving packet");
				rd_cnt = recv(sockfd, hbuf, 8192, 0);
				if (rd_cnt < 0) break;
				else if (rd_cnt == 0) return -1;
				hbuf[rd_cnt] = 0;
				printPacket(hbuf, rd_cnt);
				server_timeout = 0;
			}
		}
	} while (1);
   
	/* Other stuff */

	(void)close(sockfd);
	(void)ag_logger("AG Status Test exit...");
	return(OK);
}
