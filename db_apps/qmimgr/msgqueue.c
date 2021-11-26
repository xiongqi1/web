#include <stdio.h>

#include "msgqueue.h"

#include "minilib.h"

static int msgqueue_get_next_tail(struct msgqueue_t* q)
{
	int new_t;

	new_t=(q->t+1)%MSGQUEUE_MAX_MSG;
	if(new_t==q->h)
		return -1;

	return new_t;
}

int msgqueue_is_empty(struct msgqueue_t* q)
{
	return q->h==q->t;
}

int msgqueue_is_full(struct msgqueue_t* q)
{
	return msgqueue_get_next_tail(q)>=0;
}

int msgqueue_remove(struct msgqueue_t* q)
{
	if(msgqueue_is_empty(q)) {
		SYSLOG(LOG_ERROR,"queue is empty - nothing to remove");
		goto err;
	}

	q->h=(q->h+1)%MSGQUEUE_MAX_MSG;
	return 0;

err:
	return -1;
}

struct qmimsg_t* msgqueue_peek(struct msgqueue_t* q,unsigned char* msg_type,unsigned short* tran_id)
{
	if(msgqueue_is_empty(q))
		return NULL;

	if(msg_type)
		*msg_type=q->msg_types[q->h];

	if(tran_id)
		*tran_id=q->tran_ids[q->h];

	return q->msgs[q->h];
}

int msgqueue_add(struct msgqueue_t* q, const struct qmimsg_t* msg, unsigned char msg_type,unsigned short tran_id)
{
	int new_t;
	struct qmimsg_t* dst_msg;

	// get next tail
	new_t=msgqueue_get_next_tail(q);
	if(new_t<0) {
		SYSLOG(LOG_ERROR,"queue is full - no space for the new msg");
		goto err;
	}

	// copy
	dst_msg=q->msgs[q->t];
	qmimsg_copy_from(dst_msg,msg);

	// copy msg type
	q->msg_types[q->t]=msg_type;
	// copy trans_id
	q->tran_ids[q->t]=tran_id;

	// increase q
	q->t=new_t;
	return 0;

err:
	return -1;
}

void msgqueue_destroy(struct msgqueue_t* q)
{
	int i;

	if(!q)
		return;

	// delete tlvs
	for(i=0;i<MSGQUEUE_MAX_MSG;i++)
		qmimsg_destroy(q->msgs[i]);

	_free(q);
}

struct msgqueue_t* msgqueue_create()
{
	struct msgqueue_t* q;
	int i;

	// create the object
	q=(struct msgqueue_t*)_malloc(sizeof(struct msgqueue_t));
	if(!q) {
		SYSLOG(LOG_ERROR,"failed to allocate msgqueue_t - size=%d",sizeof(struct msgqueue_t));
		goto err;
	}

	// create msgs
	for(i=0;i<MSGQUEUE_MAX_MSG;i++) {
		q->msgs[i]=qmimsg_create();
		if(!q->msgs[i])
			goto err;
	}

	return q;

err:
	msgqueue_destroy(q);

	return NULL;

}
