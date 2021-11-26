#ifndef __SIGNORMTERM_H__
#define __SIGNORMTERMH__

#include "base.h"
#include <signal.h>


void signormterm_resetTermSig(void);
void signormterm_regTermSig(int iSig);
BOOL signormterm_getTermSig(void);
void signormterm_ignoreSig(int iSig);

#endif
