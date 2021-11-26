#pragma once
#include "tmsAPI.h"
#include "nciETH.h"
#include "nci1AG.h"
#include "parameters.h"


// lmpid and rmpid valid value 1 ~ 8191.
#define VALID_LRMPID(x) (x >0 && x <8192)


#define ZEROCHK_MACADR(macAdr)						\
	((macAdr[0] == macAdr[1]) && (macAdr[2] == macAdr[3]) &&	\
	 (macAdr[4] == macAdr[5]) && (macAdr[0] == macAdr[2]) &&	\
	 (macAdr[0] == macAdr[4]))


typedef unsigned long ulong;

typedef struct _lbm_data
{
	unsigned int	rmpid;	///Dot1ag.Mda.Lmp.Lbm.rmpid
	MACADR			destmac;	///Dot1ag.Mda.Lmp.Lbm.destmac
	unsigned int	priority;	///Dot1ag.Mda.Lmp.Lbm.priority
	unsigned int	timeout;	///Dot1ag.Mda.Lmp.Lbm.timeout
	unsigned int	rate;	///Dot1ag.Mda.Lmp.Lbm.rate
	unsigned long	nextLBMtransID;	///Dot1ag.Mda.Lmp.Lbm.nextLBMtransID
	unsigned long 	LBMtransID;
	unsigned int  TLVDataLen;	//Dot1ag.lmp.lbm.tlvdatalen
	int 			lbmRespCount;



}lbm_data;


#define MAX_LTR	256

typedef struct _ltm_data
{


	unsigned int	rmpid;	///Dot1ag.Mda.Lmp.Ltm.rmpid
	MACADR			destmac;	///Dot1ag.Mda.Lmp.Ltm.destmac
	unsigned int	priority;	///Dot1ag.Mda.Lmp.Ltm.priority
	unsigned int	flag;	///Dot1ag.Mda.Lmp.Ltm.flag
	unsigned int	ttl;	///Dot1ag.Mda.Lmp.Ltm.ttl
	unsigned int	timeout;	///Dot1ag.Mda.Lmp.Ltm.timeout
	unsigned int	nextLTMtransID;	///Dot1ag.Mda.Lmp.Ltm.nextLTMtransID
	unsigned int 	LTMtransID;


	//int 		 status;
	//char		 lasterr[64];

}ltm_data;



#define LTR_ID_SIZE	32
typedef struct _ltr_data
{
	ETH1AG_LTMREPLY ltmreply[MAX_LTR];
	int			 ltr_num;
}ltr_data;

#ifdef NCI_Y1731
typedef struct _lmm_data
{
	unsigned int 	runID;	/* LMM transaction ID*/

	unsigned int	rmpid;	///rmpid for LMM

	unsigned int	timeout;	///timeout for LMM
	MACADR			destmac;	///destination MAC address for LMM

	LOSSVALUE 		current;	/* read-only of current LM values */
	LOSSVALUE 		accum;		/* read-only of accum LM values */
	LOSSVALUE 		ratio;		/* read-only of ratio LM values */


}lmm_data;


typedef struct _dmm_data
{

	unsigned int runID;	/* DMM transaction ID*/
	unsigned int	rmpid;	///rmpid for DMM

	unsigned int	timeout;	///timeout for DMM
	MACADR			destmac;	///destination MAC address for DMM


	unsigned int Rate; 	/*DMM or 1DM sending rate, in ms*/
	unsigned int Type; 	/* DMM type*/
	unsigned int TLVDataLen;	//Y1731.lmp.dmm.tlvdatalen

	OAMTSTAMP Dly; 	/*last Delay value*/
	OAMTSTAMP DlyAvg; /*the average Delay value*/
	OAMTSTAMP DlyMin; /*the minimium Delay value*/
	OAMTSTAMP DlyMax; /*the maximium Delay value*/
	OAMTSTAMP Var; 	/*last Variation value*/
	OAMTSTAMP VarAvg; /*the average Variation value*/
	OAMTSTAMP VarMin; /*the minimium Variation value*/
	OAMTSTAMP VarMax; /*the maximium Variation value*/
	unsigned int Count; 	/*number of Delay samples*/


}dmm_data;

#define MAX_MULTI_SLM_SESSION 20

#define SLM_STATE_NONE 	0
#define SLM_STATE_SENT 	1
#define SLM_STATE_RECV 	2
#define SLM_STATE_ERR 	-1

typedef struct _slm_data
{

	unsigned int 	runID;	/* SLM transaction ID*/
	unsigned int	rmpid;	///rmpid for SLM

	unsigned int	timeout;	///timeout for SLM
	MACADR			destmac;	///destination MAC address for SLM
	unsigned int 	Rate; 	/*SLM sending rate, in ms*/
	unsigned int  TLVDataLen;	//Y1731.lmp.slm.tlvdatalen

	unsigned int 	testID[MAX_MULTI_SLM_SESSION];
	char			state[MAX_MULTI_SLM_SESSION];
	int 			testID_num;

	unsigned int	Count[MAX_MULTI_SLM_SESSION];	/* number of Rx Responses received */
	LOSSVALUE 		curr[MAX_MULTI_SLM_SESSION];	/* current Loss values */
	LOSSVALUE 		accum[MAX_MULTI_SLM_SESSION];	/* accum Loss values */
	LOSSVALUE 		ratio[MAX_MULTI_SLM_SESSION];	/* ratio Loss values */

}slm_data;


