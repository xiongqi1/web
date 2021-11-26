#pragma once

#include "session.h"
#include "tmsAPI.h"
#include "apiETHLn.h"
#include "nciETH.h"
#include "nci1AG.h"

//	ETHLCTRL_NCISTART = 0x3000,
				/* Pre-O/S - Overall TMS Package init */
				/* No additional parameters
				 */
static inline int ethlctrl_ncistart(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node,  ETHLCTRL_NCISTART);
}

//	ETHLCTRL_NCIUNSTART,	/* Pre-O/S - reestablish pre-Start condition */
				/* No additional parameters
				 */
static inline int ethlctrl_nciunstart(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_NCIUNSTART);
}


//	ETHLCTRL_WARMSTART,	/* Enable/disable Warm Start */
				/* param1 = (int) ENABLE or DISABLE
				 */
static inline int ethlctrl_warmstart(Session *pSession, int param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_WARMSTART, param1);
}

//	ETHLCTRL_REGAPI,	/* Pre-O/S - Set up a Line's APIs */
				/* param1 = (void *) ptr to _ethLinCLBK() API
				 * param2 = (void *) ptr to _ethDrvCTRL() API
				 * param3 = (void *) ptr to _ethDrvPOLL() API
				 */

static inline int ethlctrl_regapi(Session *pSession, void *param1, void *param2, void *param3)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_REGAPI, param1, param2, param3);
}

//	ETHLCTRL_RESET,		/* Binds and configures a Line */
				/* param1 = (void *) hdw device base address
				 * param2 = (int) hdw device subchannel
				 * param3 = (void *) ptr to ETH_CONFIG
				 */
static inline int ethlctrl_reset(Session *pSession, void *param1, int param2, void *param3)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_RESET, param1, param2, param3);
}


///	ETHLCTRL_NEWCONTEXT,	/* Post-O/S - Create an operating context */
				/* param1 = (int) priority
				 * param2 = (int) mboxSize
				 * param3 = (int) stackSize
				 * param4 = (void **) load with Context
				 */
static inline int ethlctrl_newcontext(Session *pSession, int param1, int param2, int param3, void **param4)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_NEWCONTEXT, param1, param2, param3, param4);
}

//	ETHLCTRL_REGCONTEXT,	/* Post-O/S - Set the line under the context */
				/* param1 = (void *) Context
				 */
static inline int ethlctrl_regcontext(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_REGCONTEXT, param1);
}

//	ETHLCTRL_UNREGCONTEXT,	/* Unhook a line from its Context */
				/* No additional parameters
				 */
static inline int ethlctrl_unregcontext(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_UNREGCONTEXT);
}

//	ETHLCTRL_UNRESET,	/* Unbinds and shuts down a Line */
				/* No additional parameters
				 */
static inline int ethlctrl_unreset(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_UNRESET);
}

//	ETHLCTRL_UNREGAPI,	/* Deletes a line from the TMS */
				/* No additional parameters
				 */
static inline int ethlctrl_unregapi(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_UNREGAPI);
}

//	ETHLCTRL_DELCONTEXT,	/* Deletes a Context from the TMS */
				/* param1 = (void *) Context
				 */
static inline int ethlctrl_delcontext(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_DELCONTEXT, param1);
}


		/***** Shortcuts *****/

//ETHLCTRL_RERESET,
/* A Shortcut to quickly reconfig a
				 * pre-existing line.  It must already
				 * be under Context Control, and have no
				 * peripheral processing.
				 */
				/* param1 = (void *) ptr to ETH_CONFIG
				 */

static inline int ethlctrl_rereset(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_RERESET, param1);
}

//ETHLCTRL_REREGAPI,
			/* A Shortcut to quickly set up new APIs
				 * on a pre-existing line.  It must already
				 * be under Context Control, and have no
				 * peripheral processing.
				 */
				/* param1 = (void *) ptr to _ethLinCLBK() API
				 * param2 = (void *) ptr to _ethDrvCTRL() API
				 * param3 = (void *) ptr to _ethDrvPOLL() API
				 * param4 = (void *) ptr to ETH_CONFIG
				 */
static inline int ethlctrl_regregapi(Session *pSession, void* param1, void* param2, void* param3, void *param4)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_REREGAPI, param1, param2, param3, param4);
}

//	ETHLCTRL_DESTROY,
	/* A Shortcut to quickly tear down a
				 * pre-existing line and delete it.  It must
				 * already be under Context Control, and
				 * have no peripheral processing.
				 */
				/* No additional parameters
				 */
static inline int ethlctrl_destroy(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_REGCONTEXT);
}



		/***** General *****/

//	ETHLCTRL_DEVICE,	/* User-defined call directly to the driver */
				/* param1 = (void * or ulong) user-defined
				 * param2 = (void * or ulong) user-defined
				 * param3 = (void * or ulong) user-defined
				 * param4 = (void * or ulong) user-defined
				 */

static inline int ethlctrl_device(Session *pSession, void* param1, void* param2, void* param3, void *param4)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_DEVICE, param1, param2, param3, param4);
}

		/***** ETH1AG Management *****/

//ETHLCTRL_1AGREG,	/* Register for ETH1AG Processing */
				/* param1 = (void *) ptr to ETH1AG_MDACONFIG
				 */
static inline int ethlctrl_1agreg(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGREG, param1);
}

//ETHLCTRL_1AGUNREG,	/* Terminate ETH1AG Processing */
				/* No additional parameters
				 */
static inline int ethlctrl_1agunreg(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGUNREG);
}

