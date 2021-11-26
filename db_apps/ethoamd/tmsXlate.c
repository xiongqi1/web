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



/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/* Note that Preprocessing is dependent on whether the caller is the source,
 * or the destination, whereas Postprocessing is dependent of whether it is
 * user-side or kernel-side.  Both sides use the same table to describe how
 * to translate the API parameters.
 *
 *
 * Take the TMS API:
 *
 *	CTRL:  The source is always the user-side, and the destination
 *		is always the kernel-side. 
 *
 *	POLL:  The source is always the user-side, and the destination
 *		is always the kernel-side.  Same as CTRL.
 *
 *	CLBK:  The source is always the kernel-side, and the destination
 *		is always the user-side.  Opposite of CTRL and POLL
 *
 *
 * The only reason an API fcode has a table-entry is because one or more
 * of the API parameters involve some sort of pointer-based data.
 * Otherwise, all API parameters are taken as-is, i.e. any fcode not
 * listed in the table uses the passed parameters without modification.
 *
 * A fcode can have more than one table-entry.  This can be for various
 * reasons.  There can be more than one parameter that is a pointer,
 * or there may be different actions required going into the ioctl-call
 * versus coming out of it.  For example, CTRL calls usually want to put
 * data (pointed to by a pointer-parameter) to the TMS, whereas a POLL call
 * will want to get data from the TMS and place it at a pointer location.
 *
 * What is common among all of this is that the size of the data is always
 * known beforehand.
 *
 * With all of that in mind, generally, a table-entry consists of the fcode,
 * the parameter that holds the pointer, what to do with the data that the
 * pointer points to (copy it or not), and the size (in bytes) of the data's
 * space.  Think of a table-entry as an action item for that fcode.
 * The indicated action is applied prior to the ioctl-call if the prepost
 * field is 1, after the ioctl-call if the prepost field is 2, or both if
 * prepost is a 3.
 *
 *
 * Consider the following entry for example:
 *
 *	{1, TE1LCTRL_RESET,	P3, PCOPY, SZTBL,  sizeof(TE1_CONFIG)},
 *
 * The above example says that the TE1LCTRL_RESET API call, prior to making
 * the ioctl-call (prepost=1), needs to copy the data (PCOPY) pointed to by
 * the the 3rd parameter (P3) to the ioctl-record.  The number of bytes to be
 * copied is directly recorded in the table (SZTBL), as sizeof(TE1_CONFIG).
 * If there were no other entry for the TE1LCTRL_RESET fcode, then no further
 * action is taken after the ioctl-call is executed.
 *
 *
 * What the various table-entries mean:
 *
 *    prepost - This is bitmapped.  Preprocess=1, Postprocess=2, both=3
 *
 *	PCOPY - The pointer refers to a preset data-space of size,
 *		where the data was copied from program-to-record.
 *
 *		Pre-Proc
 *			Src-side - Copy size data from program-to-data.
 *			Dst-side - Take a pointer to the data space.
 *
 *		Post-Proc
 *			Src-side - Copy size data from data-to-program.
 *			Dst-side - Do nothing.
 *
 *
 *	PREFR - The pointer refers to a blank data-space of size,
 *		but data is not copied from program-to-record.
 *
 *		Pre-Proc
 *			Src-side - Set pointer to size data space. No copy.
 *			Dst-side - Take a pointer to the data space.
 *
 *		Post-Proc
 *			Src-side - Copy size data from data-to-ptr
 *			Dst-side - Do nothing.
 *
 *
 *	PPOLL - This simply permits a single entry optimized for polling,
 *		but it represents both a Preproc and Postproc action.
 *		It is a combination of PREFR in the preproc, and PCOPY
 *		in the postproc.  It is typically used anywhere a CTRL
 *		or POLL call needs to return something.  For example,
 *		it is used for the NEWCONTEXT CTRL call, and almost
 *		universally for all POLL calls.
 *
 *
 *	SZTBL - The size of the data-space is taken directly from the table.
 *
 *	SZPRM - The size of the data-space is taken from another parameter.
 *
 *	SZSTR - The size of the data-space is taken as the strlen+1
 *		of the string pointed to by the parameter.
 *
 *	SZFOO - The size of the data-space is taken from the return-value
 *		of a custom function.
 *
 *
 * Another example:
 *
 *	{3, TE1LCTRL_NEWCONTEXT,  P4, PPOLL, SZTBL,  sizeof(void **)},
 *
 * Although it's a CTRL call, the P4 parameter points to the space in which
 * the TMS is supposed to return the context-handle.  In this case, we can
 * use the optimized PPOLL pointer-type (used mostly for POLL calls) to
 * describe the P4 pointer.  Note that prepost is 3, which means the action
 * is executed both Pre-ioctl and Post-ioctl call.  This is because the PPOLL
 * pointer-type is just a shorthand method of having to make the following
 * two entries, which would also be valid:
 *
 *	{1, TE1LCTRL_NEWCONTEXT,  P4, PREFR, SZTBL,  sizeof(void **)},
 *	{2, TE1LCTRL_NEWCONTEXT,  P4, PCOPY, SZTBL,  sizeof(void **)},
 *
 *
 *
 * Custom Translations:
 *
 * Custom translation routines are the "fooPtr" in the table entry.  Custom
 * translations are rare and generally not desired because they complicate
 * the user-side to kernel-side work.  However, there are a few canned custom
 * translations that work the same for all of TMS, regardless of the package.
 *
 *	xxxCTRL_NCISTART   - apiStart()
 *	xxxCTRL_NCIUNSTART - apiUNStart()
 *	xxxCTRL_REGAPI     - apiReg()
 *	xxxCTRL_UNREGAPI   - apiUNReg()
 *	xxxCTRL_REREGAPI   - apiREReg()
 *	xxxCTRL_DESTROY    - apiDestroy()
 *
 * You can combine Custom translations with table-entry translations.
 * If the fooPtr entry is non-null, then the translator calls foo() first.
 * 
 * A Custom foo() routine always returns one of the following values:
 *
 *	API_TERMINATE - There was an error, and the API is aborted.
 *		 	This is usually detected during the processing
 *			of the table-entries.
 *
 *	API_COMPLETE  - Pre or post processing for the API has completed
 *			successfully.  If Preproc phase, then the ioctl-call
 *			is executed immediately.  If Postproc phase, then we
 *			are done with everything.
 *
 *	API_SKIP      - Skip the action-item but continue through the table.
 *
 *	API_APPLY     - Apply the action item and continue through the table
 *
 *
 * The Translator handles complex translations automatically.  There are
 * three cases of note within TMS that are documented here.
 *
 *	xxxLPOLL_ALARMTIME and xxxLPOLL_LOOPTIME:
 *		Most PCOPY table entries throughout TMS are set to run only
 *		during the preproc phase because we are copying user-side
 *		data to the ioRecord.  But PCOPY also works in the postproc
 *		phase.  PCOPY for both phases are used when polling for the
 *		Alarm and/or Loop integration times in the TE1/TE3/OCN
 *		packages.  These poll-actions require that the user-side
 *		pre-fill the unitID field of the UNIT_TIME structure so
 *		that the kernel-side knows which time-values to return.
 *		Thus, the table entry for those API-calls use PCOPY for
 *		both the preproc and the postproc (prepost = 3) phases.
 *		The API table entries use preproc-PCOPY to copy the
 *		prefilled UNIT_TIME from the App to the ioRecord,
 *		and postproc-PCOPY to copy the final value back to
 *		the App.
 *
 *	ETHLCTRL_1AGCMD:
 *		This API has a P4 that can optionally be NULL, or a pointer
 *		to a macAdr, or a pointer to a structure, or a hard-value.
 *		This means the size can be 0 or not, depending on which 1AG
 *		command (in P1) is being performed.  P5 can also be different
 *		sizes based on the command.  To handle the multi-flavored API,
 *		the table requires two entries for the P5 parameter, but
 *		having more than one entry for the same parameter only works
 *		if only one of them will return a size-value, all other
 *		entries for that same parameter must return 0-size, otherwise
 *		the translator will return a failure.  All entries can return
 *		0-size, but never more than one can return a non-zero size.
 *		The general rule is that a table can have more than one entry
 *		for the same parameter, but only one of those entries can
 *		return non-zero.
 */
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/



