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

#include "utils.h"
#include "tms.h"
#include "tmsUSER.h"/*--------------------------------------------------------------------------*/
/*------------------ General Ethernet Callback Handler ---------------------*/
/*--------------------------------------------------------------------------*/
/* This is your App Callback.  The TMS makes this callback whenever it needs
 * to make an asynchronous notification to your Application.  You could
 * implement this as a 3AH-only Callback, a 1AG/1731-only Callback, or as
 * a unified callback as I did here.  The only reason I made it unified is
 * because the Ethernet TMS Package has a very simple callback with only
 * two fcodes being specific to the particular protocol in use.
 *
 * The function prototype of the Callback is according to the API Manual.
 * The implementation of the callback is up to you.  For this Demo, I simply
 * do some screen I/O.  You can choose to ignore a callback if you wish.
 *
 * Some callbacks may repeat and appear to be redundant.  Be aware that the
 * initialization process may potentially cause several seemingly redundant
 * callbacks.  The latest instance of any particular callback is always the
 * one that applies.
 *
 *			WARNING - WARNING - WARNING
 * You cannot make another API call from within the context of the Callback.
 * This is against the API.  If you wish to make a Ctrl or Poll call into the
 * TMS as a result the TMS making a callback, then you need to do it outside
 * the context of the callback.  You risk lock-out if you ignore this warning.
 * Refer to the GeneralREADME.txt file for more information.
 */
int myCallback(int node, ETHLCLBK_FC fcode, ...)
{
    int retval = SUCCESS;
    va_list alist;


    va_start (alist, fcode);

    NTCLOG_DEBUG("%d:Node Callback, fcode=0x%x - ", node, fcode);

    switch (fcode)
    {

    case ETHLCLBK_HOOKINTERRUPT:
    case ETHLCLBK_UNHOOKINTERRUPT:
    case ETHLCLBK_DEVICE:
    case ETHLCLBK_WARMSTART:
        /* The Ethernet TMS Package does not currently make these
         * callbacks.  They are present simply to maintain a uniform
         * API across all TMS packages.  Generally, they are specific
         * to actual hardware requirements and chip-device drivers.
         * However, you should note that the mechanism is present
         * and may become active in future releases.
         */
        NTCLOG_DEBUG("Ignored API callback\n");
        break;

    case ETHLCLBK_FATALERROR:
        /* Fatal errors either from TMS or the device-driver.
         * They are not always fatal, but they indicate that
         * something is broken and cannot continue.  You have
         * to pick apart the error code to determine what part
         * of the TMS or driver made the callback, and why.
         */
    {
        int errorType;
        unsigned long code;
        errorType = va_arg(alist, int);
        code = va_arg(alist, unsigned long);

        NTCLOG_ERR("FATALERROR - %s Code = 0x%08lx\n",
               (errorType == FATALERROR_TMS) ?
               "TMS" : "DRV", code);
    }
    break;

    case ETHLCLBK_1AGEVENT:	/* 1AG/1731 Event notification */
    {
        int param1;
		UNITYPE param2, param3, param4;
		param1 = va_arg(alist, int);
		param2 = va_arg(alist, UNITYPE);
		param3 = va_arg(alist, UNITYPE);
		param4 = va_arg(alist, UNITYPE);

        /* The 3AH HelloWorld Demo will have this routine
         * present but functionally empty, only so that it
         * links without error.  If you are not doing
         * 1AG/1731, then you can ignore this callback.
         */
        retval = handle1AGevent(node,
                                param1, param2, param3, param4);
    }
    break;

    case ETHLCLBK_3AHEVENT:	/* 3AH Event notification */
    {
        int param1;
		UNITYPE param2, param3, param4;
		param1 = va_arg(alist, int);
		param2 = va_arg(alist, UNITYPE);
		param3 = va_arg(alist, UNITYPE);
		param4 = va_arg(alist, UNITYPE);

        /* The 1AG or 1731 HelloWorld Demos will have this
         * routine present functionally empty, only so that
         * it links without error.  If you are not doing
         * 3AH, then you can ignore this callback.
         */
        retval = handle3AHevent(node,
                                param1, param2, param3, param4);
    }
    break;

    default:
        NTCLOG_ERR("ERROR: UNKNOWN API callback\n");
        retval = 0;
        break;
    }

    va_end (alist);
    return(retval);
}


/*--------------------------------------------------------------------------*/
/* This routine performs a setup sequence that is common to all of the TMS
 * Ethernet protocols.  The setup is the same regardless of protocol, with
 * only minor differences in data structure field-values.  The passed-in
 * parameters are used to cover those differences.
 *
 * Note that the sequence in this routine is only run one time because this
 * Demo has only one node, thus the node parameter.  If you have more than
 * one node, then some of these steps should NOT be repeated.
 *
 * Each step is explained within its block-comment.
 */
