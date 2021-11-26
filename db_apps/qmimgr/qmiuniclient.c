#include <stdio.h>
#include <sys/select.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "minilib.h"
#include "qmiservtran.h"
#include "msgqueue.h"
#include "qmiioctl.h"
#include "qmidef.h"

#include "qmiuniclient.h"
#include "funcschedule.h"


struct qmi_service_cfg_t qmi_service_cfg[QMIUNICLIENT_SERVICE_CLIENT]={
#ifdef CONFIG_LINUX_QMI_DRIVER
	[QMICTL] = {1,1,"QMICTL"},
#else
	/* Qualcomm QMI driver internally maintains QMICTL and applications are not supposed to deal with QMICTL */
	[QMICTL] = {0,1,"QMICTL"},
#endif
	[QMIWDS] = {1,1,"QMIWDS"},
	[QMIDMS] = {1,1,"QMIDMS"},
	[QMINAS] = {1,1,"QMINAS"},
	[QMIQOS] = {0,0,"QMIQOS"},
#ifdef V_SMS_QMI_MODE_y
	[QMIWMS] = {1,1,"QMIWMS"},
#else
	[QMIWMS] = {0,0,"QMIWMS"},
#endif
	[QMIPDS] = {1,0,"QMIPDS"},

#ifdef QMI_VOICE_y
	[QMIVOICE] = {1,1,"QMIWMS"},
#endif

	[QMIUIM] = {1,0,"QMIUIM"},
	[QMILOC] = {1,0,"QMILOC"},
	[QMIIPV6]= {1,1,"QMIIPV6"}
};

static unsigned short qmiuniclient_inc_trans_id()
{
	static unsigned short uniclient_trans_id=0;

	// transaction id has to be non-zero
	uniclient_trans_id++;
	if(!uniclient_trans_id)
		uniclient_trans_id++;

	return uniclient_trans_id;
}

void qmiuniclient_register_callback(struct qmiuniclient_t* uni,qmiuniclient_callback callback)
{
	uni->callback=callback;
}

int qmiuniclient_setfds(struct qmiuniclient_t* uni, int* maxfd, fd_set* readfds, fd_set* writefds)
{
	int i;
	int fd;
	struct qmiservtran_t* servtran;

	#ifdef CONFIG_LINUX_QMI_DRIVER
	__UNUSED(fd);

	qmimux_set_readfds(uni->qmux,maxfd,readfds);
	for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {

		/* bypass if no servtran */
		servtran=uni->servtrans[i];
		if(!servtran)
			continue;

		/* toggle writefds if traffic */
		if(!msgqueue_is_empty(uni->tx_qs[i])) {
			SYSLOG(LOG_DEBUG,"[linux] putting servtrans fd %d to writefds",uni->qmux->qcqmi_dev);
			qmimux_set_writefds(uni->qmux,maxfd,writefds);
		}
	}
	#else

	if(maxfd) {
		for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {

			servtran=uni->servtrans[i];
			if(!servtran)
				continue;

			fd=servtran->qcqmi_dev;
			if(fd<0)
				continue;

			if(*maxfd<fd)
				*maxfd=fd;

			#ifdef DEBUG
			SYSLOG(LOG_DEBUG,"putting servtrans fd %d to readfds",fd);
			#endif
			FD_SET(fd, readfds);

			if(!msgqueue_is_empty(uni->tx_qs[i])) {
				SYSLOG(LOG_DEBUG,"putting servtrans fd %d to writefds",fd);
				FD_SET(fd, writefds);
			}
		}
	}

	uni->readfds=readfds;
	uni->writefds=writefds;
	#endif

	return 0;
}

/*

	ret < 0  : error
	ret == 0 : normal
	ret == 1 : tran_id received
	ret == 2 : tran_id timeout
*/

