#ifndef __EZQMI_H__
#define __EZQMI_H__

#include "qmidef.h"
#include "qmiservtran.h"
#include "qmimsg.h"
#include "qmiuniclient.h"
#include "funcschedule.h"

#define QMIMGR_RCCH_QUEUE_SIZE	10
#define QMIMGR_RCCH_RDB_NAME	"voice.command"

// qmimgr configurations - qmi request
#define QMIMGR_GENERIC_RESP_TIMEOUT 5
#define QMIMGR_TIMEOUT_10_S 10
#define QMIMGR_MIDLONG_RESP_TIMEOUT 30
#define QMIMGR_CONNECTION_RESP_TIMEOUT 45
#define QMIMGR_LONG_RESP_TIMEOUT 60
// qmimgr configurations - at command
#define QMIMGR_AT_RESP_TIMEOUT 5
#define QMIMGR_AT_SELECT_TIMEOUT 60

#define RES_STR_KEY(id,msg_id,idx)	( ((unsigned long long)(id))<<48 | ((unsigned long long)(msg_id))<<32 | (idx) )
#define RES_STR_KEY_SUB(id,msg_id,sub_msg_id,idx)	RES_STR_KEY(id,msg_id,(sub_msg_id<<24)| (idx))
#define EZQMI_DB_VAL_MAX_LEN	256


#define _qmi_easy_req_init(er,serv_id,msg) _qmi_easy_req_init_ex(er,serv_id,msg,#msg)

struct qmi_easy_req_t {
	struct qmimsg_t* msg;
	struct qmimsg_t* rmsg;
	struct qmimsg_t* imsg;

	int serv_id;
	unsigned short msg_id;
	const char* msg_name;

	unsigned short qmi_result;
	unsigned short qmi_error;
	unsigned short qmi_ext_error;
	unsigned short tran_id;
};


int _qmi_easy_req_init_ex(struct qmi_easy_req_t* er,int serv_id,unsigned short msg_id, const char* msg_name);
void _qmi_easy_req_fini(struct qmi_easy_req_t* er);
int _qmi_easy_req_do(struct qmi_easy_req_t* er,int t,int l,void* v,int to);
const struct qmitlv_t* _get_tlv(struct qmimsg_t* msg,unsigned char t,int min_len);

int _set_str_db_with_no_prefix(const char* dbvar,const char* dbval,int dbval_len);
const char* _get_str_db_ex(const char* dbvar,const char* defval,int prefix);

int qmimgr_init();
void qmimgr_fini(int termbysignal);
int qmimgr_loop();


// qmi local functions
int _request_qmi_tlv_ex(struct qmimsg_t* msg,int serv_id,unsigned short msg_id,unsigned short* tran_id,unsigned char t,unsigned short l,void* v,int clear);
int _request_qmi_tlv(struct qmimsg_t* msg,int serv_id,unsigned short msg_id,unsigned short* tran_id,unsigned char t,unsigned short l,void* v);
int _request_qmi(struct qmimsg_t* msg,int serv_id,unsigned short msg_id,unsigned short* tran_id);
struct qmimsg_t* _wait_qmi_response(int serv_id,int timeout,unsigned short tran_id,unsigned short* result,unsigned short* error,unsigned short* ext_error);
struct qmimsg_t* _wait_qmi_response_ex(int serv_id,int timeout,unsigned short tran_id,unsigned short* result,unsigned short* error,unsigned short* ext_error,int rdb);
struct qmimsg_t* _wait_qmi_indication(int serv_id,int timeout,unsigned short msg_id,unsigned short* result,unsigned short* error,unsigned short* ext_error);
struct qmimsg_t* _wait_qmi_indication_ex(int serv_id, int timeout, unsigned short msg_id, unsigned short* result, unsigned short* error, unsigned short* ext_error, int rdb);
int _wait_qmi_until(int serv_id,int timeout,unsigned short tran_id, unsigned short msg_id);
// at local functions
int _request_at(const char* cmd,int timeout,unsigned short* tran_id);
const char* _get_str_db(const char* dbvar,const char* defval);

int _qmi_easy_req_do_async_ex(struct qmi_easy_req_t* er, int t, int l, void* v, int to_resp, int to_ind, int verbose, int rdb);
int _qmi_easy_req_do_ex(struct qmi_easy_req_t* er,int t,int l,void* v,int to,int verbose,int rdb);

int _system(const char* cmd);
int _wait_at_until(unsigned short tran_id,int timeout);

struct strqueue_t* _wait_at_response(unsigned short tran_id,int timeout);
// misc local functions
void sig_handler(int signum);
void enter_singleton(void);
void release_singleton(void);
int _qmi_gps_determine(int to);
void _batch_cmd_stop_gps_on_pds(void);
int _batch_cmd_start_gps_on_pds(int agps);

/* rdb */
int _set_str_db(const char* dbvar,const char* dbval,int dbval_len);
int _set_reset_db(const char* dbvar);
int _set_fmt_db_ex(const char* dbvar,const char* fmt,...);
int _get_int_db_ex(const char* dbvar,int defval,int prefix);
int _get_int_db(const char* dbvar,int defval);
int _set_fmt_dbvar_ex(int prefix,const char* fmt,const char* dbval,...);
const char* _get_fmt_dbvar_ex(int prefix,const char* fmt,const char* defval,...);
const char* _get_prefix_dbvar(const char* dbvar);

