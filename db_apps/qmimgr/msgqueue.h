#ifndef __MSGQUEUE_H__
#define __MSGQUEUE_H__

#include "qmimsg.h"

#define MSGQUEUE_MAX_MSG	64

struct msgqueue_t {
	struct qmimsg_t* msgs[MSGQUEUE_MAX_MSG];
	unsigned char msg_types[MSGQUEUE_MAX_MSG];
	unsigned short tran_ids[MSGQUEUE_MAX_MSG];

	int h;
	int t;
};

struct msgqueue_t* msgqueue_create();
void msgqueue_destroy(struct msgqueue_t* q);

int msgqueue_add(struct msgqueue_t* q, const struct qmimsg_t* msg, unsigned char msg_type,unsigned short tran_id);
struct qmimsg_t* msgqueue_peek(struct msgqueue_t* q,unsigned char* msg_type,unsigned short* tran_id);

int msgqueue_remove(struct msgqueue_t* q);
int msgqueue_is_full(struct msgqueue_t* q);
int msgqueue_is_empty(struct msgqueue_t* q);


#endif
