/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "cdcs_syslog.h"

#include "./queue.h"

static const char* const_incremented(const struct at_queue_t* q, const char* p)
{
	return p == q->end - 1 ? q->storage : p + 1;
}

static char* incremented(struct at_queue_t* q, char* p)
{
	return p == q->end - 1 ? q->storage : p + 1;
}

int at_queue_empty(const struct at_queue_t* q)
{
	return q->head == q->last;
}

int at_queue_full(const struct at_queue_t* q)
{
	return const_incremented(q, q->tail) == q->head;
}

void at_queue_clear(struct at_queue_t* q)
{
	q->head = q->storage;
	q->tail = q->head;
	q->last = q->head;
	q->end = q->storage + sizeof(q->storage);
}

struct at_queue_t* at_queue_create(void)
{
	struct at_queue_t* q = malloc(sizeof(struct at_queue_t));
	if (!q)
	{
		return NULL;
	}
	at_queue_clear(q);
	q->end = q->storage + sizeof(q->storage);
	return q;
}

void at_queue_free(struct at_queue_t* q)
{
	free(q);
}

static int at_write_byte(struct at_queue_t* q, char c)
{
	if (at_queue_full(q))
	{
		SYSLOG_ERR("failed: queue full");
		return -1;
	}
	switch (c)
	{
        /* '\n' should not be removed here for some 3G module(i.e Ericsson) using
         * line feed as a line separator instead carridge return.
         */
        case '\n':
		case '\r':
			if (q->last == q->tail)
			{
				break;
			}
			*q->tail = 0;
			q->tail = incremented(q, q->tail);
			q->last = q->tail;
			break;
		default:
			*q->tail = c;
			q->tail = incremented(q, q->tail);
			break;
	}
	return 0;
}

int at_queue_write(struct at_queue_t* q, const char* buf, size_t size)
{
	while (size--)
	{
		if (at_write_byte(q, *buf++) < 0)
		{
			return -1;
		}
	}
	return 0;
}

const char* at_queue_readline(struct at_queue_t* q)
{
	if (at_queue_empty(q))
	{
		return NULL;
	}
	char* p = q->response;
	while (*q->head != 0)
	{
		*p++ = *q->head;
		q->head = incremented(q, q->head);
	}
	*p++ = *q->head;
	q->head = incremented(q, q->head);
	return q->response;
}

void at_queue_debug_print(struct at_queue_t* q)
{
	const char* p;
	SYSLOG_DEBUG("-------- AT command response queue --------");
	SYSLOG_DEBUG("head=%d, tail=%d, last=%d", (q->head - q->storage), (q->tail - q->storage), (q->last - q->storage));
	*q->tail = 0;
	for (p = q->head; p != q->tail; p += (strlen(p) + 1))
	{
		SYSLOG_DEBUG("%s", p);
	}
	SYSLOG_DEBUG("-------- AT command response queue end --------");
}
