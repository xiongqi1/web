#ifndef __TICK64_H__
#define __TICK64_H__

void tick64_init(void);
unsigned long long tick64_get_ms(void);

int timerfd_init(unsigned int period);
void timerfd_readTimer(int fd);

#endif