#include "tmsAPI.h"




/*--------------------------------------------------------------------------*/
static int apiGetParamIndex(int param);
static TMS_API_PACKAGE *apiGetPackage(int apiType);
static UNITYPE apiGetSrcVal(TMS_IO_RECORD *recPtr, int param);
static int apiClbkType(_UI32_ apiType);


/* Round up X to the nearest UNITYPE boundary
 */
#undef  ROUNDUP
#define ROUNDUP(x)		\
	(int)(((x+(sizeof(UNITYPE)-1))/sizeof(UNITYPE))*sizeof(UNITYPE))



/* so the external implementation in the .inc files know the size
 */
int tmsAPIrecordSize = 0;



#ifndef __KERNEL__
	#include "tmsXlateUser.inc"
#else
	#include "tmsXlateKrnl.inc"
#endif



/*--------------------------------------------------------------------------*/
#define TE1_API_PACKAGE		NULL
#define TE1LIU_API_PACKAGE	NULL
#define TE3_API_PACKAGE		NULL
#define TE3LIU_API_PACKAGE	NULL
#define OCN_API_PACKAGE		NULL
#define ISDN_API_PACKAGE	NULL
#define SSM_API_PACKAGE		NULL
#define ETH_API_PACKAGE		NULL
#define HDW_API_PACKAGE		NULL


