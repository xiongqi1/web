#ifndef __PING_H
#define __PING_H

#include <time.h>
#include "comms.h"


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
#ifdef _KERNEL_UDPECHO_TIMESTAMP_
	int64_t	   kgsi;
	int64_t 	   kgri;
	int64_t	   gre_gsi;
	int64_t 	   gre_gri;
	
#endif

} __attribute__((packed)) TEchoPack;



int ping(TParameters *pParameters, TConnectSession *pSession);



#endif
