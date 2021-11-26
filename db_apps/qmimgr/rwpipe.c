#include "rwpipe.h"

#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <syslog.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>

#ifdef CONFIG_USE_ASTERISK
/* include asterisk headers */
#include "asterisk.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/format_cache.h"
#include "asterisk/causes.h"
#include "asterisk/lock.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#else
#endif

#include "qmirdbctrl.h"

#ifdef CONFIG_USE_ASTERISK

/* redefine log defines - override incorrect asterisk defines */
#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#endif

/* qmsg_t - queue message */
static struct rwpipe_qmsg_t* qmsg_alloc(int trans_id,const char* str,int timeout);
static void qmsg_destroy(struct rwpipe_qmsg_t* qmsg);
static void qmsg_complete(struct rwpipe_qmsg_t* qmsg,int expired);

/* pq_t - private queue */
static struct rwpipe_pq_t* pq_create(rwpipe_pq_filter_func_t pq_filter);
static void pq_destroy(struct rwpipe_pq_t* pq);


/* sigq_t - signal-able queue */
static struct rwpipe_sigq_t* rwpipe_sigq_create(int use_self_pipe);
static void rwpipe_sigq_destroy(struct rwpipe_sigq_t *sigq);
static int rwpipe_sigq_signal(struct rwpipe_sigq_t* sigq,int autolock);
static int rwpipe_sigq_wait(struct rwpipe_sigq_t* sigq,struct timespec* ts,int autolock);
static int rwpipe_sigq_get_fd(struct rwpipe_sigq_t* sigq);
static void rwpipe_sigq_distory_all_msgs(struct rwpipe_sigq_t *sigq);
static void rwpipe_sigq_clear_signal(struct rwpipe_sigq_t* sigq,int autolock);


/* local peripheral functions */

int __get_realtime_sec()
{
	struct timespec ts_now;

	clock_gettime(CLOCK_REALTIME,&ts_now);

	return ts_now.tv_sec;
}



/* rwpipe functions */

int rwpipe_pop_rd_msg(struct rwpipe_t* pp,struct rwpipe_pq_t* pq,char* buf,int buf_len,int to)
{
	struct rwpipe_qmsg_t* qmsg = NULL;
	struct rwpipe_sigq_t* sigq;
	int stat = -1;

	struct timespec ts_s;
	struct timespec ts_to;
	struct timespec ts_now;

	/* select read queue */
	if(pq)
		sigq = pq->rq;
	else
		sigq = pp->rq;

	/* get start time */
	clock_gettime(CLOCK_REALTIME,&ts_now);

	/* set timeout */
	ts_to=ts_now;
	ts_to.tv_sec+=to;

	/* set s */
	ts_s=ts_now;

	while(1) {

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_lock(&sigq->mutex);
		#else
		pthread_mutex_lock(&sigq->mutex);
		#endif
		{
			/* wait if queue is empty */
			if(!pp->rq_background_process && list_empty(&sigq->qh) && to)
				rwpipe_sigq_wait(sigq,&ts_to,0);

			/* take one if queue is not empty */
			if(!list_empty(&sigq->qh)) {
				qmsg=container_of(sigq->qh.next,struct rwpipe_qmsg_t, list);
				list_del(&qmsg->list);
			}

			#ifdef CONFIG_USE_ASTERISK
			ast_mutex_unlock(&sigq->mutex);
			#else
			pthread_mutex_unlock(&sigq->mutex);
			#endif
		}

		clock_gettime(CLOCK_REALTIME,&ts_now);

		/* break if we got a msg */
		if(qmsg || (ts_now.tv_sec-ts_s.tv_sec>=to))
			break;

		if(pp->rq_background_process)
			pp->rq_background_process(pp);
	}

	/* remove private queue from list */
	if(pq) {
		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_lock(&pp->qh_pq_mutex);
		#else
		pthread_mutex_lock(&pp->qh_pq_mutex);
		#endif
		{
			list_del(&pq->list);

			#ifdef CONFIG_USE_ASTERISK
			ast_mutex_unlock(&pp->qh_pq_mutex);
			#else
			pthread_mutex_unlock(&pp->qh_pq_mutex);
			#endif
		}
	}

	if(qmsg) {
		snprintf(buf,buf_len,qmsg->msg);

		pp->recv_id=qmsg->trans_id;

		syslog(LOG_INFO,"[CCHP] read %s",buf);

		qmsg_destroy(qmsg);

		stat = 0;
	}

	rwpipe_gc_rd_msg(pp);

	return stat;
}