#ifdef TE1_PACKAGE
	#include "te1API.tbl"
#endif

#ifdef TE1LIU_PACKAGE
	#include "liuTE1API.tbl"
#endif

#ifdef TE3_PACKAGE
	#include "te3API.tbl"
#endif

#ifdef TE3LIU_PACKAGE
	#include "liuTE3API.tbl"
#endif

#ifdef OCN_PACKAGE
	#include "ocnAPI.tbl"
#endif

#ifdef ISDN_PACKAGE
	#include "isdnAPI.tbl"
#endif

#ifdef SSM_PACKAGE
	#include "ssmAPI.tbl"
#endif

#ifdef ETH_PACKAGE
	#include "ethAPI.tbl"
#endif


#include "hdwAPI.tbl"


/*--------------------------------------------------------------------------*/
static TMS_API_PACKAGE *apiGetPackage(int apiType)
{
void *pkgPtr;


	switch (apiType) {
	case TMS_TE1LCTRL:
	case TMS_TE1LPOLL:
	case TMS_TE1LCLBK:
		pkgPtr = TE1_API_PACKAGE;
		break;

	case TMS_LIU1LCTRL:
	case TMS_LIU1LPOLL:
	case TMS_LIU1LCLBK:
		pkgPtr = TE1LIU_API_PACKAGE;
		break;

	case TMS_TE3LCTRL:
	case TMS_TE3LPOLL:
	case TMS_TE3LCLBK:
		pkgPtr = TE3_API_PACKAGE;
		break;

	case TMS_LIU3LCTRL:
	case TMS_LIU3LPOLL:
	case TMS_LIU3LCLBK:
		pkgPtr = TE3LIU_API_PACKAGE;
		break;

	case TMS_OCNLCTRL:
	case TMS_OCNLPOLL:
	case TMS_OCNLCLBK:
		pkgPtr = OCN_API_PACKAGE;
		break;

	case TMS_ISDNLCTRL:
	case TMS_ISDNLPOLL:
	case TMS_ISDNLCLBK:
		pkgPtr = ISDN_API_PACKAGE;
		break;

	case TMS_SSMLCTRL:
	case TMS_SSMLPOLL:
	case TMS_SSMLCLBK:
		pkgPtr = SSM_API_PACKAGE;
		break;

	case TMS_ETHLCTRL:
	case TMS_ETHLPOLL:
	case TMS_ETHLCLBK:
		pkgPtr = ETH_API_PACKAGE;
		break;

	case TMS_TMSKILL:
	case HDW_GENERAL:
	case HDW_PEEKBYTE:
	case HDW_PEEKWORD:
	case HDW_PEEKLONG:
	case HDW_POKEBYTE:
	case HDW_POKEWORD:
	case HDW_POKELONG:
		pkgPtr = HDW_API_PACKAGE;
		break;

	default:
		pkgPtr = NULL;
		break;
	}


	return(pkgPtr);
}