int ethOpen(Session **pSessions, int max_session, char *drvString)
{
    //ETH_CONFIG cfgReset;
    void  *ctrlptr, *pollptr;
	int sessionID;

    /* Open the TMS KLM module.
     * A Convenience call that opens the file-descriptor of the
     * TMS-device (KLM).  This will not succeed unless the KLM has
     * already been loaded into the kernel, and you have proper
     * execute-permissions.  You may need to have root permissions
     * to do either or both of these things.
     *
     * The tmsOPEN() function is located in the tmsUSER.c file.
     * If you look at the function, you will see that all it does
     * is open() the TMS-device and store away the file-descriptor,
     * all standard stuff required of any Linux App wanting access
     * to a device.  We just bury it into this call so you don't
     * have to mess with the dirty work.
     *
     * Note - if tmsOPEN() has already been called and the device is
     * already open, then the function returns without error.
     * Some non-Linux environments do not need this call.
     *
     * This call only needs to be made once per App-execution.
     * It is not specific to a node.
     */
    if (!tmsOPEN())
    {

        API_FAILED(0, tmsOPEN);
    }



    /* Terminate any past TMS execution.
     * Another Convenience call.  Since this is a HelloWorld Demo,
     * I don't know the state of the TMS KLM due to any previous
     * execution of this demo.  I want to start everything up in a
     * deterministic way.  This call almost always passes.  If it fails,
     * it usually means the TMS KLM is seriously stuck somehow, and it
     * cannot unload its resources.  This usually amounts to failed
     * attempts to terminate a previously spawned TMS kernel task.
     *
     * This does not do a tmsCLOSE(), nor does it unload the KLM,
     * but anything the TMS KLM was doing is terminated.
     *
     * This call is optional, and once per App-execution.
     * It is not specific to a node.
     */
    if (!tmsKILL())
    {

        API_FAILED(0, tmsKILL);
    }



    /*
     * From now on, I start using the standard TMS API calls.
     * You can do things in the same order if you wish, but the
     * sequence is not necessarily fixed until we get into the
     * node-specific calls.  I try to explain the reasoning of
     * each call within the block-comments.
     */



    /* Start the TMS Package.
     * The very first thing to do is an NCISTART of the TMS package.
     * TMS uses this call to initialize various internal data structures,
     * set up defaults, and prepare itself for general usage.  You don't
     * need to call this for every node because the node parameter is
     * ignored, so I just use 0 for the node number.
     *
     * The _NCISTART call is made once per App-execution, and once per
     * TMS Package.  It is not specific to a node.
     */
    if (_ethLinCTRL(0, ETHLCTRL_NCISTART) != SUCCESS)
    {

        API_FAILED(0, ETHLCTRL_NCISTART);
    }



    /* Create a Context.
     * These are kernel-side tasks.  We call them "Contexts" for a
     * more generic term.  The nodes will operate under these contexts.
     * You can run as many or as few contexts as you desire, and you
     * can run one context per node, or all nodes under one context,
     * it is up to you.  How you map the control of nodes under contexts
     * is a system engineering duty, but the following general
     * guidelines may help.
     *
     * For 1AG/1731:  The best and most robust choice for 1AG/1731
     * would be to place each node under its own Context.
     *
     * For 3AH:  It makes more sense, and is more efficient, to place
     * multiple nodes under one Context.  How many is up to you, and is
     * dependent on the general setup of your platform/product/system.
     * 3AH packets are low frequency of only once per second, and the
     * amount of work being done is very low... until someone turns on
     * a loopback.  When a loopback is active on the near-end, the node
     * will "see" all packets, and the work-load will go way up.
     *
     * In this case, I am only going to create one Context.  Again, the
     * node number is ignored and I just set it to 0.  Later, I will bind
     * the actual node to this context.  The handle to the context is
     * returned in the context-variable.
     *
     * The _NEWCONTEXT call is made for every Context you create.
     * It is not specific to a node.
     */
	for (sessionID =0; sessionID < max_session; sessionID++)
	{
		Session *pSession = pSessions[sessionID];
		if(pSession ==NULL) continue;
		
		if (_ethLinCTRL(0, ETHLCTRL_NEWCONTEXT, ETH_TASK_PRIORITY,
						ETH_MBOX_SIZE, ETH_STACK_SIZE, &pSession->m_context) != SUCCESS)
		{

			API_FAILED(0, ETHLCTRL_NEWCONTEXT);
		}

		/* Get the Driver Handle.
		 * You need to poll for the driver's entry-points before you can
		 * make the _REGAPI call.  The _REGAPI call needs to pass three
		 * pointers.  One pointer is your App's callback function that
		 * lives in your user-side App, but the other two pointers are
		 * a problem because they are the addresses of the driver's API
		 * entry points.  They live in kernel-space.  To get those two
		 * pointers from kernel-space and into user-space so that you can
		 * feed them back with the _REGAPI call, you have to first make
		 * the _DRVHANDLE poll call with a string that represents the
		 * desired driver.
		 *
		 * The identifier-string depends on which driver you wish to get
		 * the handles for.  The string was passed in as a parameter.
		 *
		 *  The TMS Ethernet Package has two drivers:
		 *
		 *	For 3AH:       "LINUXETH3AH"
		 *	For 1AG/1731:  "LINUXETH1AG"
		 *
		 * Again, we don't yet have a node, so I just use 0 for the
		 * node number.
		 *
		 * The _DRVHANDLE poll is made every time you need the driver
		 * entry-points, unless you keep the pointers around for future use.
		 * It is not specific to a node.
		 */
		if (_ethLinPOLL(0, ETHLPOLL_DRVHANDLE,
						drvString, &ctrlptr, &pollptr) != SUCCESS)
		{

			API_FAILED(0, ETHLPOLL_DRVHANDLE);
		}
	


		/* Create the Node.
		 * Now that I have the two handles for the driver, I can create
		 * the node that is associated with that driver and which will
		 * reference my callback.  Note that I am using a common callback
		 * for both 1AG/1731 and 3AH handling since the only real difference
		 * between the two is the _xxxEVENT function-code.
		 *
		 * The _REGAPI call binds the callback, the driver, and the node
		 * into a coherent operating unit.  All future references will
		 * require the node identifier.  The identifier can be any unique
		 * integer value.  You might have N nodes, 0 to N-1, so you might
		 * want to place this call, and some of the following API-calls,
		 * into a for-loop.  This Demo has only one node.
		 *
		 * The node will not actually be running until after it is placed
		 * under control of the Context that we have created and we have
		 * registered it for some TMS-services.
		 *
		 * The _REGAPI call is node-specific, and it is made once for
		 * every node you create.
		 */
		if (_ethLinCTRL(pSession->m_node, ETHLCTRL_REGAPI,
						myCallback, ctrlptr, pollptr) != SUCCESS)
		{

			API_FAILED(0, ETHLCTRL_REGAPI);
		}
	}
    return 0;
}

