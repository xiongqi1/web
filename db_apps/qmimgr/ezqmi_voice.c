#include "g.h"

#include "ezqmi_voice.h"
#include "qmirdbctrl.h"
#include "strarg.h"
#include "indexed_rdb.h"

/*

	* QMI voice call flow

	initiate and finalize flow

		main() ---+---> qmimgr_init() -------> init_qmivoice()
                          +
                          +---> qmimgr_fini() -------> fini_qmivoice()

	QMI flow

		qmimgr_callback_on_common() : main callback from QMI layer - runs when a QMI message arrives
		+
		+----> qmimgr_callback_on_voice() : main voice callback - runs when a VOICE QMI message arrives
		       +
		       +----> qmivoice_process_wr_msg_event()


	RDB flow

		qmimgr_on_db_command() : main RDB callback - runs when a RDB command arrives
		+
		+----> DB_COMMAND_VAR_VOICE_CTRL ---> qmivoice_on_rdb_ctrl() : read a MSG from CTRL RDB to Read queueu
		+                                     +
		+                                     +----> qmivoice_process_rd_msg_event()
		+                                            +
		+                                            +----> qmivoice_on_msg_ctrl() : runs when a CTRL MSG arrives
		+                                                   +
		+                                                   +----> _qmi_voice_on_cmd_suppl_forward()
		+                                                   +
		+                                                   +----> qmivoice_process_wr_msg_event()
		+
		+----> DB_COMMAND_VAR_VOICE_NOTI ---> qmivoice_process_wr_msg_event() : runs when a VOICE NOTI MSG gets reset - write a MSG from Write queue to NOTI


	Function description

		qmivoice_process_wr_msg_event() : write a MSG from Write queue to NOTI RDB - flush write queue
		qmivoice_process_rd_msg_event() : read a MSG from Read queue and call qmivoice_on_msg_ctrl() - process read queue

*/

extern struct rdb_session* _s;

static struct rwpipe_t* _pp = NULL;
static struct indexed_rdb_t* _ir = NULL;

static struct resourcetree_element_t res_qmi_voice_strings[]={
     	/* QMI_VOICE - QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_INFO*/
	#define __M QMI_VOICE_ALL_CALL_STATUS_IND

		#define __S QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_INFO
			#define __I 0
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_ORIGINATION),"ORIGINATION"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_INCOMING),"INCOMING"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_CONVERSATION),"CONVERSATION"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_CC_IN_PROGRESS),"IN_PROGRESS"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_ALERTING),"ALERTING"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_HOLD),"HOLD"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_WAITING),"WAITING"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_DISCONNECTING),"DISCONNECTING"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_END),"END"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_STATE_SETUP),"SETUP"},
			#undef __I

			#define __I 1
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_VOICE),"VOICE"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_VOICE_IP),"VOICE_IP"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_VT),"VT"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_VIDEOSHARE),"VIDEOSHARE"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_TEST),"TEST"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_OTAPA),"OTAPA"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_STD_OTASP),"STD_OTASP"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_NON_STD_OTASP),"NON_STD_OTASP"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_EMERGENCY),"EMERGENCY"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_SUPS),"SUPS"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_EMERGENCY_IP),"EMERGENCY_IP"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_TYPE_EMERGENCY_VT),"EMERGENCY_VT"},
			#undef __I

			#define __I 2
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_DIRECTION_MO),"MO"},
			{RES_STR_KEY_SUB(__M,__S,__I,CALL_DIRECTION_MT),"MT"},
			#undef __I
		#undef __S

		#define __S QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_REMOTE_PARTY_NUMBER
			#define __I 0
				 {RES_STR_KEY_SUB(__M,__S,__I,PRESENTATION_ALLOWED),"ALLOWED"},
				 {RES_STR_KEY_SUB(__M,__S,__I,PRESENTATION_RESTRICTED),"RESTRICTED"},
				 {RES_STR_KEY_SUB(__M,__S,__I,PRESENTATION_NUM_UNAVAILABLE),"NUM_UNAVAILABLE"},
				 {RES_STR_KEY_SUB(__M,__S,__I,PRESENTATION_PAYPHONE),"PAYPHONE"},
			#undef __I
		#undef __S

	#undef __M


};