int qmiuniclient_process_select(struct qmiuniclient_t* uni, int wait_serv_id, unsigned short wait_tran_id, unsigned short wait_msg_id, struct qmimsg_t** ret_msg)
{
	int fd;
	struct qmiservtran_t* servtran;

	int i;
	int j;

	unsigned char msg_type;
	unsigned short recv_tran_id;

	struct msgqueue_t* q;
	struct qmimsg_t* msg;

	unsigned char dummy;
	unsigned short msg_tran_id;


	#ifdef CONFIG_LINUX_QMI_DRIVER
	int byte_to_waste;
	void* sdu;
	int sdu_len;
	int service_id;
	int written_len;

	/* process read event - read qmux into read queue */
	if(qmimux_is_read_set(uni->qmux)) {
		SYSLOG(LOG_DEBUG,"###qmimux### got read event");
		qmimux_process_read_event(uni->qmux);
	}

	/* process read queue - read SDUs from queue into msgqueue */
	while(1) {

		/* peek a sdu from qmimux */
		byte_to_waste=qmimux_peek_sdu(uni->qmux,&service_id,&sdu,&sdu_len);

		/* break if no sdu */
		if(byte_to_waste<0)
			break;

		SYSLOG(LOG_DEBUG,"###qmimux### got a SDU (service_id=%d,sdu_len=%d)",service_id,sdu_len);

		servtran=uni->servtrans[service_id];
		
		if(!servtran) {
			SYSLOG(LOG_DEBUG,"###qmimux### service id out of range (service_id=%d,sdu_len=%d)",service_id,sdu_len);
		}
		else {
			/* convert to sdu packet */
			if(qmiservtran_read_from_buf(servtran,(char*)sdu,sdu_len,&msg_type,&recv_tran_id)>=0) {
				// weird situation since we do not use multiple msgs
				if(servtran->rx_msg_count!=1) {
					SYSLOG(LOG_ERROR,"###qmimux### multiple msgs received in a immeidiate transaction - serv=%d,tran=%d,msg_cnt=%d",service_id,wait_tran_id,servtran->rx_msg_count);
				}

				SYSLOG(LOG_DEBUG,"###qmimux### queueing qmimsg into rx queue - servtran=%d,msg_count=%d",service_id,servtran->rx_msg_count);

				q=uni->rx_qs[service_id];

				// copy all msgs into rx queue
				for(j=0;j<servtran->rx_msg_count;j++) {
					msgqueue_add(q,servtran->rx_msgs[j],msg_type,recv_tran_id);
				}
			}
		}

		/* waste sdu */
		qmimux_waste_sdu(uni->qmux,byte_to_waste);
	}


	if(qmimux_is_write_set(uni->qmux)) {
		SYSLOG(LOG_DEBUG,"###qmimux### got write event");

		for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {
			servtran=uni->servtrans[i];
			if(!servtran)
				continue;
			
			q=uni->tx_qs[i];

			if(!msgqueue_is_empty(q)) {

				msg=msgqueue_peek(q,&dummy,&msg_tran_id);

				SYSLOG(LOG_DEBUG,"###qmimux### writing msg (wait_tran_id=%d) into servtrans %d",msg_tran_id,i);

				// build msg
				servtran->tx_msg_count=1;
				qmimsg_copy_from(servtran->tx_msgs[0],msg);

				SYSLOG(LOG_COMM,"###qmimux### send - service_id=%d, msg_id=0x%04x",i,servtran->tx_msgs[0]->msg_id);

				// write to buffer
				written_len=qmiservtran_write_to_buf(servtran,msg_tran_id,servtran->qmi_tx_buf,QMISERVTRAN_TXRX_BUF_SIZE);
				if(written_len<0) {
					SYSLOG(LOG_ERR,"###qmimux### failed to write to buffer");
					continue;
				}

				/* write to dev */
				if(qmimux_process_write_event(uni->qmux,i,servtran->stype,servtran->qmi_tx_buf,written_len)<0) {
					SYSLOG(LOG_OPERATION,"###qmimux### write operation failed - try later");
					continue;
				}

				msgqueue_remove(q);
			}
			else {
				SYSLOG(LOG_DUMP,"###qmimux### tx queue is empty!");
			}
		}
	}

	#else

	// do read and write
	for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {

		servtran=uni->servtrans[i];
		if(!servtran)
			continue;

		fd=servtran->qcqmi_dev;
		if(fd<0)
			continue;

		if(FD_ISSET(fd, uni->readfds)) {
			SYSLOG(LOG_DEBUG,"servtrans %d got read event",i);

			if(qmiservtran_phys_read(servtran,&msg_type,&recv_tran_id)>0) {
				SYSLOG(LOG_DEBUG,"read qmimsg servtrans=%d, msg_type=0x%02x, wait_tran_id=%d",i,msg_type,recv_tran_id);

				// weird situation since we do not use multiple msgs
				if(servtran->rx_msg_count!=1) {
					SYSLOG(LOG_ERROR,"multiple msgs received in a immeidiate transaction - serv=%d,tran=%d,msg_cnt=%d",i,wait_tran_id,servtran->rx_msg_count);
				}

				SYSLOG(LOG_DEBUG,"queueing qmimsg into rx queue - servtran=%d,msg_count=%d",i,servtran->rx_msg_count);

				q=uni->rx_qs[i];

				// copy all msgs into rx queue
				for(j=0;j<servtran->rx_msg_count;j++) {
					msgqueue_add(q,servtran->rx_msgs[j],msg_type,recv_tran_id);
				}
			}
		}

		if(FD_ISSET(fd, uni->writefds)) {
			SYSLOG(LOG_DEBUG,"servtrans %d got write event",i);
			q=uni->tx_qs[i];

			if(!msgqueue_is_empty(q)) {

				msg=msgqueue_peek(q,&dummy,&msg_tran_id);

				SYSLOG(LOG_DEBUG,"writing msg (wait_tran_id=%d) into servtrans %d",msg_tran_id,i);

				// build msg
				servtran->tx_msg_count=1;
				qmimsg_copy_from(servtran->tx_msgs[0],msg);

				SYSLOG(LOG_COMM,"###qmi### send - service_id=%d, msg_id=0x%04x",i,servtran->tx_msgs[0]->msg_id);

				// write
				if(qmiservtran_phys_write(servtran,msg_tran_id)<0) {
					SYSLOG(LOG_ERROR,"write operation failed - throwing away the packet");
				}

				msgqueue_remove(q);
			}
			else {
				SYSLOG(LOG_DUMP,"tx queue is empty!");
			}
		}

	}

	#endif

	// process read buffer
	for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {

		servtran=uni->servtrans[i];
		if(!servtran)
			continue;

		#ifdef CONFIG_LINUX_QMI_DRIVER
		/* buildin qmimux does not use handles from service transaction objects */
		__UNUSED(fd);
		#else
		fd=servtran->qcqmi_dev;
		if(fd<0)
			continue;
		#endif

		q=uni->rx_qs[i];

		// call callback
		#ifdef DEBUG
		SYSLOG(LOG_DEBUG,"dequeueing qmimsg from rx queue - servtran=%d",i);
		#endif
		while(!msgqueue_is_empty(q)) {
			msg=msgqueue_peek(q,&msg_type,&msg_tran_id);

			// if transaction matched
			if( (i==wait_serv_id) && (wait_tran_id==msg_tran_id) && (ret_msg!=0) ) {
				SYSLOG(LOG_DUMP,"immeidate recv packed detected");

				qmimsg_copy_from(uni->immediate_msg, msg);
				*ret_msg=uni->immediate_msg;

				msgqueue_remove(q);
				goto interrupt;
			}

			if( (i==wait_serv_id) && (msg_type==QMI_MSGTYPE_INDI) && (wait_msg_id==msg->msg_id) && (ret_msg!=0) ) {
				SYSLOG(LOG_DUMP,"Asynchronous Indication Message detected");

				qmimsg_copy_from(uni->immediate_msg, msg);
				*ret_msg=uni->immediate_msg;

				msgqueue_remove(q);
				goto interrupt;
			}

			SYSLOG(LOG_DEBUG,"msg(wait_tran_id=%d,msg_tran_id=%d,msg_id=0x%04x) dequeued - servtran=%d",wait_tran_id,msg_tran_id,msg->msg_id,i);
			SYSLOG(LOG_COMM,"###qmi### dequeue - service_id=%d,msg_id=0x%04x",i,msg->msg_id);

			if(uni->callback) {
				SYSLOG(LOG_DEBUG,"calling recv handler - servtran=%d",i);
				uni->callback(uni,i,msg_type,msg_tran_id,msg);
			}

			if(i==wait_serv_id) {
				if( (wait_msg_id>0) && (wait_msg_id==msg->msg_id) ) {
					SYSLOG(LOG_DUMP,"wait for msg_id(0x%04x) - matched",wait_msg_id);

					msgqueue_remove(q);
					goto interrupt;
				}
				else if( (wait_tran_id>0) && (wait_tran_id==msg_tran_id) ) {
					SYSLOG(LOG_DUMP,"wait for tran_id(0x%04x) - matched",wait_tran_id);

					msgqueue_remove(q);
					goto interrupt;
				}
			}

			msgqueue_remove(q);
		}

		#ifdef DEBUG
		SYSLOG(LOG_DEBUG,"dequeueing finished - servtran=%d",i);
		#endif
	}

	return 0;

interrupt:
	return 1;
}

