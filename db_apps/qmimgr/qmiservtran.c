

// QMI Server Transaction

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "config.h"
#include "qmiservtran.h"

#include "minilib.h"
#include "qmimsg.h"
#include "qmiioctl.h"

void qmiservtran_destory(struct qmiservtran_t* servtran)
{
	int i;

	if(!servtran)
		return;

	qmiservtran_close(servtran);

	// destroy msgs
	for(i=0;i<QMISERVTRAN_MAX_MSG_COUNT;i++) {
		qmimsg_destroy(servtran->tx_msgs[i]);
		qmimsg_destroy(servtran->rx_msgs[i]);
	}

	_free(servtran);
}

struct qmiservtran_t* qmiservtran_create(const char* qcqmidev_name,int service_id)
{
	struct qmiservtran_t* servtran;
	int i;

	// allocate object
	servtran=_malloc(sizeof(struct qmiservtran_t));
	if(!servtran) {
		SYSLOG(LOG_ERROR,"failed to allocate qmiservtran_t - size=%d",sizeof(struct qmiservtran_t));
		goto err;
	}

	servtran->qcqmi_dev=-1;

	// create msgs
	for(i=0;i<QMISERVTRAN_MAX_MSG_COUNT;i++) {
		servtran->tx_msgs[i]=qmimsg_create();
		if(!servtran->tx_msgs[i])
			goto err;

		servtran->rx_msgs[i]=qmimsg_create();
		if(!servtran->rx_msgs[i])
			goto err;
	}

	// init. memeber
	servtran->qcqmi_dev=-1;
	servtran->service_id=service_id;
	servtran->qcqmidev_name=qcqmidev_name;

	return servtran;

err:
	qmiservtran_destory(servtran);
	return NULL;
}

void qmiservtran_close(struct qmiservtran_t* servtran)
{
	if(!servtran)
		return;

	if(servtran->qcqmi_dev>=0)
		close(servtran->qcqmi_dev);
	servtran->qcqmi_dev=-1;
}

static int qmiservtran_ioctl(struct qmiservtran_t* servtran,int cmd,void* argp)
{
	return ioctl(servtran->qcqmi_dev, cmd,argp);
}

static int qmiservtran_get_client_id(struct qmiservtran_t* servtran)
{
	return qmiservtran_ioctl(servtran, IOCTL_QMI_GET_SERVICE_FILE,(void*)(servtran->service_id));
}

int qmiservtran_read_from_buf(struct qmiservtran_t* servtran,const void* buf,int buf_len,unsigned char* msg_type,unsigned short* tran_id)
{
	struct qmi_phys_tran_hdr* phys_tran;

	unsigned char phys_msg_type;
	unsigned short phys_tran_id;

	struct qmimsg_t* msg;

	int i;

	int read_len;
	int msg_len;
	void* read_ptr;

	// extract transaction header
	phys_tran=(struct qmi_phys_tran_hdr*)buf;
	phys_msg_type=phys_tran->ctrl_flag;
	phys_tran_id=read16_from_little_endian(phys_tran->tran_id);
	read_len=sizeof(*phys_tran);

	/* convert QMICTL CF to normal CF - QMICTL has a different CF flags - start from bit0 */
	if(servtran->service_id==QMICTL) {
		phys_msg_type<<=1;
	}

	// check reserved - must be 0
	if(phys_msg_type & QMISERVTRAN_CTRL_MASK) {
		SYSLOG(LOG_ERROR,"nonzero value detected in reserved area of transaction header - control flags=0x%02x",phys_tran->ctrl_flag);
		goto err;
	}

	// check msg type
	if( (phys_msg_type!=QMI_MSGTYPE_RESP) && (phys_msg_type!=QMI_MSGTYPE_INDI) ) {
		SYSLOG(LOG_ERROR,"incorrect msg type detected in transaction header - msg_type=0x%02x",phys_msg_type);
		goto err;
	}

	// check transaction id
	if( (phys_msg_type!=QMI_MSGTYPE_INDI) && !phys_tran_id ) {
		SYSLOG(LOG_ERROR,"zero transaction id detected in transaction header - tran_type=0x%04x",phys_tran_id);
		goto err;
	}

	if(msg_type)
		*msg_type=phys_msg_type;
	if(tran_id)
		*tran_id=phys_tran_id;

	SYSLOG(LOG_DEBUG,"phys_msg_type=0x%02x,phys_tran_id=%d,hdr_len=%d,buf_len=%d",phys_msg_type,phys_tran_id,read_len,buf_len);
	i=0;
	while(read_len<buf_len) {

		// check if more than QMISERVTRAN_MAX_MSG_COUNT msg in transaction
		if(i>=QMISERVTRAN_MAX_MSG_COUNT) {
			SYSLOG(LOG_ERROR,"trans(tran_id=%d) has more than %d msgs - ignored",phys_tran_id,QMISERVTRAN_MAX_MSG_COUNT);
			goto err;
		}

		// get msg
		msg=servtran->rx_msgs[i];

		// read msg
		read_ptr=(char*)buf+read_len;
		msg_len=qmimsg_read_from_buf(msg,read_ptr,buf_len-read_len);
		if(msg_len<0) {
			goto err;
		}

		SYSLOG(LOG_DEBUG,"tlv idx=%d, msg_len=%d, tlv_count=%d",i,msg_len,msg->tlv_count);

		read_len+=msg_len;
		i++;
	}

	servtran->rx_msg_count=i;

	SYSLOG(LOG_DEBUG,"rx_msg_count=%d",servtran->rx_msg_count);

	return 0;

err:
	return -1;
}

