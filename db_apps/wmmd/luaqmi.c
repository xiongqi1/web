#define _GNU_SOURCE

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

#include "luaqmi.h"

/* local functions */
static void send_msg_async_cb(qmi_client_type chndl, unsigned int msg_id, void *resp, unsigned int resp_len, void *resp_cb_ref, qmi_client_error_type transp_err);
static void luaqmi_remove_all_msg_async(struct luaqmi_t* lq);
static void luaqmi_set_async_flag(struct luaqmi_t* lq);

#ifdef DEBUG
#define _log3(...) _log_printf("[DBG]",__FUNCTION__,__LINE__,__VA_ARGS__)
#else
#define _log3(...) do {} while(0)
#endif

#define _log(...) _log_printf("[INFO]",__FUNCTION__,__LINE__,__VA_ARGS__)
#define _err(...) _log_printf("[ERR]",__FUNCTION__,__LINE__,__VA_ARGS__)

#define min(a,b) (((a)<(b))?(a):(b))

/* local structures */

struct trans_entity_t {
	struct luaqmi_t* lq;

	qmi_client_type chndl;
	int ind;
	unsigned int msg_id;
	void *resp;
	unsigned int resp_recv_len;
	unsigned int resp_buf_len;

	qmi_client_error_type transp_err;

	qmi_txn_handle thndl; /* tx handle */

	int ref; /* user reference */

	struct trans_entity_t* next;
	struct trans_entity_t* prev;
};

struct luaqmi_t {
	int serv_id; /* qmi service id */
	qmi_client_type nhndl; /* notifier handle */
	qmi_client_type chndl; /* client handle */

	qmi_idl_service_object_type shndl;

	qmi_cci_os_signal_type qmi_os_params; /* qmi os parameters */

	int pipefd[2]; /* self-pipe */

	int te_mutex_inited; /* te queue initiation flag */
	pthread_mutex_t te_mutex; /* te queue mutex */
	struct trans_entity_t te_qh; /* te queue header */
};


void _log_printf(const char* prefix, const char* func, int line, const char* fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	syslog(LOG_DEBUG, "%s %s:%d %s", prefix, func, line, buf);
}


/* te link functions */

static int te_is_empty(struct trans_entity_t* qh)
{
	return qh->next == qh;
}

static void te_insert_entity(struct trans_entity_t* te, struct trans_entity_t* prev, struct trans_entity_t* next)
{
	next->prev = te;
	te->next = next;
	te->prev = prev;
	prev->next = te;
}

static void te_remove_entity_inbetween(struct trans_entity_t* te, struct trans_entity_t* next)
{
	next->prev = te;
	te->next = next;
}

static void te_remove_entity(struct trans_entity_t* te)
{
	/* byapss if not linked */
	if(!te->prev || !te->next)
		return;

	te_remove_entity_inbetween(te->prev, te->next);

	te->prev = NULL;
	te->next = NULL;
}

static void te_add_entity_tail(struct trans_entity_t* te, struct trans_entity_t* qh)
{
	te_insert_entity(te, qh->prev, qh);
}

static void te_delete_entity(struct trans_entity_t* te)
{
	if(!te)
		return;

	_log3("free te (te=%p,resp=%p)", te, te->resp);

	if(te->resp)
		free(te->resp);
	te->resp = NULL;

	free(te);
}

static struct trans_entity_t* te_create_entity(struct luaqmi_t* lq, int resp_len, int ref)
{
	struct trans_entity_t* te;

	te = calloc(1, sizeof(*te));
	if(!te) {
		_log3("failed to allocate response entity");
		goto err;
	}

	te->lq = lq;
	te->resp_buf_len = resp_len;

	te->resp = calloc(1, resp_len);
	if(!te->resp) {
		_log3("failed to allocate resp");
		goto err;
	}

	/* put additional reference information */
	te->ref = ref;

	_log3("allocate te (te=%p,resp=%p)", te, te->resp);

	return te;
err:
	te_delete_entity(te);
	return NULL;
}

/* luaqmi functions */

static void luaqmi_add_te_queue(struct luaqmi_t* lq, struct trans_entity_t* te)
{
	struct trans_entity_t* te_qh;

	/* tail tail */
	pthread_mutex_lock(&lq->te_mutex);
	{
		te_qh = &lq->te_qh;

		_log3("QUEUE te=%p,serv_id=0x%04x,msg_id=0x%04x,resp=%p,resp_recv_len=%d,transp_err=%d", te, lq->serv_id, te->msg_id, te->resp, te->resp_recv_len, te->transp_err);
		te_add_entity_tail(te, te_qh);

		/* signal */
		_log3("set async lq");
		luaqmi_set_async_flag(lq);

		pthread_mutex_unlock(&lq->te_mutex);
	}
}

