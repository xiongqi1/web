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
 * This lives and compiles in user-space for the TMS Translation Layer.
 */




#ifndef TMSLINKUSER_H
#define TMSLINKUSER_H



/*--------------------------------------------------------------------------*/
typedef struct _userlist {
	struct _userlist *head;		/* pointer to the head of a list */
	struct _userlist *tail;		/* pointer to the tail of a list */
	void *recptr;			/* ptr to record */
}USERLIST;


/* These are all located in the tmsLinkUser.c file
 */
extern int _vfLinkerInit(void);

extern void _vfLinkerUNInit(void);

extern void _vfInit(USERLIST *lptr, void *recPtr);

extern USERLIST *_vfUNLink(USERLIST *lptr);

extern void _vfLink(USERLIST *lptr, USERLIST *recptr);

extern void _vfTailLink(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfLast(USERLIST *lptr);

extern USERLIST *_vfNext(USERLIST *lptr);

extern void _vfPutSafe(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfGetSafe(USERLIST *lptr);

extern USERLIST *_vfWalk(USERLIST *lptr, USERLIST *recptr);

extern USERLIST *_vfWalkR(USERLIST *lptr, USERLIST *recptr);

extern void *_vfGetRecord(USERLIST *lptr);

extern void _vfUNLinkMiddle(USERLIST *lptr, USERLIST *recptr);



#endif	/* TMSLINKUSER_H */
