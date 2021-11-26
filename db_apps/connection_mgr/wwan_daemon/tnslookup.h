#ifndef __TNSLOOKUP_H__
#define __TNSLOOKUP_H__

void tnslookup_change_dns_server(const char* dns_server);
void tnslookup_fini();
int tnslookup_init(const char* dev); 

int tnslookup_recv();
int tnslookup_isready();
int tnslookup_send(const char* name);

int tnslookup_isinit(void);
#endif