static void service_ind_cb(qmi_client_type chndl, unsigned int msg_id, void* ind_buf, unsigned int ind_buf_len, void* ind_cb_data)
{
	struct luaqmi_t* lq = (struct luaqmi_t*)ind_cb_data;
	struct trans_entity_t* te;
	uint32_t c_payload_len;
	int rc;

	_log3("Indication: msg_id=0x%x buf_len=%d", (unsigned int)msg_id, ind_buf_len);

	/*
	char* inbuf=(char*)ind_buf;
	int i;
	for(i=0;i<ind_buf_len;i++) {
		_log3("indi: %d = 0x%02x (%c)",i,inbuf[i], inbuf[i] & 0x7f);
	}
	*/

	/* get c payload length */
	rc = qmi_idl_get_message_c_struct_len(lq->shndl, QMI_IDL_INDICATION, msg_id, &c_payload_len);
	if(rc) {
		_err("failed to get c paylad - rc=%d", rc);
		goto err;
	}

	_log3("Indication: msg_id=0x%x buf_len=%d, c_payload_len=%d", (unsigned int)msg_id, ind_buf_len, c_payload_len);

	te = te_create_entity(lq, c_payload_len, 0);
	if(!te) {
		_err("te_create_entity() failed");
		goto err;
	}

	rc = qmi_client_message_decode(chndl, QMI_IDL_INDICATION, msg_id, ind_buf, ind_buf_len, te->resp, (int)c_payload_len);
	if(rc) {
		_err("qmi_client_message_decode() failed");
		goto err;
	}

	/* build te */
	te->chndl = chndl;
	te->ind = 1;
	te->msg_id = msg_id;
	te->resp_recv_len = c_payload_len;
	te->transp_err = 0;

	luaqmi_add_te_queue(lq, te);

err:
	return;
}

void luaqmi_delete(struct luaqmi_t* lq)
{
	_log3("CALL lq=%p", lq);

	if(!lq)
		return;

	_log3("remove client handle");
	luaqmi_remove_all_msg_async(lq);

	_log3("delete client handle");
	if(lq->chndl)
		qmi_client_release(lq->chndl);
	lq->chndl = NULL;

	_log3("delete client notifier");
	if(lq->nhndl)
		qmi_client_release(lq->nhndl);
	lq->nhndl = NULL;

	_log3("delete te_mutex");
	if(lq->te_mutex_inited)
		pthread_mutex_destroy(&lq->te_mutex);
	lq->te_mutex_inited = 0;

	_log3("close pipe-r");
	if(lq->pipefd[0] > 0)
		close(lq->pipefd[0]);
	lq->pipefd[0] = 0;

	_log3("close pipe-w");
	if(lq->pipefd[1] > 0)
		close(lq->pipefd[1]);
	lq->pipefd[1] = 0;

	_log3("delete luaqmi");
	free(lq);

	_log3("RETURN");
}

int luaqmi_get_async_fd(struct luaqmi_t* lq)
{
	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	return lq->pipefd[0];

err:
	return -1;
}

static void luaqmi_set_async_flag(struct luaqmi_t* lq)
{
	int dummy = 0;

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	/* write dummy to pipe */
	write(lq->pipefd[1], &dummy, sizeof(dummy));

err:
	return;
}

void luaqmi_update_flag(struct luaqmi_t* lq)
{
	char dummy[1024];
	struct trans_entity_t* te_qh;

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	/* waste pipe */
	while(read(lq->pipefd[0], dummy, sizeof(dummy)) > 0);

	pthread_mutex_lock(&lq->te_mutex);
	{
		te_qh = &lq->te_qh;

		if(!te_is_empty(te_qh))
			luaqmi_set_async_flag(lq);

		pthread_mutex_unlock(&lq->te_mutex);
	}

err:
	return;
}

qmi_client_error_type luaqmi_send_msg(struct luaqmi_t* lq, unsigned int msg_id, void *req, unsigned int req_len, void *resp, unsigned int resp_len, unsigned int timeout_msecs)
{
	_log3("CALL lq=%p,msgid=0x%04x,req=%p,req_len=%d,resp=%p,resp_len=%d,timeout=%d", lq, msg_id, req, req_len, resp, resp_len, timeout_msecs);

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	qmi_client_type chndl = lq->chndl;

	return qmi_client_send_msg_sync(chndl, msg_id, req, req_len, resp, resp_len, timeout_msecs);

err:
	return -1;
}