int rwpipe_pop_wr_msg(struct rwpipe_t* pp,char* buf,int buf_len)
{
	struct rwpipe_qmsg_t* qmsg = NULL;
	int stat = -1;

	rwpipe_gc_wr_msg(pp);

	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&pp->wq->mutex);
	#else
	pthread_mutex_lock(&pp->wq->mutex);
	#endif
	{
		if(!list_empty(&pp->wq->qh)) {

			qmsg=container_of(pp->wq->qh.next,struct rwpipe_qmsg_t, list);

			list_del(&qmsg->list);
		}

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&pp->wq->mutex);
		#else
		pthread_mutex_unlock(&pp->wq->mutex);
		#endif
	}

	if(qmsg) {
		snprintf(buf,buf_len,"%d %s",(unsigned char)qmsg->trans_id,qmsg->msg);

		syslog(LOG_INFO,"[CCHP] write %s",buf);

		qmsg_complete(qmsg,0);
		qmsg_destroy(qmsg);

		stat = 0;
	}

	return stat;
}

int rwpipe_get_wq_fd(struct rwpipe_t* pp)
{
	return rwpipe_sigq_get_fd(pp->wq);
}

static void rwpipe_sigq_distory_all_msgs(struct rwpipe_sigq_t *sigq)
{
	struct rwpipe_qmsg_t* qmsg;

	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&sigq->mutex);
	#else
	pthread_mutex_lock(&sigq->mutex);
	#endif
	{
		while(!list_empty(&sigq->qh)) {
			qmsg=container_of(sigq->qh.next,struct rwpipe_qmsg_t, list);

			list_del(&qmsg->list);
			qmsg_destroy(qmsg);
		}

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&sigq->mutex);
		#else
		pthread_mutex_unlock(&sigq->mutex);
		#endif

	}
}

void rwpipe_clear_wq_signal(struct rwpipe_t* pp)
{
	rwpipe_sigq_clear_signal(pp->wq,1);
}

void rwpipe_destroy(struct rwpipe_t* pp)
{
	if(!pp)
		return;

	if(pp->qh_pq_mutex_init) {
		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_destroy(&pp->qh_pq_mutex);
		#else
		pthread_mutex_destroy(&pp->qh_pq_mutex);
		#endif
	}
	pp->qh_pq_mutex_init=0;

	rwpipe_sigq_destroy(pp->rq);
	rwpipe_sigq_destroy(pp->wq);

	free(pp);
}

struct rwpipe_t* rwpipe_create(int queue_timeout,rwpipe_background_process_func_t rq_background_process)
{
	struct rwpipe_t* pp;

	pp=calloc(1,sizeof(*pp));
	if(!pp) {
		syslog(LOG_ERR,"calloc() faeild in rwpipe_create()");
		goto err;
	}

	/* initaite members */
	pp->queue_timeout = queue_timeout;

	pp->wq=rwpipe_sigq_create(1);
	if(!pp->wq) {
		syslog(LOG_ERR,"rwpipe_sigq_create(1) faeild in rwpipe_create()");
		goto err;
	}

	pp->rq=rwpipe_sigq_create(0);
	if(!pp->rq) {
		syslog(LOG_ERR,"rwpipe_sigq_create(1) faeild in rwpipe_create()");
		goto err;
	}