/*--------------------------------------------------------------------------*/
static int apiGetParamIndex(int param)
{
int index;

	index = param - P1;

	if ((index < 0) || (index >= MAX_IO_PARAMS))
		return(0);

	return(index);
}


/*--------------------------------------------------------------------------*/
static UNITYPE apiGetSrcVal(TMS_IO_RECORD *recPtr, int param)
{
	return(recPtr->param[apiGetParamIndex(param)].srcVal);
}


/*--------------------------------------------------------------------------*/
static _UI32_ apiGetSrcSize(TMS_IO_RECORD *recPtr, TMS_API_TABLE *apiPtr)
{
_UI32_ size;
API_SIZEFOO fooptr;


	switch (apiPtr->sizeType) {

	case SZTBL:
		size = (_UI32_)apiPtr->size;
		break;

	case SZPRM:
		size = (_UI32_)apiGetSrcVal(recPtr, (int)apiPtr->size);
		break;

	case SZSTR:
		size = (_UI32_)(strlen((char *)apiGetSrcVal(recPtr,
						(int)apiPtr->size)) + 1);
		break;

	case SZFOO:
		fooptr = (API_SIZEFOO)apiPtr->size;
		size = (*fooptr)(recPtr, apiPtr);
		break;

	default:
		size = 0;
		break;
	}

	return(size);
}


/*--------------------------------------------------------------------------*/
static TMS_IO_PARAM *apiGetParamPtr(TMS_IO_RECORD *recPtr,
					TMS_API_TABLE *apiPtr)
{
int index;

	if ((index = apiGetParamIndex(apiPtr->param)) < 0)
		return(NULL);

	return(&recPtr->param[index]);
}


/*--------------------------------------------------------------------------*/
/* Callbacks have special requirements because their data-space size
 * is fixed by the user-side.  The user-side has to pre-allocate the
 * data-space to hold the callback data.
 *
 * Callback records have a fixed size that is maximized to the largest amount
 * of data possible from a TMS package.  This is because although callbacks
 * are generated kernel-side, there is no way for the user-side to pull data
 * from the kernel-side.  So the callback record's data-space has to already
 * be present, and big enough, when the kernel-side callback is ready to put
 * data to it.
 *
 * The kernel-side skips this step and proceeds with the parameter-parsing
 * normally.  The size is checked at the end of parsing to make sure the
 * callback will fit within the user-side's data space.
 */
static int apiClbkType(_UI32_ apiType)
{
	switch (apiType) {

	case TMS_TE1LCLBK:
	case TMS_LIU1LCLBK:
	case TMS_TE3LCLBK:
	case TMS_LIU3LCLBK:
	case TMS_OCNLCLBK:
	case TMS_ISDNLCLBK:
	case TMS_SSMLCLBK:
	case TMS_ETHLCLBK:
		return(1);

	default:
		return(0);
	}
}


