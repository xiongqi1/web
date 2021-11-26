/*
 *
 * Copyright 1997-2010 NComm, Inc.  All rights reserved.
 *
 *
 *                     *** Important Notice ***
 *                           --- V6.72 ---
 *            This notice may not be removed from this file.
 *
 * The version 6.72 T1/E1 LIU software contained in this file may only be used
 *
 * within a valid license agreement between your company and NComm, Inc. The
 * license agreement includes the definition of a PROJECT.
 *
 * The T1/E1 LIU software is licensed only for the T1/E1 LIU application
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


#ifndef _TMS_H
#define _TMS_H



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "session.h"

/* Get things that let the HelloWorld Demos compile for different O/Ss
 */
//#include "bspHW.h"



/* Get the API and Convenience Calls.
 * This will also include the local target.h file.
 */
#include "tmsAPI.h"



/* This #define is not used anywhere except in this header-file.
 * It is present only to illustrate MBox size-dependency on the number
 * of Nodes.
 */
#define MAX_NODES		1



/* These are used when you create a context with the _NEWCONTEXT API call.
 * Under Linux, the context is created by TMS as a kernel-side thread.
 * These are not firm numbers.  Adjust them as needed.
 *
 * Linux does not care about stack size or task priority, but they still have
 * to be defined, so their values are set according to a vxWorks environment.
 *
 * The MBox size is not a firm number.  The number chosen is conservative
 * and subject to tweaking once you start finalizing your system.  The MBox
 * size represents the number of messages it can hold, where one message
 * is four unsigned longs.  Not enough size and the MBox will overflow,
 * and messages will be lost.  Too much size and it unnecessarily allocates
 * memory that is never used.  The recommended size is different for each
 * TMS Package.
 *
 * The Context owns the MBox, and the Nodes under that Context are feeding
 * the MBox, along with your App.  Therefore, the maximum size of the MBox
 * is dependent on the number of Nodes controlled by the Context.
 */
#define ETH_TASK_PRIORITY	100
#define ETH_STACK_SIZE		(4 * 1024)
#define ETH_MBOX_SIZE		(16 * MAX_NODES)



#define API_FAILED(node, str)		\
	fprintf(stderr, "Node %d: %s API call failed\n", node, #str);	\
	return(-1);



/* Located in tms.c
 */
extern int ethOpen(Session **pSessions, int max_session, char *drvString);

/* reset device,
	bind this node
*/
extern int ethReset(Session *pSession);


extern int ethClose();


/* Setup the Remote End Points.
 * This is the same setup process regardless of 1AG or 1731.
*/
int setupRMP(int node, int rmpid);

/* Setup the Local End Points.
 * This is the same setup process regardless of 1AG or 1731.
 *
 * I am going to assume that I have two MEPs; one up-MEP and one
 * down-MEP.  I am going to assign them to two different ports,
 * but they could be assigned to the same port because they are
 * different directions.  You can assign as many as you need.
 *
 * Because there is more required information for an LMP,
 * and because the primary API call for this function requires
 * a pointer to the struct anyway, I am just going to fill out
 * the struct and pass it to my LMP setup-routine.
 */

int setupLMP(Session *pSession, int lmpid, int direction, int port, int vid, int vidtype, MACADR macAddr);


/* Located in hello3AH.c, but stubbed out in the 1AG/1731 HelloWorld Demos.
 */
extern int handle3AHevent(int node, unsigned long p1, unsigned long p2,
                          unsigned long p3, unsigned long p4);



/* Located in hello1AG.c, but stubbed out in the 3AH HelloWorld Demos.
 * Y.1731 uses the 1AG callback.
 */
extern int handle1AGevent(int node, int p1, UNITYPE p2,
						UNITYPE p3, UNITYPE p4);


#endif	/* _TMS_H */

