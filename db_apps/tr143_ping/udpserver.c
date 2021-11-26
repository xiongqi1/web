/* UDP Echo server */

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char shortopts[] = "p:l:d:r:s:i:I:t:a:v?";

typedef struct TEchoPack{
/*	UDP part
	unsigned short SourcePort;
	unsigned short DestinationPort;
	unsigned short Length;
	unsigned short Checksum;
	*/
	unsigned int TestGenSN;
	unsigned int TestRespSN;
	unsigned int TestRespRecvTimeStamp;
	unsigned int TestRespReplyTimeStamp;
	unsigned int TestRespReplyFailureCount;
} __attribute__((packed)) TEchoPack;

// call gettimeofday, convert to int64_t
int64_t gettimeofday64()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec*1000000 +tv.tv_usec;
}

void usage(char **argv)
{
	fprintf(stderr, "\nUsage: %s -s <srcIP> [-p srcPort] [-d delay] [-v]  \n", argv[0]);
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-l local IPv4 address the server is binding to\n");
	fprintf(stderr, "\t-p local UDP port the server is listening. Default value is 5000\n");
	fprintf(stderr, "\t-d delay(ms) for each ping\n");
	fprintf(stderr, "\t-r delay(ms) add receive path delay\n");
	fprintf(stderr, "\t-s delay(ms) add send path delay\n");
	fprintf(stderr, "\t-i TestGenSN, simulate client send path loss\n");
	fprintf(stderr, "\t-I TestGenSN, simulate client receive path loss\n");
	fprintf(stderr, "\t-a TestGenSN, postponse this packet\n");
	fprintf(stderr, "\t-t delay(ms), after this timer turn over, timer\n");
	fprintf(stderr, "\t-v increase the verbosity\n");
	fprintf(stderr, "\t-? display this message\n");
	fprintf(stderr, "\n");
}
#define MAX_PACKET_SIZE 65505
char mesg[MAX_PACKET_SIZE];
char mesg2[sizeof(TEchoPack)];

typedef struct Session
{
	unsigned int m_remote_ip; // network order
	unsigned short m_remote_port;	//network order
	unsigned int m_TestRespSN;
	unsigned int m_TestGenSN;
	unsigned int m_failureCount;
	time_t	 	m_lasttimestamp;// last modification
}Session;

#define MAX_SESSION 1024

#define SESSION_TIMEOUT 60

Session g_session[MAX_SESSION];

int g_session_count =0;

//find or create session for  remote ip and port
Session * find_session(unsigned int ip, unsigned short port, int TestGenSN)
{
	int i;
	int empty_session=-1;
	time_t current_time = time(NULL);
	for(i =0; i< g_session_count; i++)
	{
		if( g_session[i].m_remote_ip == ip && g_session[i].m_remote_port == port)
		{

			g_session[i].m_TestGenSN = TestGenSN;
			if(TestGenSN <= 2)
			{
				g_session[i].m_TestRespSN =TestGenSN;
				g_session[i].m_failureCount =0;
			}
			g_session[i].m_lasttimestamp = time(NULL);

			return &g_session[i];
		}
		else
		{
			if( (current_time - g_session[i].m_lasttimestamp)  > SESSION_TIMEOUT )
			{
				g_session[i].m_remote_ip =0;
				g_session[i].m_remote_port =0;
				empty_session = i;
			}
		}
	}
	if (empty_session < 0)
	{
		if( g_session_count < MAX_SESSION)
		{
			empty_session = g_session_count++;
		}
		else
		{
			return NULL;
		}
	}

	g_session[empty_session].m_remote_ip = ip;
	g_session[empty_session].m_remote_port = port;
	g_session[empty_session].m_TestGenSN= TestGenSN;
	g_session[empty_session].m_TestRespSN =1;
	g_session[empty_session].m_failureCount =0;
	g_session[empty_session].m_lasttimestamp= time(NULL);
	return &g_session[empty_session];
}

#define IGNORE_METHOD_UKNOWN 	 0
#define IGNORE_METHOD_KEEP_RESPSN 1
#define IGNORE_METHOD_ADD_RESPSN 2

struct TestGenSN_Record
{
	int TestGenSN;
	int Method;
};

