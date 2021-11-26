
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "tms.h"
#include "rdb_comms.h"
#include "session.h"
#include "ethlctrl.h"
#include "ethlpoll.h"
#include "rdbrpc.h"

#ifdef _DEBUG
#define NOTIFY_STATE_CHANGE(x)	rdb_set_p1_str(DOT1AG_TEST_MEP, x);
#else
#define NOTIFY_STATE_CHANGE(x) printf("%d:%s\n", pSession->m_node, x);
#endif

/* located in ../1ag/setupEndPoints.c
 */
extern int setupRMPEndPoint(int node, ETH1AG_RMPCONFIG *rmpPtr);
extern int setupLMPEndPoint(int node, ETH1AG_LMPCONFIG *lmpPtr);

/*--------------------------------------------------------------------------*/
/*------------------------- 1AG Settings -----------------------------------*/
/*--------------------------------------------------------------------------*/
/* These are 1AG specific values. Set them according to your platform.
 *
 * In TMS, a Maintenance Domain/Association is called an "MDA" for short.
 * The MDA is used to form a 48-byte entity-identifier, called the Maintenance
 * Association Identifier, or MAID, within the CCM network packets.
 * Note that the MAID is not the same as your node identifier.  The MAID
 * can be formatted in several different ways and the format you choose is
 * indicated in the format field of the ETH1AG_MDACONFIG structure.
 * Refer to 802.1AG for other possible formats of the MAID.  I use the
 * string-format in this Demo.
 */
#define MY_1AG_DOMAINNAME	"MD1"
#define MY_1AG_ASSOCNAME	"MA1"


/* All of the following are common to both 1AG and 1731.
 *
 * The device is usually a bridge, like "br0" or something, but it can be
 * any other device.  I set it to a NIC only for ease of demonstration,
 * but if you actually have a bridge-device, then you should edit _DEVNAME
 * and _DEVMAC accordingly.
 *
 * Note: The _RESET call will fail if the device name does not already exist.
 */



/*--------------------------------------------------------------------------*/
/*---------------------- 1AG Callback Handler ------------------------------*/
/*--------------------------------------------------------------------------*/
/* Here I handle a few of the potential 1AG/1731 events.  A node running
 * Y.1731 makes many of the same callbacks as 1AG.  There are several other
 * callbacks that are not included in this Hello World demo.
 *
 * Event Callbacks in 1AG/1731 contain the MDA/LMP/RMP identifier in p2.
 * This value is used to indicate which of the three entities the callback
 * applies to.  The identifier for an MDA is always 0, and the identifiers
 * for LMPs and RMPs are the same values that you assigned when you added
 * the LMPs and RMPs to the MDA during setup.
 *
 * Remember that it is the MDA (the Node) that makes callbacks, not LMPs
 * and RMPs.  The identifier is just used to indicate, if applicable, the
 * LMP or RMP that the callback applies to.
 *
 * The reason the identifier for Xconn and Unexp is the MDA and not an LMP
 * is because when those conditions occur, they will generally occur across
 * all LMPs everywhere, and it would result in a thrashing-of-notifications.
 * The 802.1AG Standard is very specific that these two notifications reflect
 * an any-or-nothing condition within the MD, and that if further info is
 * needed, the Sys Admin can poll the available information to determine the
 * source of any problem.
 */
