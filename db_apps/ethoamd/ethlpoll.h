#pragma once
#include "apiETHLn.h"
#include "session.h"

//	ETHLPOLL_DRVHANDLE = 0x3200,
				/* Return a driver's API handles */
				/* param1 = (char *) driver identifier string
				 * param2 = (void **) ptr for drvCTRL handle
				 * param3 = (void **) ptr for drvPOLL handle
				 */
static inline int ethlpoll_drvhandle(Session *pSession, char *param1, void **param2,void **param3)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_DRVHANDLE, param1, param2, param3);
}

//	ETHLPOLL_CONTEXT,	/* Return a Line's Context */
				/* param1 = (void **) ptr for Context
				 */
static inline int ethlpoll_context(Session *pSession, void**param1)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_CONTEXT, param1);
}

//	ETHLPOLL_CONFIG,	/* Return current Line Config settings */
				/* param1 = (void *) ptr for ETH_CONFIG
			 	 */
static inline int ethlpoll_config(Session *pSession, ETH_CONFIG **param1)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_CONFIG, param1);
}

//	ETHLPOLL_DEVICE,
			/* User-defined poll directly to the driver */
				/* param1 = (void * or ulong) user-defined
				 * param2 = (void * or ulong) user-defined
				 * param3 = (void * or ulong) user-defined
				 * param4 = (void * or ulong) user-defined
				 */


static inline int ethlpoll_device(Session *pSession, void *param1, void *param2, void *param3, void *param4)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_DEVICE, param1, param2, param3, param4);
}

		/***** ETH1AG Management *****/

//	ETHLPOLL_1AGVAL,
			/* Poll a tracked value */
				/* param1 = (int) MDA/LMP/RMP identifier
				 *		use 0 for MDA-identifier
				 * param2 = (int) one of ETH1AG_VALUE
				 * param3 = (void *) ptr for returned data
				 */
static inline int ethlpoll_1agval(Session *pSession, int param1, ETH1AG_VALUE param2, void *param3)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_1AGVAL, param1, param2, param3);
}

//	ETHLPOLL_1AGTLV,	/* Poll a global TLV setting */
				/* param1 = (int) one of ETH1AG_TLVID
				 * param2 = (void *) ptr for returned TLV data
				 */
static inline int ethlpoll_1agtlv(Session *pSession, ETH1AG_TLVID param1, void *param2)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_1AGTLV, param1, param2);
}

//ETHLPOLL_1AGLTM,	/* Poll a Link Trace Reply Record */
				/* param1 = (int) local MP identifier
				 * param2 = (ulong) transaction ID
				 * param3 = (int) one of ETH1AG_LTMREPLYID
				 * param4 = (ulong) ETH1AG_LTMREPLYID parameter
				 * param5 = (void *) ptr for ETH1AG_LTMREPLY
				 */

static inline int ethlpoll_1agltm(Session *pSession, int param1, ulong param2, ETH1AG_LTMREPLYID param3, ETH1AG_LTMREPLYID param4, ETH1AG_LTMREPLY* param5)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_1AGLTM, param1, param2, param3, param4, param5);
}


		/***** ETH3AH Management *****/

//	ETHLPOLL_3AHVAL
		/* Poll a tracked value */
				/* param1 = (int) required but ignored in 3AH
				 * param2 = (int) one of ETH3AH_VALUE
				 * param3 = (void *) ptr for returned data
				 */
static inline int ethlpoll_3ahval(Session *pSession, int param1, int param2,void *param3)
{
	return _ethLinPOLL(pSession->m_node, ETHLPOLL_3AHVAL, param1, param2, param3);
}


#define GET_ETH_PARAM(s, i, c, v, m, e) bok = ethlpoll_1agval(s, i, c, (void*)v); \
								if(!bok) { err = -1;\
								build_err_msg(e, m, i, ""#c);\
								goto lab_err;	}

