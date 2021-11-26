/* 
 * 
 * Copyright 1997-2011 NComm, Inc.  All rights reserved.
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
 * This is a clone of nciVFifo.c but it lives and compiles in user-space.
 */


#include "tmsAPI.h"
#include "tmsLinkUser.h"



/*--------------------------------------------------------------------------*/
static XLT_CRITICAL_TYPE linkLock;
static void *tmsUserLock = NULL;

int _vfLinkerInit(void)
{
	if (tmsUserLock)
		return(0);

	if (!XLT_CRITICAL_CREATE(&linkLock)) {
		return(0);
	}

	tmsUserLock = &linkLock;

	return(1);
}

void _vfLinkerUNInit(void)
{
	if (tmsUserLock) {
		XLT_CRITICAL_DELETE(tmsUserLock);
	}

	tmsUserLock = NULL;
}

static void _usrCritEnter(void)
{
	if (tmsUserLock) {
		XLT_CRITICAL_LOCK(tmsUserLock);
	}
}

static void _usrCritExit(void)
{
	if (tmsUserLock) {
		XLT_CRITICAL_UNLOCK(tmsUserLock);
	}
}


/*--------------------------------------------------------------------------*/
/* Initializes the head and tail pointers of the USERLIST structure.
 */
void _vfInit(USERLIST *lptr, void *recPtr)
{
	lptr->head   = NULL;
	lptr->tail   = NULL;
	lptr->recptr = recPtr;
}


/*--------------------------------------------------------------------------*/
/* Unlinks a record, if any, from the tailptr of the double-linked list and
 * returns a pointer to the unlinked data, else returns NULL.
 */
USERLIST *_vfUNLink(USERLIST *lptr)
{
USERLIST *recptr = NULL;


	_usrCritEnter();

	if (lptr->tail != NULL) {

		recptr = lptr->tail;

		if (recptr->tail != NULL)
			recptr->tail->head = recptr->head;

		lptr->tail = recptr->tail;

		if (lptr->head == recptr)
			lptr->head = NULL;

		recptr->tail = recptr->head = NULL;
	}

	_usrCritExit();

	return(recptr);
}


/*--------------------------------------------------------------------------*/
/* Links a record into the headptr of a double-linked list.
 */
void _vfLink(USERLIST *lptr, USERLIST *recptr)
{

	_usrCritEnter();

	recptr->head = lptr->head;

	recptr->tail = NULL;

	if (recptr->head != NULL)
		recptr->head->tail = recptr;

	lptr->head = recptr;

	if (lptr->tail == NULL)
		lptr->tail = recptr;

	_usrCritExit();
}


/*--------------------------------------------------------------------------*/
/* Links a record into the tailptr of a double-linked list.
 */
void _vfTailLink(USERLIST *lptr, USERLIST *recptr)
{

	_usrCritEnter();

	recptr->tail = lptr->tail;

	recptr->head = NULL;

	if (recptr->tail != NULL)
		recptr->tail->head = recptr;

	lptr->tail = recptr;

	if (lptr->head == NULL)
		lptr->head = recptr;

	_usrCritExit();
}


/*--------------------------------------------------------------------------*/
/* Returns a pointer to the LIFO record (head-ptr), if any, else returns NULL.
 */
USERLIST *_vfLast(USERLIST *lptr)
{
void *rval;

	_usrCritEnter();

	rval = lptr->head;

	_usrCritExit();

	return(rval);
}


/*--------------------------------------------------------------------------*/
/* Returns a pointer to the FIFO record (tail-ptr), if any, else returns NULL.
 */
USERLIST *_vfNext(USERLIST *lptr)
{
void *rval;

	_usrCritEnter();

	rval = lptr->tail;

	_usrCritExit();

	return(rval);
}


/*--------------------------------------------------------------------------*/
/* These are used when Walking a linked list and it's unknown if the lptr
 * Record may have been unlinked out of the list or not.  Call the first
 * function before the operation on recptr, then call the second afterwards
 * to get the proper Walk-pointer.  This will maintain the proper Walk
 * whether recptr is still in the list or not.  Lptr is the address of
 * a USERLIST record where the safe pointers are to be stored.
 */
void _vfPutSafe(USERLIST *lptr, USERLIST *recptr)
{
	_usrCritEnter();

	lptr->tail = recptr->tail;
	lptr->head = recptr->head;

	_usrCritExit();
}

USERLIST *_vfGetSafe(USERLIST *lptr)
{
void *rval;

	_usrCritEnter();

	if (lptr->head)
		rval = lptr->head->tail;
	else if (lptr->tail)
		rval = lptr->tail->head;
	else rval = lptr;

	_usrCritExit();

	return(rval);
}


/*--------------------------------------------------------------------------*/
/* Returns a pointer to the next FIFO record (tail-ptr), if any, when walking
 * a linked list.  If recptr is a NULL upon entry, then it starts at the
 * beginning of the list.  Returns NULL when at the end of the list.
 */
USERLIST *_vfWalk(USERLIST *lptr, USERLIST *recptr)
{
void *rval;

	_usrCritEnter();

	if (recptr == NULL)
		recptr = lptr;

	rval = recptr->tail;

	_usrCritExit();

	return(rval);
}

USERLIST *_vfWalkR(USERLIST *lptr, USERLIST *recptr)
{
void *rval;

	_usrCritEnter();

	if (recptr == NULL)
		recptr = lptr;

	rval = recptr->head;

	_usrCritExit();

	return(rval);
}


/*--------------------------------------------------------------------------*/
/* Returns a record pointer of the element
 */
void *_vfGetRecord(USERLIST *lptr)
{
void *rval;

	_usrCritEnter();

	rval = lptr->recptr;

	_usrCritExit();

	return(rval);
}


/*--------------------------------------------------------------------------*/
/* Unlinks an arbitrary record from the double-linked list.
 */
void _vfUNLinkMiddle(USERLIST *lptr, USERLIST *recptr)
{

	_usrCritEnter();

	if (recptr->tail != NULL)
		recptr->tail->head = recptr->head;

	if (recptr->head != NULL)
		recptr->head->tail = recptr->tail;

	if (lptr->head == recptr)
		lptr->head = recptr->head;

	if (lptr->tail == recptr)
		lptr->tail = recptr->tail;

	recptr->tail = recptr->head = NULL;

	_usrCritExit();
}