/* reset device,
	bind this node
*/
int ethReset(Session *pSession) //,  const char *devName, unsigned char *devMac, int if_index, int bridge)
{
    ETH_CONFIG cfgReset;

    /* Reset the Node.
     * Place the node into its default state with the _RESET call.
     * Lots of internal TMS preparation goes on within the _RESET call,
     * most of it meant to establish a deterministic start-state both
     * within TMS and within the driver.
     *
     * The config structure first has to be setup in preparation for
     * the _RESET call.  Note that this config structure is called the
     * RESET config-struct.  There may be other config structures for
     * different things in future API calls.  Here, we are essentially
     * binding this node to a specific ethernet device and mac.  For
     * 1AG/1731, this is typically a bridge device.  For 3AH, it would
     * be some ethernet port.  The device must already exist, otherwise
     * the _RESET call will fail.
     *
     * The general form of the _RESET call is to include subchannels and
     * base-addresses of hardware chips.  This is because the other TMS
     * packages and device drivers (T1/E1/T3/E3 etc.) have chip-specific
     * hardware requirements.  None of that applies to the Ethernet
     * TMS Package, so I just set those call-parameters to 0.
     *
     * The _RESET call is node-specific, and it is made once for
     * every node you create.
     */
    memset((void *)&cfgReset, 0, sizeof(ETH_CONFIG));

	if(pSession->m_if_name[0] == 0)
	{
		return -1;
	}
	// attached device  name
    strcpy(cfgReset.devName, pSession->m_if_name);

	cfgReset.bridge = pSession->m_if_bridge;
    cfgReset.insertPriorityTag = pSession->m_insert_priority_tag;


    if (_ethLinCTRL(pSession->m_node, ETHLCTRL_RESET, 0, 0, &cfgReset) != SUCCESS)
    {

        API_FAILED(pSession->m_node, ETHLCTRL_RESET);
    }



    /* Place the Node under Context control.
     * This is where I bind the node to the Context.  You cannot do this
     * call until after the _RESET call.  After _REGCONTEXT completes,
     * the node is actually running under control of the O/S kernel task
     * that you created in a previous step.  However, the node will not
     * be doing anything useful until we register a TMS-service on it.
     *
     * The _REGCONTEXT call is node-specific, and it is made once for
     * every node you create.
     */
    if (_ethLinCTRL(pSession->m_node, ETHLCTRL_REGCONTEXT, pSession->m_context) != SUCCESS)
    {
        /* The device has been bound to this node, it should be released */
        _ethLinCTRL(pSession->m_node, ETHLCTRL_UNRESET);
        API_FAILED(pSession->m_node, ETHLCTRL_REGCONTEXT);
    }



    /* At this point, we have completed all of the preliminary TMS setup
     * that is common to all protocols.  In fact, all of the previous API
     * calls are common to all other TMS packages as well.  It does not
     * matter which TMS Package you are using (TE1, SONET, SDH, Ethernet),
     * the API is uniform across all packages in terms of the required
     * preliminary setup sequences.
     */
    return(0);
}


int ethClose()
{
	return tmsCLOSE();
}
