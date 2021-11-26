#ifndef __TICKCOUNT_H__
#define __TICKCOUNT_H__


typedef long long tick;

tick getTickCountMS(void);
void initTickCount(void);
void finiTickCount(void);

#endif
