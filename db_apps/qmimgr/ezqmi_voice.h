#ifndef __QMIVOICE_H__
#define __QMIVOICE_H__

int init_qmivoice(void);
void fini_qmivoice(void);

int qmivoice_process_wr_msg_event();

int qmivoice_on_rdb_ctrl(const char* rdb, const char* val);

void qmimgr_callback_on_voice(unsigned char msg_type,struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti);


#endif
