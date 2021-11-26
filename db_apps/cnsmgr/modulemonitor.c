#include "modulemonitor.h"

static clock_t _tickTouch=-1;

static enum
{
	heartbeatstatus_invalid,
	heartbeatstatus_startup,
	heartbeatstatus_beating,
} _heartBeatStat=heartbeatstatus_invalid;

///////////////////////////////////////////////////////////////////////////////////////////////////
void modulemonitor_resetTick(void)
{
	_heartBeatStat=heartbeatstatus_invalid;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
BOOL modulemonitor_isBeating(void)
{
	switch(_heartBeatStat)
	{
		case heartbeatstatus_invalid:
		{
			_tickTouch=__getTickCount();
			_heartBeatStat=heartbeatstatus_startup;
			return TRUE;
		}

		case heartbeatstatus_startup:
		case heartbeatstatus_beating:
		{
			clock_t tickCur=__getTickCount();
			clock_t msGap=(tickCur-_tickTouch)/__getTicksPerSecond();
			clock_t msPeriod;

			msPeriod=(_heartBeatStat==heartbeatstatus_startup)?MODULEMONITOR_HEARTBEAT_INIT:MODULEMONITOR_HEARTBEAT;

			return msGap<msPeriod;
		}
	}
	
	return FALSE;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void modulemonitor_beatIt(void)
{
	_heartBeatStat=heartbeatstatus_beating;

	_tickTouch=__getTickCount();
}