#endif

typedef struct _lmp_data
{


	unsigned int	mpid;	///Dot1ag.Mda.Lmp.mpid
			  int	direction;	///Dot1ag.Mda.Lmp.direction
	unsigned int	port;	///Dot1ag.Mda.Lmp.port
	unsigned int	vid;	///Dot1ag.Mda.Lmp.vid
	unsigned int	vidtype;///Dot1ag.Mda.Lmp.vidtype
	MACADR			macAdr;	///Dot1ag.Mda.Lmp.macAdr

	unsigned int	CoS;	///Dot1ag.Mda.Lmp.CoS


	unsigned int	CCMsent;	///Dot1ag.Mda.Lmp.CCMsent
	unsigned int	xconnCCMdefect;	///Dot1ag.Mda.Lmp.xconnCCMdefect
	unsigned int	errorCCMdefect;	///Dot1ag.Mda.Lmp.errorCCMdefect
	unsigned int	CCMsequenceErrors;	///Dot1ag.Mda.Lmp.CCMsequenceErrors
	unsigned int	LBRsInOrder;	///Dot1ag.Mda.Lmp.LBRsInOrder
	unsigned int	LBRsOutOfOrder;	///Dot1ag.Mda.Lmp.LBRsOutOfOrder
	unsigned int	LBRnoMatch;	///Dot1ag.Mda.Lmp.LBRnoMatch
	unsigned int	LBRsTransmitted;	///Dot1ag.Mda.Lmp.LBRsTransmitted
	unsigned int	LTRsUnexpected;	///Dot1ag.Mda.Lmp.LTRsUnexpected
	unsigned int 	TxCounter;			//readonly
	unsigned int 	RxCounter;			//readonly


#ifdef NCI_Y1731
	unsigned int	YCoS;	///Y1731.Lmp.CoS

	int AIStxOn;		/* read-only status of AIS Tx packets */
	int AISrxOn;		/* read-only status of AIS Rx packets */

	int AISmulticast;	/* rd/wr - ignored if AISauto/AISforced = 0 */
	MACADR AISunicastMAC;	/* rd/wr - ignored if AISauto/AISforced = 0 */

#endif

}lmp_data;


typedef struct _lmp_state
{

	int				MEP_active_ready;
	int				MEPactive;	///Dot1ag.Mda.Lmp.MEPactive

	int				LMP_enable_ready;	/// LMP enabled

	int				CCMenabled;	///Dot1ag.Mda.Lmp.CCMenabled
	int				CCM_enable_ready;
	int				Reload_config;


#ifdef NCI_Y1731


	int AISforced;		/* read/write to force AIS on */
	int AIS_force_ready;  // whether AIS is forced to enable
	int AISauto;		/* read/write enable auto-AIS during faults */
	int AIS_auto_ready;  // whether AIS is enabled to auto sending


#endif
	int 	status;
	char	lasterr[64];

}lmp_state;

#define MDA_ID_TYPE_LEN	48

typedef struct _mda_data
{

	unsigned int	MdLevel;	///Dot1ag.Mda.MdLevel
	unsigned int	PrimaryVID;	///Dot1ag.Mda.PrimaryVID
	unsigned int	MDFormat;	///Dot1ag.Mda.MDFormat
	char 			MdIdType2or4[MDA_ID_TYPE_LEN+1];	///Dot1ag.Mda.MdIdType2or4
	unsigned int	MaFormat;	///Dot1ag.Mda.MaFormat
	unsigned int 	MaLength; 	/// auto configed
	char			MaIdType2[MDA_ID_TYPE_LEN+1];	///Dot1ag.Mda.MaIdType2
	unsigned int	MaIdNonType2;	///Dot1ag.Mda.MaIdNonType2
	unsigned int	CCMInterval;	///Dot1ag.Mda.CCMInterval
	unsigned int	lowestAlarmPri;	///Dot1ag.Mda.lowestAlarmPri
	int				errorCCMdefect;	///Dot1ag.Mda.errorCCMdefect
	int				xconCCMdefect;	///Dot1ag.Mda.xconCCMdefect
	unsigned int	CCMsequenceErrors;	///Dot1ag.Mda.CCMsequenceErrors
	unsigned int	fngAlarmTime;	///Dot1ag.Mda.fngAlarmTime
	unsigned int	fngResetTime;	///Dot1ag.Mda.fngResetTime
	unsigned int	someRMEPCCMdefect;	///Dot1ag.Mda.someRMEPCCMdefect
	unsigned int	someMACstatusDefect;	///Dot1ag.Mda.someMACstatusDefect
	unsigned int	someRDIdefect;	///Dot1ag.Mda.someRDIdefect
	unsigned int	highestDefect;	///Dot1ag.Mda.highestDefect
	unsigned int	fngState;	///Dot1ag.Mda.fngState
	///unsigned int	CoS;	///Dot1ag.Mda.CoS --move to LMP
	///	CoStoEXP;	///Dot1ag.Mda.CoStoEXP
	///	CoStoDSCP;	///Dot1ag.Mda.CoStoDSCP
	///	SmartEdgeAddress;	///Dot1ag.Mda.SmartEdgeAddress
	///unsigned int	MPLStag;	///Dot1ag.Mda.MPLStag
	char			avcid[MAX_AVCID_LEN+1];


#ifdef NCI_Y1731
	char			MegId[MDA_ID_TYPE_LEN+1];	///Y1731.Mda.MegId
	unsigned int	MegLevel;	///Y1731.Mda.MegLevel
	unsigned int	MegIdFormat;	///Y1731.Mda.MegIdFormat
	unsigned int 	MegIdLength; 	///Y1731.Mda.MegIdFormat

	int AISinterval;	/* read/write AIS-pkt Tx interval */
	int almSuppressed;	/* read-only status */

#endif

}mda_data;

