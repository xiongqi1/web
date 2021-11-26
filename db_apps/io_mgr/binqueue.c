/*
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
#include "binqueue.h"
#include "commonIncludes.h"

struct binqueue_t {
	int h;
	int t;

	char* buf;
	int buf_len;
};

int binqueue_get_free_len(struct binqueue_t* queue)
{
	int used;
	
	used=(queue->t+queue->buf_len-queue->h)%queue->buf_len;
	
	return queue->buf_len-used-1;
}
		
void binqueue_clear(struct binqueue_t* queue)
{
	queue->h = queue->t = 0;
}

int binqueue_is_empty(struct binqueue_t* queue)
{
	return queue->h == queue->t;
}

void binqueue_waste(struct binqueue_t* queue, int waste)
{
	queue->h = (queue->h + waste) % queue->buf_len;
}

int binqueue_peek(struct binqueue_t* queue, void* dst, int dst_len)
{
	char* ptr = (char*)dst;
	int h = queue->h;

	while (ptr<(char*)dst+dst_len)	{
		if (h == queue->t)
			break;

		// copy
		*ptr++ = queue->buf[h];

		// inc. hdr
		h = (h+1) % queue->buf_len;
	}

	return (int)(ptr -(char*)dst);
}

int binqueue_write(struct binqueue_t* queue, const void* src, int src_len)
{
	char* ptr = (char*)src;
	int iNewT;

	while (ptr<(char*)src+src_len) {
		// get new T
		iNewT = (queue->t + 1) % queue->buf_len;
		if (iNewT == queue->h)
			break;

		// copy
		queue->buf[queue->t] = *ptr++;

		// inc. tail
		queue->t = iNewT;
	}

	return (int)(ptr -(char*)src);
}

void binqueue_destroy(struct binqueue_t* queue)
{
	if(!queue)
		return;
	
	free(queue->buf);
	free(queue);
}

struct binqueue_t* binqueue_create(int len)
{
	struct binqueue_t* queue;
	
	// create the object
	queue=(struct binqueue_t*)malloc(sizeof(struct binqueue_t));
	if(!queue) {
		DBG(LOG_ERR,"failed to allocate binqueue_t - size=%d",sizeof(struct binqueue_t));
		goto err;
	}
	
	/* clear members */
	memset(queue,0,sizeof(*queue));
	
	len++;
	queue->buf=malloc(len);
	if(!queue->buf) {
		DBG(LOG_ERR,"failed to allocate buf - size=%d",len);
		goto err;
	}
	
	queue->buf_len=len;
	
	return queue;
	
err:
	binqueue_destroy(queue);
	return NULL;	
}