int handle1AGevent(int node, int p1, UNITYPE p2,
                   UNITYPE p3, UNITYPE p4)
{
	Session *pSession=NULL;
	int pdu_error =0;
	int pdu_completed =0;
	if (node >=0 && node < g_config.m_max_session)
	{
		pSession = g_session[node];
	}
	if(pSession ==NULL )
	{
		return 0;
		NTCSLOG_DEBUG("%d:1AG Event Callback Not Handled", node);
	}
    /* p1 is an ETH1AG_EVENT, p2 is the identifier, and p3 and p4
     * are any other parameters that may accompany the event.
     * Not all events will use all parameters.
     */
    switch ((int)p1)
    {
    case CCM_XCONNECT:
        /* Xconn is the logical-OR of any Xconn (mismerge)
         * and any Level Mismatch.  If this indication is
         * clear, then it is clear everywhere.  You can Poll
         * the MDA or LMPs if you need to know the details
         * of a SET condition.
         */
        NTCSLOG_DEBUG("Cross-Connect %s", (p3) ? "SET" : "CLR");
        break;
	case CCM_RDIMEP:

		NTCSLOG_DEBUG("last rdi changed, RMP %d, RDI %d", (int)p1, (int)p2);
		break;
    case CCM_UNEXPMEP:
        /* Unexp is the logical-OR of any Unexpected MEP and
         * any CCM-Interval Mismatch.  If this indication is
         * clear, then it is clear everywhere.  You can Poll
         * the MDA or LMPs if you need to know the details
         * of a SET condition.
         */
        NTCSLOG_INFO("Unexpected MEP %s, RMP %d", (p3) ? "SET" : "CLR", (int)p2);

        break;

    case CCM_LOCMEP:
        NTCSLOG_DEBUG("Loss of Continuity with RMP %d %s",
               (int)p2, (p3) ? "SET" : "CLR");
        break;

    case CCM_MDLEVEL:
        NTCSLOG_DEBUG("CCM RMP %d MDLevel mismatch %d",
               (int)p2, (p3) );
        break;



    case CCM_ALARM:
        NTCSLOG_DEBUG("Fault Alarm %s: Highest Defect = %d",
               (p3) ? "SET" : "CLR", (int)p4);
        break;

	case CCM_NEWRMP:
	{
		unsigned char *mac = (void *)p4;
		NTCSLOG_DEBUG("New RMP %d macAdr %02x:%02x:%02x:%02x:%02x:%02x\n",
			(int)p2, mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
	}
	break;

	case LBM_COMPLETE:			/* Tx of LBM Msgs completed */
        NTCSLOG_DEBUG("LBM completed");
        pSession->m_lbm_state.Event.m_completed =1;
		break;
	case LTM_REPLY:			/* state of Dest's response to LTM */
		//mPtr, LTM_REPLY(p1), LMPIDENT(p2), ltmTxPtr->transID(p3), 1(p4)
		NTCSLOG_DEBUG("LTR reply");
		if(p4)
		{
			pSession->m_ltm_state.Event.m_reply_found++;
        }
        else
        {
			pSession->m_ltm_state.Event.m_completed=1;
		}
		break;
#ifdef NCI_Y1731

	/* PDU event(lmpPtr, PDUCLBK_RXCOMPLETE(p1), runID(p2), pduPtr->pduID(p3));*/
	case  PDUCLBK_DELETED:	/* Delete completed	  - deleted */
		NTCSLOG_DEBUG("PDU DELETED, ID=%d", p3);
		break;
	case  PDUCLBK_PASSED:		/* Setup/Restart passed	  - running */
		NTCSLOG_DEBUG("PDU PASSED, ID=%d", p3);
		break;
	case  PDUCLBK_OVERFLOW:	/* Overflow		  - stopped */
		NTCSLOG_DEBUG("PDU OVERFLOW, ID=%d", p3);
		pdu_error = EVENT_ERROR_PDU_OVERFLOW;
		break;
	case  PDUCLBK_TXFAILED:	/* Tx of PDU failed	  - stopped */
		NTCSLOG_DEBUG("PDU TX FAILED, ID=%d", p3);
		pdu_error = EVENT_ERROR_PDU_TXFAILED;
		break;

	case  PDUCLBK_TXCOMPLETE:	/* Tx complete
				 *	- stopped if no Rx-timeout,
				 *	  else running
				 */
		NTCSLOG_DEBUG("PDU TXCOMPLETE, ID=%d", p3);

		break;

	case  PDUCLBK_RXCOMPLETE:	/* Rx complete/timed out  - stopped */
		NTCSLOG_DEBUG("PDU RXCOMPETE, ID=%d", p3);
		pdu_completed = 1;
		break;

	case  PDUCLBK_STOPPED:	/* Stop command completed - stopped */
		NTCSLOG_DEBUG("PDU STOPPED, ID=%d", p3);
		pdu_completed = 1;
		break;

	case  PDUCLBK_CONFLICT:	/* Conflict detected	  - stopped */
		NTCSLOG_DEBUG("PDU CONFLICT, ID=%d", p3);
		pdu_error = EVENT_ERROR_PDU_TXFAILED;
		break;

	case  PDUCLBK_FAILED:		/* Setup/Restart failed	  - stopped */
		NTCSLOG_DEBUG("PDU FAILED, ID=%d", p3);
		pdu_error = EVENT_ERROR_PDU_TXFAILED;
		break;

#if 0
	case AIS_RECV:
		NTCSLOG_DEBUG("Rx AIS %s\n", (p3) ? "SET" : "CLR");
		if(p3)
		{
			pSession->m_ais_recv_count++;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_AISRxon, 1);
		}
		else
		{
			pSession->m_ais_recv_count=0;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_AISRxon, 0);
		}
		break;

	case LCK_RECV:
		NTCSLOG_DEBUG("Rx LCK %s: LMP %d\n", (p3) ? "SET" : "CLR", (int)p4);
		if(p3)
		{
			pSession->m_lck_recv_count++;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_LCKRxon, 1);
		}
		else
		{
			pSession->m_lck_recv_count=0;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_LCKRxon, 0);
		}
		break;
	case CSF_RECV:
		NTCSLOG_DEBUG("Rx LCK %s: LMP %d\n", (p3) ? "SET" : "CLR", (int)p4);
		if(p3)
		{
			pSession->m_lck_recv_count++;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_CSFRxon, 1);
		}
		else
		{
			pSession->m_lck_recv_count=0;
			RDB_SET_P1_BOOLEAN(Y1731_Lmp_CSFRxon, 0);
		}
		break;
#endif
#endif

    default:
        NTCSLOG_DEBUG("%d:1AG Event Callback Not Handled", node);
        return(0);
    }

    if (pdu_error||pdu_completed)
    {
		if(pdu_error) pdu_completed =1;

		if(p3 == pSession->m_dmm_data.runID)
		{
			pSession->m_dmm_state.Event.m_completed =pdu_completed;
			pSession->m_dmm_state.Event.m_has_error = pdu_error;
		}
		else if(p3 == pSession->m_lmm_data.runID)
		{
			pSession->m_lmm_state.Event.m_completed =pdu_completed;
			pSession->m_lmm_state.Event.m_has_error = pdu_error;
		}
		else if(p3 >= pSession->m_slm_data.runID)
		{
			int n =p3 - pSession->m_slm_data.runID;
			if (n  < pSession->m_slm_data.testID_num &&
				pSession->m_slm_data.state[n] == SLM_STATE_SENT)
			{

				int completed =1;
				pSession->m_slm_data.state[n] = SLM_STATE_RECV;

				for(n =0; n <pSession->m_slm_data.testID_num; n++)
				{
					if( pSession->m_slm_data.state[n] == SLM_STATE_SENT)
					{
						completed =0;
						break;
					}
				}
				if(completed)
				{
					pSession->m_slm_state.Event.m_completed =pdu_completed;
					pSession->m_slm_state.Event.m_has_error = pdu_error;
				}
			}
		}


    }


    return(SUCCESS);
}


/*--------------------------------------------------------------------------*/
/*----------------------- Stubbed for this Demo ----------------------------*/
/*--------------------------------------------------------------------------*/
/* This is called from myCallack() in the helloCommon.c file, but it is only
 * present so the HelloWorld Demo will link.  This should never get called.
 */
int handle3AHevent(int node, unsigned long p1, unsigned long p2,
                   unsigned long p3, unsigned long p4)
{
    fprintf(stderr, "Node %d: OUT OF PLACE callback", node);
    return(0);
}


/*--------------------------------------------------------------------------*/
/*--------------------------- 1AG Registration -----------------------------*/
/*--------------------------------------------------------------------------*/
/* Complete the node's setup procedure for 1AG Operation.
 * 1AG is more complicated and involved than 3AH.  You need to have more
 * up-front information to configure the node.  A single node represents
 * a Maintenance Domain/Maintenance Association and all of its endpoints,
 * both local and remote.
 */