	#ifdef CONFIG_USE_ASTERISK
	if(ast_mutex_init(&pp->qh_pq_mutex)<0) {
	#else
	if(pthread_mutex_init(&pp->qh_pq_mutex,NULL)<0) {
	#endif
		syslog(LOG_ERR,"pthread_mutex_init() failed in rwpipe_create() - %s",strerror(errno));
		goto err;
	}
	pp->qh_pq_mutex_init=1;

	INIT_LIST_HEAD(&pp->qh_pq);
	pp->rq_background_process=rq_background_process;

	return pp;
err:
	rwpipe_destroy(pp);
	return NULL;
}

int rwpipe_feed_read(struct rwpipe_t* pp, const char* val)
{
	struct rwpipe_qmsg_t* qmsg;
	int trans_id;
	struct rwpipe_sigq_t* sigq;
	char* p;

	/* get trans id */
	trans_id=strtoul(val,&p,10);
	if((trans_id<0) || ((trans_id==ULONG_MAX) && (errno==ERANGE))) {
		syslog(LOG_ERR,"trans-id out of range (val=%s)",val);
		goto err;
	}
	
	/* skip number and space */
	while(*p) {
		if(!isdigit(*p))
			break;
		p++;
	}

	if(*p && isblank(*p))
		p++;

	syslog(LOG_INFO,"[CCHP] feed %s",p);

	/* allocate qmsg */
	qmsg=qmsg_alloc(trans_id, p, pp->queue_timeout);
	if(!qmsg) {
		syslog(LOG_ERR,"qmsg_alloc() failed in rwpipe_feed_read()");
		goto err;
	}

	/* set to_foreground */
	struct list_head* pos;
	struct list_head* n;
	struct rwpipe_pq_t* pq;

	/* use rq by default */
	sigq=pp->rq;

	/* for each private queue */
	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&pp->qh_pq_mutex);
	#else
	pthread_mutex_lock(&pp->qh_pq_mutex);
	#endif
	{

		list_for_each_safe(pos,n,&pp->qh_pq) {
			pq=container_of(pos,struct rwpipe_pq_t, list);

			/* if trans_id is matching or if filter returns true */
			if( (!pq->pq_filter && (pq->trans_id == qmsg->trans_id)) || (pq->pq_filter && pq->pq_filter(pq,qmsg)) ) {
				sigq=pq->rq;

				syslog(LOG_INFO,"[CCHP] filtered %s",qmsg->msg);

				break;
			}
		}

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&pp->qh_pq_mutex);
		#else
		pthread_mutex_unlock(&pp->qh_pq_mutex);
		#endif

	}

	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&sigq->mutex);
	#else
	pthread_mutex_lock(&sigq->mutex);
	#endif
	{
		list_add_tail(&qmsg->list,&sigq->qh);
		rwpipe_sigq_signal(sigq,0);

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&sigq->mutex);
		#else
		pthread_mutex_unlock(&sigq->mutex);
		#endif
	}



	return 0;
err:
	return -1;
}

int rwpipe_post_write_printf(struct rwpipe_t* pp, struct rwpipe_pq_t* pq, int timeout,int reply, char* format,...)
{
	char msg[RDB_MAX_VAL_LEN];

	va_list ap;

	va_start(ap,format);
	vsnprintf(msg,sizeof(msg),format,ap);
	va_end(ap);

	return rwpipe_post_write(pp,pq,timeout,reply,msg);
}

