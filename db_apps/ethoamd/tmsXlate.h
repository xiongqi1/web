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


/*
 * The TMS Translation Layer manages the TMS API calls across the user-kernel
 * memory boundaries in those operating systems that have that type of memory
 * separation.  These operating systems include Linux and Windows.
 * Other operating systems may have similar architectures, and they would use
 * this interface also.
 *
 * The Translation Layer software consists of 3 parts:
 *
 *	1. The User Side code - this is code that is compiled and linked to
 *		the user-side Application when the Application is built.
 *		This code is in the tmsXlateUser.inc file.
 *
 *	2. The Kernel Side code - this is code that is compiled and linked
 *		to the kernel-side TMS module.  This code is built when the
 *		the TMS KLM is built.
 *		This code is in the tmsXlateKrnl.inc file.
 *
 *	3. Common code - this code is common to both of the first two parts.
 *		It is code that is compiled and linked twice.  Once for the
 *		user-side App using the compiler/linker flags that are
 *		specific to building the user-side App, and once for the
 *		kernel-side TMS module using the compiler/linker flags that
 *		are specific to a kernel-module build.
 *		This code is in the tmsXlate.c file.
 *
 */


#ifndef TMSXLATE_H
#define TMSXLATE_H


/* Set by the tmsAPIopen() routine for the benefit of the
 * external implementations.
 */
extern int tmsAPIrecordSize;


#include "tmsOSxlate.h"		/* this resides in the BSP */



/*--------------------------------------------------------------------------*/
/* The record is optimized by default or if not set or set to 1,
 * but a Windows-build will explicitly set this macro to 0.
 */
#ifndef IOCTL_RECORD_OPTIMIZED
	#define IOCTL_RECORD_OPTIMIZED	1
#endif


#define MAX_IO_PARAMS		6	/* the max of all TMS packages */
#define MAX_IO_DATA		2048


struct _tmsIOParam {

	UNITYPE srcVal;		/* the original value set by the Src-side
				 */
	int pType;
	int pWhen;
	int pIndex;
	int pSize;

	UNITYPE dstVal;		/* the data-space ptr set and used by the
				 * Dst-side, and used only on the Dst-side
				 */
};

typedef struct _tmsIOParam	TMS_IO_PARAM;


struct _tmsIORecord {
	_UI32_ apiType;
	_UI32_ line;
	_UI32_ fcode;
	_UI32_ id1;
	_UI32_ id2;

	int retval;		/* the API's returned SUCCESS or ERR value */
	int index;		/* only used when building the record */
	int datasize;		/* bytecount of data field */
	int custom;		/* flag if the fcode has a custom entry */

	TMS_IO_PARAM param[MAX_IO_PARAMS];

	unsigned char *data;

	/* The Windows implementation will set the macro to 0.
	 * Windows needs a contiguous memory-block for ioctl,
	 * so the *data pointer will point to this dataBuffer.
	 */
#if !IOCTL_RECORD_OPTIMIZED
	unsigned char dataBuffer[MAX_IO_DATA];
#endif
};

typedef struct _tmsIORecord	TMS_IO_RECORD;

#define TMS_IO_RECSIZE		sizeof(TMS_IO_RECORD)


/*--------------------------------------------------------------------------*/
#define API_PREPROC	1
#define API_POSTPROC	2
#define API_BOTHPROC	(API_PREPROC | API_POSTPROC)


enum _apiResult {
	API_TERMINATE = 100,		/* there was a problem */
	API_COMPLETE,			/* nothing more to do */
	API_SKIP,			/* skip the table-action */
	API_APPLY			/* apply the table-action */
};

typedef enum _apiResult API_RESULT;


enum _apiParams {
	P1	= 100,
	P2,
	P3,
	P4,
	P5,
	P6,
	PNONE,		/* all parameters default to this type */
	PCOPY,
	PREFR,
	PPOLL,
	SZTBL,
	SZPRM,
	SZSTR,
	SZFOO
};



struct _apiPackage {
	int init;
	char *name;
	void *tblPtr;
	int tblCount;
	void *custPtr;
	int custCount;
	int clbkMaxData;
	int clbkType;
	void *clbk;
	void *usrkrnlData;	/* used for user-or-krnl specific data */
};

typedef struct _apiPackage	TMS_API_PACKAGE;


typedef struct _tmsAPItable	TMS_API_TABLE;

typedef	_UI32_ (*API_SIZEFOO)(TMS_IO_RECORD *recPtr, TMS_API_TABLE *apiPtr);

struct _tmsAPItable {
	int prepost;
	_UI32_ fcode;
	int param;
	int paramType;
	int sizeType;
	UNITYPE size;
	API_RESULT (*custom)(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr,
			TMS_API_TABLE *apiPtr, int pre, int src);
	int identifier;		/* used by custom if it wants */
};


/*--------------------------------------------------------------------------*/
/* Located in tmsXlate.c
 */
extern int tmsAPIsrc(int apiType, _UI32_ line,
		_UI32_ fcode, _UI32_ id1, _UI32_ id2, ...);

extern int tmsAPIdst(int apiType, TMS_IO_RECORD *recPtr);

extern int tmsAPIopen(void *vPtr);

extern int tmsAPIclose(void *vPtr);

extern int tmsAPIclbktype(int apiType);

extern int tmsAPIload(void *vPtr);

extern int tmsAPIunload(void *vPtr);


/*--------------------------------------------------------------------------*/
/* Located in tmsXlateKrnl.inc
 */
extern int tmsAPIclbk(int apiType, _UI32_ line, _UI32_ fcode,
			_UI32_ id1, _UI32_ id2, UNITYPE *pArray);


/* used in the redirected callback of the xxxAPI.tbl files
 */
#define TMS_CLBK_PARAMS(listStart, pArray)				\
	va_start (alist, listStart);					\
	{int i;								\
		for (i = 0; i < MAX_IO_PARAMS; i++) {			\
			pArray[i] = va_arg(alist, UNITYPE);		\
		}							\
	}								\
	va_end (alist)


#endif	/* TMSXLATE_H */

