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




/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*
 *	This is the User-side TMS Translation Layer code.
 *
 *	This file gets #included in the tmsXlate.c file when
 *	compiling the User-side App.
 */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/



#include "tmsLinkUser.h"



/*--------------------------------------------------------------------------*/
struct _tmsAPIRecord {
	XLT_DEVICE_HANDLE fhandle;
	USERLIST PackageList;
} tmsAPIRecord = {(XLT_DEVICE_HANDLE)0};


struct _apiLine {
	_UI32_ line;
	_UI32_ fcode;
	_UI32_ id1;
	_UI32_ id2;
	int (*clbk)(_UI32_ line, _UI32_ fcode, ...);
	USERLIST next;
};

typedef struct _apiLine TMS_API_LINE;



/* The following is bound to the apiPackageRecord in it's usrkrnlData field.
 * It is malloced in apiStart and freed in apiUNStart.  It is used by the
 * user-side callback mechanism to track all TMS_API_LINE records for a
 * given TMS Package.
 */
struct _apiLineList {
	XLT_THREAD_TYPE clbkThreadID;
	TMS_API_LINE initial;
	TMS_API_LINE *clbkLinePtr;
	TMS_API_PACKAGE *pkgPtr;
	USERLIST LineList;
	USERLIST NextPackage;
};

typedef struct _apiLineList TMS_API_LINELIST;



/*--------------------------------------------------------------------------*/
/* Run as a process waiting for the blocked ioCtl.
 *
 * The only reason we pass the line/fcode/id1/id2 values in the clbk-thread,
 * instead of all-zeros, is so the kernel-side can make a log entry upon
 * user-side CLBK-Enter into the Translation Layer.  If the log-level for
 * the line was enabled, then we can track it.  If we just pass all-zeros,
 * then we cannot filter the CLBK-Enter log entries.  Using this method,
 * we can log the user-side CLBK-Exit from the Translation Layer and the
 * corresponding user-side CLBK-Enter back into the Translation Layer.
 * This would not be possible if we passed all-zeros for the parameters.
 */
static int apiCLBKthread(void *arg)
{
TMS_API_PACKAGE *pkgPtr = arg;
TMS_API_LINELIST *lineList = pkgPtr->usrkrnlData;
TMS_API_LINE *lPtr;


	XLT_THREAD_INIT(arg);

	while (1) {

		if ((lPtr = lineList->clbkLinePtr) == NULL)
			lPtr = &lineList->initial;

		if (!tmsAPIsrc(pkgPtr->clbkType, lPtr->line, lPtr->fcode,
		     lPtr->id1, lPtr->id2, 0, 0, 0, 0, 0, 0)) {

			/* Nothing we can do if there is an error,
			 * so give an indication and sleep for a bit.
			 */
			if (lPtr != &lineList->initial) {

				fprintf(stderr, "Retrying tmsAPIsrc() "
					"in apiCLBKthread()\n");
			}

			XLT_SLEEP(1);	/* in seconds */
		}
	}

	return(1);
}


/*--------------------------------------------------------------------------*/
/* Create a new line record.
 */