static struct resourcetree_t* _res = NULL;


/* local macro functions */

#define __STR(vc) indexed_rdb_get_cmd_str(_ir,(vc))

static int _qmi_voice_get_callforwarding(int reason,int* service_status,int* service_class,int* no_reply_timer, char* number,int number_len)
{
	struct qmi_easy_req_t er;
	int rc;

	const struct qmitlv_t* tlv;

	/* initiate results */
	if(service_status)
		*service_status=-1;
	if(service_class)
		*service_class=-1;
	if(number)
		*number=0;
	if(no_reply_timer)
		*no_reply_timer=-1;


	/* init req */
	rc=_qmi_easy_req_init(&er,QMIVOICE,QMI_VOICE_GET_CALL_FORWARDING);
	if(rc<0)
		goto err;

	/* build request */
	unsigned char reason_v;
	reason_v=(unsigned char)reason;

	/* build tvls */
	qmimsg_add_tlv(er.msg,QMI_VOICE_GET_CALL_FORWARDING_REQ_TYPE_CALL_FORWARDING_REASON,sizeof(reason_v),&reason_v);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_info* cfinfo;
	struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_info_entry_t* e;

	/* return tlv - call id */
	tlv=_get_tlv(er.rmsg,QMI_VOICE_GET_CALL_FORWARDING_RESP_TYPE_GET_CALL_FORWARDING_INFO,sizeof(*cfinfo));
	if(!tlv)
		goto err;

	cfinfo=(struct qmi_voice_get_call_forwarding_resp_get_call_forwarding_info*)tlv->v;
	e=cfinfo->e;

	/* support a single result - no multiple inqury type is allowed */
	if(!cfinfo->num_instances)
		goto err;

	SYSLOG(LOG_OPERATION,"###voice### num_instances = %d",cfinfo->num_instances);
	SYSLOG(LOG_OPERATION,"###voice### service_status=%d",e->service_status);
	SYSLOG(LOG_OPERATION,"###voice### service_class=%d",e->service_class);
	SYSLOG(LOG_OPERATION,"###voice### number_len=%d",e->number_len);

	/* return results */
	if(service_status)
		*service_status=e->service_status;
	if(service_class)
		*service_class=e->service_class;
	if(number)
		__strncpy(number,e->number,__min(number_len,e->number_len+1));
	if(no_reply_timer)
		*no_reply_timer = e->no_reply_timer;

	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

static int _qmi_voice_set_callforwarding(int active, int reason, int num_type, int num_plan,int no_reply_timer, const char* number)
{
	struct qmi_easy_req_t er;
	int rc;

#if 0
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] msg_type=0x02 tran_id=0x001e, msg_id=0x0033, tlv_count=2
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] TLV 0 type=0x02,len=4
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] 01 00 5C 00                                     ..\.
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] TLV 1 type=0x10,len=2
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] 75 00                                           u.
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] got QMI_MSGTYPE_RESP
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [generictree_find] no element found - key=2814754062073948(0x000a00010000005c)
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [_get_tlv] TLV type(0xe0) not found - msg_id=0x0033, tlv_count=2
Jan  1 15:33:35 ntc_140attdev user.debug qmimgr[7669]: [qmimgr_callback_on_preprocess] ###qmimsg### tran_id=30,msg_id=0x0033,qmi_result=0x0001,qmi_error=unknown(0x005c),qmi_ext_error(0x0000)
#endif

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIVOICE,QMI_VOICE_SET_SUPS_SERVICE);
	if(rc<0)
		goto err;

	/* build tvls - QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_SERV_INFO */
	struct qmi_voice_set_sups_service_req_serv_info serv_info;
	serv_info.voice_serivce=active;
	serv_info.reason=reason;
	qmimsg_add_tlv(er.msg,QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_SERV_INFO,sizeof(serv_info),&serv_info);

	/* build tvls - QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NUMBER */
	if(number) {
		char* forwarding_number;
		int forwarding_number_len;

		SYSLOG(LOG_OPERATION,"###voice### add number (no=%s)",number);

		forwarding_number=strdupa(number);
		forwarding_number_len=strlen(forwarding_number);
		qmimsg_add_tlv(er.msg,QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NUMBER,forwarding_number_len,forwarding_number);
	}

	/* build tvls - QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NO_REPLY_TIMER */
	if(no_reply_timer>=0) {
		unsigned char no_reply_timer_v;

		SYSLOG(LOG_OPERATION,"###voice### add reply timer (timer=%d)",no_reply_timer);

		no_reply_timer_v=(unsigned char)no_reply_timer;
		qmimsg_add_tlv(er.msg,QMI_VOICE_SET_SUPS_SERVICE_REQ_TYPE_CALL_FORWARDING_NO_REPLY_TIMER,sizeof(no_reply_timer_v),&no_reply_timer_v);
	}

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* return tlv - call id */
	_qmi_easy_req_fini(&er);

	return 0;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