int luaqmi_remove_msg_async(struct luaqmi_t* lq)
{
	int stat = -1;
	struct trans_entity_t* te = NULL;
	struct trans_entity_t* te_qh;

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	pthread_mutex_lock(&lq->te_mutex);
	{
		te_qh = &lq->te_qh;

		/* bypass if empty */
		if(te_is_empty(te_qh))
			goto fini;

		/* hand over results */
		te = te_qh->next;

		/* remove from list */
		_log3("remove te from queue (te=%p)");
		te_remove_entity(te);

		stat = 0;

fini:
		pthread_mutex_unlock(&lq->te_mutex);
	}

	if(te) {
		_log3("delete te (te=%p)");
		te_delete_entity(te);
	}

	return stat;
err:
	return -1;
}

static void luaqmi_remove_all_msg_async(struct luaqmi_t* lq)
{
	while(!(luaqmi_remove_msg_async(lq) < 0));
}

qmi_client_error_type luaqmi_recv_msg_async(struct luaqmi_t* lq, unsigned int* msg_id, void *resp, unsigned int* resp_len, qmi_client_error_type* transp_err, int* ind, int* ref)
{
	int stat = -1;
	struct trans_entity_t* te = NULL;
	struct trans_entity_t* te_qh;

	_log3("CALL lq=%p,msgid_ptr=%p,resp=%p,resp_len_ptr=%p,resp_len=%d,transp_err_ptr=%p", lq, msg_id, resp, resp_len, *resp_len, transp_err);

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	pthread_mutex_lock(&lq->te_mutex);
	{
		te_qh = &lq->te_qh;

		/* bypass if empty */
		if(te_is_empty(te_qh))
			goto fini;

		/* hand over results */
		te = te_qh->next;

		_log3("POP te=%p,serv_id=0x%04x,msg_id=0x%04x,resp=%p,resp_recv_len=%d,transp_err=%d,ind=%d,ref=%d", te, lq->serv_id, te->msg_id, te->resp, te->resp_recv_len, te->transp_err, te->ind, te->ref);

		if(*resp_len < te->resp_recv_len) {
			_err("too small buffer used for QMI msg, waste (resp_len=%d,resp_recv_len=%d)", resp_len, te->resp_recv_len);
			goto fini_waste;
		}

		*ind = te->ind;
		*msg_id = te->msg_id;
		memcpy(resp, te->resp, te->resp_recv_len);
		*resp_len = te->resp_recv_len;
		*transp_err = te->transp_err;
		*ref = te->ref;

		stat = 0;

fini_waste:
		/* remove from list */
		_log3("remove te from queue (te=%p)");
		te_remove_entity(te);

fini:
		pthread_mutex_unlock(&lq->te_mutex);
	}

	/* free te */
	if(te) {
		_log3("delete te (te=%p)");
		te_delete_entity(te);
	}

	_log3("RETURN stat=%d", stat);

	return stat;

err:
	return -1;
}

qmi_client_error_type luaqmi_cancel_msg_async(struct luaqmi_t* lq, void** te_app_ptr)
{
	struct trans_entity_t* te = (struct trans_entity_t*)*te_app_ptr;

	qmi_client_error_type qerr;

	_log3("CALL lq=%p,te=%p", lq, te);

	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	/* tail tail */
	pthread_mutex_lock(&lq->te_mutex);
	{
		te_remove_entity(te);
		pthread_mutex_unlock(&lq->te_mutex);
	}

	/* cancel qmi */
	qerr = qmi_client_delete_async_txn(lq->chndl, te->thndl);

	/* delete te */
	te_delete_entity(te);

	return qerr;

err:
	return -1;
}

qmi_client_error_type luaqmi_send_msg_async(struct luaqmi_t* lq, unsigned int msg_id, void *req, unsigned int req_len, unsigned int resp_len, void** te_app_ptr, int ref)
{
	struct trans_entity_t* te;

	qmi_client_error_type qerr;

	_log3("CALL lq=%p,msgid=0x%04x,req=%p,req_len=%d,resp_len=%d,te_app_ptr=%p,ref=%d", lq, msg_id, req, req_len, resp_len, te_app_ptr, ref);
	if(!lq) {
		_err("invalid lq detected");
		goto err;
	}

	/* create te */
	te = te_create_entity(lq, resp_len, ref);
	if(!te) {
		_log3("te_create_entity() failed");
		goto err;
	}

	_log3("create te (te=%p,resp=%p,resp_buf_len=%d)", te, te->resp, te->resp_buf_len);

	_log3("call qmi_client_send_msg_async(te=%p)", te);
	qerr = qmi_client_send_msg_async(lq->chndl, msg_id, req, req_len, te->resp, te->resp_buf_len, send_msg_async_cb, te, &te->thndl);

	if(!qerr)
		*te_app_ptr = te;

	_log3("RETURN qerr=%d,te=%p", qerr, te);

	return qerr;

err:
	return -1;
}

