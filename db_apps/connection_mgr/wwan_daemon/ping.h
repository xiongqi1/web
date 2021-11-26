#ifndef __PING_H__
#define __PING_H__

int ping_recv();
int ping_sendto(const char* szHost,int timeout);
void ping_fini();
int ping_init(const char* szDev);
int ping_isInit();

int ping_isIfUp(const char* szIfNm);

#endif