static int _qmi_voice_answer(int call_id)
{
	struct qmi_easy_req_t er;
	int rc;

	const struct qmitlv_t* tlv;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIVOICE,QMI_VOICE_ANSWER_CALL);
	if(rc<0)
		goto err;

	/* build request */
	unsigned char call_id_v;
	call_id_v = (unsigned char)call_id;

	/* build tvls */
	qmimsg_add_tlv(er.msg,QMI_VOICE_ANSWER_CALL_REQ_TYPE_CALL_ID,sizeof(call_id_v),&call_id_v);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* return tlv - call id */
	tlv=_get_tlv(er.rmsg,QMI_VOICE_ANSWER_CALL_RESP_TYPE_CALL_ID,sizeof(call_id_v));
	if(!tlv)
		goto err;

	call_id_v=*(unsigned char*)tlv->v;

	_qmi_easy_req_fini(&er);

	return call_id_v;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

static int _qmi_voice_hangup(int call_id)
{
	struct qmi_easy_req_t er;
	int rc;

	const struct qmitlv_t* tlv;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIVOICE,QMI_VOICE_END_CALL);
	if(rc<0)
		goto err;

	/* build request */
	unsigned char call_id_v;
	call_id_v = (unsigned char)call_id;

	/* build tvls */
	qmimsg_add_tlv(er.msg,QMI_VOICE_END_CALL_REQ_TYPE_CALL_ID,sizeof(call_id_v),&call_id_v);

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,QMIMGR_GENERIC_RESP_TIMEOUT);
	if(rc<0)
		goto err;

	/* return tlv - call id */
	tlv=_get_tlv(er.rmsg,QMI_VOICE_END_CALL_RESP_TYPE,sizeof(call_id_v));
	if(!tlv)
		goto err;

	call_id_v=*(unsigned char*)tlv->v;

	_qmi_easy_req_fini(&er);

	return call_id_v;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

