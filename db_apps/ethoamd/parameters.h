#ifndef __PARAMETERS_H
#define __PARAMETERS_H

#define ACTIVE_COUNT_UPDATE_PERIOD 4 //second
#define MAC_STRING_LEN 20
#define MAX_IFNAME_LEN	32
#define MAX_AVCID_LEN	15
#define MAX_CHANGEDIF_LEN (MAX_IFNAME_LEN+10)

#define MAX_SESSION	5
#define BUFLEN 1024	// must be >= length of rdb_flag_list

#define MAX_RMP_ID		8192
#define MAX_RMP_OBJ_NUM	32



typedef struct TConfig
{
	int			m_running;
	int			m_force_rdb_read;         // force read rdb
	// from command line parameters
	int			m_verbosity;	// verbostiy level

	int			m_run_once;		// whether it runs once
	int			m_remove_rdb;
	int			m_script_debug;

	char		m_tempbuf[BUFLEN];
	int			m_max_session;

}TConfig;


typedef struct TParameters
{
	int				m_used;
	char			m_if_name[MAX_IFNAME_LEN+1]; // from command line


	char			m_avcid[MAX_AVCID_LEN+1];

	unsigned char	m_destmac[6];

	int				m_sendLTM;
	int				m_LBMsToSend;
	int				m_enableMEP;
	int				m_enableCCM;
	int				m_Y1731_Action;
	int				m_sendLmm;
	int				m_sendDmm;
	int				m_sendSlm;

	int				m_peerMode;

	int				m_lmpid;
	int				m_rmpid[MAX_RMP_OBJ_NUM];
	int				m_rmpid_num;
	int				m_timeout;
	int				m_priority;
	int				m_ttl;
	int				m_direction;
	int				m_port;
	int				m_vid;
	int				m_vidtype;
	int				m_ltmflag;
	int				m_if_cos;

	int				m_mda_mdlevel;

	const char*	m_tlv_data_file;
	int				load_parameter;
	int				set_avcid;
	int				set_vid;
	int				set_ifname;

} TParameters;

#endif