int rwpipe_post_write(struct rwpipe_t* pp, struct rwpipe_pq_t* pq, int timeout,int replay, const char* msg)
{
	struct rwpipe_qmsg_t* qmsg;
	int trans_id;

	syslog(LOG_INFO,"[CCHP] post %s",msg);

	/* do not use zero trans id */
	if(!pp->trans_id)
		pp->trans_id++;

	/* get current trans id */
	if(replay)
		trans_id=pp->recv_id;
	else
		trans_id=pp->trans_id++;

	if(!timeout)
		timeout=pp->queue_timeout;

	qmsg=qmsg_alloc(trans_id, msg, timeout);
	if(!qmsg) {
		syslog(LOG_ERR,"qmsg_alloc() failed in rwpipe_post_write()");
		goto err;
	}

	/* link private queue to list */
	if(pq) {
		pq->trans_id = trans_id;

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_lock(&pp->qh_pq_mutex);
		#else
		pthread_mutex_lock(&pp->qh_pq_mutex);
		#endif
		{
			list_add_tail(&pq->list,&pp->qh_pq);

			#ifdef CONFIG_USE_ASTERISK
			ast_mutex_unlock(&pp->qh_pq_mutex);
			#else
			pthread_mutex_unlock(&pp->qh_pq_mutex);
			#endif
		}
	}

	/* link qmsg to list */
	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&pp->wq->mutex);
	#else
	pthread_mutex_lock(&pp->wq->mutex);
	#endif

	{
		list_add_tail(&qmsg->list,&pp->wq->qh);
		rwpipe_sigq_signal(pp->wq,0);

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&pp->wq->mutex);
		#else
		pthread_mutex_unlock(&pp->wq->mutex);
		#endif
	}


	return 0;

err:
	return -1;
}

int rwpipe_post_and_get_printf(struct rwpipe_t* pp,char *reply,int reply_len,int to, char* format,...)
{
	char msg[RDB_MAX_VAL_LEN];

	va_list ap;

	va_start(ap,format);
	vsnprintf(msg,sizeof(msg),format,ap);
	va_end(ap);

	return rwpipe_post_and_get(pp,reply,reply_len,to,msg);
}

int rwpipe_post_and_get(struct rwpipe_t* pp,char *reply,int reply_len,int to, const char* msg)
{
	struct rwpipe_pq_t* pq = NULL;
	int stat = -1;

	/* allocate private queue */
	pq=pq_create(NULL);
	if(!pq) {
		syslog(LOG_ERR,"pq_create() failed");
		goto err;
	}

	if(!to)
		to=pp->queue_timeout;

	/* post msg */
	stat = rwpipe_post_write(pp,pq,to,0,msg);
	if(stat<0) {
		syslog(LOG_ERR,"rwpipe_post_write() failed (stat=%d)",stat);
		goto err;
	}

	/* wait for private queue */
	stat=rwpipe_pop_rd_msg(pp,pq,reply,reply_len,to);
	if(stat<0) {
		syslog(LOG_DEBUG,"rwpipe_pop_rd_msg() failed (stat=%d)",stat);
		goto err;
	}

	/* remove any extra queued msg */
	rwpipe_sigq_distory_all_msgs(pq->rq);

	pq_destroy(pq);

	return 0;
err:
	pq_destroy(pq);
	return -1;
}

static void rwpipe_gc_msg_ex(struct rwpipe_t* pp,struct rwpipe_sigq_t* sigq)
{
	struct list_head* pos;
	struct list_head* n;
	struct rwpipe_qmsg_t* qmsg;

	struct list_head qh;

	int cur;

	const char* queue_name;

	if(pp->wq == sigq)
		queue_name="write";
	else if(pp->rq == sigq)
		queue_name="read";
	else
		queue_name="unknown";

	INIT_LIST_HEAD(&qh);

	/* collect expired msgs */
	#ifdef CONFIG_USE_ASTERISK
	ast_mutex_lock(&sigq->mutex);
	#else
	pthread_mutex_lock(&sigq->mutex);
	#endif
	{
		cur = __get_realtime_sec();

		list_for_each_safe(pos,n,&sigq->qh) {
			qmsg=container_of(pos,struct rwpipe_qmsg_t,list);

			if(cur - qmsg->timestamp < qmsg->timeout)
				continue;

			list_move(&qmsg->list, &qh);
		}

		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_unlock(&sigq->mutex);
		#else
		pthread_mutex_unlock(&sigq->mutex);
		#endif
	}

	/* destroy qmsg */
	list_for_each_safe(pos,n,&qh) {

		qmsg=container_of(pos,struct rwpipe_qmsg_t,list);

		syslog(LOG_ERR,"queue timeout (q=%s,msg=%s)",queue_name,qmsg->msg);

		qmsg_complete(qmsg,1);

		qmsg_destroy(qmsg);
	}


}

