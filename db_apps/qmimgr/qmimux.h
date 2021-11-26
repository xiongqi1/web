#ifndef __QMIMUX_H__
#define __QMIMUX_H__

#include "qmidef.h"
#include "minilib.h"
#include "binqueue.h"

#include <sys/select.h>

#define QMIMUX_RX_QUEUE_SIZE	(8*1024)
#define QMIMUX_SDU_BUF_SIZE	(4*1024)

struct qmi_phys_mux_hdr {
	unsigned char if_type;	/* interface type */
	unsigned short len;	/* length of qmux message, including qmux header */
	unsigned char cflag;	/* bit 7 - sender type (1=service, 0=control point), remaining bits are 0 */
	unsigned char stype;	/* service type */
	unsigned char cid;	/* client id */
} __packed;

struct client_key {
	unsigned char stype; // device side service type, eg. WDS, CTL, NAS etc..
	unsigned char cid;   // client id given by the device when subscribed to a service type
	unsigned char valid; // non-zero if key is valid
};

struct qmimux_t {
	int qcqmi_dev; 			/* device file handle  */
	const char* qcqmidev_name;	/* qcqmi device file name */
	struct binqueue_t* rx_q;	/* binary queue for mux layer */

	struct client_key client_keys[QMIUNICLIENT_SERVICE_CLIENT];

	char rx_buf[QMIMUX_SDU_BUF_SIZE];
	char usr_buf[QMIMUX_SDU_BUF_SIZE];

	char tx_buf[QMIMUX_SDU_BUF_SIZE];

	fd_set* readfds;
	fd_set* writefds;
};


void qmimux_close(struct qmimux_t *m);
int qmimux_open(struct qmimux_t *m);
int qmimux_set_writefds(struct qmimux_t *m, int *maxfd, fd_set *writefds);
int qmimux_set_readfds(struct qmimux_t *m, int *maxfd, fd_set *readfds);
int qmimux_is_write_set(struct qmimux_t *m);
int qmimux_is_read_set(struct qmimux_t *m);
int qmimux_get_client_id(struct qmimux_t *m, int service_id);
int qmimux_set_client_id(struct qmimux_t *m, int service_id, unsigned char stype, int client_id);
int qmimux_process_write_event(struct qmimux_t *m, int service_id, int stype, void *sdu, int sdu_len);
int qmimux_process_read_event(struct qmimux_t *m);
int qmimux_peek_sdu(struct qmimux_t *m, int *service_id, void **sdu, int *sdu_len);
void qmimux_destroy(struct qmimux_t *m);
struct qmimux_t *qmimux_create(const char *qcqmidev_name);
void qmimux_waste_sdu(struct qmimux_t* m,int byte_to_waste);
int is_quectel_qmi_proxy(const char* qcqmidev_name);

#endif