/*--------------------------------------------------------------------------*/
/* This performs API-translations according to the Package's API Table.
 * If it is not in the table, there is no translation performed.
 * Only pointer-types get processed.  When it is done, the ioRecord has
 * the type of each pointer-parameter, their data-size, and their index
 * into the data-array where their data is recorded.
 * Any non-pointer parameter is skipped.
 */
static int apiPrepareRecord(TMS_IO_RECORD *recPtr, TMS_API_PACKAGE *pkgPtr)
{
int i, size;
TMS_API_TABLE *apiPtr = pkgPtr->tblPtr;
TMS_IO_PARAM *pPtr;


#ifndef __KERNEL__
	if (apiClbkType(recPtr->apiType)) {
		recPtr->datasize = ROUNDUP(pkgPtr->clbkMaxData);

	#if !IOCTL_RECORD_OPTIMIZED
		if (recPtr->datasize > MAX_IO_DATA)
			return(0);
	#endif

		return(1);
	}
#endif


	/* Check for any custom entries and just record
	 * the presence of an entry for this fcode.
	 */
	if ((apiPtr = pkgPtr->custPtr) != NULL) {
		for (i = 0; i < pkgPtr->custCount; i++, apiPtr++) {
			if (apiPtr->fcode != recPtr->fcode)
				continue;

			recPtr->custom = 1;
		}
	}


	/* Now process the general API table
	 */
	apiPtr = pkgPtr->tblPtr;
	recPtr->index = 0;

	for (i = 0; i < pkgPtr->tblCount; i++, apiPtr++) {

		if (apiPtr->fcode != recPtr->fcode)
			continue;

		/* cannot apply pointer-manipulation if there
		 * is no pointer indicated in the table.
		 */
		if (!apiPtr->param)
			return(0);	/* should never happen */


		/* if the size-result is 0, which is permitted,
		 * then it is the same as not having the table-entry,
		 * so we skip the entry.
		 */
		if (!(size = apiGetSrcSize(recPtr, apiPtr)))
			continue;


		/* Get the parameter's address in the ioRecord
		 */
		if ((pPtr = apiGetParamPtr(recPtr, apiPtr)) == NULL)
			return(0);	/* should never happen */


		/* At this point, this better be 0, otherwise something
		 * is corrupt in the API-table because you cannot apply
		 * multiple table-actions to the same pointer.
		 */
		if (pPtr->pWhen != 0)
			return(0);


		pPtr->pWhen = apiPtr->prepost;

		pPtr->pType = apiPtr->paramType;

		pPtr->pSize = size;

		pPtr->pIndex = recPtr->index;


		/* round the size up to the nearest UNITYPE boundary
		 * and add it to the running total
		 */
		recPtr->index += ROUNDUP(pPtr->pSize);
	}

	recPtr->datasize = recPtr->index;


#if !IOCTL_RECORD_OPTIMIZED
	if (recPtr->datasize > MAX_IO_DATA)
		return(0);
#endif


#ifdef __KERNEL__
	/* This is just a quick safety-check to make sure there is enough
	 * space in the user-side preallocated data-space for callbacks.
	 */
	if (recPtr->datasize > ROUNDUP(pkgPtr->clbkMaxData))
		return(0);
#endif

	return(1);
}


/*--------------------------------------------------------------------------*/
static int apiDoCustom(TMS_IO_RECORD *recPtr,
		TMS_API_PACKAGE *pkgPtr, int pre, int src)
{
int i;
TMS_API_TABLE *apiPtr;


	/* Nothing to do if there are no custom entries,
	 * or no custom table, so it returns benignly.
	 */
	if (!recPtr->custom)
		return(1);

	if ((apiPtr = pkgPtr->custPtr) == NULL)
		return(0);	/* this should never happen */


	for (i = 0; i < pkgPtr->custCount; i++, apiPtr++) {

		if (apiPtr->fcode != recPtr->fcode)
			continue;

		if (!(apiPtr->prepost & pre))
			continue;

		if (apiPtr->custom != NULL) {

			switch ((*apiPtr->custom)(recPtr, pkgPtr, apiPtr,
					(pre == API_PREPROC), src)) {

			case API_TERMINATE:	/* there was a problem */
				return(0);

			case API_COMPLETE:	/* nothing more to do */
				return(1);

			default:
				return(0);	/* should never happen */
			}
		}
	}

	return(1);
}