int registerFor1AGService(Session *pSession)
{
    ETH1AG_MDACONFIG mdaConfig;


    /* Now we register for 1AG Processing on the node.
     *
     * This creates the Maintenance Domain/Associaton entity,
     * but TMS still calls it an MDA.  The MDA will not yet have
     * any endpoints; we assign those afterward.
     *
     * Before I make the _1AGREG call, I have to first fill out the
     * 1AG config-struct.  This assigns the MDA operating information
     * to the node.  The values in these fields are dictated by the
     * 802.1AG standard.
     *
     * The TMS API is uniform in terms of 1AG or 1731 operations,
     * and there is much overlap between the 1AG and Y.1731 API calls.
     * The _1AGREG call is where you make your decision as to whether
     * or not the node is doing 1AG or 1731.
     */
    memset((void *)&mdaConfig, 0, sizeof(ETH1AG_MDACONFIG));

	/* Set the Level, and do not run the node in 1731 mode.
     * All other config-struct fields, if any, are left set to 0
     */
	mdaConfig.vidPrimary  = pSession->m_mda_data.PrimaryVID;///the Primary VID (0 means no vlans)
    mdaConfig.enableY1731 = pSession->m_mda_state.Y1731_MDA_enable;

	if(mdaConfig.enableY1731)
	{
		mdaConfig.mdLevel     = pSession->m_mda_data.MegLevel;
		mdaConfig.mdFormat = 1;
		mdaConfig.mdLength = 0;
		mdaConfig.maFormat = pSession->m_mda_data.MegIdFormat;
		mdaConfig.maLength = pSession->m_mda_data.MegIdLength;
		strcpy((void *)mdaConfig.maID, pSession->m_mda_data.MegId);
	}
	else
	{

		mdaConfig.mdLevel     = pSession->m_mda_data.MdLevel; // MY_MDA_LEVEL;

		 /* Use the string format to describe the Domain value.
			 */

		mdaConfig.mdFormat = pSession->m_mda_data.MDFormat;// 4;
		if(mdaConfig.mdFormat <1 || mdaConfig.mdFormat >4) return -1;
		if(mdaConfig.mdFormat == 1)
		{
			mdaConfig.mdLength =0;
		}
		else
		{
			mdaConfig.mdLength = strlen(pSession->m_mda_data.MdIdType2or4); //strlen(MY_1AG_DOMAINNAME);
			strcpy((void *)mdaConfig.mdID, pSession->m_mda_data.MdIdType2or4);
		}
		NTCSLOG_DEBUG("mdFormat=%d, mdLength=%d", mdaConfig.mdFormat, mdaConfig.mdLength);



		/* Use the string format to describe the Association value.
		 * Yes, it's a different number.
		 */
		mdaConfig.maFormat =  pSession->m_mda_data.MaFormat;// 2;
		switch(mdaConfig.maFormat)
		{
		case 1: ///1: Vid - 2 bytes,
		case 3:///3: Uint16  - 2 bytes
			mdaConfig.maLength =2;
			pSession->m_mda_data.MaLength =2;
			memcpy(mdaConfig.maID, &pSession->m_mda_data.MaIdNonType2, 2 );
			break;
		case 2: ///2: String (up to 46 max)
			//NTCSLOG_DEBUG("registerFor1AGService: maid=%s",pSession->m_mda_data.MaIdType2);
			mdaConfig.maLength = strlen(pSession->m_mda_data.MaIdType2); //strlen(MY_1AG_ASSOCNAME);
			pSession->m_mda_data.MaLength =mdaConfig.maLength;
			strcpy((void *)mdaConfig.maID, pSession->m_mda_data.MaIdType2);
			break;
		case 4: ///4: VPN id- 3 bytes
			mdaConfig.maLength =3;
			pSession->m_mda_data.MaLength =3;
			memcpy(mdaConfig.maID, &pSession->m_mda_data.MaIdNonType2, 3);
			break;
		default:
			return -1;
		}

	}//if(pSession->m_mda_state.Y1731_MDA_enable)

	if(mdaConfig.mdLevel <0 || mdaConfig.mdLevel >7)
	{
		fprintf(stderr, "Incorrent mdlevel=%d\n", mdaConfig.mdLevel);
		return(-1);

	}

    if (_ethLinCTRL(pSession->m_node, ETHLCTRL_1AGREG, &mdaConfig) != SUCCESS)
    {

        API_FAILED(pSession->m_node, ETHLCTRL_1AGREG);
    }

	return 0;
}


/* Setup the Remote End Points.
 * This is the same setup process regardless of 1AG or 1731.
*/
int setupRMP(int node, int rmpid)
{
    ETH1AG_RMPCONFIG rmpConfig;

    memset((void *)&rmpConfig, 0, sizeof(ETH1AG_RMPCONFIG));

	if( VALID_LRMPID(rmpid))
	{
		rmpConfig.ident = rmpid; //MY_MDA_RMPIDENT1;
		if (_ethLinCTRL(node, ETHLCTRL_1AGNEWRMP, &rmpConfig) != SUCCESS)
		{

			API_FAILED(node, ETHLCTRL_1AGNEWRMP);
			return -1;
		}

		return 0;
	}
	return -1;
}

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

int setupLMP(Session *pSession, int lmpid, int direction, int port, int vid, int vidtype, MACADR macAddr)
{
    ETH1AG_LMPCONFIG lmpConfig;

    memset((void *)&lmpConfig, 0, sizeof(ETH1AG_LMPCONFIG));

	NTCSLOG_DEBUG("setupLMP(node=%d, lmpid=%d, direction=%d, port=%d, vid=%d, vidtype=%d,mac=%02x:%02x:%02x:%02x:%02x:%02x)",
				pSession->m_node, lmpid, direction, port,vid, vidtype,
			macAddr[0], macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);


	/* --  validate lmp parameter */

	///1)VALID_LRMPID(lmpid)

	///2)direction must be -1, 1, 0

	///3)/* 22.4 - Stations cannot have MIPs or Up-MEPs*/
	if (pSession->m_if_bridge ==0 && direction >= 0)
	{
		fprintf(stderr, "Stations cannot have MIPs or Up-MEPs\n");
		return(-1);
	}
	///4)	/* 12.14.6.3.3.a7 - Up-MEP cannot be configured with no VID	 */
//	if (direction == 1)
//	{
//		vid = pSession->m_mda_data.MdLevel;
//	}

    /* ------- Assign an up-MEP LMP to Port 1----------
     */
	if( VALID_LRMPID(lmpid))
	{
		lmpConfig.ident     =  lmpid; //MY_MDA_LMPIDENT1;
		lmpConfig.direction =  direction;//1;	/* 1 = up, -1 = dn, 0 = MIP */
		lmpConfig.port      =  port; //1;	/* Record-only. Ignored by TMS */

		/* The next two fields only apply to VLANs.
		 * They can be 0 otherwise.
		 */
		lmpConfig.vid     = vid; //0;		/* the vlan id, else 0 */
		lmpConfig.vidType = vidtype; //0;		/* LMP_CVLAN or LMP_SVLAN, else 0 */
	//	lmpConfig.vid     = 2;		/* the vlan id, else 0 */
	//	lmpConfig.vidType = LMP_CVLAN;		/* LMP_CVLAN or LMP_SVLAN, else 0 */

		/* the macAdr of the port that the LMP is attached to
		 */
	   // if (!str2MAC(lmpConfig.macAdr, pSession->m_bridge_port_mac1))//MY_MDA_PORTMAC1))
		//    return(-1);

		memcpy(lmpConfig.macAdr, macAddr, 6);


		/* Assign a Local Maintenance Point (LMP) to the Node.
		 */
		if (_ethLinCTRL(pSession->m_node, ETHLCTRL_1AGNEWLMP, &lmpConfig) != SUCCESS)
		{

			API_FAILED(pSession->m_node, ETHLCTRL_1AGNEWLMP);
		}


		return 0;
	}
	return -1;

}

