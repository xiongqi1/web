#ifndef __AUTOAPN_H__
#define __AUTOAPN_H__

int init_scan_apn_callbacks(void (*user_start_wwan)(void),void (*user_stop_wwan)(void),int (*user_is_wwan_up)(void),void (*user_save_to_nvram)(void));

int do_post_check(void);
int do_scan_apn(void);

int init_post_check(int wwan_no);
void init_scan_apn(int profile_no,const char* country);

void notify_conn_term(void);

#endif