int qmiuniclient_write(struct qmiuniclient_t* uni,int serv_id,struct qmimsg_t* msg,unsigned short* tran_id)
{
	unsigned short cur_tran_id;

	/* check if servtran exists */
	if(!uni->servtrans[serv_id]) {
		SYSLOG(LOG_ERR,"###qmimux## access to absent servtran (serv_id=%d)",serv_id);
		goto err;
	}

	cur_tran_id=qmiuniclient_inc_trans_id();

	if(msgqueue_add(uni->tx_qs[serv_id],msg,0,cur_tran_id)<0)
		goto err;

	if(tran_id)
		*tran_id=cur_tran_id;

	return 0;
err:
	return -1;
}

void qmiuniclient_destroy_servtran(struct qmiuniclient_t* uni,int serv_id)
{
	/* destory failed servtrans */
	if(uni->servtrans[serv_id])
		qmiservtran_destory(uni->servtrans[serv_id]);
	uni->servtrans[serv_id]=NULL;
}

void qmiuniclient_destroy(struct qmiuniclient_t* uni)
{
	int i;

	if(!uni)
		return;

	// delete transactions
	for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++)  {
		qmiuniclient_destroy_servtran(uni,i);
		msgqueue_destroy(uni->tx_qs[i]);
		msgqueue_destroy(uni->rx_qs[i]);
	}

	qmimsg_destroy(uni->immediate_msg);

	#ifdef CONFIG_LINUX_QMI_DRIVER
	qmimux_close(uni->qmux);
	qmimux_destroy(uni->qmux);
	#endif

	_free(uni);
}

