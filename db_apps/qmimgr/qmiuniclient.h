#ifndef __QMIUNICLIENT_H__
#define __QMIUNICLIENT_H__

#include <sys/select.h>

#include "qmimsg.h"

#ifdef CONFIG_LINUX_QMI_DRIVER
#include "qmimux.h"
#endif

struct qmiuniclient_t;

typedef void (*qmiuniclient_callback)(struct qmiuniclient_t* uni,int serv_id,unsigned char msg_type,unsigned short tran_id,struct qmimsg_t* msg);

struct qmiuniclient_t {
	struct qmiservtran_t* servtrans[QMIUNICLIENT_SERVICE_CLIENT];
	struct msgqueue_t* tx_qs[QMIUNICLIENT_SERVICE_CLIENT];
	struct msgqueue_t* rx_qs[QMIUNICLIENT_SERVICE_CLIENT];
	qmiuniclient_callback callback;

	struct qmimsg_t* immediate_msg;

	#ifdef CONFIG_LINUX_QMI_DRIVER
	struct qmimux_t* qmux;
	#else
	fd_set* readfds;
	fd_set* writefds;
	#endif
};

struct qmi_service_cfg_t {
	int enable;
	int mandatory;
	const char* name;
};

extern struct qmi_service_cfg_t qmi_service_cfg[];

// t : unsigned char
// l : unsigned short
// v : void*


struct qmiuniclient_t* qmiuniclient_create(const char* qmidev_name,int instance);
void qmiuniclient_destroy(struct qmiuniclient_t* uni);
int qmiuniclient_write(struct qmiuniclient_t* uni,int serv_id,struct qmimsg_t* msg,unsigned short* tran_id);
void qmiuniclient_register_callback(struct qmiuniclient_t* uni,qmiuniclient_callback callback);

int qmiuniclient_setfds(struct qmiuniclient_t* uni, int* maxfd, fd_set* readfds, fd_set* writefds);
int qmiuniclient_process_select(struct qmiuniclient_t* uni, int wait_serv_id, unsigned short wait_tran_id, unsigned short wait_msg_id, struct qmimsg_t** ret_msg);

void qmiuniclient_destroy_servtran(struct qmiuniclient_t* uni,int servtran);

#endif
