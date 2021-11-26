/*
 *
 * Copyright 1997-2010 NComm, Inc.  All rights reserved.
 *
 *
 *                     *** Important Notice ***
 *                           --- V6.72 ---
 *            This notice may not be removed from this file.
 *
 * The version 6.72 ETHERNET OAM software contained in this file may only be
 * used
 * within a valid license agreement between your company and NComm, Inc. The
 * license agreement includes the definition of a PROJECT.
 *
 * The ETHERNET OAM software is licensed only for the ETHERNET OAM application
 *
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
 * ----------------------------------------------------------------------------
 *
 *	Refer to the xxxREADME.txt files for need-to-know information
 *	concerning general information and usage requirements of
 *	the TMS API.
 *
 *	The block comments contained within this file are specific
 *	to this particular TMS Package.
 *
 *	You should read and study the HelloWorld Demo in detail.
 *
 * ----------------------------------------------------------------------------
 */



#include "tms.h"




/*--------------------------------------------------------------------------*/
/*--------------- RMP End Point Setup for both 1AG and 1731 ----------------*/
/*--------------------------------------------------------------------------*/
/* This assigns an RMP to the Node.  Not much to it except to tell it the
 * RMP's ID.  You can assign as many as you need.  The MDA will look for
 * CCM packets from that RMP.  The MDA keeps a lists of all RMPs that you
 * assign to it.  If it receives CCM packets from RMPs not on the list,
 * or from different MDAs, then it indicates a connectivity or configuration
 * problem somewhere within the network.
 *
 * The RMPs have to be uniquely identified within the MDA among all other
 * end points within the same MDA/Level.  The identifier is used when
 * assembling the CCM packets and when making callbacks to your App.
 */
int setupRMPEndPoint(int node, ETH1AG_RMPCONFIG *rmpPtr)
{

    /* Assign a Remote Maintenance Point (RMP) to the Node.
     */
    if (_ethLinCTRL(node, ETHLCTRL_1AGNEWRMP, rmpPtr) != SUCCESS)
    {

        API_FAILED(node, ETHLCTRL_1AGNEWRMP);
    }

    return(1);
}


/*--------------------------------------------------------------------------*/
/*--------------- LMP End Point Setup for both 1AG and 1731 ----------------*/
/*--------------------------------------------------------------------------*/
/* This assigns an LMP to the Node and activates it.  An LMP has more setup
 * requirements than an RMP.
 *
 * An LMP is usually a Maintenance End Point (MEP), but it could be a
 * Maintenance Intermediate Point (MIP) instead.  The difference is that
 * a MEP has an up/down direction, whereas a MIP has no direction.
 * Also, MIPs do not source CCM packets, but they will process them under
 * certain circumstances.
 *
 * Basically, in an overly simplified view, you can think of a MEP as an
 * individual port on your bridge-device, but this is not always the case.
 * There are very detailed technical explanations concerning MEPs and MIPs
 * in the standards publications.
 */
int setupLMPEndPoint(int node, ETH1AG_LMPCONFIG *lmpPtr)
{

    /* Assign a Local Maintenance Point (LMP) to the Node.
     */
    if (_ethLinCTRL(node, ETHLCTRL_1AGNEWLMP, lmpPtr) != SUCCESS)
    {

        API_FAILED(node, ETHLCTRL_1AGNEWLMP);
    }



    /* Now we have to Activate and CCM-Enable each LMP.  These are two
     * separate steps because you could have, for example, an active
     * MEP that does not send out CCM packets.
     *
     * We have to tell the MDA which LMP the call refers to.
     * Do that with the LMP identifier.
     */

    /* ------- Activate and CCM-Enable the LMP ----------
     */

 #if 0

    if (_ethLinCTRL(node, ETHLCTRL_1AGVAL,
                    lmpPtr->ident, LMP_ACTIVE, 1) != SUCCESS)
    {

        API_FAILED(node, ETHLCTRL_1AGVAL-LMP_ACTIVE);
    }


    if (_ethLinCTRL(node, ETHLCTRL_1AGVAL,
                    lmpPtr->ident, LMP_CCMENABLED, 1) != SUCCESS)
    {

        API_FAILED(node, ETHLCTRL_1AGVAL-LMP_CCMENABLED);
    }

#endif
    return(1);
}