int main(int argc, char**argv)
{
	int sockfd,n,ret;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	int verbosity = 0;
	unsigned cnt = 0;;
	TEchoPack *pPack = (TEchoPack *)mesg;
	char *localIP = "127.0.0.1";
	unsigned short localport = 5000;
	//int TestRespSN =1;
	int TestGenSN =0;
	int failureCount =0;
	int delay =0;
	int receive_path_delay =0;
	int send_path_delay =0;

	struct TestGenSN_Record ignored_TestGenSN[20];
	int ignored_TestGenSN_num =0;


	int postponse_TestGenSN = -1;
	int clock_turnover_timeout = 0;
	int64_t start_time = gettimeofday64(); // - 0xff800000;
	int64_t duration;

	int i;
	int ignored =0;

	/* Check the input parameter */
	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'l':
				localIP = optarg;
				break;
			case 'p':
				localport = atoi(optarg);
				break;
			case 'd':
				delay= atoi(optarg);
				break;
			case 'r':
				receive_path_delay = atoi(optarg);
				break;
			case 's':
				send_path_delay = atoi(optarg);
				break;
			case 'i':
				ignored_TestGenSN[ignored_TestGenSN_num].TestGenSN= atoi(optarg);
				ignored_TestGenSN[ignored_TestGenSN_num].Method= IGNORE_METHOD_KEEP_RESPSN;
				ignored_TestGenSN_num++;
				break;
			case 'I':
				ignored_TestGenSN[ignored_TestGenSN_num].TestGenSN= atoi(optarg);
				ignored_TestGenSN[ignored_TestGenSN_num].Method=IGNORE_METHOD_ADD_RESPSN;
				ignored_TestGenSN_num++;
				break;

			case 't':
				clock_turnover_timeout = atoi(optarg)*1000;
				break;
			case 'a':
				postponse_TestGenSN= atoi(optarg);
				break;
			case 'v':
				verbosity++ ;
				break;
			case '?':
				usage(argv);
				return 2;
		}
	}


	/* Setup the server socket */
	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;


	inet_aton(localIP, (struct in_addr *)&servaddr.sin_addr.s_addr);
	servaddr.sin_port = htons(localport);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

	// Details
	if(verbosity > 0)
	{
	   printf("UDP Echo server: local IP: %s port: %d \n", localIP, localport);
	}


	/* loop */

	for (;;)
	{

		len = sizeof(cliaddr);
		n = recvfrom(sockfd,mesg,MAX_PACKET_SIZE, 0, (struct sockaddr*)&cliaddr, &len);
		if(n > 0)
		{
			Session *current_session;
			if(clock_turnover_timeout >0 )
			{

				start_time = gettimeofday64() - (0x100000000LLU - clock_turnover_timeout);
				clock_turnover_timeout =0;

			}
			if(receive_path_delay)
			{
				usleep(receive_path_delay*1000);
			}

			TestGenSN= htonl(pPack->TestGenSN);

			current_session = find_session(cliaddr.sin_addr.s_addr, cliaddr.sin_port, TestGenSN);
			if(current_session ==NULL) continue;
			cnt++;
resend:


			if( postponse_TestGenSN == TestGenSN)
			{
				memcpy(mesg2, mesg, sizeof(TEchoPack));
				continue;
			}

			duration = gettimeofday64() - start_time;
			pPack->TestRespRecvTimeStamp = htonl((unsigned int)duration);
			pPack->TestRespReplyFailureCount = htonl(current_session->m_failureCount);
			pPack->TestRespSN = htonl(current_session->m_TestRespSN);



			if(verbosity > 0)
			{
				printf("receive from %s:%d, TestGenSN=%d, TestRespSN=%d, duration=%llx\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), TestGenSN, current_session->m_TestRespSN,duration);
			}
			if(delay)
			{
				usleep(delay*1000);
			}
			duration = gettimeofday64() - start_time;
			pPack->TestRespReplyTimeStamp= htonl((unsigned int)duration);

			if(send_path_delay)
			{
				usleep(send_path_delay*1000);
			}

			ignored =0;

			for(i =0; i< ignored_TestGenSN_num; i++)
			{
				if(ignored_TestGenSN[i].TestGenSN == TestGenSN)
				{
					ignored = ignored_TestGenSN[i].Method;
					break;
				}
			}
			if(ignored)
			{
				printf("packet TestGenSN=%d, discarded\n", TestGenSN);
				//current_session->m_failureCount++;
				if(ignored == IGNORE_METHOD_ADD_RESPSN)
				{
					current_session->m_TestRespSN++;
				}

			}
			else
			{
				sendto(sockfd,mesg,n,0, (struct sockaddr*)&cliaddr, len);
				current_session->m_TestRespSN++;

			}
			if( (postponse_TestGenSN+1) ==  TestGenSN)
			{
				memcpy(mesg, mesg2, sizeof(TEchoPack));
				TestGenSN= htonl(pPack->TestGenSN);
				//printf("%d, %d\n",postponse_TestGenSN,TestGenSN );
				goto resend;
			}
			printf("Packets received so far: %d\n",cnt);
		}
		else
		{
			failureCount++;

			if(verbosity > 0)
			{
			   perror("Warning: recv error ");
			}
		}
	}

	// Probably never run.
	return 0;
}