/*--------------------------------------------------------------------------*/
/* This follows the truth-table for the paramType that is described in
 * the block comments at the top of this file.  The pointers are handled
 * differently depending on if the Src-side or Dst-side is calling the
 * function.  Sources set data in a data-array, and Destinations take a
 * pointer to the data in the data-array.
 */
static int apiPreTableAction(TMS_IO_RECORD *recPtr,
				TMS_API_PACKAGE *pkgPtr, int src)
{
int i;
TMS_IO_PARAM *pPtr;


	if (!apiDoCustom(recPtr, pkgPtr, API_PREPROC, src))
		return(0);


	pPtr = recPtr->param;

	for (i = 0; i < MAX_IO_PARAMS; i++, pPtr++) {

		if (!(pPtr->pWhen & API_PREPROC))
			continue;

		switch (pPtr->pType) {
		case PCOPY:
			if (!src)
				break;

			/* Set the data from program-space to data-space
			 */
			memcpy(&recPtr->data[pPtr->pIndex],
					(void *)pPtr->srcVal, pPtr->pSize);
			break;

		case PREFR:
		case PPOLL:
			break;

		case PNONE:
			continue;

		default:
			return(0);	/* should never happen */
		}

		/* Take a pointer to the data-space
		 */
		if (!src) {
			pPtr->dstVal = UNICAST(&recPtr->data[pPtr->pIndex]);
		}
	}

	return(1);
}


/*--------------------------------------------------------------------------*/
/* This follows the truth-table for the paramType that is described in
 * the block comments at the top of this file.  The pointers are handled
 * differently depending on if the Src-side or Dst-side is calling the
 * function.
 */
static int apiPostTableAction(TMS_IO_RECORD *recPtr,
				TMS_API_PACKAGE *pkgPtr, int src)
{
int i;
TMS_IO_PARAM *pPtr;


	if (!apiDoCustom(recPtr, pkgPtr, API_POSTPROC, src))
		return(0);


	/* According to the truth-table, only the Src-side has
	 * anything to do, so the Dst-side returns benignly
	 */
	if (!src)
		return(1);


	pPtr = recPtr->param;

	for (i = 0; i < MAX_IO_PARAMS; i++, pPtr++) {

		if (!(pPtr->pWhen & API_POSTPROC))
			continue;

		switch (pPtr->pType) {
		case PCOPY:
		case PPOLL:
		case PREFR:
			break;

		case PNONE:
			continue;

		default:
			return(0);	/* should never happen */
		}

		/* copy from the ioRecord data-space to the program-space
		 */
		memcpy((void *)pPtr->srcVal,
			&recPtr->data[pPtr->pIndex], pPtr->pSize);
	}

	return(1);
}


/*--------------------------------------------------------------------------*/
/* Makes a TMS_IO_RECORD out of the TMS API interface parameters and sends
 * it to the Dst-side.  If we are user-side, then this is for CTRL and POLL
 * API calls.  If we are kernel-side, then this is for CLBK API calls.
 *
 * Nothing is passed by reference.  Everything is passed by value.
 * Therefore, for those API-calls that have pointer-references,
 * we need to dereference the pointers and place the referenced data
 * into the IO-record.  The API-table holds the info on how to do that.
 */
