
/*
 * D? queue routines
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


#include "dq.h"
#include "commonIncludes.h"

struct dq_t {
	int num_tokens;
	int token_len;
	int token_buf_size;
	int token_count;
	int h;
	int t;
	char tokens[1];
};

void dq_clear(struct dq_t* dq)
{
	dq->h=0;
	dq->t=0;
	dq->token_count = 0;
}

#define step_pointer(idx) (idx) += dq->token_len; if ( (idx) >= dq->token_buf_size ) (idx) = 0
#define is_empty()		(0==dq->token_count)

int dq_is_empty(struct dq_t *dq)
{
	return is_empty();
}

int dq_waste(struct dq_t *dq)
{
	/* bypass if empty */
	if(is_empty())
		goto err;

	/* increase t */
	step_pointer(dq->h);
	dq->token_count--;
	return 0;

err:
	return -1;
}

void* dq_peek(struct dq_t *dq, int *count )
{
	*count = dq->token_count;
	/* bypass if empty */
	if(is_empty())
		goto err;

	/* return token at h */
	return &dq->tokens[dq->h];

err:
	return NULL;
}

int dq_get_free_space(struct dq_t *dq)
{
	return dq->num_tokens-dq->token_count;
}

int dq_push(struct dq_t *dq, void* token)
{
	if(0==dq_get_free_space(dq)) {
		DBG(LOG_ERR,"queue overflow detected");
		goto err;
	}

	/* copy token to tail */
	int t=dq->t;
	memcpy( &dq->tokens[t],token,dq->token_len);
	step_pointer(t);

	/* update t */
	dq->t=t;
	dq->token_count++;
	return 0;

err:
	return -1;
}

void dq_destroy(struct dq_t* dq)
{
	if(!dq)
		return;

	/* delete dq */
	free(dq);
}

struct dq_t *dq_create(int q_size,int token_len)
{
	struct dq_t* dq;
	int token_buf_size = q_size*token_len;

	/* allocate for dq object */
	dq=malloc(sizeof(*dq)+token_buf_size-1);
	if(!dq) {
		DBG(LOG_ERR,"failed to allocate dq object");
		goto err;
	}
	memset(dq,0,sizeof(*dq));

	/* setup members */
	dq->num_tokens=q_size;
	dq->token_len=token_len;
	dq->token_buf_size = token_buf_size;
	return dq;

err:
	return NULL;
}
