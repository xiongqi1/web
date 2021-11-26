#ifndef __EZQMI_SMS_H__
#define __EZQMI_SMS_H__

enum sms_msg_type_t {
	smt_unknown,
	smt_cdma,
 	smt_umts,
	smt_max,
};

enum sms_code_type_t {
	dcs_7bit_coding=0,
	dcs_8bit_coding=1,
	dcs_ucs2_coding=2,

 	dcs_7bit_ascii_coding=3, /* cdma code type */
	dcs_unknown=-1,
};

int _qmi_sms_init(enum sms_msg_type_t sms_type,const char* input_coding_scheme);
int _qmi_sms_send(enum sms_msg_type_t sms_type,const char* smsc,const char* dest,enum sms_code_type_t code_to,const char* text,int text_len);
int _qmi_sms_get_smsc(int* address_type,char* address,int address_len);
int _qmi_sms_set_smsc(int address_type,const char* address);
void qmimgr_on_db_command_sms(const char* db_var, int db_var_idx,const char* db_cmd, int db_cmd_idx);
int _qmi_sms_set_event(int en);
int _qmi_sms_register_indications(char* tlinfo,char* nwreg,char* callstat,char* sready,char* bcevent);
int _qmi_sms_get_service_ready_status(int* ind,int* rstat);
int _qmi_sms_update_sms_type(int* rstat);

int _qmi_sms_delete(enum sms_msg_type_t sms_type, unsigned char* storage_type,unsigned char* tag_type,unsigned int* msg_idx);
int _qmi_sms_get_list_messages(enum sms_msg_type_t sms_type, unsigned char storage_type,unsigned char tag_type,unsigned* rmsg_idx,unsigned char* rtag_type,int* cnt);
int _qmi_sms_get_message_proto(unsigned char* msg_mode);

int _qmi_sms_change_tag(enum sms_msg_type_t sms_type,unsigned char storage_type,unsigned int storage_index,unsigned char tag_type);
int _qmi_sms_init_nv_route();

int _qmi_sms_cook_last_in_pool();
int _qmi_sms_eat_last_in_pool();

int _qmi_sms_readall();
int _qmi_sms_init_sms_type(enum sms_msg_type_t sms_type);
void _qmi_sms_update_sms_type_based_on_firmware();

void qmimgr_schedule_sms_readall();
void qmimgr_callback_on_schedule_sms_readall(struct funcschedule_element_t* element);
void qmimgr_update_sms_cfg();
void qmimgr_callback_on_schedule_sms_qmi(struct funcschedule_element_t* element);
void qmimgr_callback_on_schedule_sms_spool(struct funcschedule_element_t* element);

#endif