int qmiservtran_phys_read(struct qmiservtran_t* servtran, unsigned char* msg_type,unsigned short* tran_id)
{
	int read_len;

	#ifdef QMIMGR_CONFIG_READ_FREEZE_WORKAROUND
	alarm(QMISERVTRAN_RX_TIMEOUT);
	#endif	
	
	read_len=read(servtran->qcqmi_dev,servtran->qmi_rx_buf,sizeof(servtran->qmi_rx_buf));
	if(read_len<0) {
		SYSLOG(LOG_ERROR,"read system call failed - size=%d,service_id=%d",sizeof(servtran->qmi_rx_buf),servtran->service_id);
		goto err;
	}

	#ifdef QMIMGR_CONFIG_READ_FREEZE_WORKAROUND
	alarm(0);
	#endif

	SYSLOG(LOG_COMM,"###qmi### recv - read_len=%d",read_len);
	_dump(LOG_DUMP,__FUNCTION__,servtran->qmi_rx_buf,read_len);

	if(qmiservtran_read_from_buf(servtran,servtran->qmi_rx_buf,read_len,msg_type,tran_id)<0)
		goto err;

	return read_len;

err:
	return -1;
}

int qmiservtran_write_to_buf(struct qmiservtran_t* servtran, unsigned short tran_id, void* buf, int buf_len)
{
	struct qmi_phys_tran_hdr* phys_tran;
	void* ptr;

	int i;
	int written_len;
	int total_written_len;

	// fill up transaction header
	phys_tran=(struct qmi_phys_tran_hdr*)buf;
	phys_tran->ctrl_flag=QMI_MSGTYPE_REQ;
	write16_to_little_endian(tran_id,phys_tran->tran_id);

	// get initial buf detail
	total_written_len=sizeof(*phys_tran);

	i=0;
	while(i<servtran->tx_msg_count) {

		ptr=(char*)buf+total_written_len;
		written_len=qmimsg_write_to_buf(servtran->tx_msgs[i],ptr,buf_len-total_written_len);
		if(written_len<0)
			goto err;

		total_written_len+=written_len;
		i++;
	}
	return total_written_len;

err:
	return -1;
}

int qmiservtran_phys_write(struct qmiservtran_t* servtran, unsigned short tran_id)
{
	int written_len;
	int phys_written_len;

	// build phys packet
	written_len=qmiservtran_write_to_buf(servtran,tran_id,servtran->qmi_tx_buf,QMISERVTRAN_TXRX_BUF_SIZE);
	if(written_len<0)
		goto err;

	_dump(LOG_DUMP,__FUNCTION__,servtran->qmi_tx_buf,written_len);

	// write to the driver
	phys_written_len=write(servtran->qcqmi_dev,servtran->qmi_tx_buf,written_len);

	// qmi transaction is never able to be processed with broken size - treat it as error
	if(phys_written_len!=written_len) {
		SYSLOG(LOG_ERROR,"written size mismatching - req_to_write=%d,actual_written=%d",written_len,phys_written_len);
		goto err;
	}

	return written_len;

err:
	return -1;
}


int qmiservtran_open(struct qmiservtran_t* servtran)
{
	int stat;

	// open
	servtran->qcqmi_dev=open(servtran->qcqmidev_name,O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(servtran->qcqmi_dev<0) {
		SYSLOG(LOG_ERROR,"failed to open %s",servtran->qcqmidev_name);
		goto fini;
	}

	// do not request client for control channel
	if(servtran->service_id!=0) {
		// get client id
		stat=qmiservtran_get_client_id(servtran);
		if(stat<0) {
			SYSLOG(LOG_ERROR,"failed to send IOCTL_QMI_GET_SERVICE_FILE (service_id=%d) - %s(%d)",servtran->service_id, strerror(errno),errno);
			goto err;
		}
	}

fini:
	return servtran->qcqmi_dev;

err:
	qmiservtran_close(servtran);
	return -1;
}

