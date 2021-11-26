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

#include "utils.h"
#include "tmsAPI.h"
#include "tmsUSER.h"


struct _tmsUSRRecord tmsUSRRecord = {-1};



/*--------------------------------------------------------------------------*/
#include <signal.h>

static void _tms_SIGSEGV_handler(int signal, siginfo_t *sPtr, void *context)
{
char *str1, *str2;
int code;

	str1 = (sPtr->si_code & 0xffff0000) ? "Kernel-level" : "User-level";
	code = sPtr->si_code & 0xffff;

	switch (code) {
	case (SEGV_MAPERR & 0xffff):
		str2 = "Address not mapped to object";
		break;
	case (SEGV_ACCERR & 0xffff):
		str2 = "Invalid permissions for mapped object";
		break;
	default:
		str2 = "Unknown SEGV code";
		break;
	}

	NTCSLOG_ERR("\n\rTMS %s SIGSEGV: Error %d, %s Addr %p\n\r\t%s\n\r",
			str1, sPtr->si_errno, (sPtr->si_errno & 0x02) ?
			"Writing" : "Reading", sPtr->si_addr, str2);

	_exit(-1);
}


/*--------------------------------------------------------------------------*/
/* Open the file-descriptor for the TMS Kernel-module driver.
 */
int tmsOPEN(void)
{
struct sigaction sa;

	/* return benignly if it's already open
	 */
	if (tmsUSRRecord.fhandle >= 0)
		return(1);

	tmsUSRRecord.fhandle = -1;

	tmsUSRRecord.fhandle = open(FULL_DEVICE_NAME, O_RDWR, 0777);

	if (tmsUSRRecord.fhandle < 0) {
		NTCSLOG_ERR("Failed to open device %s: %s\n",
				FULL_DEVICE_NAME, strerror(errno));
		return(0);
	}

	sigemptyset(&sa.sa_mask);
	sa.sa_flags     = SA_SIGINFO;
	sa.sa_sigaction = _tms_SIGSEGV_handler;

	if (sigaction(SIGSEGV, &sa, NULL))
		NTCSLOG_ERR("Failed to attach TMS SIGSEGV handler\n");

	return(1);
}


/*--------------------------------------------------------------------------*/
/* Closes the file-descriptor for the TMS Kernel-module driver.  The TMS may
 * still be running on the kernel-side if the App desires.  To cease some,
 * or all TMS operations gracefully, the App should perform the proper TMS
 * tear-down sequences that are documented in the API manual.  Alternately,
 * the App can forcefully, and abruptly kill all TMS operation with the
 * tmsKILL() call.
 */
int tmsCLOSE(void)
{
	/* return benignly if it's already closed
	 */
	if (tmsUSRRecord.fhandle < 0)
		return(1);

	if (close(tmsUSRRecord.fhandle) < 0) {
		NTCSLOG_ERR("Failed to close device %s: %s\n",
				FULL_DEVICE_NAME, strerror(errno));
		return(0);
	}

	tmsUSRRecord.fhandle = -1;

	return(1);
}


/*--------------------------------------------------------------------------*/
/* This provides an easy, user-side interface to terminate ALL krnl-side
 * TMS module operations.  This is for those circumstances where a crashed
 * user-App needs to regain control of the TMS.  This causes a full shutdown
 * of the krnl-side TMS module.  The kernel-module is NOT unloaded.
 */
int tmsKILL(void)
{
int retval = 1;

	/* SHUTDOWN only needs to be called once, regardless of how many
	 * TMS packages are installed or running.  Extra SHUTDOWN calls are
	 * redundant and return benignly.  Since I don't know which TMS
	 * packages are installed and running, I'll just hit them all.
	 */
	retval &= TE1_SHUTDOWN();
	retval &= LIU1_SHUTDOWN();
	retval &= TE3_SHUTDOWN();
	retval &= OCN_SHUTDOWN();
	retval &= ISDN_SHUTDOWN();
	retval &= SSM_SHUTDOWN();
	retval &= ETH_SHUTDOWN();

	return(retval);
}


/*--------------------------------------------------------------------------*/
/* A general purpose hardware platform read/write routine.
 */
int tmsPEEK(void *adr, void *rdBuf, int size, int count)
{
int max, cmd;
HWRECORD hwRec;
HWRECORD *hwPtr = &hwRec;


	hwPtr->fcode = 0;
	hwPtr->p1 = (ULONG)adr;
	hwPtr->p2 = (ULONG)count;
	hwPtr->p4 = hwPtr->p5 = hwPtr->p6 = 0;


	switch (size) {
	case 1:
	case 'b':
		cmd = HDW_IOCTL_PEEKBYTE;
		max = MAX_HDW_PAYLOAD;
		break;

	case 2:
	case 'w':
		cmd = HDW_IOCTL_PEEKWORD;
		max = MAX_HDW_PAYLOAD/sizeof(unsigned short);
		count *= sizeof(unsigned short);
		break;

	case 4:
	case 'l':
		cmd = HDW_IOCTL_PEEKLONG;
		max = MAX_HDW_PAYLOAD/sizeof(unsigned long);
		count *= sizeof(unsigned long);
		break;

	default:
		return(0);
	}


	hwPtr->p2 = (hwPtr->p2 > max) ? max : hwPtr->p2;

	if (tmsIOCTL(cmd, hwPtr) < 0)
		return(0);

	memcpy(rdBuf, hwPtr->dataPayload, count);

	return(1);
}


/*--------------------------------------------------------------------------*/
/* A general purpose hardware platform write routine.
 */
int tmsPOKE(void *adr, void *wrBuf, int size, int count)
{
int max, cmd;
HWRECORD hwRec;
HWRECORD *hwPtr = &hwRec;


	hwPtr->fcode = 0;
	hwPtr->p1 = (ULONG)adr;
	hwPtr->p2 = (ULONG)count;
	hwPtr->p4 = hwPtr->p5 = hwPtr->p6 = 0;


	switch (size) {
	case 1:
	case 'b':
		cmd = HDW_IOCTL_POKEBYTE;
		max = MAX_HDW_PAYLOAD;
		break;

	case 2:
	case 'w':
		cmd = HDW_IOCTL_POKEWORD;
		max = MAX_HDW_PAYLOAD/sizeof(unsigned short);
		count *= sizeof(unsigned short);
		break;

	case 4:
	case 'l':
		cmd = HDW_IOCTL_POKELONG;
		max = MAX_HDW_PAYLOAD/sizeof(unsigned long);
		count *= sizeof(unsigned long);
		break;

	default:
		return(0);
	}


	hwPtr->p2 = (hwPtr->p2 > max) ? max : hwPtr->p2;

	memcpy(hwPtr->dataPayload, wrBuf, count);

	if (tmsIOCTL(cmd, hwPtr) < 0)
		return(0);

	return(1);
}

