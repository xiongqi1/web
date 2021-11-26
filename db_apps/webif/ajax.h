#ifndef AJAX_H
#define AJAX_H


void indicateUnobtainable(void);
char* modelToString(int model);
int processCDMAMsgRes(u_char respType, u_char forStatusPage);
void reqCDMAInfo(u_char forStatusPage);
int status_refresh(void);


#endif
