/* 
 * 
 * Copyright 1997-2013 NComm, Inc.  All rights reserved.
 * 
 * 
 *                     *** Important Notice ***
 *            This notice may not be removed from this file.
 * 
 * The APS, E1, E3, ETHERNET OAM, PRI ISDN, SONET/SDH, SSM, T1/E1 LIU, T1, T3
 * software contained in this file may only be used within a 
 * valid license agreement between your company and NComm, Inc. The license 
 * agreement includes the definition of a PROJECT.
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



#ifndef TMSLINUXXLATE_H
#define TMSLINUXXLATE_H



/*--------------------------------------------------------------------------*/
/* Common values that are not specific, generally, to User/Kernel side
 * and the User may wish to tweak them.
 */
#if defined(_PLATFORM_Serpent)
	#define DEVICE_MAJOR			233
#else
	#define DEVICE_MAJOR			243
#endif

#define DEVICE_NAME			"tms"

#define FULL_DEVICE_NAME		"/dev/tms"



/*--------------------------------------------------------------------------*/
#define XLT_DEVICE_IOCODE_TYPE		unsigned int

#define XLT_IOCODE_MAGIC		'k'

#define XLT_DEVICE_IOCODE_SET(ioCode)	_IOWR(XLT_IOCODE_MAGIC, ioCode, void *)

#define XLT_DEVICE_IOCODE_GET(ioCode)	_IOC_NR(ioCode)



#ifndef __KERNEL__
/*--------------------------------------------------------------------------*/
/*------------------------- User-side Only ---------------------------------*/
/*--------------------------------------------------------------------------*/
/* These are all used in the tmsXlateUser.inc file.  The User-side part of
 * the TMS Translation Layer will use these.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <malloc.h>


/*--------------------------------------------------------------------------*/
/* These are named this way so that, hopefully, they won't clash with
 * anything the User may have defined, and so we know exactly where they
 * are being used.
 */
#define XLT_THREAD_TYPE				pthread_t

#define XLT_THREAD_CREATE(threadIDPtr, fooPtr, args)			\
	((pthread_create(threadIDPtr, NULL, (void *)fooPtr, args)) ?	\
		0 : pthread_detach(*(threadIDPtr)), 1)

#define XLT_THREAD_KILL(threadID)					\
	((pthread_cancel(threadID)) ? 0 : 1)

#define XLT_THREAD_INIT(arg)						\
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); 	  	\
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL)

#define XLT_CRITICAL_TYPE			pthread_mutex_t

#define XLT_CRITICAL_CREATE(lidptr)		\
			(pthread_mutex_init(lidptr, NULL)) ? 0 : 1

#define XLT_CRITICAL_DELETE(lid)		pthread_mutex_destroy(lid)

#define XLT_CRITICAL_LOCK(lid)			pthread_mutex_lock(lid)

#define XLT_CRITICAL_UNLOCK(lid)		pthread_mutex_unlock(lid)

#define XLT_SLEEP(secs)				sleep(secs)

#define XLT_DEVICE_HANDLE			int

#define XLT_DEVICE_OPEN(handle, devName)				\
		{XLT_DEVICE_HANDLE fhandle;				\
			fhandle = open(devName, O_RDWR, 0777);		\
			handle = (XLT_DEVICE_HANDLE)			\
				  ((fhandle < 0) ? 0 : fhandle);	\
		}

#define XLT_DEVICE_POSTOPEN(vPtr)		1

#define XLT_DEVICE_PRECLOSE(vPtr)		1

#define XLT_DEVICE_CLOSE(handle)					\
		{int retval;						\
			retval = close(handle);				\
			handle = (XLT_DEVICE_HANDLE)			\
				  ((retval < 0) ? handle : 0);		\
		}

#define XLT_DEVICE_SEND(handle, ioCode, recPtr)				\
		(ioctl(handle, ioCode, recPtr) < 0) ? 0 : 1


#else	/* __KERNEL__ */

/*--------------------------------------------------------------------------*/
/*----------------------- Kernel-side Only ---------------------------------*/
/*--------------------------------------------------------------------------*/
/* All Kernel-side work is already addressed via the general TMS port of
 * the Operating System.  What is present here, if anything, is used in the
 * tmsLINUXkmod.c file, or the tmsXlateKrnl.inc file
 */

#define XLT_KRNL_NOTIFY(event, ioCode)		tmsSendEvent(event)

#define XLT_KRNL_BLOCK(event, condition) 		\
		if (!tmsWaitEvent(event, condition))	\
			return(0);


#endif	/* __KERNEL__ */


#endif	/* TMSLINUXXLATE_H */