int tmsAPIsrc(int apiType, _UI32_ line,
			_UI32_ fcode, _UI32_ id1, _UI32_ id2, ...)
{
int i, retval = 0;
TMS_API_PACKAGE *pkgPtr;
TMS_IO_RECORD *recPtr;
va_list alist;


	if ((recPtr = createSrcRecord()) == NULL)
		return(0);


	va_start (alist, id2);
	
	recPtr->apiType	= apiType;
	recPtr->line	= line;
	recPtr->fcode	= fcode;
	recPtr->id1	= id1;
	recPtr->id2	= id2;

	for (i = 0; i < MAX_IO_PARAMS; i++) {

		recPtr->param[i].srcVal = va_arg(alist, UNITYPE);
		recPtr->param[i].pType  = PNONE;
		recPtr->param[i].pWhen  = 0;
		recPtr->param[i].pIndex = 0;
		recPtr->param[i].pSize  = 0;
		recPtr->param[i].dstVal = recPtr->param[i].srcVal;
	}

	va_end (alist);


	if ((pkgPtr = apiGetPackage(apiType)) == NULL)
		goto bagout;

	if (!apiPrepareRecord(recPtr, pkgPtr))
		goto bagout;

	if (!createSrcRecordExtension(recPtr))
		goto bagout;

	if (!apiPreTableAction(recPtr, pkgPtr, 1))
		goto bagout;

	if (!apiIOCTLsrc(recPtr, pkgPtr))
		goto bagout;

	if (!apiPostTableAction(recPtr, pkgPtr, 1))
		goto bagout;

	/* Cannot access recPtr after completSrcRecord because it may
	 * have already passed the record to the callback thread and
	 * freed it.  Only if retval is 0 is the recPtr still valid.
	 */
	retval = completeSrcRecord(recPtr, pkgPtr);


bagout:
	cleanupSrcRecord(recPtr, retval);

	return(retval);
}


/*--------------------------------------------------------------------------*/
/* This routine catches the TMS_IO_RECORD that the Src sent, picks it apart,
 * and makes the call to the appropriate API-interface.
 */
int tmsAPIdst(int apiType, TMS_IO_RECORD *recPtr)
{
int retval = 0;
TMS_API_PACKAGE *pkgPtr;


#if !IOCTL_RECORD_OPTIMIZED
	recPtr->data = (recPtr->datasize) ? recPtr->dataBuffer : NULL;
#endif

	if ((pkgPtr = apiGetPackage(apiType)) == NULL)
		goto bagout;

	if (!apiPreTableAction(recPtr, pkgPtr, 0))
		goto bagout;

	if (!apiIOCTLdst(recPtr, pkgPtr))
		goto bagout;

	if (!apiPostTableAction(recPtr, pkgPtr, 0))
		goto bagout;

	retval = recPtr->retval;

bagout:
	cleanupDstRecord(recPtr, retval);

	return(recPtr->retval);
}


/*--------------------------------------------------------------------------*/
/* Both user and kernel sides calls these after open and before close.
 * On the User-side, they are called from the tmsOPEN and tmsCLOSE routines.
 * On the Kernel-side, they are called from the kernel-module's open/close
 * hooks.
 */
int tmsAPIopen(void *vPtr)
{
	tmsAPIrecordSize = TMS_IO_RECSIZE;

	return(apiOpen(vPtr));
}

int tmsAPIclose(void *vPtr)
{
	return(apiClose(vPtr));
}


/*--------------------------------------------------------------------------*/
/* Returns 1 if it is a TMS Callback Code, else returns 0.
 */
int tmsAPIclbktype(int apiType)
{
	return(apiClbkType(apiType));
}


/*--------------------------------------------------------------------------*/
/* These are Kernel-side only, and are called when the kernel-module
 * is loaded/unloaded.  They are not used on the User-side.
 */
int tmsAPIload(void *vPtr)
{
	return(apiLoad(vPtr));
}

int tmsAPIunload(void *vPtr)
{
	return(apiUnload(vPtr));
}

