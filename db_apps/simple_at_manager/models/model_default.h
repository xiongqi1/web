#ifndef __MODEL_DEFAULT_H__
#define __MODEL_DEFAULT_H__

#include "../model/model.h"
#include "../at/at.h"

int model_default_set_status(const struct model_status_info_t* chg_status,const struct model_status_info_t* new_status, const struct model_status_info_t* err_status);
int model_default_get_status(const struct model_status_info_t* status_needed, struct model_status_info_t* new_status,struct model_status_info_t* err_status);

int update_sim_status(void);
int update_roaming_status(void);
int update_signal_strength(void);
int update_imsi(void);
int update_network_name(void);
int update_service_type(void);
int update_call_forwarding(void);
int update_configuration_setting(const struct model_status_info_t* new_status);
int update_ccid(void);
int update_call_msisdn(void);
int update_sim_hint(void);
int update_pdp_status(void);
int update_date_and_time(void);

int handleGetOpList(void);
int handleSetOpList(void);


int model_default_write_sim_status(const char* sim_status);
int default_handle_command_sim(const struct name_value_t* args);

void convClearPwd(char* szOffPin);

const char* _getNextToken();
const char* _getFirstToken(const char* szSrc, const char* szDeli);

int wait_for_sim_card(int count);
int updateSIMStat();
int verifyPin(const char* szPin);

char * convert_Arfcn2BandType (int protocol, int number);
void initialize_band_selection_mode(const struct model_status_info_t* new_status);

int _isMCCMNC(const char* str);
int _getMCC(const char* str);
int _getMNC(const char* str);

#define SET_LOG_LEVEL_TO_SIM_LOGMASK_LEVEL     setlogmask(LOG_UPTO(log_db[LOGMASK_SIM].loglevel));
#define SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL      setlogmask(LOG_UPTO(log_db[LOGMASK_AT].loglevel));

#define	SIZE_OF_ADDTBL	64
typedef struct
{
	int	bandClass;
	char	*bandName;
	int	min;		// min value of Downlink ARFCN
	int	max;		// max value of Downlink ARFCN
	int	additional[SIZE_OF_ADDTBL];   // terminated by 0.
} arfcnStructType;

/* run-time model type flag based on AT+CGMM - cdma or umts or 2G */
typedef enum {cinterion_type_umts,cinterion_type_2g,cinterion_type_cdma} cinterion_eum_type;
extern cinterion_eum_type cinterion_type;

extern arfcnStructType afrcnTbl[];
extern arfcnStructType uafrcnTbl[];
extern arfcnStructType eafrcnTbl[];
extern int support_pin_counter_at_cmd;
extern volatile char last_failed_pin[16];

extern int creg_network_stat;
extern int sierra_change_tx_mic_gain(void);

extern int handleUpdateNetworkStat(int sync);
extern BOOL is_potsbridge_disabled(void);
extern int default_handle_command_nwctrl(const struct name_value_t* args);

int updateRoamingStatus_method1(int nwMCC, int nwMNC);
int updateRoamingStatus_method2(int nwMCC, int nwMNC);
int updateRoamingStatus_method3(void);
int update_loglevel_via_logmask(void);
void sync_operation_mode(int service_type);
int convert_network_type(int iMode);

extern void update_debug_level(void);
extern int getPDPStatus(int pid);
extern int wait_on_network_reg_stat(int registered,int sec, int net_stat);


extern const struct command_t model_default_commands[];
extern const struct notification_t model_default_notifications[];

#endif