const char* convert_decimal_degree_to_degree_minute(double decimal, int latitude, char* dir, int dir_len);

int _set_str_db_ex(const char* dbvar, const char* dbval, int dbval_len, int prefix);
int _set_str_db(const char* dbvar, const char* dbval, int dbval_len);

#define PROTOTYPE_SET_TYPE_DB_ACCESS_FUNC(type) \
	int _set_##type##_db_ex(const char* dbvar,type dbval,const char* suffix,int prefix); \
	int _set_##type##_db(const char* dbvar,type dbval,const char* suffix);


PROTOTYPE_SET_TYPE_DB_ACCESS_FUNC(int)       /*  _set_int_db_ex() _set_int_db() */
PROTOTYPE_SET_TYPE_DB_ACCESS_FUNC(float)     /*  _set_float_db_ex() _set_float_db() */

extern const char* qmi_port;
extern const char* at_port;
extern int verbosity;
extern int verbosity_cmdline;
extern int instance;
extern const char* last_port;
extern int be_daemon;
extern int sms_disabled;

extern char wwan_prefix[];
extern int wwan_prefix_len;
extern struct funcschedule_t* sched;
extern int current_radio;


// Hash resources - dbcmds
////////////////////////////////////////////////////////////////////////////////

enum {
	/* profile commands */
	DB_COMMAND_VAL_READ=1,
	DB_COMMAND_VAL_WRITE,
	DB_COMMAND_VAL_ACTIVATE,
	DB_COMMAND_VAL_DEACTIVATE,
	DB_COMMAND_VAL_CLEAN,
	DB_COMMAND_VAL_DELETE,
	DB_COMMAND_VAL_GETDEF,
	DB_COMMAND_VAL_SETDEF,

 	/* SIM card commands */
	DB_COMMAND_VAL_ENABLE,
	DB_COMMAND_VAL_DISABLE,
	DB_COMMAND_VAL_CHANGEPIN,
	DB_COMMAND_VAL_VERIFYPIN,
	DB_COMMAND_VAL_CHECK,
	DB_COMMAND_VAL_VERIFYPUK,
	DB_COMMAND_VAL_ENABLEPIN,
	DB_COMMAND_VAL_DISABLEPIN,

 	/* band selection commands */
	DB_COMMAND_VAL_1,
	DB_COMMAND_VAL_5,
	DB_COMMAND_VAL_NEGATIVE_1,

 	/* generic commands */
	DB_COMMAND_VAL_SET,
	DB_COMMAND_VAL_GET,

 	/* GPS commands */
	DB_COMMAND_VAL_AGPS,

 	/* SMS commands */
	DB_COMMAND_VAL_SENDDIAG,
	DB_COMMAND_VAL_SEND,
	DB_COMMAND_VAL_READALL,
	#if 0
	DB_COMMAND_VAL_DELETE, /* defined in profile command */
	#endif
	DB_COMMAND_VAL_DELALL,
	DB_COMMAND_VAL_SETSMSC,

	DB_COMMAND_VAL_UPDATE,
};

enum {
	schedule_key_sms_queue=1,
	schedule_key_sms_generic,
	schedule_key_sms_spool,
};

/* qmi request footprint */
struct qmi_req_footprint_t {
	struct linkedlist list;
	unsigned short tran_id;
	void * data;
	int data_len;
};
#define MAX_REQ_FOOTPRINT_COUNT 16
extern int qmi_req_footprint_add(unsigned short tran_id, const void * data, int data_len);
extern int qmi_req_footprint_remove(unsigned short tran_id, void ** data, int * data_len);
extern int qmi_req_footprint_purge(void);

#if 0
1 DS_PROFILE_REG_RESULT_FAIL General Failure
2 DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL The request contains an invalid profile handle.
3 DS_PROFILE_REG_RESULT_ERR_INVAL_OP An invalid operation was requested.
4 DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE The request contains an invalid technology type.
5 DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM The request contains an invalid profile number.
6 DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT The request contains an invalid profile identifier.
7 DS_PROFILE_REG_RESULT_ERR_INVAL The request contains an invalid argument other than profile number and profile identifier received
8 DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED Profile registry has not been initialized yet
9 DS_PROFILE_REG_RESULT_ERR_LEN_INVALID The request contains a parameter with invalid length.
10 DS_PROFILE_REG_RESULT_LIST_END End of the profile list was reached while searching for the requested profile.
11 DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID The request contains an invalid subscrition identifier.
12 DS_PROFILE_REG_INVAL_PROFILE_FAMILY The request contains an invalid profile family.
1001 DS_PROFILE_REG_3GPP_INVAL_PROFILE_FAMILY The request contains an invalid 3GPP profile family.
1002 DS_PROFILE_REG_3GPP_ACCESS_ERR An error was encountered while accessing the 3GPP profiles.
1003 DS_PROFILE_REG_3GPP_CONTEXT_NOT_DEFINED The given 3GPP profile doesn’t have a valid context.
#endif

#endif
