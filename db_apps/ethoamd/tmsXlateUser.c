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



/*--------------------------------------------------------------------------*/
/* The presence of this file is simply to permit an easier dependency
 * solution within typical makefiles.
 *
 * Problems arise in 'make' because the tmsXlate.c file is used both for
 * the user-side build and the kernel-side build.  The resulting .o file
 * would normally be tmsXlate.o in both cases.  This is a problem in
 * make-environments whereby both the user-side and the kernel-side are
 * built in the same work-directory.
 *
 * Using this include-file method, we can have an explicit tmsXlateUser.c
 * and tmsXlateKrnl.c, and their associated .o files, without generating
 * any conflicts in make.  It also aleviates the need for the user-side
 * App to require special accommodations for generating a .o from a .c of
 * a different name.
 */ 

#include "tmsXlate.c"
