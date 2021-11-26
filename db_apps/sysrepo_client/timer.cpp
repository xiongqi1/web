/*
 * Timer support
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "timer.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/timerfd.h>

/* tick - high 32 bit */
static unsigned long high;
/* tick - low 32 bit */
static unsigned long low;
/* tick - if low 32 bit is valid or not */
static int low_valid;
static clock_t ticks_per_sec;

static inline unsigned long tick64_get_32()
{
	struct tms buf;
	return (unsigned long) times(&buf);
}

static inline MsTick tick64_get_64()
{
	/* get now */
	unsigned long now = tick64_get_32();

	/* calc. carriers */
	if (low_valid && now < low) {
		high++;
	}

	return (((MsTick) high) << 32) | now;
}

MsTick getNowMs()
{
	if (ticks_per_sec==0) {
		/* init. locals */
		low = 0;
		high = 0;
		low_valid = 0;

		/* get ticks per second */
		ticks_per_sec = sysconf(_SC_CLK_TCK);
	}

	/* mask the top 11 bits for the conversion */
	MsTick tick64 = tick64_get_64() & ~0xffe0000000000000ULL;
	return tick64 * 1000 / ticks_per_sec;
}

// This is called with the period in milliseconds and the applications FileSelector object
TimerFd::TimerFd(unsigned int period, FileSelector &fileSelector) :
		timer_fd(-1),
		wakeups_missed(0)
{
	int fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (fd == -1)
		return;
	timer_fd = fd;
	/* Make the timer periodic */
	uint sec = period / 1000;
	uint ms = period % 1000;
	uint ns = ms * 1000000;
	struct itimerspec itval;
	itval.it_interval.tv_sec = sec;
	itval.it_interval.tv_nsec = ns;
	itval.it_value.tv_sec = sec;
	itval.it_value.tv_nsec = ns;
	if ( timerfd_settime(timer_fd, 0, &itval, NULL) == -1 )
		return;
	fileSelector.addMonitor(timer_fd, onDataAvailable, this);
}

void TimerFd::readTimer()
{
	unsigned long long missed;

	/* Wait for the next timer event. If we have missed any the number is written to "missed" */
	int ret = read(timer_fd, &missed, sizeof(missed));
	if (ret == -1) {
//			perror("read timer");
		return;
	}

	/* "missed" should always be >= 1, but just to be sure, check it is not 0 anyway */
	if (missed > 0)
		wakeups_missed += (missed - 1);
}

void TimerFd::onDataAvailable(void * _param, void *pFm)
{
	TimerFd *tmr = (TimerFd *) _param;
	tmr->readTimer();
	tmr->onTimeOut();
}