typedef struct _mda_state
{
	int 			PeerMode; //////Dot1ag.Mda.PeerMode
	int				PeerMode_ready;
	int				MDA_config_changed;
	int				MDA_extra_config_changed;


	int				Y1731_MDA_enable;	/// Y1731.Mda.Enable
	int				Y1731_enable_ready;/// current Y1731 status

	int 		status;
	char		lasterr[64];

}mda_state;

typedef struct _rmp_data
{
	int				tms_enabled;		//interal 1ag state
	int				rdb_created;		//external rdb state
	unsigned int	rmpid;				//writeonly
	int 			ccmDefect; 			//readonly
	int 			lastccmRDI;			//readonly
	unsigned int 	lastccmPortState;	//readonly
	unsigned int 	lastccmIFStatus;	//readonly
	unsigned int 	lastccmSenderID;	//readonly
	unsigned int 	TxCounter;		//readonly
	unsigned int 	RxCounter;		//readonly
	MACADR			lastccmmacAddr;		//readonly
	int 			status;				//readonly

}rmp_data;

#define EVENT_ERROR_PDU_OK		 0
#define EVENT_ERROR_PDU_OVERFLOW -1
#define EVENT_ERROR_PDU_TXFAILED -2



typedef struct _Event1AG
{
	int m_send_request;
	int m_send_start_time;
	int m_completed;
	int m_has_error;
	int m_reply_found;

}Event1AG;

typedef struct __action_state
{

	unsigned int 	Send;			/* start transmission */
	int 			Send_started;  	/* whether transmission is started */

	Event1AG	Event;

	int 			status;
	//char			lasterr[64];

}action_state;



typedef struct Session
{
	TConfig*		m_config;
	struct Session** m_sessions;

	int				m_node; // tms node
	void			*m_context;

	char			m_if_name[MAX_IFNAME_LEN+1];
	unsigned char	m_if_mac[6];
	unsigned char	m_lmp_mac[6];
	int				m_if_cos;
	int				m_if_bridge; // it is bridge interface
	int				m_removing_if; // interface is removing
	int				m_insert_priority_tag; // whether to insert priority tag, if the cfm comes from UNID
	uint32_t		m_priority_drop[MAX_STACKED_VTAGS];


	mda_data		m_mda_data;
	mda_state		m_mda_state;

	lbm_data		m_lbm_data;
	action_state	m_lbm_state;

	ltm_data		m_ltm_data;
	action_state	m_ltm_state;

	lmp_data		m_lmp_data;
	lmp_state		m_lmp_state;



	ltr_data		m_ltr_data;
	rmp_data		m_rmp_data[MAX_RMP_OBJ_NUM]; // map for all possible RMP
	int				m_rmp_lastid;

#ifdef NCI_Y1731

	lmm_data		m_lmm_data;
	action_state	m_lmm_state;

	dmm_data		m_dmm_data;
	action_state	m_dmm_state;

	slm_data		m_slm_data;
	action_state	m_slm_state;

#endif

	int m_mep_state_changed;
	int m_ccm_state_changed;
	int m_ais_state_changed;

	int m_ais_recv_count;

}Session;

int open_session(Session **pSession, int sessionID, TConfig *pConfig);

void close_all_session(TConfig *pConfig);


int rdb_set_2str_all(TConfig *pConfig, const char* rdbname,
					const char *str1, const char *str2);

/*
	load mda config from rdb
*/
int load_mda(Session *pSession);

/* lookup avc bridge interface name
$ 0 -- operation success
$ -1 -- failed
*/
int get_session_if(Session *pSession);

/*
unlock the avc bridge interface
$0 -- operation success
$-1 -- error
*/
int unlock_avc_if(Session *pSession);

// check whether mda config changed
// >0 -- changed
// 0 -- not changed
int check_mda_config(Session *pSession);



int main_loop(TConfig *pConfig);

/// form error message eg:
/// can not set parameter 'ltm.transid'
void build_err_msg(char *buf, const char *msg, int id, const char *key_name);

extern TConfig		g_config;
extern Session		*g_session[MAX_SESSION];