static int _qmi_voice_dial(const char* dial,int timeout)
{
	struct qmi_easy_req_t er;
	int rc;

	struct qmi_voice_dial_call_req_calling_number* req_cn;
	int req_cn_len;
	int dial_len;

	const struct qmitlv_t* tlv;

	/* init req */
	rc=_qmi_easy_req_init(&er,QMIVOICE,QMI_VOICE_DIAL_CALL);
	if(rc<0)
		goto err;

	/* build request of calling number */
	dial_len=strlen(dial);
	req_cn_len=sizeof(*req_cn)+dial_len;
	req_cn=alloca(req_cn_len);

	/* build tvls */
	memcpy(req_cn->calling_number,dial,dial_len);
	qmimsg_add_tlv(er.msg,QMI_VOICE_DIAL_CALL_REQ_TYPE_CALLING_NUMBER,req_cn_len,req_cn);

	/* use default timeout */
	if(!timeout)
		timeout=QMIMGR_MIDLONG_RESP_TIMEOUT;

	/* init. tlv */
	rc=_qmi_easy_req_do(&er,0,0,NULL,timeout);
	if(rc<0)
		goto err;

	/* return tlv - call id */
	unsigned char call_id;
	tlv=_get_tlv(er.rmsg,QMI_VOICE_DIAL_CALL_REQ_TYPE_CALL_ID,sizeof(call_id));
	if(!tlv)
		goto err;

	call_id=*(unsigned char*)tlv->v;

	_qmi_easy_req_fini(&er);

	return call_id;

err:
	_qmi_easy_req_fini(&er);
	return -1;
}

int _qmi_voice_on_cmd_suppl_forward(int idx, struct strarg_t* a)
{
	int service;
	int reason;
	const char* dest_no = NULL;
	int stat = -1;

	int argi;

	if(a->argc<3) {
		SYSLOG(LOG_NOTICE,"###voice### missing parameter found for SUPPL FORWARD (argc=%d)",a->argc);
		goto err;
	}

	argi=1;

	/* read forward type */
	if(!strcmp(a->argv[argi],"UNCONDITIONAL")) {
		reason = VOICE_REASON_FWD_UNCONDITIONAL;
	}
	else if(!strcmp(a->argv[argi],"NOANSWER")) {
		reason = VOICE_REASON_FWD_NOREPLY;
	}
	else if(!strcmp(a->argv[argi],"BUSY")) {
		reason = VOICE_REASON_FWD_MOBILEBUSY;
	}
	else {
		SYSLOG(LOG_ERR,"###voice### unknown FORWARD type parameter (param=%s)",a->argv[argi]);
		goto err;
	}
	argi++;

	/* read action */
	if(!strcmp(a->argv[argi],"STATUS")) {
		int service_status;
		int service_class;
		int no_reply_timer;
		char number[RDB_MAX_VAL_LEN];
		const char* status_str;

		stat = _qmi_voice_get_callforwarding(reason,&service_status,&service_class,&no_reply_timer,number,sizeof(number));

		if(service_status==SERVICE_STATUS_ACTIVE)
			status_str="ACTIVE";
		else if(service_status==SERVICE_STATUS_INACTIVE)
			status_str="INACTIVE";
		else
			status_str="ERR";

		/* reply result to rdb */
		if(!*number)
			rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %s %s %s", __STR(vc_suppl_forward),a->argv[1],a->argv[2],status_str);
		else
			rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %s %s %s %s %d", __STR(vc_suppl_forward),a->argv[1],a->argv[2],status_str,number,no_reply_timer);
	}
	else {
		int no_reply_timer=-1;

		if(!strcmp(a->argv[argi],"REGISTER")) {
			service = VOICE_SERVICE_REGISTER;
			argi++;

			/* get phone number */
			dest_no = a->argv[argi];
			if(!dest_no) {
				SYSLOG(LOG_ERR,"###voice### no destination number found for REGISTER");
				goto err;
			}
		}
		else if(!strcmp(a->argv[argi],"ACTIVATE")) {
			service = VOICE_SERVICE_ACTIVATE;
		}
		else if(!strcmp(a->argv[argi],"DEACTIVATE")) {
			service = VOICE_SERVICE_DEACTIVATE;
		}
		else if(!strcmp(a->argv[argi],"ERASE")) {
			service = VOICE_SERVICE_ERASE;
		}
		else {
			SYSLOG(LOG_ERR,"###voice### unknown FORWARD action parameter (param=%s)",a->argv[argi]);
			goto err;
		}
		argi++;

		/* get timeout */
		if(a->argv[argi]) {
			no_reply_timer=atoi(a->argv[argi]);
			argi++;
		}


		stat = _qmi_voice_set_callforwarding(
			service,
			reason,
			QMI_VOICE_NUM_TYPE_NATIONAL,
			QMI_VOICE_NUM_PLAN_NATIONAL,
			no_reply_timer,
   			dest_no);

		/* reply result to rdb */
		rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %s %s %s", __STR(vc_suppl_forward),a->argv[1],a->argv[2],stat<0?"ERR":"OK");
	}

	return stat;
err:
	return -1;
}

