/* 
 * 
 * Copyright 1997-2010 NComm, Inc.  All rights reserved.
 * 
 * 
 *                     *** Important Notice ***
 *                           --- V6.72 ---
 *            This notice may not be removed from this file.
 * 
 * The version 6.72 APS, E1, E3, ETHERNET OAM, PRI ISDN, SONET/SDH, SSM, T1/E1
 * LIU, T1, T3 software contained in this file may only be used 
 * within a valid license agreement between your company and NComm, Inc. The 
 * license agreement includes the definition of a PROJECT.
 * 
 * The APS, E1, E3, ETHERNET OAM, PRI ISDN, SONET/SDH, SSM, T1/E1 LIU, T1, T3
 * software is licensed only for the APS, E1, E3, ETHERNET OAM, PRI ISDN,
 * SONET/SDH, SSM, T1/E1 LIU, T1, T3 application 
 * and within the project definition in the license. Any use beyond this 
 * scope is prohibited without executing an additional agreement with 
 * NComm, Inc. Please refer to your license agreement for the definition of 
 * the PROJECT.
 * 
 * This software may be modified for use within the above scope. All 
 * modifications must be clearly marked as non-NComm changes.
 * 
 * If you are in doubt of any of these terms, please contact NComm, Inc. 
 * at sales@ncomm.com. Verification of your company's license agreement 
 * and copies of that agreement also may be obtained from:
 *  
 * NComm, Inc.
 * 130 Route 111  
 * Suite 201
 * Hampstead, NH 03841
 * 603-329-5221 
 * sales@ncomm.com
 * 
 */




#ifndef TMSUSER_H
#define TMSUSER_H


#ifndef __KERNEL__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <malloc.h>


struct _tmsUSRRecord {
	int fhandle;
};

extern struct _tmsUSRRecord tmsUSRRecord;


extern int tmsOPEN(void);

extern int tmsCLOSE(void);

extern int tmsKILL(void);

extern int tmsPEEK(void *adr, void *rdBuf, int size, int count);

extern int tmsPOKE(void *adr, void *wrBuf, int size, int count);


/*--------------------------------------------------------------------------*/
typedef struct _userlist {
	struct _userlist *head;		/* pointer to the head of a list */
	struct _userlist *tail;		/* pointer to the tail of a list */
	void *recptr;			/* ptr to record */
}USERLIST;


extern void _vfInit(USERLIST *lptr, void *recPtr);

extern USERLIST *_vfUNLink(USERLIST *lptr);

extern void _vfLink(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfLast(USERLIST *lptr);

extern USERLIST *_vfNext(USERLIST *lptr);

extern void _vfPutSafe(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfGetSafe(USERLIST *lptr);

extern USERLIST *_vfWalk(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfWalkR(USERLIST *lptr, USERLIST *recptr);

extern void *_vfGetRecord(USERLIST *lptr);

extern void _vfUNLinkMiddle(USERLIST *lptr, USERLIST *recptr);

#endif	/* __KERNEL__ */


/*--------------------------------------------------------------------------*/
#ifdef TE1_PACKAGE
	#define TE1_SHUTDOWN()	\
		((_te1LinCTRL(0, TE1LCTRL_SHUTDOWN) == SUCCESS) ? 1 : 0)
#else
	#define TE1_SHUTDOWN()	1
#endif

#ifdef TE1LIU_PACKAGE
	#define LIU1_SHUTDOWN()	\
		((_TE1liuLinCTRL(0, TE1LIULCTRL_SHUTDOWN) == SUCCESS) ? 1 : 0)
#else
	#define LIU1_SHUTDOWN()	1
#endif

#ifdef TE3_PACKAGE
	#define TE3_SHUTDOWN()	\
		((_te3LinCTRL(0, TE3LCTRL_SHUTDOWN) == SUCCESS) ? 1 : 0)
#else
	#define TE3_SHUTDOWN()	1
#endif

#ifdef OCN_PACKAGE
	#define OCN_SHUTDOWN()	\
		((_ocnLinCTRL(0, OCNLCTRL_SHUTDOWN, 0, 0) == SUCCESS) ? 1 : 0)
#else
	#define OCN_SHUTDOWN()	1
#endif

#ifdef ISDN_PACKAGE
	#define ISDN_SHUTDOWN()	\
		((_isdnLinCTRL(0, ISDNLCTRL_SHUTDOWN, 0, 0) == SUCCESS) ? 1 : 0)
#else
	#define ISDN_SHUTDOWN()	1
#endif

#ifdef SSM_PACKAGE
	#define SSM_SHUTDOWN()	\
		((_ssmLinCTRL(0, SSMLCTRL_SHUTDOWN) == SUCCESS) ? 1 : 0)
#else
	#define SSM_SHUTDOWN()	1
#endif

#ifdef ETH_PACKAGE
	#define ETH_SHUTDOWN()	\
		((_ethLinCTRL(0, ETHLCTRL_SHUTDOWN) == SUCCESS) ? 1 : 0)
#else
	#define ETH_SHUTDOWN()	1
#endif


/*--------------------------------------------------------------------------*/
typedef enum {
	TMS_IOCTL_TE1LINCTRL,
	TMS_IOCTL_TE1LINPOLL,
	TMS_IOCTL_TE1LINCLBK,

	TMS_IOCTL_LIU1LINCTRL,
	TMS_IOCTL_LIU1LINPOLL,
	TMS_IOCTL_LIU1LINCLBK,

	TMS_IOCTL_TE3LINCTRL,
	TMS_IOCTL_TE3LINPOLL,
	TMS_IOCTL_TE3LINCLBK,

	TMS_IOCTL_LIU3LINCTRL,
	TMS_IOCTL_LIU3LINPOLL,
	TMS_IOCTL_LIU3LINCLBK,

	TMS_IOCTL_OCNLINCTRL,
	TMS_IOCTL_OCNLINPOLL,
	TMS_IOCTL_OCNLINCLBK,

	TMS_IOCTL_ISDNLINCTRL,
	TMS_IOCTL_ISDNLINPOLL,
	TMS_IOCTL_ISDNLINCLBK,

	TMS_IOCTL_SSMLINCTRL,
	TMS_IOCTL_SSMLINPOLL,
	TMS_IOCTL_SSMLINCLBK,

	TMS_IOCTL_ETHLINCTRL,
	TMS_IOCTL_ETHLINPOLL,
	TMS_IOCTL_ETHLINCLBK,

	HDW_IOCTL_GENERAL,
	HDW_IOCTL_PEEKBYTE,
	HDW_IOCTL_PEEKWORD,
	HDW_IOCTL_PEEKLONG,
	HDW_IOCTL_POKEBYTE,
	HDW_IOCTL_POKEWORD,
	HDW_IOCTL_POKELONG

} TMS_IOCTL;

#define tmsIOCTL(cmd, ptr)	(ioctl(tmsUSRRecord.fhandle, cmd, ptr))


/*--------------------------------------------------------------------------*/
#define MAX_HDW_PAYLOAD		32

struct _hwRecord {
	int fcode;
	unsigned long p1;
	unsigned long p2;
	unsigned long p3;
	unsigned long p4;
	unsigned long p5;
	unsigned long p6;
	unsigned char dataPayload[MAX_HDW_PAYLOAD];
};

typedef struct _hwRecord HWRECORD;

#define HDW_REC_SIZE	sizeof(HWRECORD)


#endif	/* TMSUSER_H */