//ETHLCTRL_1AGNEWLMP,	/* Add a Local Maint. Point */
				/* param1 = (void *) ptr to ETH1AG_LMPCONFIG
				 */
static inline int ethlctrl_1agnewlmp(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGNEWLMP, param1);
}

//ETHLCTRL_1AGNEWRMP,	/* Add a Remote Maint. Point */
				/* param1 = (void *) ptr to ETH1AG_RMPCONFIG
				 */
static inline int ethlctrl_1agnewrmp(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGNEWRMP, param1);
}

//	ETHLCTRL_1AGDELLMP,	/* Delete a Local Maint. Point */
				/* param1 = (int) Local MP Identifier
				 */
static inline int ethlctrl_1agdellmp(Session *pSession, int param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGDELLMP, param1);
}
//ETHLCTRL_1AGDELRMP,	/* Delete a Remote Maint. Point */
				/* param1 = (int) Remote MP Identifier
				 */
static inline int ethlctrl_1agdelrmp(Session *pSession, int param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGDELRMP, param1);
}

//ETHLCTRL_1AGCMD,	/* Control an operation */
				/* param1 = (int) one of ETH1AG_COMMAND
				 * param2 = (ulong) dependent parameter
				 * param3 = (ulong) dependent parameter
				 * param4 = (ulong) dependent parameter
				 * param5 = (ulong) dependent parameter
				 */



static inline int ethlctrl_1agcmd(Session *pSession, ETH1AG_COMMAND param1, ulong param2, ulong param3, ulong *param4, void* param5)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGCMD, param1, param2, param3, param4, param5);
}
//	ETHLCTRL_1AGVAL,	/* Set a tracked value */
				/* param1 = (int) MDA/LMP/RMP identifier
				 *		use 0 for MDA-identifier
				 * param2 = (int) one of ETH1AG_VALUE
				 * param3 = (ulong) dependent parameter
				 */
static inline int ethlctrl_1agval(Session *pSession, int param1, ETH1AG_VALUE param2, ulong param3)
{
	//printf("ethlctrl_1agval: node=%d, param1=%d, param2=%d, param3=0x%lx\n", pSession->m_node, param1, param2, param3);
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGVAL, param1, param2, param3);
}

//	ETHLCTRL_1AGTLV,	/* Set a global TLV */
				/* param1 = (int) one of ETH1AG_PDUID
				 * param2 = (int) one of ETH1AG_TLVID
				 * param3 = (void *) ptr to a union of
				 *	type ETH1AG_TLV which holds the
				 *	desired data
				 */				 
static inline int ethlctrl_1agtlv(Session *pSession, int param1,int param2, void* param3)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_1AGTLV, param1, param2, param3);
}


		/***** ETH3AH Management *****/

//ETHLCTRL_3AHREG,	/* Register for ETH3AH Processing */
				/* param1 = (void *) ptr to ETH3AH_CONFIG
				 */
static inline int ethlctrl_3ahreg(Session *pSession, void *param1)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_3AHREG, param1);
}

//	ETHLCTRL_3AHUNREG,	/* Terminate ETH3AH Processing */
				/* No additional parameters
				 */
static inline int ethlctrl_3ahunreg(Session *pSession)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_3AHUNREG);
}

//	ETHLCTRL_3AHCMD,	/* Control an operation */
				/* param1 = (int) one of ETH3AH_COMMAND
				 * param2 = (ulong) dependent parameter
				 * param3 = (ulong) dependent parameter
				 * param4 = (ulong) dependent parameter
				 * param5 = (ulong) dependent parameter
				 */
static inline int ethlctrl_3ahcmd(Session *pSession, int param1, ulong param2, ulong param3, ulong param4, ulong param5)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_3AHCMD, param1, param2, param3, param4, param5);
}

//	ETHLCTRL_3AHVAL		/* Set a tracked value */
				/* param1 = (int) required but ignored in 3AH
				 * param2 = (int) one of ETH3AH_VALUE
				 * param3 = (ulong) dependent parameter
				 */

static inline int ethlctrl_3ahval(Session *pSession, int param1, int param2, ulong param3)
{
	return _ethLinCTRL(pSession->m_node, ETHLCTRL_3AHVAL, param1, param2, param3);
}


#define ETH_SET_PARAM(s, i, c, v, m, e) bok = ethlctrl_1agval(s, i, c, v); \
								if(!bok) { err =-1;\
								build_err_msg(e, m, i, ""#c);\
								goto lab_err;	}

#define ETH_1AGCMD(s, c, p1, p2, p3, p4, e) bok = ethlctrl_1agcmd(s, c, p1, p2, p3, p4); \
								if(!bok) { err =-1;\
								strcpy(e, "cannot execute 802.1ag request : "#c);\
								NTCSLOG_INFO("cannot execute 802.1ag request: "#c);\
								goto lab_err;	}

#define ETH_1AGTLV(s, pduid,tlvid, p, e) bok = ethlctrl_1agtlv(s, pduid, tlvid, p); \
								if(!bok) { err =-1;\
								strcpy(e, "cannot set 802.1ag "#tlvid);\
								NTCSLOG_INFO("cannot set 802.1ag : "#tlvid);\
								goto lab_err;	}

#define ETH_1AG_ERROR(tlvid, e) { err =-1;\
								strcpy(e, "cannot set 802.1ag "#tlvid);\
								NTCSLOG_INFO("cannot set 802.1ag : "#tlvid);\
								goto lab_err;	}