int qmivoice_on_msg_ctrl(int idx, struct strarg_t* a)
{
	int stat=-1;

	switch(idx) {
		case vc_suppl_forward: {
			stat=_qmi_voice_on_cmd_suppl_forward(idx,a);
			break;
		}

		case vc_call: {
			int call_id;
			const char* dest;
			int timeout;

			if(a->argc<2) {
				SYSLOG(LOG_OPERATION,"###voice### [CALL] argc incorrect (argc=%d)",a->argc);
				break;
			}

			dest=a->argv[1];
			timeout=atoi(a->argv[2]);

			SYSLOG(LOG_OPERATION,"###voice### [CALL] qmi dial '%s' %d",dest,timeout);
			call_id=_qmi_voice_dial(dest,timeout);

			/* reply */
			SYSLOG(LOG_OPERATION,"###voice### [CALL] got result from qmi dial (call_id=%d)",call_id);
			if(call_id<0)
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %s", __STR(vc_call),"ERR");
			else
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %d %s", __STR(vc_call),call_id,"OK");
			break;
		}

		case vc_answer: {
			int call_id_to_answer;
			int call_id;

			if(a->argc<2) {
				SYSLOG(LOG_OPERATION,"###voice### [ANSWER] argc incorrect (argc=%d)",a->argc);
				break;
			}

			call_id_to_answer=atoi(a->argv[1]);

			SYSLOG(LOG_OPERATION,"###voice### [ANSWER] qmi answer %d",call_id_to_answer);
			call_id=_qmi_voice_answer(call_id_to_answer);

			/* reply */
			SYSLOG(LOG_OPERATION,"###voice### [ANSWER] got result from qmi answer (call_id=%d)",call_id);
			if(call_id<0)
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %d %s", __STR(vc_answer),call_id_to_answer,"ERR");
			else
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %d %s", __STR(vc_answer),call_id,"OK");
			break;

			break;
		}

		case vc_hangup: {
			int call_id_to_hangup;
			int call_id;

			if(a->argc<2) {
				SYSLOG(LOG_OPERATION,"###voice### [HANGUP] argc incorrect (argc=%d)",a->argc);
				break;
			}

			call_id_to_hangup=atoi(a->argv[1]);

			SYSLOG(LOG_OPERATION,"###voice### [HANGUP] qmi hangup %d",call_id_to_hangup);
			call_id=_qmi_voice_hangup(call_id_to_hangup);

			/* reply */
			SYSLOG(LOG_OPERATION,"###voice### [HANGUP] got result from qmi hangup (call_id=%d)",call_id);
			if(call_id<0)
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %d %s", __STR(vc_hangup),call_id_to_hangup,"ERR");
			else
				rwpipe_post_write_printf(_pp, NULL, 0, 1, "%s %d %s", __STR(vc_hangup),call_id,"OK");
			break;
		}
		#if 0
		case vc_call_notify: {
			SYSLOG(LOG_OPERATION,"###voice### got vc_call_notify");
			break;
		}
		#endif
	}

	qmivoice_process_wr_msg_event();

	return stat;
}

int qmivoice_process_rd_msg_event()
{
	char msg[RDB_MAX_VAL_LEN];
	struct strarg_t* a;

	int cmd_idx;

	if(!(rwpipe_pop_rd_msg(_pp,NULL,msg,sizeof(msg),0)<0)) {

		a=strarg_create(msg);

		cmd_idx=indexed_rdb_get_cmd_idx(_ir,a->argv[0]);
		if(cmd_idx<0) {
			syslog(LOG_ERR,"unknown command recieved (msg='%s')",msg);
		}
		else {
			syslog(LOG_DEBUG, "cmd (%s#%d) received (msg=%s)", a->argv[0], cmd_idx, msg);
			qmivoice_on_msg_ctrl(cmd_idx, a);
		}

		strarg_destroy(a);
	}

	return 0;
}

