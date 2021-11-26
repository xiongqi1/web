#ifndef __MODULEMONITOR_H__
#define __MODULEMONITOR_H__

#include "base.h"

#define MODULEMONITOR_HEARTBEAT				10
#define MODULEMONITOR_HEARTBEAT_INIT	20

BOOL modulemonitor_isBeating(void);
void modulemonitor_beatIt(void);
void modulemonitor_resetTick(void);


#endif