struct qmiuniclient_t* qmiuniclient_create(const char* qmidev_name,int instance)
{
	struct qmiuniclient_t* uni;
	struct qmi_service_cfg_t* cfg;
	int i;
	int j;
	int stat=-1;

	// create the object
	uni=(struct qmiuniclient_t*)_malloc(sizeof(struct qmiuniclient_t));
	if(!uni) {
		SYSLOG(LOG_ERROR,"failed to allocate qmiuniclient_t - size=%d",sizeof(struct qmiuniclient_t));
		goto err;
	}

	#ifdef CONFIG_LINUX_QMI_DRIVER
	uni->qmux=qmimux_create(qmidev_name);
	if(!uni->qmux) {
		SYSLOG(LOG_ERR,"###qmimux## failed to create qmux object - %s",strerror(errno));
		goto err;
	}

	if(qmimux_open(uni->qmux)<0) {
		SYSLOG(LOG_ERR,"###qmimux## failed to open (dev_name=%s) - %s",qmidev_name,strerror(errno));
		goto err;
	}
	#endif

	// create qmi service transactions
	for(i=0;i<QMIUNICLIENT_SERVICE_CLIENT;i++) {
		
		/* init. per-servtran members */
		uni->servtrans[i]=NULL;
		uni->tx_qs[i]=NULL;
		uni->rx_qs[i]=NULL;
		
		/* get qmi service configuration */
		cfg=&qmi_service_cfg[i];

		/* bypass if disabled */
		if(!cfg->enable)
			continue;

		uni->servtrans[i]=qmiservtran_create(qmidev_name,i);
		if(!uni->servtrans[i])
			goto err;

		#ifdef CONFIG_LINUX_QMI_DRIVER
		/* nothing to do */
		__UNUSED(stat);
		__UNUSED(j);
		#else
		for(j=0;(j<5) && ((stat=qmiservtran_open(uni->servtrans[i]))<0);j++) {
			SYSLOG(LOG_ERROR,"open a service failed - service=%d #%d/5",i,j);
			sleep(1);
		}

		if(stat<0) {
			if(cfg->mandatory) {
				SYSLOG(LOG_ERR,"###qmimux## failed to open a mandatory servtran#%d",i);
				goto err;
			}

			/* destory failed servtrans */
			qmiuniclient_destroy_servtran(uni,i);

			SYSLOG(LOG_OPERATION,"###qmimux## ignore failure to open an optional servtran#%d",i);
			continue;
		}
		#endif

		uni->tx_qs[i]=msgqueue_create();
		if(!uni->tx_qs[i])
			goto err;

		uni->rx_qs[i]=msgqueue_create();
		if(!uni->rx_qs[i])
			goto err;

		// WDS service in QMI device offers IPv6 or IPv4 data mode but they must be in separated client.
		// serv_id (user side) and stype (devive side) relationship can be n:1
		// to uniquely identify a connection, device will also use client id.
		// For QMIIPV6, we set QMI WDS service type here, client id will be setup in later stage.
		uni->servtrans[i]->stype = i == QMIIPV6 ? QMIWDS : i;
	}

	// create qmi msg
	uni->immediate_msg=qmimsg_create();
	if(!uni->immediate_msg)
		goto err;


	return uni;

err:
	qmiuniclient_destroy(uni);
	return NULL;
}

