#ifndef __QMISERVTRAN_H__
#define __QMISERVTRAN_H__

#include "minilib.h"
#include "qmidef.h"

#define QMISERVTRAN_RX_TIMEOUT		5
#define QMISERVTRAN_TXRX_BUF_SIZE	2048		// max transaction TX & RX packet length
#define QMISERVTRAN_MAX_MSG_COUNT	10		// max service message count in a transaction
#define QMISERVTRAN_CTRL_MASK		0xF9

struct qmi_phys_tran_hdr {
	unsigned char ctrl_flag;
	unsigned short tran_id;
} __packed;


struct qmiservtran_t {
	int qcqmi_dev;
	const char* qcqmidev_name;

	int service_id;
	unsigned char stype;

	char qmi_tx_buf[QMISERVTRAN_TXRX_BUF_SIZE];
	char qmi_rx_buf[QMISERVTRAN_TXRX_BUF_SIZE];

	int tx_msg_count;
	struct qmimsg_t* tx_msgs[QMISERVTRAN_MAX_MSG_COUNT];

	int rx_msg_count;
	struct qmimsg_t* rx_msgs[QMISERVTRAN_MAX_MSG_COUNT];
};

int qmiservtran_selftest();

struct qmiservtran_t* qmiservtran_create(const char* qcqmidev_name,int service_id);
void qmiservtran_destory(struct qmiservtran_t* servtran);
void qmiservtran_close(struct qmiservtran_t* servtran);
int qmiservtran_open(struct qmiservtran_t* servtran);

int qmiservtran_phys_write(struct qmiservtran_t* servtran, unsigned short tran_id);
int qmiservtran_phys_read(struct qmiservtran_t* servtran, unsigned char* msg_type,unsigned short* tran_id);

int qmiservtran_read_from_buf(struct qmiservtran_t* servtran,const void* buf,int buf_len,unsigned char* msg_type,unsigned short* tran_id);
int qmiservtran_write_to_buf(struct qmiservtran_t* servtran, unsigned short tran_id, void* buf, int buf_len);

#endif
