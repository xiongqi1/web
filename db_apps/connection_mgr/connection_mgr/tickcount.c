#include <unistd.h>
#include <sys/times.h>

#include "tickcount.h"

static clock_t _clkTicksPerSec;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static clock_t __getTicksPerSecond(void)
{
	return sysconf(_SC_CLK_TCK);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static clock_t __getTickCount(void)
{
	struct tms tm;

	return times(&tm);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void initTickCount(void)
{
	_clkTicksPerSec = __getTicksPerSecond();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void finiTickCount(void)
{
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
tick getTickCountMS(void)
{
	return (tick)__getTickCount()*1000 / _clkTicksPerSec;
}

