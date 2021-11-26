
#include "tick64.h"
#include "commonIncludes.h"

#include <sys/timerfd.h>

/* tick - high 32 bit */
static unsigned long high;
/* tick - low 32 bit */
static unsigned long low;
/* tick - if low 32 bit is valid or not */
static int low_valid;

static clock_t ticks_per_sec;

void tick64_init()
{
	/* init. locals */
	low=0;
	high=0;
	low_valid=0;

	/* get ticks per second */
	ticks_per_sec=sysconf(_SC_CLK_TCK);
}

static unsigned long tick64_get_32()
{
	struct tms buf;
	return (unsigned long)times(&buf);
}

static unsigned long long tick64_get_64()
{
	/* get now */
	unsigned long now=tick64_get_32();
	/* calc. carriers */
	if(low_valid && now<low) {
		high++;
	}
	return (((long long)high)<<32) | now;
}

unsigned long long tick64_get_ms()
{
	/* mask the top 11 bits for the conversion */
	unsigned long long tick64=tick64_get_64() & ~0xffe0000000000000ULL;
	return tick64*1000/ticks_per_sec;
}

// This is called with the period in milliseconds
int timerfd_init(unsigned int period)
{
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd>0) {
		/* Make the timer periodic */
		uint sec = period / 1000;
		uint ms = period % 1000;
		uint ns = ms * 1000000;
		struct itimerspec itval;
		itval.it_interval.tv_sec = sec;
		itval.it_interval.tv_nsec = ns;
		itval.it_value.tv_sec = sec;
		itval.it_value.tv_nsec = ns;
		if ( timerfd_settime(fd, 0, &itval, NULL) == -1 )
			return -1;
	}
	return fd;
}

void timerfd_readTimer(int fd)
{
	unsigned long long missed;
	int ret = read(fd, &missed, sizeof(missed));
	if (ret == -1) {
		return;
	}
}