static void *_newLineRecord(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{
TMS_API_LINE *linePtr;


	pkgPtr = pkgPtr;	/* unused */

	if ((linePtr = malloc(sizeof(TMS_API_LINE))) != NULL) {

		memset(linePtr, 0, sizeof(TMS_API_LINE));

		linePtr->line = recPtr->line;
		linePtr->id1  = recPtr->id1;
		linePtr->id2  = recPtr->id2;

		_vfInit(&linePtr->next, linePtr);
	}

	return(linePtr);
}


/*--------------------------------------------------------------------------*/
/* Delete the line record.
 */
static void _delLineRecord(TMS_API_LINE *lPtr)
{
	free((void *)lPtr);
}


/*--------------------------------------------------------------------------*/
/* Link the line record.
 */
static void _linkLineRecord(TMS_API_PACKAGE *pkgPtr, TMS_API_LINE *lPtr)
{
TMS_API_LINELIST *lineList = pkgPtr->usrkrnlData;

	_vfLink(&lineList->LineList, &lPtr->next);
}


/*--------------------------------------------------------------------------*/
/* Unlink the line record.
 */
static void _unlinkLineRecord(TMS_API_PACKAGE *pkgPtr, TMS_API_LINE *lPtr)
{
TMS_API_LINELIST *lineList = pkgPtr->usrkrnlData;

	_vfUNLinkMiddle(&lineList->LineList, &lPtr->next);
}


/*--------------------------------------------------------------------------*/
/* Return the line record from the linked-list
 */
static void *_getLineRecord(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{
USERLIST *lPtr = NULL;
TMS_API_LINE *linePtr;
TMS_API_LINELIST *lineList;


	if ((lineList = pkgPtr->usrkrnlData) == NULL)
		return(NULL);


	while ((lPtr = _vfWalk(&lineList->LineList, lPtr)) != NULL) {

		linePtr = _vfGetRecord(lPtr);

		if ((linePtr->line == recPtr->line) &&
		    (linePtr->id1  == recPtr->id1)   &&
		    (linePtr->id2  == recPtr->id2)) {

			return(linePtr);
		}
	}

	return(NULL);
}


/*--------------------------------------------------------------------------*/
/* Cleans up everything on the user-side related to one Package.
 * This is called from apiUNStart and _delAllPackages.
 */
static int _delPackage(TMS_API_PACKAGE *pkgPtr)
{
USERLIST *lptr;
TMS_API_LINELIST *lineList;


	/* benign if it's already down
	 */
	if (!pkgPtr->init)
		return(1);


	pkgPtr->init = 0;

	if ((lineList = pkgPtr->usrkrnlData) == NULL) {

		pkgPtr->init = 1;

		return(0);
	}


	if (!XLT_THREAD_KILL(lineList->clbkThreadID)) {

		pkgPtr->init = 1;

		return(0);
	}


	while ((lptr = _vfUNLink(&lineList->LineList)) != NULL)
		_delLineRecord(_vfGetRecord(lptr));


	_vfUNLinkMiddle(&tmsAPIRecord.PackageList, &lineList->NextPackage);


	free(lineList);

	pkgPtr->usrkrnlData = NULL;

	return(1);
}


/*--------------------------------------------------------------------------*/
/* Cleans up everything on the user-side related to all Packages.
 * This is called from tmsKILL.
 */
static int _delAllPackages(void)
{
USERLIST *lptr;
TMS_API_LINELIST *lineList;
int retval = 1;


	while ((lptr = _vfUNLink(&tmsAPIRecord.PackageList)) != NULL) {

		lineList = _vfGetRecord(lptr);

		if (!_delPackage(lineList->pkgPtr)) {

			retval = 0;
		}
	}

	return(retval);
}


/*--------------------------------------------------------------------------*/
/* The following are all used by the API Tables of the various TMS Packages.
 *
 * Starts the user-side callback-thread that is awaiting callbacks from TMS.
 * Runs only if it's user-side pre-processing.
 */
static API_RESULT apiStart(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{
TMS_API_LINELIST *lineList;


	if (!src || !pre)
		return(API_COMPLETE);


	/* benign if it's already up
	 */
	if (pkgPtr->init)
		return(API_COMPLETE);


	pkgPtr->init = 1;


	if ((lineList = malloc(sizeof(TMS_API_LINELIST))) == NULL) {

		pkgPtr->init = 0;

		return(API_TERMINATE);
	}

	memset(lineList, 0, sizeof(TMS_API_LINELIST));

	_vfInit(&lineList->LineList, NULL);
	_vfInit(&lineList->NextPackage, lineList);

	pkgPtr->usrkrnlData = lineList;


	/* Prevent a null-dereference on the very first
	 * CLBK-Enter of the CLBK-thread
	 */
	lineList->initial.line  = 0xFFFFFFFF;
	lineList->initial.fcode = 0xFFFFFFFF;
	lineList->initial.id1   = 0xFFFFFFFF;
	lineList->initial.id2   = 0xFFFFFFFF;
	lineList->clbkLinePtr   = &lineList->initial;
	lineList->pkgPtr	= pkgPtr;


	if (!XLT_THREAD_CREATE(&lineList->clbkThreadID,
				(void *)apiCLBKthread, pkgPtr)) {

		pkgPtr->usrkrnlData = NULL;

		free(lineList);

		pkgPtr->init = 0;

		return(API_TERMINATE);
	}

	_vfLink(&tmsAPIRecord.PackageList, &lineList->NextPackage);

	return(API_COMPLETE);
}


/*--------------------------------------------------------------------------*/
/* Kill the user-side callback-thread that apiStart fired up.
 * Runs only if it's user-side post-processing.
 */
static API_RESULT apiUNStart(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{

	recPtr = recPtr;	/* ignored */
	apiPtr = apiPtr;


	if (!src || pre)
		return(API_COMPLETE);

	return((_delPackage(pkgPtr)) ? API_COMPLETE : API_TERMINATE);
}


/*--------------------------------------------------------------------------*/
/* Bind the user-side callback to its line record.
 * Runs only if it's user-side post-processing.
 */
static API_RESULT apiReg(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{
TMS_API_LINE *lPtr;


	if (!src || pre)
		return(API_COMPLETE);


	/* assure it's not already registered for this line
	 */
	if (_getLineRecord(recPtr, pkgPtr) != NULL)
		return(API_TERMINATE);


	lPtr = _newLineRecord(recPtr, pkgPtr);

	if (lPtr == NULL)
		return(API_TERMINATE);


	lPtr->clbk = (void *)apiGetSrcVal(recPtr, apiPtr->param);

	_linkLineRecord(pkgPtr, lPtr);

	return(API_COMPLETE);
}


/*--------------------------------------------------------------------------*/
/* This line record is disappearing
 * Runs only if it's user-side post-processing.
 */
static API_RESULT apiUNReg(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{
TMS_API_LINE *lPtr;


	if (!src || pre)
		return(API_COMPLETE);


	if ((lPtr = _getLineRecord(recPtr, pkgPtr)) == NULL)
		return(API_TERMINATE);

	_unlinkLineRecord(pkgPtr, lPtr);

	_delLineRecord(lPtr);

	return(API_COMPLETE);
}


/*--------------------------------------------------------------------------*/
/* The line record may be getting a different callback bound to it.
 * Runs only if it's user-side post-processing.
 */
static API_RESULT apiREReg(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{
TMS_API_LINE *lPtr;


	if (!src || pre)
		return(API_COMPLETE);

	if ((lPtr = _getLineRecord(recPtr, pkgPtr)) == NULL)
		return(API_TERMINATE);

	lPtr->clbk = (void *)apiGetSrcVal(recPtr, apiPtr->param);

	return(API_COMPLETE);
}


/*--------------------------------------------------------------------------*/
/* This is usually called Postproc-only and is used to kill the line.
 * Runs only if it's user-side post-processing.
 */
static API_RESULT apiDestroy(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src)
{
	if (!src || pre)
		return(API_COMPLETE);

	return(apiUNReg(recPtr, pkgPtr, apiPtr, pre, src));
}


/*--------------------------------------------------------------------------*/
/* These typically do nothing unless the macros are defined
 */
static int apiOpen(void *vPtr)
{
int retval;

	retval = XLT_DEVICE_POSTOPEN(vPtr);

	return(retval);
}


static int apiClose(void *vPtr)
{
int retval;

	retval = XLT_DEVICE_PRECLOSE(vPtr);

	return(retval);
}


/*--------------------------------------------------------------------------*/
/* These do nothing on user-side
 */
static int apiLoad(void *vPtr)
{
	vPtr = vPtr;	/* ignored */
	return(1);
}

static int apiUnload(void *vPtr)
{
	vPtr = vPtr;	/* ignored */
	return(1);
}


/*--------------------------------------------------------------------------*/
static void *createSrcRecord(void)
{
void *recPtr;

	if ((recPtr = malloc(TMS_IO_RECSIZE)) == NULL)
		return(NULL);

	memset(recPtr, 0, TMS_IO_RECSIZE);

	return(recPtr);
}


/*--------------------------------------------------------------------------*/
static int createSrcRecordExtension(TMS_IO_RECORD *recPtr)
{
	if (!recPtr->datasize)
		return(1);

#if IOCTL_RECORD_OPTIMIZED

	/* Note that malloc() does not auto-zero the memory
	 */
	if ((recPtr->data = malloc(recPtr->datasize)) == NULL)
		return(0);

#else
	recPtr->data = recPtr->dataBuffer;
#endif

	memset(recPtr->data, 0, recPtr->datasize);

	return(1);
}


/*--------------------------------------------------------------------------*/
static void cleanupSrcRecord(TMS_IO_RECORD *recPtr, int retval)
{
#if IOCTL_RECORD_OPTIMIZED
	if (recPtr->data)
		free(recPtr->data);
#else
	recPtr->data = NULL;
#endif

	free(recPtr);
}


/*--------------------------------------------------------------------------*/
static void cleanupDstRecord(TMS_IO_RECORD *recPtr, int retval)
{
	/* nothing to do here */
}


/*--------------------------------------------------------------------------*/
static int completeSrcRecord(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{

	if (apiClbkType(recPtr->apiType)) {

		return(tmsAPIdst(recPtr->apiType, recPtr));
	}

	return(recPtr->retval);
}


/*---------------------------------------------------------------------------*/
/* Since we are User-side, the record is being sent to the Kernel-side.
 */
static int apiIOCTLsrc(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{
int retval;
XLT_DEVICE_IOCODE_TYPE ioCtlCode;


	/* send the record to the kernel-side
	 */
	ioCtlCode = XLT_DEVICE_IOCODE_SET(recPtr->apiType);

	retval = XLT_DEVICE_SEND(tmsAPIRecord.fhandle, ioCtlCode, recPtr);

#if !IOCTL_RECORD_OPTIMIZED
	recPtr->data = (recPtr->datasize) ? recPtr->dataBuffer : NULL;
#endif

	return(retval);
}


/*---------------------------------------------------------------------------*/
/* Since we are User-side, the record is coming from the Kernel-side CLBK.
 */
static int apiIOCTLdst(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{
_UI32_ line, fcode, id1, id2;
UNITYPE p0, p1, p2, p3, p4, p5;
int doID = 0;
TMS_API_LINE *lPtr;
TMS_API_LINELIST *lineList;



	switch (recPtr->apiType) {

	case TMS_TE1LCLBK:
	case TMS_LIU1LCLBK:
	case TMS_TE3LCLBK:
	case TMS_LIU3LCLBK:
		break;

	case TMS_OCNLCLBK:
	case TMS_ISDNLCLBK:
		doID = 1;
		break;

	case TMS_SSMLCLBK:
	case TMS_ETHLCLBK:
		break;

	default:
		return(0);
	}

	if ((lPtr = _getLineRecord(recPtr, pkgPtr)) == NULL)
		return(0);


	/* This is for the benefit of log entries on the kernel-side when
	 * the CLBK-thread re-enters the Translation Layer.  Thus we will
	 * be able to tell when the callback returns from the App.
	 */
	lineList = pkgPtr->usrkrnlData;
	lineList->clbkLinePtr = lPtr;
	lPtr->fcode = recPtr->fcode;


	/* Callbacks can be NULL, meaning there is no callback for
	 * this line, but that is allowed.  Also, the krnl-side
	 * callback consumer may have returned a -1, which means
	 * the clbkThread did not error, but it did not retrieve
	 * a callback-record either, so it should try again.
	 * This can be the case with multi-core machines.
	 */
	if ((!lPtr->clbk) || (recPtr->retval == -1))
		return(1);


	line	= recPtr->line;
	fcode	= recPtr->fcode;
	id1	= recPtr->id1;
	id2	= recPtr->id2;
	p0	= recPtr->param[0].dstVal;
	p1 	= recPtr->param[1].dstVal;
	p2	= recPtr->param[2].dstVal;
	p3	= recPtr->param[3].dstVal;
	p4	= recPtr->param[4].dstVal;
	p5	= recPtr->param[5].dstVal;

	if (doID) {
		(*lPtr->clbk)(line, fcode, id1, id2, p0, p1, p2, p3, p4, p5);
	}
	else {
		(*lPtr->clbk)(line, fcode, p0, p1, p2, p3, p4, p5);
	}

	return(1);
}


/*--------------------------------------------------------------------------*/
/* Opens the file-descriptor for the TMS Kernel-module driver.
 */
int tmsOPEN(void)
{

	/* return benignly if it's already open
	 */
	if (tmsAPIRecord.fhandle)
		return(1);


	if (!_vfLinkerInit())
		goto bagout;


	XLT_DEVICE_OPEN(tmsAPIRecord.fhandle, FULL_DEVICE_NAME);

	if (!tmsAPIRecord.fhandle)
		goto bagout;


	if (!tmsAPIopen(&tmsAPIRecord))
		goto bagout;


	if (!tmsKILL())
		goto bagout;


	_vfInit(&tmsAPIRecord.PackageList, NULL);

	return(1);


bagout:
	if (tmsAPIRecord.fhandle) {

		/* The macro returns 0 or 1, but have to assign it to
		 * something to avoid a compiler-warning in some O/Ss
		 */
		XLT_DEVICE_CLOSE(tmsAPIRecord.fhandle);
	}

	_vfLinkerUNInit();

	tmsAPIRecord.fhandle = (XLT_DEVICE_HANDLE)0;

	fprintf(stderr, "Failed to open device %s\n", FULL_DEVICE_NAME);

	return(0);
}


/*--------------------------------------------------------------------------*/
/* Closes the file-descriptor for the TMS Kernel-module driver.
 */
int tmsCLOSE(void)
{

	/* return benignly if it's already closed
	 */
	if (!tmsAPIRecord.fhandle)
		return(1);


	if (!tmsKILL())
		goto bagout;


	if (!tmsAPIclose(&tmsAPIRecord))
		goto bagout;


	XLT_DEVICE_CLOSE(tmsAPIRecord.fhandle);

	if (tmsAPIRecord.fhandle)
		goto bagout;


	_vfLinkerUNInit();

	return(1);


bagout:
	fprintf(stderr, "Failed to close device %s\n", FULL_DEVICE_NAME);

	return(0);
}


/*--------------------------------------------------------------------------*/
/* Nothing special to do except to send the ioctl TMSKILL command to the
 * other side.  This causes a full shutdown of TMS, both user and krnl side.
 * The kernel-module is NOT unloaded.
 */
int tmsKILL(void)
{

	if (!tmsAPIRecord.fhandle)	/* make sure it is Open */
		return(0);


	if (!tmsAPIsrc(TMS_TMSKILL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
		return(0);


	return(_delAllPackages());
}


/*--------------------------------------------------------------------------*/
/* A general-purpose interface for App-level calls to the kernel-side
 * board.c file of the BSP
 */
int tmsBSP(_UI32_ fcode, UNITYPE recPtr, UNITYPE recSize)
{

	if (!tmsAPIRecord.fhandle)	/* make sure it is Open */
		return(0);


	return(tmsAPIsrc(HDW_GENERAL, 0, fcode, 0, 0,
				recPtr, recSize, 0, 0, 0, 0));
}


/*--------------------------------------------------------------------------*/
/*------------------------ PEEK and POKE -----------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/* General purpose hardware platform read/write utilites.
 *
 *	adr    = hardware address to be retrieved
 *
 *	datBuf = location to place the retrieved data
 *
 *	size   = size of each data unit to be retrieved
 *		 This can be  1, 2, 4, or 'b', 'w', 'l' (for byte, word, long)
 *
 *	count  = How many data units to be retrieved
 */
/*--------------------------------------------------------------------------*/
int tmsPEEK(void *adr, void *datBuf, int size, int count)
{
_UI32_ apiType;


	if (!tmsAPIRecord.fhandle)	/* make sure it is Open */
		return(0);


	switch (size) {
	case 1:
	case 'b':
		apiType = HDW_PEEKBYTE;
		break;
	case 2:
	case 'w':
		apiType = HDW_PEEKWORD;
		break;
	case 4:
	case 'l':
		apiType = HDW_PEEKLONG;
		break;
	default:
		return(0);
	}

	return(tmsAPIsrc(apiType, 0, apiType, 0, 0,
			adr, datBuf, size, count, 0, 0));
}

/*--------------------------------------------------------------------------*/
int tmsPOKE(void *adr, void *datBuf, int size, int count)
{
_UI32_ apiType;


	if (!tmsAPIRecord.fhandle)	/* make sure it is Open */
		return(0);


	switch (size) {
	case 1:
	case 'b':
		apiType = HDW_POKEBYTE;
		break;
	case 2:
	case 'w':
		apiType = HDW_POKEWORD;
		break;
	case 4:
	case 'l':
		apiType = HDW_POKELONG;
		break;
	default:
		return(0);
	}

	return(tmsAPIsrc(apiType, 0, apiType, 0, 0,
			adr, datBuf, size, count, 0, 0));
}