int qmivoice_process_wr_msg_event()
{
	const char* noti;
	char msg_to_write[RDB_MAX_VAL_LEN];
	const char* rdb="voice.command." QMIRDBCTRL_NOTI;

	noti = _get_str_db(rdb,"");

	/* bypass if rdb is not blank */
	if(*noti)
		goto fini;

	rwpipe_clear_wq_signal(_pp);

	/* bypass if no data is taken */
	if(rwpipe_pop_wr_msg(_pp,msg_to_write,sizeof(msg_to_write))<0)
		goto fini;

	_set_str_db(rdb,msg_to_write,-1);

fini:
	return 0;
}

int qmivoice_on_rdb_ctrl(const char* rdb, const char* val)
{
	int stat = -1;

	/* bypass if blank */
	if(!*val)
		goto err;

	/* feed val to pp */
	stat=rwpipe_feed_read(_pp,val);
	if(stat<0) {
		syslog(LOG_ERR,"on_rdb_noti() failed in on_rdb_ctrl() - %s",strerror(errno));
		goto err;
	}

	/* reset rdb */
	_set_reset_db(rdb);

	qmivoice_process_rd_msg_event();

	return stat;

err:
	return -1;
}

void qmimgr_callback_on_voice(unsigned char msg_type,struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
	const struct qmitlv_t* tlv;
	int i;

	switch(msg->msg_id) {
		case QMI_VOICE_ALL_CALL_STATUS_IND: {

			/*
				* MO
					# call
					Wed Jul 15 15:32:40 EST 2015: RING 1 MO VOICE IN_PROGRESS
					Wed Jul 15 15:32:42 EST 2015: CALL -1
					Wed Jul 15 15:32:43 EST 2015: RING 1 MO VOICE ORIGINATION

					# conversation
					Wed Jul 15 15:32:44 EST 2015: RING 1 MO VOICE ALERTING
					Wed Jul 15 15:32:58 EST 2015: RING 1 MO VOICE CONVERSATION

					# disconnect
					Wed Jul 15 15:33:22 EST 2015: RING 1 MO VOICE DISCONNECTING
					Wed Jul 15 15:33:22 EST 2015: RING 1 MO VOICE END

				* MT
					# call
					Wed Jul 15 15:35:25 EST 2015: RING 1 MT VOICE SETUP
					Wed Jul 15 15:35:26 EST 2015: RING 1 MT VOICE INCOMING
					# hangup
					Wed Jul 15 15:35:47 EST 2015: RING 1 MT VOICE DISCONNECTING
					Wed Jul 15 15:35:47 EST 2015: RING 1 MT VOICE END
			*/

			struct qmi_voice_all_call_status_type_call_info* call_info;

			struct qmi_voice_all_call_status_ind_remote_party_number* remote_no;

			SYSLOG(LOG_OPERATION,"###voice### got QMI_VOICE_ALL_CALL_STATUS_IND");

			/* addon info - remote number */
			char* addon_remote_number[256]={0,};

			/* parse remote numbers */
			tlv=_get_tlv(msg,QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_REMOTE_PARTY_NUMBER,sizeof(*remote_no));
			if(tlv) {
				struct qmi_voice_all_call_status_ind_remote_party_number_entry_t* e;
				int e_len;

				char line[RDB_MAX_VAL_LEN];
				char no[RDB_MAX_VAL_LEN];
				const char* pi;

				remote_no=(struct qmi_voice_all_call_status_ind_remote_party_number*)tlv->v;

				e=remote_no->e;
				for(i=0;i<remote_no->num_instance;i++) {

					/* get info */
					__strncpy(no,e->number,__min(sizeof(no),e->number_len+1));
					pi=resourcetree_lookup(_res,RES_STR_KEY_SUB(msg->msg_id,tlv->t,0,e->number_pi));

					if(pi) {
						snprintf(line,sizeof(line),"%s %s",pi,no);
						addon_remote_number[e->call_id]=strdupa(line);
					}

					/* increase e */
					e_len=sizeof(*e)+e->number_len;
					e=(struct qmi_voice_all_call_status_ind_remote_party_number_entry_t*)((char*)e+e_len);
				}
			}


			/* get call information */
			tlv=_get_tlv(msg,QMI_VOICE_ALL_CALL_STATUS_IND_TYPE_CALL_INFO,sizeof(*call_info));
			if(tlv) {
				struct qmi_voice_all_call_status_type_call_info_entry_t* e;
				const char* call_stat;
				const char* call_type;
				const char* call_dir;

				call_info=(struct qmi_voice_all_call_status_type_call_info*)tlv->v;

				for(i=0;i<call_info->num_instance;i++) {
					e=&call_info->e[i];

					call_stat=resourcetree_lookup(_res,RES_STR_KEY_SUB(msg->msg_id,tlv->t,0,e->call_state));
					call_type=resourcetree_lookup(_res,RES_STR_KEY_SUB(msg->msg_id,tlv->t,1,e->call_type));
					call_dir=resourcetree_lookup(_res,RES_STR_KEY_SUB(msg->msg_id,tlv->t,2,e->direction));

					/* check call status */
					if(!call_stat) {
						SYSLOG(LOG_ERROR,"###voice### unknown call status found (call_state=%d)",e->call_state);
						continue;
					}

					/* check call status */
					if(!call_type) {
						SYSLOG(LOG_ERROR,"###voice### unknown call type found (call_type=%d)",e->call_type);
						continue;
					}

					/* check call status */
					if(!call_dir) {
						SYSLOG(LOG_ERROR,"###voice### unknown call direction found (direction=%d)",e->direction);
						continue;
					}

					/* post ring */
					if(addon_remote_number[e->call_id])
						rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s %s %s %s", __STR(vc_call_notify),e->call_id,call_dir,call_type,call_stat,addon_remote_number[e->call_id]);
					else
						rwpipe_post_write_printf(_pp, NULL, 0, 0, "%s %d %s %s %s", __STR(vc_call_notify),e->call_id,call_dir,call_type,call_stat);
				}
			}

			break;
		}


		default:
			SYSLOG(LOG_ERROR,"###voice### unknown voice msg (msg_id=0x%04x)",msg->msg_id);
			break;
	}

	qmivoice_process_wr_msg_event();
}

void fini_qmivoice(void)
{
	SYSLOG(LOG_OPERATION,"###voice### destroying _pp");
	rwpipe_destroy(_pp);

	// destroy string resources
	SYSLOG(LOG_OPERATION,"###voice### destroying resources");
	resourcetree_destroy(_res);

	indexed_rdb_destroy(_ir);

}

int init_qmivoice(void)
{
	// create string resources
	SYSLOG(LOG_OPERATION,"###voice### creating res_qmi_voice_strings resource");
	_res=resourcetree_create(res_qmi_voice_strings,__countof(res_qmi_voice_strings));
	if(!_res) {
		SYSLOG(LOG_ERROR,"failed to create string resource - res_qmi_strings");
		goto err;
	}

	/* create index rdb */
	_ir = indexed_rdb_create(rcchp_cmds,rcchp_cmds_count);
	if(!_ir) {
		syslog(LOG_ERR, "cannot create indexed rdb");
		goto err;
	}

	SYSLOG(LOG_OPERATION,"###voice### creating _pp");
	_pp=rwpipe_create(QMIRDBCTRL_TIMEOUT,NULL);
	if(!_pp) {
		syslog(LOG_ERR,"cannot create rwpipe");
		goto err;
	}


	return 0;
err:
	return -1;
}