#ifdef _AVCID_MAC
/* check whether the interface to attach is busy*/
static int busyif_session(Session *pSession)
{
	int session_ID=1;
	for(; session_ID < pSession->m_config->m_max_session; session_ID++)
	{
		Session *p = pSession->m_sessions[session_ID];
		if( p && pSession  != p )
		{
			if (p->m_mda_state.PeerMode_ready)
			{
				if( strcasecmp(pSession->m_if_name, p->m_if_name) ==0)
				{
					return 1;
				}
			}
		}
	}
	return 0;
}

#endif


int  startup_1ag(Session *pSession)
{
	NTCSLOG_DEBUG("startup_1ag >>>");

	if ( load_mda(pSession) <0)
	{
		NTCSLOG_WARNING("Incorrect MDA config\n");
		RDB_SET_P1_2STR(MDA_Status, "Error: ", "Incorrect MDA config" );
		return -1;

	}


	// lookup the interface
	if(get_session_if(pSession) < 0){
		NTCSLOG_ERR("Cannot find device \'%s\'",pSession->m_if_name);
		RDB_SET_P1_2STR(MDA_Status, "Error: Cannot find device: ", pSession->m_if_name);
		return -1;
	} else{
		NTCSLOG_INFO("bridge interface \'%s\'", pSession->m_if_name);
	}


#ifdef _AVCID_MAC
	/* stop if the same interface is used already*/
	if(busyif_session(pSession)){
		NTCSLOG_WARNING("Device %s has been used in other session", pSession->m_if_name);
		RDB_SET_P1_2STR(MDA_Status, "Error: Busy device: ", pSession->m_if_name);
		return -1;
	}

	/* generate LMP mac from AVCID*/
	memset(pSession->m_lmp_mac, 0, sizeof(pSession->m_lmp_mac));
	if(pSession->m_mda_data.avcid[0])
	{
		// Example1: AVC000001095584 = Sw MAC 02:00:01:09:55:84
		// Example2: AVC000002095584 = Sw MAC 02:00:02:09:55:84
		unsigned short b[6];
		char *p = &pSession->m_mda_data.avcid[strlen(pSession->m_mda_data.avcid) -10];
		if(p > pSession->m_mda_data.avcid)
		{
			if(sscanf(p,"%02hx%02hx%02hx%02hx%02hx",
					&b[1],&b[2],&b[3],&b[4],&b[5]) == 5 )
			{
				pSession->m_lmp_mac[0]= 0x02;
				pSession->m_lmp_mac[1]= b[1];
				pSession->m_lmp_mac[2]= b[2];
				pSession->m_lmp_mac[3]= b[3];
				pSession->m_lmp_mac[4]= b[4];
				pSession->m_lmp_mac[5]= b[5];

			}
		}

	}
	RDB_SET_P1_MAC(LMP_macAdr, pSession->m_lmp_mac);


#endif

	/*
		reset and bind hardware device
	*/
	if (ethReset(pSession) < 0){
		NTCSLOG_WARNING("Cannot bind local device %s\n",  pSession->m_if_name);
		RDB_SET_P1_2STR(MDA_Status, "Error: Cannot bind to device: ", pSession->m_if_name);
        return -1;
    }


    /* Now we have to "register" for the desired TMS-service.
     * This will actually cause the node to begin doing work.
     * Assuming that we successfully return from the registration
     * process, the node will be running the protocol.
     */
    if (registerFor1AGService(pSession) < 0)
    {
		ethlctrl_unregcontext(pSession);
		ethlctrl_unreset(pSession);
		NTCSLOG_WARNING("register 802.1ag service\n");
		RDB_SET_P1_2STR(MDA_Status, "Error: ", "Cannot register 802.1ag service" );
		return -1;
	}

	pSession->m_mda_state.MDA_config_changed =0;
	pSession->m_mda_state.PeerMode_ready =pSession->m_mda_state.PeerMode;
	//sync  Y1731_enable_ready
	pSession->m_mda_state.Y1731_enable_ready = pSession->m_mda_state.Y1731_MDA_enable;
	if(pSession->m_mda_state.Y1731_enable_ready)
	{
		RDB_SET_P1_STR(MDA_Status, "Success: 802.1ag/Y1731 is ready");
	}else
	{
		RDB_SET_P1_STR(MDA_Status, "Success: 802.1ag is ready");
	}
	NTCSLOG_DEBUG("startup_1ag <<<");

	return 0;
}


void shutdown_1ag(Session *pSession)
{
	if (pSession->m_mda_state.PeerMode_ready)
	{
		NTCSLOG_DEBUG("shutdown_1ag >>>");
		//MEP_disable(pSession);
		LMP_disable(pSession);
		RMP_disable_all(pSession);
		//pSession->m_lmp_data.LMPenable =0;
		//ethlctrl_nciunstart(pSession);
		ethlctrl_1agunreg(pSession);
		ethlctrl_unregcontext(pSession);
		ethlctrl_unreset(pSession);
		//ethlctrl_delcontext(pSession, pSession->m_context);
		unlock_avc_if(pSession);
		pSession->m_mda_state.MDA_config_changed =0;
		pSession->m_mda_state.PeerMode_ready = 0;
		pSession->m_mda_state.Y1731_enable_ready = 0;
		pSession->m_lbm_state.Send_started = 0;
		pSession->m_ltm_state.Send_started = 0;
		pSession->m_lmm_state.Send_started = 0;
		pSession->m_dmm_state.Send_started = 0;
		pSession->m_slm_state.Send_started = 0;

		RDB_SET_P1_STR(MDA_Status, "802.1ag is not started");
		NTCSLOG_DEBUG("shutdown_1ag <<<");
	}
}
// update 1ag config if it is changed ,

