#ifndef __NEW_RDB_CCHP__
#define __NEW_RDB_CCHP__

#define _GNU_SOURCE

#define CONFIG_USE_ASTERISK

#ifdef CONFIG_USE_ASTERISK
/* include asterisk headers */
#include "asterisk.h"
#include "asterisk/lock.h"
#else
#endif

#include <stdio.h>
#include <pthread.h>

#include "list.h"

struct rwpipe_t;
struct rwpipe_pq_t;
struct rwpipe_qmsg_t;
struct rwpipe_event_t;
struct rwpipe_sigq_t;

typedef void (*rwpipe_qmsg_complete_func_t)(struct rwpipe_qmsg_t* qmsg, int expired);
typedef void (*rwpipe_qmsg_free_func_t)(struct rwpipe_qmsg_t* qmsg);
typedef int (*rwpipe_pq_filter_func_t)(struct rwpipe_pq_t* pq, struct rwpipe_qmsg_t* qmsg);

/* qmsg_t - queue message */

struct rwpipe_qmsg_t {
	struct list_head list;

	int trans_id;
	int timestamp;

	char* msg;
	int timeout;

	void* ref;

	rwpipe_qmsg_complete_func_t complete;
	rwpipe_qmsg_free_func_t free;
};

typedef int (*rwpipe_pq_func_t)(struct rwpipe_pq_t* pq, struct rwpipe_qmsg_t* qmsg);


/* pq_t - private queue */

struct rwpipe_pq_t {
	struct list_head list;

	int trans_id;
	struct rwpipe_sigq_t* rq;

	rwpipe_pq_func_t pq_filter;
};

typedef int (*rwpipe_background_process_func_t)(struct rwpipe_t* pp);

/* sigq_t - signal-able queue */


struct rwpipe_sigq_t {
	struct list_head qh;

	int use_self_pipe;

	int cond_inited;
#ifdef CONFIG_USE_ASTERISK
	ast_cond_t cond;
#else
	pthread_cond_t cond;
#endif

	int mutex_inited;
#ifdef CONFIG_USE_ASTERISK
	ast_mutex_t mutex;
#else
	pthread_mutex_t mutex;
#endif

	int pipefd[2];
};

/* rwpipe_t - rwpipe */

struct rwpipe_t {

	int trans_id;

	int queue_timeout;

	int recv_id;

	struct rwpipe_sigq_t* wq; /* write queue */
	struct rwpipe_sigq_t* rq; /* read queue */

	int qh_pq_mutex_init;
#ifdef CONFIG_USE_ASTERISK
	ast_mutex_t qh_pq_mutex; /* private queue mutext */
#else
	pthread_mutex_t qh_pq_mutex; /* private queue mutext */
#endif
	struct list_head qh_pq; /* private queue header */

	rwpipe_background_process_func_t rq_background_process;
};

struct rwpipe_t* rwpipe_create(int queue_timeout, rwpipe_background_process_func_t rq_background_process);
void rwpipe_destroy(struct rwpipe_t* pp);

int rwpipe_get_wq_fd(struct rwpipe_t* pp);
void rwpipe_clear_wq_signal(struct rwpipe_t* pp);

int rwpipe_feed_read(struct rwpipe_t* pp, const char* val);
int rwpipe_post_write(struct rwpipe_t* pp, struct rwpipe_pq_t* pq, int timeout, int replay, const char* msg);
int rwpipe_post_write_printf(struct rwpipe_t* pp, struct rwpipe_pq_t* pq, int timeout, int reply, char* format, ...);

int rwpipe_pop_rd_msg(struct rwpipe_t* pp, struct rwpipe_pq_t* pq, char* buf, int buf_len, int to);
int rwpipe_pop_wr_msg(struct rwpipe_t* pp, char* buf, int buf_len);

void rwpipe_gc_wr_msg(struct rwpipe_t* pp);
void rwpipe_gc_rd_msg(struct rwpipe_t* pp);

int rwpipe_post_and_get(struct rwpipe_t* pp, char *reply, int reply_len, int to, const char* msg);
int rwpipe_post_and_get_printf(struct rwpipe_t* pp, char *reply, int reply_len, int to, char* format, ...);


#endif