void rwpipe_gc_wr_msg(struct rwpipe_t* pp)
{
	rwpipe_gc_msg_ex(pp,pp->wq);
}
void rwpipe_gc_rd_msg(struct rwpipe_t* pp)
{
	rwpipe_gc_msg_ex(pp,pp->rq);
}

/* signal-able queue functions */

static int rwpipe_sigq_get_fd(struct rwpipe_sigq_t* sigq)
{
	return sigq->pipefd[0];
}

static int rwpipe_sigq_wait(struct rwpipe_sigq_t* sigq,struct timespec* ts,int autolock)
{
	int stat = -1;

	if(sigq->use_self_pipe)
		goto err;

	if(autolock) {
		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_lock(&sigq->mutex);
		#else
		pthread_mutex_lock(&sigq->mutex);
		#endif
	}

	{
		#ifdef CONFIG_USE_ASTERISK
		stat=ast_cond_timedwait(&sigq->cond,&sigq->mutex,ts);
		#else
		stat=pthread_cond_timedwait(&sigq->cond,&sigq->mutex,ts);
		#endif
		if(stat==ETIMEDOUT)
			stat=-1;


		if(autolock) {
			#ifdef CONFIG_USE_ASTERISK
			ast_mutex_unlock(&sigq->mutex);
			#else
			pthread_mutex_unlock(&sigq->mutex);
			#endif
		}
	}

	return stat;

err:
	return -1;
}

static void rwpipe_sigq_clear_signal(struct rwpipe_sigq_t* sigq,int autolock)
{
	int dummy;

	while(read(sigq->pipefd[0],&dummy,sizeof(dummy))>0);
}

static int rwpipe_sigq_signal(struct rwpipe_sigq_t* sigq,int autolock)
{
	int dummy = 0;
	int stat = -1;

	if(sigq->use_self_pipe) {
		if(write(sigq->pipefd[1],&dummy,sizeof(dummy))<0) {
			syslog(LOG_ERR,"write() failed in rwpipe_sigq_signal() - %s",strerror(errno));
			goto err;
		}

		stat=0;
	}
	else {
		if(autolock) {
			#ifdef CONFIG_USE_ASTERISK
			ast_mutex_lock(&sigq->mutex);
			#else
			pthread_mutex_lock(&sigq->mutex);
			#endif
		}

		#ifdef CONFIG_USE_ASTERISK
		stat=ast_cond_signal(&sigq->cond);
		#else
		stat=pthread_cond_signal(&sigq->cond);
		#endif

		if(stat<0)
			syslog(LOG_ERR,"pthread_cond_signal() failed in rwpipe_sigq_signal() - %s",strerror(errno));

		if(autolock) {
			#ifdef CONFIG_USE_ASTERISK
			stat=ast_mutex_unlock(&sigq->mutex);
			#else
			stat=pthread_mutex_unlock(&sigq->mutex);
			#endif
		}
	}

	return stat;
err:
	return -1;
}

static void rwpipe_sigq_destroy(struct rwpipe_sigq_t *sigq)
{
	if(!sigq)
		return;

	if(sigq->pipefd[0]>0)
		close(sigq->pipefd[0]);

	if(sigq->pipefd[1]>0)
		close(sigq->pipefd[1]);

	if(sigq->cond_inited) {
		#ifdef CONFIG_USE_ASTERISK
		ast_cond_destroy(&sigq->cond);
		#else
		pthread_cond_destroy(&sigq->cond);
		#endif
	}
	sigq->cond_inited=0;

	if(sigq->mutex_inited) {
		#ifdef CONFIG_USE_ASTERISK
		ast_mutex_destroy(&sigq->mutex);
		#else
		pthread_mutex_destroy(&sigq->mutex);
		#endif
	}
	sigq->mutex_inited=0;

	free(sigq);
}