int auto_start_1ag(Session *pSession)
{
	int err =0;
	if( pSession->m_mda_state.PeerMode_ready)
	{
		if(pSession->m_mda_state.MDA_config_changed)
		{
			shutdown_1ag(pSession);
			err = startup_1ag(pSession);
		}
	}
	else
	{
		err = startup_1ag(pSession);
	}

	return err;
}

// sync the MDA_PeerMode and 1ag config
void MDA_sync(Session *pSession)
{
	int err;

	if (pSession->m_mda_state.PeerMode)
	{
		err = auto_start_1ag(pSession);
		if(err == 0)
		{
			if( pSession->m_lmp_state.CCMenabled)
			{
				// will enable LMP,  MEP and CCM
				CCM_enable(pSession);
			}
			else if( pSession->m_lmp_state.MEPactive)
			{
				// will enable LMP and  MEP
				MEP_enable(pSession);
			}
			else
			{
				// make sure LMP enabled.
				if (LMP_enable(pSession) ==0)
				{
					// disable MEP
					MEP_disable(pSession);

					CCM_disable(pSession);
				}
			}
			RMP_retrieve(pSession);
			RMP_enable_all(pSession);
		}
	}
	else
	{
		pSession->m_lmp_state.MEPactive =0;
		shutdown_1ag(pSession);
	}
}



/// rdb notify logical
/// 1. MDA_Enable changed: 	 then
///		if MEPActive == 1 then update_1ag_config(MEP_Disable, LMP_Disable), LMP_Enable, MEP_Enable
///					LMP_enable =1
///					MDA_PeerMode =1
///		if MEPActive == 0 MEP_disable, MDA_Sync(MEP_active, LMP_enable, then MDA_PeerMode)

/// 2. LTM_send >0 then
///		update_1ag_config( start it if it is not start),
///		send LTM
///		wait for reply
///		On finish or error, MDA_Sync(MEP_active, LMP_enable, then MDA_PeerMode)

/// 3. MEPActive Changed then
///  	if MEPActive == 1 then	update_1ag_config( start it if it is not start),LMP_Enable, MEP_Enable
///						LMP_enable =1
///						MDA_PeerMode =1
///		if MEPActive == 0 MEP_disable, MDA_Sync(MEP_active, LMP_enable, then MDA_PeerMode)
/*--------------------------------------------------------------------------*/
/*----------------------------- Main loop---------------------------------------*/
/*--------------------------------------------------------------------------*/
/*
 */