static void send_msg_async_cb(qmi_client_type chndl, unsigned int msg_id, void *resp, unsigned int resp_len, void *resp_cb_ref, qmi_client_error_type transp_err)
{
	struct trans_entity_t* te;
	struct luaqmi_t* lq;

	_log3("CALL chndl=%p,msgid=0x%04x,resp=%p,resp_len=%d,te=%p,transp_err=%d", chndl, msg_id, resp, resp_len, resp_cb_ref, transp_err);

	_log3("obtain te");
	te = (struct trans_entity_t*)resp_cb_ref;
	if(!te) {
		_err("incorrect resp_cb_ref detected");
		goto fini;
	}

	_log3("obtain lq");
	lq = te->lq;

	_log3("serv_id=0x%04x,msg_id=0x%04x,te=%p,resp=%p,resp_buf_len=%d,ref=%d", lq->serv_id, msg_id, te, te->resp, te->resp_buf_len, te->ref);

	/* build te */
	te->ind = 0;
	te->chndl = chndl;
	te->msg_id = msg_id;
	te->resp_recv_len = resp_len;
	te->transp_err = transp_err;
	memcpy(te->resp, resp, te->resp_recv_len);

	luaqmi_add_te_queue(lq, te);

fini:
	_log3("RETURN");
	return;
}

struct luaqmi_t* luaqmi_new(int serv_id, qmi_idl_service_object_type shndl, int timeout)
{
	uint32_t num_services = 0;
	uint32_t num_entries = 0;

	qmi_client_error_type qerr;
	qmi_service_info info[10];

	struct luaqmi_t* lq = NULL;

	_log3("CALL shndl=%p", shndl);

	_log3("obtain NAS object");
	lq = calloc(1, sizeof(*lq));
	if(!lq) {
		_err("failed to allocate luaqmi");
		goto err;
	}

	lq->serv_id = serv_id;
	lq->shndl = shndl;

	if(!shndl) {
		_err("invalid shndl detected");
		goto err;
	}

	/* initiate members */
	_log3("initiate queue");
	lq->te_qh.prev = &lq->te_qh;
	lq->te_qh.next = &lq->te_qh;

	_log3("create self-pipe");
	if(pipe2(lq->pipefd, O_NONBLOCK) < 0) {
		_err("failed to create pipe - %s", strerror(errno));
		goto err;
	}

	_log3("create te_mutex");
	if(pthread_mutex_init(&lq->te_mutex, NULL) < 0) {
		_err("failed to create te_mutex - %s", strerror(errno));
		goto err;
	}
	lq->te_mutex_inited = 1;

	_log3("initiate notifier");
	qerr = qmi_client_notifier_init(shndl,
	                                &lq->qmi_os_params,
	                                &lq->nhndl);

	_log3("wait for service - timeout=%d", timeout);
	qerr = qmi_client_get_service_list(shndl, NULL, NULL, &num_services);
	if(qerr != QMI_NO_ERR) {
		QMI_CCI_OS_SIGNAL_WAIT(&lq->qmi_os_params, timeout);
		qerr = qmi_client_get_service_list(shndl, NULL, NULL, &num_services);
	}

	if(qerr != QMI_NO_ERR) {
		_err("no service replied - qerr=%d", qerr);
		goto err;
	}

	if(num_services == 0) {
		_err("no service found");
		goto err;
	}

	/* Num entries must be equal to num services */
	_log3("get service list");
	num_entries = num_services;
	qerr = qmi_client_get_service_list(shndl,
	                                   info,
	                                   &num_entries,
	                                   &num_services);

	/* Now that the service object is populated get the NAS handle */
	_log3("initiate client (num_entries=%d,num_services=%d)", num_entries, num_services);
	qerr = qmi_client_init(&info[0],
	                       shndl,
	                       service_ind_cb,
	                       lq,
	                       NULL,
	                       &lq->chndl);


	if(qerr) {
		_log3("error while trying to initialize client");
		goto err;
	}

	_log3("RETURN lq=%p,chndl=%p", lq, lq->chndl);

	return lq;

err:
	luaqmi_delete(lq);
	return NULL;
}