static struct rwpipe_sigq_t* rwpipe_sigq_create(int use_self_pipe)
{
	struct rwpipe_sigq_t* sigq;

	sigq=calloc(1,sizeof(*sigq));
	if(!sigq)
		goto err;

	sigq->use_self_pipe=use_self_pipe;

	if(sigq->use_self_pipe) {

		/* ignore broken pipe sig - not to be killed by the signal */
		signal(SIGPIPE, SIG_IGN);

		if(pipe2(sigq->pipefd,O_NONBLOCK)<0) {
			syslog(LOG_ERR,"pipe2() failed in wpipe_queue_hdr_create() - %s",strerror(errno));
			goto err;
		}
	}
	else {
		#ifdef CONFIG_USE_ASTERISK
		if(ast_cond_init(&sigq->cond, NULL)<0) {
		#else
		if(pthread_cond_init(&sigq->cond, NULL)<0) {
		#endif
			syslog(LOG_ERR,"pthread_cond_init() failed in wpipe_queue_hdr_create() - %s",strerror(errno));
			goto err;
		}
		sigq->cond_inited=1;

	}

	#ifdef CONFIG_USE_ASTERISK
	if(ast_mutex_init(&sigq->mutex)<0) {
	#else
	if(pthread_mutex_init(&sigq->mutex, NULL)<0) {
	#endif
		syslog(LOG_ERR,"pthread_mutex_init() failed in wpipe_queue_hdr_create() - %s",strerror(errno));
		goto err;
	}
	sigq->mutex_inited=1;

	INIT_LIST_HEAD(&sigq->qh);

	return sigq;

err:
	rwpipe_sigq_destroy(sigq);
	return NULL;
}


/* private queue functions */

static void pq_destroy(struct rwpipe_pq_t* pq)
{
	if(!pq)
		return;

	rwpipe_sigq_destroy(pq->rq);

	free(pq);
}

static struct rwpipe_pq_t* pq_create(rwpipe_pq_filter_func_t pq_filter)
{
	struct rwpipe_pq_t* pq;

	pq=calloc(1,sizeof(*pq));
	if(!pq) {
		syslog(LOG_ERR,"calloc() failed in pg_alloc()");
		goto err;
	}

	pq->rq=rwpipe_sigq_create(0);
	if(!pq->rq) {
		syslog(LOG_ERR,"rwpipe_sigq_create() failed in pg_alloc()");
		goto err;
	}

	pq->pq_filter=pq_filter;

	return pq;

err:
	pq_destroy(pq);
	return NULL;
}

/* queue message functions */

static void qmsg_complete(struct rwpipe_qmsg_t* qmsg,int expired)
{
	if(qmsg->complete)
		qmsg->complete(qmsg,expired);
}

static void qmsg_destroy(struct rwpipe_qmsg_t* qmsg)
{
	if(!qmsg)
		return;

	if(qmsg->free)
		qmsg->free(qmsg);

	free(qmsg->msg);
	free(qmsg);
}

static struct rwpipe_qmsg_t* qmsg_alloc(int trans_id,const char* str,int timeout)
{
	struct rwpipe_qmsg_t* qmsg;

	qmsg=calloc(1,sizeof(*qmsg));
	if(!qmsg) {
		syslog(LOG_ERR,"calloc() failed in qmsg_alloc()");
		goto err;
	}

	INIT_LIST_HEAD(&qmsg->list);

	qmsg->trans_id=trans_id;
	qmsg->msg=strdup(str);

	qmsg->timestamp = __get_realtime_sec();
	qmsg->timeout = timeout;

	if(!qmsg->msg) {
		syslog(LOG_ERR,"strdup() failed in qmsg_alloc() - %s",strerror(errno));
		goto err;
	}



	return qmsg;

err:
	qmsg_destroy(qmsg);
	return NULL;
}


/* unit test functions */

#ifdef CONFIG_UNIT_TEST

void* thread_exec(void* ref)
{
	struct rwpipe_t* pp = (struct rwpipe_t*)ref;
	int fd_q;

	struct timeval tv;
	fd_set rfd;

	int stat;

	fd_q=rwpipe_get_wq_fd(pp);

	char msg_to_write[1024];
	char msg_read[1024];

	char buf[1024];
	int trans_id;

	while(1) {

		FD_ZERO(&rfd);
		FD_SET(fd_q, &rfd);

		tv.tv_sec=100;
		tv.tv_usec=0;

		stat=select(fd_q+1,&rfd,NULL,NULL,&tv);

		if(stat<0) {
			syslog(LOG_ERR,"select() punk!");
			break;
		}

		if(rwpipe_pop_wr_msg(pp,msg_to_write,sizeof(msg_to_write))<0) {
			rwpipe_clear_wq_signal(pp);
		}
		else {
			/* write to RDB */
			printf("[WRITE-EMUL] %s\n",msg_to_write);

			if(strstr(msg_to_write,"TIMEOUT")) {
				printf("[READ-EMUL] !!causing timeout!!\n");
			}
			else if(strstr(msg_to_write,"EXTRA")) {
				printf("[READ-EMUL] add extra reply\n");
				rwpipe_feed_read(pp,"0 EXTRA REPLY");

				snprintf(msg_read,sizeof(msg_read),"%s OK",msg_to_write);
				printf("[READ-EMUL] %s\n",msg_read);
				rwpipe_feed_read(pp,msg_read);
			}
			else {
				/* read from RDB */
				snprintf(msg_read,sizeof(msg_read),"%s OK",msg_to_write);
				printf("[READ-EMUL] %s\n",msg_read);
				rwpipe_feed_read(pp,msg_read);
			}



			*buf=0;
			sscanf(msg_to_write, "%d %s",&trans_id,buf);

			if(!strcmp(buf,"BYE")) {

				printf("thread punk!");
				break;
			}
		}
	}

	return NULL;
}

int main(int argc,char* argv[])
{
	int stat = -1;
	void* res;
	pthread_t tid;

	struct rwpipe_t* pp;

	pp=rwpipe_create(10,NULL);

	stat=pthread_create(&tid,NULL,thread_exec,pp);
	if(stat<0) {
		syslog(LOG_ERR,"pthread_create() failed in main() - %s",strerror(errno));
		goto fini;
	}

	int i;
	const char* msg;
	const char* msgs_to_post[]={
		"HELLO",
		"ABCDEFGHIJKLMNOPQRSTUVXYZ",
		"CONTROL MSG 1",
		"CONTROL MSG 2",
		NULL
	};

	char reply[1024];

	i=0;
	while((msg=msgs_to_post[i++])!=NULL) {

		printf("[POST:%d] %s\n",__get_realtime_sec(),msg);
		if(rwpipe_post_and_get(pp,msg,10,reply,sizeof(reply))>0) {
			syslog(LOG_ERR,"rwpipe_post_and_get(HELLO) failed in main()");
			goto fini;
		}

		printf("[REPLY:%d] %s\n",__get_realtime_sec(),reply);
	}

	printf("[POST:%d] %s\n",__get_realtime_sec(),"EXTRA");
	if(rwpipe_post_and_get(pp,"EXTRA",30,reply,sizeof(reply))<0) {
		printf("[REPLY:%d] !!!error!!!\n",__get_realtime_sec());
	}
	else {
		printf("[REPLY:%d] %s\n",__get_realtime_sec(),reply);
	}

	printf("[POST:%d] %s\n",__get_realtime_sec(),"TIMEOUT");
	if(rwpipe_post_and_get(pp,"TIMEOUT",10,reply,sizeof(reply))<0) {
		printf("[REPLY:%d] !!!error!!!\n",__get_realtime_sec());
	}
	else {
		printf("[REPLY:%d] %s\n",__get_realtime_sec(),reply);
	}


	rwpipe_post_and_get(pp,"BYE",10,reply,sizeof(reply));

	pthread_join(tid,&res);

fini:
	rwpipe_destroy(pp);

	return stat;
}

#endif
