#ifndef __REARSWITCH_H__
#define __REARSWITCH_H__

typedef struct {
	int iPin;
} rearswitch;

int rearswitch_readPin(rearswitch* pR);
void rearswitch_destroy(rearswitch* pR);
int rearswitch_setDir(rearswitch* pR, int fOut);
rearswitch* rearswitch_create(int iPin);
rearswitch* rearswitch_create_readpin(int iPin);

#endif