int main_loop( TConfig *pConfig)
{
	int err;
	int current_time_ms =0;
	Session *pSession;

	int sessionID;

	pConfig->m_running=1;

	rdb_set_2str_all(pConfig, MDA_Status, "802.1ag is not started", "");

#ifdef _RDB_RPC
	rpc_reset_idle();
#endif

    /* The first things that we do are generic setup sequences that
     * are common to all TMS Packages. I've broken this out into a
     * separate function because it is the same for any of the
     * TMS Packages with only minor differences.  The differences
     * are covered via the simple parameters that are passed.
     */
    if (ethOpen(g_session,  pConfig->m_max_session, "LINUXETH1AG") < 0)
    {
		NTCLOG_ERR("Cannot open TMS device\n");

		rdb_set_2str_all(pConfig, MDA_Status, "Error: ", "Cannot open TMS device");

        return(-1);
    }


    /* At this point, you should see indications in the callback.
     * The node will make asynchronous callbacks as things happen.
     * You need to handle those callbacks because they may contain
     * information or requests from the far-end.
     *
     * This Demo handles a couple of those callbacks for
     * illustrative purposes.
     *
     * Even though this App program may terminate, the Node on the
     * kernel-side will continue to execute.  Any callbacks that the
     * node makes will simply get dropped on the floor since the App
     * is not there to catch them.
     *
     * If you wish, you can simply do a tmsKILL() followed by
     * a tmsCLOSE(), and the kernel-side will terminate.
     *
     * The tmsKILL() routine shuts down everything-TMS, but there
     * are much more graceful ways to terminate the TMS KLM,
     * and/or just terminate an individual node.  There are API
     * calls that undo what each of the above calls do, thus you
     * can deterministically disassemble all or some of your nodes.
     */

//    NTCLOG_DEBUG("\n\tAll TMS 1AG setup calls completed Successfully.\n\n");
//
//    NTCLOG_DEBUG("\tAwaiting CCM packets from far end RMPs.\n\n");
//
//    NTCLOG_DEBUG("\tThere should be Connectivity indications "
//           "showing up in the callback.\n\n");
//
//    NTCLOG_DEBUG("\tWaiting in a tight-loop.  Ctrl-C to exit.\n\n");

	/* Do other work here.  Your real App should probably
	 * be set up as some sort of background task.
	 * You can do Poll calls, or more Ctrl calls as you desire.
	 */
   /// 0 -- start main loog
	NTCLOG_DEBUG("start main loop");
	while(pConfig->m_running)
	{

		/* initialize loop variables */
		fd_set fdr;
		int selected;
		int fd = rdb_fd(g_rdb_session);
		//int nfds = 1;
		struct timeval timeout = { .tv_sec = ACTIVE_COUNT_UPDATE_PERIOD, .tv_usec = 0 };

		FD_ZERO(&fdr);

		/* put database into fd read set */
		FD_SET(fd, &fdr);

		/* select */
		selected = select(fd+1 , &fdr, NULL, NULL, &timeout);

		pSession = NULL;

        /// 1 -- stop running, extern request
		if (!pConfig->m_running)
		{
			NTCLOG_DEBUG("running flag off");
			break;
		}
		else if (selected < 0) /// 2 ---faulty condition
		{

			// if system call
			if (errno == EINTR)
			{
				NTCLOG_DEBUG("system call detected");
				continue;
			}

			NTCLOG_DEBUG("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		else if ((selected > 0 && FD_ISSET(fd, &fdr)) || pConfig->m_force_rdb_read) // detect fd
		{
			int len;
			//memset(pConfig->m_tempbuf, 0, BUFLEN);
			pConfig->m_tempbuf[0] =0;
			len = BUFLEN - 1;

			err = rdb_getnames(g_rdb_session, "", pConfig->m_tempbuf, &len, TRIGGERED);
			pConfig->m_tempbuf[len] = 0;
			if (err ==0)
			{
				char *pName, *pEnd;
				char rdbname[MAX_RDB_NAME_SIZE];
				pName = pConfig->m_tempbuf;
				NTCLOG_DEBUG("detect rdb changed: %s", pName);

				while(*pName)
				{
					pEnd = strchr(pName, '&');
					if(pEnd)
					{
						*pEnd =0;
						pEnd++;
					}
					sessionID= strip_p1(rdbname, pName, pConfig->m_max_session);
					pSession = g_session[sessionID];

					//split each rdb name
					//NTCLOG_DEBUG("rdb changed: %s", pName);
					if(pSession )
					{
						int tmp;
						if(strcmp(rdbname, DOT1AG_Changed) == 0)
						{
							rdb_get_uint(pName, &tmp);// read it to clear up the notification
							/// if mda config changed, reload it
							if(check_mda_config(pSession))pSession->m_mda_state.MDA_config_changed =1;

							// check LBM
							RDB_GET_P1_UINT(LBM_LBMsToSend, &tmp);
							if(tmp != pSession->m_lbm_state.Send)
							{
								pSession->m_lbm_state.Send = tmp;
								if(pSession->m_lbm_state.Send &&
									pSession->m_lbm_state.Send_started ==0 &&
										pSession->m_lbm_state.Event.m_send_request ==0)
								{
									pSession->m_lbm_state.Event.m_send_request =1;
								}
							}
							// Check LTM
							RDB_GET_P1_UINT(LTM_send, &tmp);
							if(tmp != pSession->m_ltm_state.Send)
							{
								pSession->m_ltm_state.Send = tmp;
								if(pSession->m_ltm_state.Send&&
									pSession->m_ltm_state.Send_started ==0 &&
										pSession->m_ltm_state.Event.m_send_request ==0)
								{
									pSession->m_ltm_state.Event.m_send_request =1;
								}
							}

							//Check PeerMode
							RDB_GET_P1_BOOLEAN(MDA_PeerMode, &tmp);
							pSession->m_mda_state.PeerMode = tmp;
							///1) if peermode, enable mepactive,
							if( pSession->m_mda_state.PeerMode )
							{
								RDB_SET_P1_BOOLEAN(LMP_MEPactive, 1);
								//pSession->m_lmp_state.MEPactive =1;
								pSession->m_mep_state_changed = 1;
								pSession->m_mda_state.MDA_extra_config_changed = 1;
							}
							///2) Peer state change, then MD_config changed
							if(pSession->m_mda_state.PeerMode_ready != pSession->m_mda_state.PeerMode)
							{
								pSession->m_mda_state.MDA_config_changed = 1;
							}

							// Check CCM
							RDB_GET_P1_BOOLEAN(LMP_CCMenable, &tmp);
							if(tmp != pSession->m_lmp_state.CCMenabled)
							{
								pSession->m_lmp_state.CCMenabled = tmp;
								if(pSession->m_lmp_state.CCMenabled != pSession->m_lmp_state.CCM_enable_ready)
								{
									pSession->m_ccm_state_changed = 1;
								}
							}
							RDB_GET_P1_BOOLEAN(MDA_GetStats, &tmp);
							if(tmp)
							{
								if( MEP_collect(pSession) == 0 && RMP_collect(pSession) == 0)
								{
									RDB_SET_P1_STR(LMP_Status, "Success: collect stats");
								}
								RDB_SET_P1_BOOLEAN(MDA_GetStats, 0);
							}


							RDB_GET_P1_BOOLEAN(LMP_MEPactive, &tmp);
							if (pSession->m_lmp_state.MEPactive != tmp)
							{
								pSession->m_lmp_state.MEPactive = tmp;
								if(pSession->m_mep_state_changed ==0)
								{
									NTCSLOG_DEBUG("rdb changed %d: %s", sessionID, LMP_MEPactive);

									pSession->m_mep_state_changed = 1;
								}
								if(pSession->m_lmp_state.MEPactive )
								{
									pSession->m_lmp_state.Reload_config=1;
								}
							}
							// check VID
							RDB_GET_P1_INT(LMP_vid, &tmp);
							if(pSession->m_lmp_data.vid != tmp)
							{
								pSession->m_lmp_data.vid = tmp;
								if(pSession->m_mep_state_changed ==0 &&
										pSession->m_lmp_data.vidtype !=0)
								{
									NTCSLOG_DEBUG("rdb changed %d: %s", sessionID, LMP_vid);

									pSession->m_mep_state_changed = 1;
								}
							}
							// check VIDtype
							RDB_GET_P1_INT(LMP_vidtype, &tmp);
							if(pSession->m_lmp_data.vidtype != tmp)
							{
								pSession->m_lmp_data.vidtype = tmp;
								if(pSession->m_mep_state_changed ==0)
								{
									NTCSLOG_DEBUG("rdb changed %d: %s", sessionID, pName);

									pSession->m_mep_state_changed = 1;
								}
							}
#ifdef NCI_Y1731

							RDB_GET_P1_BOOLEAN(Y1731_Mda_Enable, &tmp);
							if(pSession->m_mda_state.Y1731_MDA_enable != tmp)
							{
								pSession->m_mda_state.Y1731_MDA_enable = tmp;
								if (pSession->m_mda_state.Y1731_MDA_enable != pSession->m_mda_state.Y1731_enable_ready)
								{
									// force disable MEP LMP, update the config
									pSession->m_mda_state.MDA_config_changed =1;
								}
							}

							// Check DMM
							RDB_GET_P1_UINT(Y1731_Dmm_Send, &tmp);
							if(pSession->m_dmm_state.Send != tmp)
							{
								pSession->m_dmm_state.Send = tmp;
								if( pSession->m_dmm_state.Send&&
										pSession->m_dmm_state.Send_started ==0 &&
										pSession->m_dmm_state.Event.m_send_request ==0)
								{
									pSession->m_dmm_state.Event.m_send_request=1;
								}

							}
							//Check SLM
							RDB_GET_P1_UINT(Y1731_Slm_Send, &tmp);
							if(pSession->m_slm_state.Send != tmp)
							{
								pSession->m_slm_state.Send = tmp;
								if( pSession->m_slm_state.Send&&
										pSession->m_slm_state.Send_started ==0 &&
										pSession->m_slm_state.Event.m_send_request ==0)
								{
									pSession->m_slm_state.Event.m_send_request=1;
								}
							}

						}

#endif
						else if(strcmp(rdbname, RMP_index) ==0)
						{
								//rdb_get_str(pName, );	// read it to clear up the notification

								RMP_update(pSession);

						}
						else
						{
							if(sessionID ==0)
							{
								goto l_check_changedif;
							}
							NTCSLOG_DEBUG("rdb %s is not monitored", pName);
						}//if DOT1AG_Changed is detected
					}
					else//if(pSession )
					{
l_check_changedif:
						/// rdb name without session ID
						// for attached AVCs, but need update or delete
						if(strcmp(pName, MDA_Changed_IF) ==0)
						{
							char buf[MAX_IFNAME_LEN+1];
							err = rdb_get_str(pName, buf, MAX_IFNAME_LEN);
							if(err == 0 && buf[0]){
								/// check each session
								for(sessionID =0; sessionID < pConfig->m_max_session; sessionID++){
									/*
										if bridge interface is tracked, reload configuration
									*/
									pSession = g_session[sessionID];
									if(pSession && pSession->m_mda_state.PeerMode && strcmp(pSession->m_if_name, buf)==0){
										pSession->m_mda_state.MDA_config_changed = 1;
										pSession->m_removing_if = 1;
										break;
									}
								}//for each session
							}//	if(avc_index >0 )
						}
						else if(strcmp(pName, MDA_Add_IF) == 0) // for new AVC, check unattached Session
						{
							char buf[MAX_IFNAME_LEN+1];
							err = rdb_get_str(pName, buf, MAX_IFNAME_LEN);
							if(err == 0 && buf[0]){
								/// check each inactive session
								for(sessionID = 0; sessionID < pConfig->m_max_session; sessionID++){
									pSession = g_session[sessionID];
									if(pSession && pSession->m_mda_state.PeerMode && !pSession->m_mda_state.PeerMode_ready){
										pSession->m_mda_state.MDA_config_changed = 1;
										pSession->m_removing_if = 0;
										NTCSLOG_DEBUG("Reload config");
									}
								}//for each session
							}//	if(avc_index >0 )
						}
#if _RDB_RPC
						else  if(strcmp(rdbname,DOT1AG_CMD_COMMAND) ==0)
						{
							int request_id;
							int objectid;
							int rmpid;
							err	= rpc_get_request(&request_id,  &objectid, &rmpid);
							if(err	> 0)
							{
								if(pSession->m_mda_state.PeerMode_ready)
								{

									int bSucces =1;

									switch(request_id)
									{
									case REQUEST_ID_ADD_RMP:// (objectid)
										// add and enable a RMP obj
										err = RMP_add(pSession, objectid);
										if(err < 0 )
										{
											rpc_set_response_msg("Error: Invalid object id.");
											bSucces =0;
										}
										else
										{
											rpc_set_response_ok(err);
										}
										break;
									case REQUEST_ID_DEL_RMP:// (objectid)
										// disable and del a RMP obj
										err = RMP_del(pSession, objectid);
										if(err < 0 )
										{
											rpc_set_response_msg("Error: Invalid object id.");
											bSucces =0;
										}
										else
										{
											rpc_set_response_msg("ok");
										}
										break;
									case REQUEST_ID_SET_RMP: // (objectid, rmpid)
										// disable and del a RMP obj
										err = RMP_set(pSession, objectid, rmpid, 1);
										if(err < 0 )
										{
											rpc_set_response_msg("Error: failed to enable RMP");
											bSucces =0;
										}
										else
										{
											rpc_set_response_msg("ok");
										}
										break;
									case REQUEST_ID_GET_RMP:			// (rmpid)=>objectid
										// disable and del a RMP obj
										err = RMP_find(pSession,  rmpid);
										if(err < 0 )
										{
											rpc_set_response_msg("Error: RMP not found");
											bSucces =0;
										}
										else
										{
											rpc_set_response_ok(err);
										}
										break;

									case REQUEST_ID_LIST_RMP:
										RMP_rpc_list(pSession);
										break;
									case REQUEST_ID_UPDATE_RMP:
										RMP_collect(pSession);
										break;


									default:
									   NTCSLOG_ERR("Unknown request id=0x%x", request_id);
									   rpc_set_response_msg("Error: Unknown request");
									   break;

									}//switch(funcNo)
									// set result to rdb
								   // NTCSLOG_DEBUG("request_id =%d, result=%d", request_id, bSucces);
								   // set_my_result(bSucces);
								}//if(pSession->m_mda_state.PeerMode_ready)
								else
								{
									rpc_set_response_msg("Error: 1ag is not configurated.");
								}

							}//if(err>0) // check request
						}
#endif
						else
						{

							NTCLOG_DEBUG("rdb %s is not monitored", pName);
						}

					}//if(pSession )

					if(pEnd ==NULL) break;
					pName = pEnd;

				}//while(*pName)
			}//if (err ==0)
		}
		else// no fd active, selected == 0, timeout 4s
		{

        }

        // process event for each session
        current_time_ms = gettimeofdayMS();
        for (sessionID =0; sessionID < pConfig->m_max_session; sessionID ++)
        {
			pSession = g_session[sessionID];
			if (pSession == NULL)  continue;

			if(pSession->m_mda_state.MDA_config_changed)
			{
				MDA_sync(pSession);
				pSession->m_mda_state.MDA_config_changed =0;

				pSession->m_mda_state.MDA_extra_config_changed =1;

				NOTIFY_STATE_CHANGE("MDA changed");
			}

			if(pSession->m_mda_state.MDA_extra_config_changed)
			{
				install_extra_mda_config(pSession);
				pSession->m_mda_state.MDA_extra_config_changed =0;

			}

			if( pSession->m_lbm_state.Event.m_send_request )
			{


				pSession->m_lbm_state.Event.m_completed =0;
				pSession->m_lbm_state.Event.m_send_start_time = current_time_ms;
				err= sendlbm_start(pSession);
				if(err)
				{
					pSession->m_lbm_state.status= err;
					sendlbm_end(pSession);

					if(pConfig->m_run_once)goto lab_end;
				}
				pSession->m_lbm_state.Event.m_send_request =0;

			}

			// check event notification
			if(pSession->m_lbm_state.Send_started)
			{
				if(pSession->m_lbm_state.Event.m_completed)
				{
					NTCSLOG_INFO("sendlbm completed");
					sendlbm_end(pSession);
					if(pConfig->m_run_once) break;
				}
			}
			if(pSession->m_ltm_state.Event.m_send_request )
			{
				pSession->m_ltm_state.Event.m_reply_found =0;
				pSession->m_ltm_state.Event.m_completed=0;
				pSession->m_ltm_state.Event.m_send_start_time = current_time_ms;
				err= sendltm_start(pSession);
				if(err)
				{
					//NTCSLOG_INFO("failed to send LTM packet");
					pSession->m_ltm_state.status= err;
					sendltm_end(pSession);

					if(pConfig->m_run_once) goto lab_end;
				}
				pSession->m_ltm_state.Event.m_send_request =0;

			}
			if(pSession->m_ltm_state.Send_started )
			{
				if(pSession->m_ltm_state.Event.m_completed ||
					(pSession->m_ltm_state.Event.m_reply_found>0 &&
					(current_time_ms - pSession->m_ltm_state.Event.m_send_start_time) > pSession->m_ltm_data.timeout) )
				{
					int i=0;
					for (i =0; i< pSession->m_ltm_state.Event.m_reply_found; i++)
					{
						collect_ltr(pSession);
					}
					pSession->m_ltm_state.Event.m_reply_found =0;
					pSession->m_ltm_state.Event.m_completed =0;
					update_rdb_ltr(pSession);
					sendltm_end(pSession);
					NTCSLOG_INFO("sendltm completed");
					if(pConfig->m_run_once) goto lab_end;
				}
			}
			// --LTM , LBM, DMM, LMM, TST all idle, then check  and change state
			if(pSession->m_lbm_state.Send_started ==0 &&
				pSession->m_ltm_state.Send_started ==0 &&
				pSession->m_dmm_state.Send_started ==0 &&
				pSession->m_slm_state.Send_started ==0 &&
				pSession->m_lmm_state.Send_started ==0)
			{
				if(pSession->m_mep_state_changed)
				{
					err = MEP_auto_start(pSession);
					pSession->m_mep_state_changed =0;

					NOTIFY_STATE_CHANGE("mepactive changed");
					// if mep state changed and enabled
					if(err ==0 && pSession->m_lmp_state.MEP_active_ready)
					{
						if(pSession->m_lmp_state.CCMenabled)
						{
							pSession->m_ccm_state_changed =1;
						}
#ifdef NCI_Y1731
						if(pSession->m_mda_state.Y1731_enable_ready)
						{
							if(pSession->m_lmp_state.AISforced || pSession->m_lmp_state.AISauto)
							{
								pSession->m_ais_state_changed=1;
							}




						}
#endif
					}

				}

				if(pSession->m_ccm_state_changed)
				{
					CCM_auto_start(pSession);
					pSession->m_ccm_state_changed =0;

					NOTIFY_STATE_CHANGE("ccmenable changed");
				}
#ifdef NCI_Y1731
				if(pSession->m_mda_state.Y1731_enable_ready)
				{
#if 0
					if(pSession->m_ais_state_changed)
					{
						AIS_auto_start(pSession);
						pSession->m_ais_state_changed =0;
						NOTIFY_STATE_CHANGE("aisforced changed");
					}


#endif



				}//if(pSession->m_mda_state.Y1731_enable_ready)
#endif
			}// --LTM , LBM, DMM, LMM, TST all idle, then check  and change state

#ifdef NCI_Y1731


#if 0
			if(pSession->m_lmm_state.Event.m_send_request)
			{
				pSession->m_lmm_state.Event.m_completed =0;
				pSession->m_lmm_state.Event.m_has_error=0;
				err = LMM_send(pSession);
				if(err)
				{
					pSession->m_lmp_state.status= err;
					LMM_send_end(pSession);
				}
				pSession->m_lmm_state.Event.m_send_request =0;
				pSession->m_lmm_state.Event.m_send_start_time = current_time_ms;
				NOTIFY_STATE_CHANGE("LMM send");
			}
#endif
			if(pSession->m_dmm_state.Event.m_send_request)
			{
				pSession->m_dmm_state.Event.m_completed =0;
				pSession->m_dmm_state.Event.m_has_error=0;

				err = DMM_send(pSession);
				if(err)
				{
					pSession->m_lmp_state.status= err;
					DMM_send_end(pSession);
				}
				pSession->m_dmm_state.Event.m_send_request =0;
				pSession->m_dmm_state.Event.m_send_start_time = current_time_ms;
				NOTIFY_STATE_CHANGE("DMM send");
			}

			if(pSession->m_slm_state.Event.m_send_request)
			{
				pSession->m_slm_state.Event.m_completed =0;
				pSession->m_slm_state.Event.m_has_error=0;

				err = SLM_send(pSession);
				if(err)
				{
					pSession->m_lmp_state.status= err;
					SLM_send_end(pSession);
				}
				pSession->m_slm_state.Event.m_send_request =0;
				pSession->m_slm_state.Event.m_send_start_time = current_time_ms;
				NOTIFY_STATE_CHANGE("SLM send");
			}

#if 0
			// check LMM, DMM and event notification
			if(pSession->m_lmm_state.Send_started)
			{
				if(pSession->m_lmm_state.Event.m_completed ||
					(current_time_ms - pSession->m_lmm_state.Event.m_send_start_time ) > pSession->m_lmm_data.timeout)
				{
					if(pSession->m_lmm_state.Event.m_completed)
					{
						NTCSLOG_INFO("sendlmm completed");
					}
					else
					{
						NTCSLOG_INFO("sendlmm timeout");
					}
					LMM_send_end(pSession);
				}
			}
#endif
			if(pSession->m_dmm_state.Send_started)
			{
				if(pSession->m_dmm_state.Event.m_completed ||
				(current_time_ms - pSession->m_dmm_state.Event.m_send_start_time ) > pSession->m_dmm_data.timeout)
				{
					if(pSession->m_dmm_state.Event.m_completed)
					{
						NTCSLOG_INFO("senddmm completed");
					}
					else
					{
						NTCSLOG_INFO("senddmm timeout");
					}

					DMM_send_end(pSession);
				}
			}

			if(pSession->m_slm_state.Send_started)
			{
				if(pSession->m_slm_state.Event.m_completed ||
				(current_time_ms - pSession->m_slm_state.Event.m_send_start_time ) > pSession->m_slm_data.timeout)
				{
					if(pSession->m_slm_state.Event.m_completed)
					{
						NTCSLOG_INFO("sendslm completed");
					}
					else
					{
						NTCSLOG_INFO("sendslm timeout");
					}

					SLM_send_end(pSession);
				}
			}


#endif
		} //for (sessionID =0; sessionID < pConfig->m_max_session; sessionID ++)

	}//while (pConfig->m_running)

lab_end:

	for(sessionID =0; sessionID < pConfig->m_max_session; sessionID++)
	{
		Session *pSession = g_session[sessionID];
		if(pSession == NULL) continue;

		shutdown_1ag(pSession);
		ethlctrl_delcontext(pSession, pSession->m_context);
		ethlctrl_nciunstart(pSession);
		ethlctrl_unregapi(pSession);
	}
	rdb_set_2str_all(pConfig, MDA_Status, "802.1ag is not installed", "");

	ethClose();
	return(0);
}
