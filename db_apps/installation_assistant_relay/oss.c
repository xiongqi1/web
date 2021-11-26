/**
 * @file oss.c
 * @brief Implements wrapper functions to interface operation system
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
#include "oss.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

typedef struct oss_linux_sync {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} oss_linux_sync_t;

struct oss_sync {
	oss_linux_sync_t impl;
};

struct oss_lock {
	pthread_mutex_t impl;
};

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static void get_realtime(unsigned int timeout, struct timespec *ts);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

oss_lock_t *oss_lock_create(void)
{
	oss_lock_t *lock;

	lock = malloc(sizeof(oss_lock_t));
	if (lock) {
		memset(lock, 0, sizeof(oss_lock_t));
		pthread_mutex_init(&lock->impl, NULL);
	}
	return lock;
}

void oss_lock_destroy(oss_lock_t *lock)
{
	pthread_mutex_destroy(&lock->impl);
	free(lock);
}

int oss_lock(oss_lock_t *lock, unsigned int timeout)
{
	if (timeout) {
		struct timespec ts;

		get_realtime(timeout, &ts);
		if (pthread_mutex_timedlock(&lock->impl, &ts)) {
			return OSS_TIMEOUT;
		}
	} else {
		if (pthread_mutex_lock(&lock->impl)) {
			return -1;
		}
	}
	return 0;
}

int oss_unlock(oss_lock_t *lock)
{
	if (pthread_mutex_unlock(&lock->impl)) {
		return -1;
	}
	return 0;
}

oss_sync_t *oss_sync_create(void)
{
	oss_sync_t *sync;

	sync = malloc(sizeof(oss_sync_t));
	if (sync) {
		memset(sync, 0, sizeof(oss_sync_t));
		pthread_mutex_init(&sync->impl.mutex, NULL);
		pthread_cond_init(&sync->impl.cond, NULL);
	}
	return sync;
}

void oss_sync_destroy(oss_sync_t *sync)
{
	pthread_mutex_destroy(&sync->impl.mutex);
	pthread_cond_destroy(&sync->impl.cond);
	free(sync);
}

int oss_sync_wait(oss_sync_t *sync, unsigned int timeout)
{
	int result;

	result = 0;
	if (pthread_mutex_lock(&sync->impl.mutex)) {
		return -1;
	}

	if (timeout) {
		struct timespec ts;

		get_realtime(timeout, &ts);
		if (pthread_cond_timedwait(&sync->impl.cond, &sync->impl.mutex, &ts)) {
			result = OSS_TIMEOUT;
		}
	} else {
		if (pthread_cond_wait(&sync->impl.cond, &sync->impl.mutex)) {
			result = -1;
		}
	}

	if (pthread_mutex_unlock(&sync->impl.mutex)) {
		result = -1;
	}
	return result;
}

int oss_sync_signal(oss_sync_t *sync)
{
	int result;

	result = 0;
	if (pthread_mutex_lock(&sync->impl.mutex)) {
		return -1;
	}
	if (pthread_cond_signal(&sync->impl.cond)) {
		result = -1;
	}
	if (pthread_mutex_unlock(&sync->impl.mutex)) {
		result = -1;
	}
	return result;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

static void get_realtime(unsigned int timeout, struct timespec *ts)
{
	unsigned int nano_sec;

	clock_gettime(CLOCK_REALTIME, ts);
	nano_sec = ts->tv_nsec + ((timeout % 1000) * 1000000);
	ts->tv_sec += (timeout / 1000);
	if (nano_sec > 1000000000) {
		ts->tv_sec += 1;
	}
	ts->tv_nsec = nano_sec % 1000000000;
}

