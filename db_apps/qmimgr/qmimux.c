#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "qmimux.h"
#include "qmiservtran.h"

#include "endian.h"

typedef unsigned short int sa_family_t;
#include <linux/un.h>
#define QUECTEL_QMI_PROXY "quectel-qmi-proxy"

// check if the given qmi_port is a Quectel QMI proxy server port
int is_quectel_qmi_proxy(const char* qmi_port) {
	return strcmp(qmi_port, QUECTEL_QMI_PROXY) == 0;
}

// open a local socket to the proxy server
// all QMI commands will be sent over this socket
static int qmi_proxy_open(const char *name) {
   int sockfd = -1;
   int reuse_addr = 1;
   struct sockaddr_un sockaddr;
   socklen_t alen;

   // create socket to Quectel QMI proxy server
   sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
   if (sockfd < 0) {
      SYSLOG(LOG_ERROR,"###qmimux### socket() failed,  errno:%d (%s)\n", errno, strerror(errno));
      return sockfd;
   }

   memset(&sockaddr, 0, sizeof(sockaddr));
   sockaddr.sun_family = AF_LOCAL;
   sockaddr.sun_path[0] = 0;
   memcpy(sockaddr.sun_path + 1, name, strlen(name));

   alen = strlen(name) + offsetof(struct sockaddr_un, sun_path) + 1;
   if (connect(sockfd, (struct sockaddr *)&sockaddr, alen) < 0) {
      close(sockfd);
      SYSLOG(LOG_ERROR,"###qmimux### connect to %s errno:%d (%s)\n", name, errno, strerror(errno));
      return -1;
   }

   setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
   fcntl(sockfd, F_SETFL, fcntl(sockfd,F_GETFL) | O_NONBLOCK);
   fcntl(sockfd, F_SETFD, FD_CLOEXEC);

   return sockfd;
}

void qmimux_close(struct qmimux_t* m)
{
	/* bypass if not open */
	if(m->qcqmi_dev<0)
		return;

	/* close and reset handle */
	close(m->qcqmi_dev);
	m->qcqmi_dev=-1;
}

int qmimux_open(struct qmimux_t* m)
{
   /* open qcqmi dev */
   if (is_quectel_qmi_proxy(m->qcqmidev_name)) {
      m->qcqmi_dev = qmi_proxy_open(m->qcqmidev_name);
   }
   else {
      m->qcqmi_dev=open(m->qcqmidev_name,O_RDWR | O_NOCTTY | O_NONBLOCK);
   }

   if (m->qcqmi_dev<0) {
      SYSLOG(LOG_ERROR,"###qmimux### failed to open %s",m->qcqmidev_name);
      goto fini;
   }
	return m->qcqmi_dev;
fini:
	return -1;
}

int qmimux_set_writefds(struct qmimux_t* m,int* maxfd, fd_set* writefds)
{
	if(maxfd) {

		/* set writefds */
		FD_SET(m->qcqmi_dev, writefds);

		/* get maxfd */
		if(m->qcqmi_dev>*maxfd)
			*maxfd=m->qcqmi_dev;
	}

	/* store fds */
	m->writefds=writefds;

	return 0;
}

int qmimux_set_readfds(struct qmimux_t* m,int* maxfd, fd_set* readfds)
{
	if(maxfd) {

		/* set readfds */
		FD_SET(m->qcqmi_dev, readfds);

		#ifdef DEBUG
		SYSLOG(LOG_DEBUG,"[linux] putting servtrans fd %d to readfds",m->qcqmi_dev);
		#endif
		
		/* get maxfd */
		if(m->qcqmi_dev>*maxfd)
			*maxfd=m->qcqmi_dev;
	}

	/* store fds */
	m->readfds=readfds;

	return 0;
}

int qmimux_is_write_set(struct qmimux_t* m)
{
	return FD_ISSET(m->qcqmi_dev,m->writefds);
}

int qmimux_is_read_set(struct qmimux_t* m)
{
	return FD_ISSET(m->qcqmi_dev,m->readfds);
}

int qmimux_get_client_id(struct qmimux_t* m,int service_id)
{
	if(service_id<0 || service_id>=QMIUNICLIENT_SERVICE_CLIENT) {
		SYSLOG(LOG_ERROR,"###qmimux### service id out of range (service_id=%d)",service_id);
		goto err;
	}

	if (m->client_keys[service_id].valid) {
		return m->client_keys[service_id].cid;
	}

	// at start-up no client exists yet, use cid=0
	return 0;
err:
	return -1;
}

int qmimux_set_client_id(struct qmimux_t* m,int service_id, unsigned char stype, int client_id)
{
	if(service_id<0 || service_id>=QMIUNICLIENT_SERVICE_CLIENT) {
		SYSLOG(LOG_ERROR,"###qmimux### service id out of range (service_id=%d)",service_id);
		goto err;
	}

	m->client_keys[service_id].cid = client_id;
	m->client_keys[service_id].stype = stype;
	m->client_keys[service_id].valid = 1;
	return 0;

err:
	return -1;
}

// lookup service id using client id and device side service type
static int qmimux_get_serv_id(struct qmimux_t* m, unsigned char stype, unsigned char cid)
{
	int i;
	for (i=0; i<QMIUNICLIENT_SERVICE_CLIENT; i++) {
		if (m->client_keys[i].valid &&
			m->client_keys[i].stype == stype &&
			m->client_keys[i].cid == cid) {
			return i;
		}
	}
	return -1;
}

int qmimux_process_write_event(struct qmimux_t* m,int service_id,int stype,void* sdu,int sdu_len)
{
	struct qmi_phys_mux_hdr* mhdr;
	struct qmi_phys_tran_hdr* sdu_shdr;
	unsigned char* sdu_dhdr;

	int body_len;
	unsigned short muxlen;

	int written;

	/* bypass if too big SDU */
	if(sdu_len>sizeof(m->tx_buf)) {
		SYSLOG(LOG_ERR,"###qmimux### too big SDU in tx (sdu_len=%d,tx_buf_len=%d)",sdu_len,sizeof(m->tx_buf));
		goto err;
	}

	/* build mux header */
	mhdr=(struct qmi_phys_mux_hdr*)m->tx_buf;
	memset(mhdr,0,sizeof(*mhdr));
	mhdr->if_type=1;	/* always 1 */
	mhdr->cflag=0;		/* 0=Control Point, 1=Service */
	mhdr->len=0;
	mhdr->stype=stype;
	mhdr->cid=qmimux_get_client_id(m,service_id);

	/* get source and destination of SDU header */
	sdu_shdr=(struct qmi_phys_tran_hdr*)sdu;
	sdu_dhdr=(unsigned char*)(mhdr+1);

	/* copy SDU header */
	*sdu_dhdr++=sdu_shdr->ctrl_flag;
	if(service_id==QMICTL) {
		*sdu_dhdr++=(unsigned char)(sdu_shdr->tran_id);
	}
	else {
		write16_to_little_endian(sdu_shdr->tran_id,*(unsigned short*)(sdu_dhdr));
		sdu_dhdr+=2;
	}

	/* copy SDU body */
	body_len=sdu_len-sizeof(*sdu_shdr);
	memcpy(sdu_dhdr,sdu_shdr+1,body_len);
	sdu_dhdr+=body_len;

	/* set length in mux header */
	muxlen=(unsigned short)( (char*)(sdu_dhdr)-(char*)m->tx_buf );
	/* SDU length does not include if_type */
	write16_to_little_endian(muxlen-1,mhdr->len);

	/* dump */
	_dump(LOG_DUMP,__FUNCTION__,m->tx_buf,muxlen);

	/* write to dev */
	written=write(m->qcqmi_dev,m->tx_buf,muxlen);

	/* TODO: use a queue and handle half-written cases */
	if(written!=muxlen) {
		
		if(errno==EAGAIN || errno==ENODEV) {
			SYSLOG(LOG_OPERATION,"###qmimux### failed to write (written=%d,muxlen=%d,errno=%d) - %s",written,muxlen,errno,strerror(errno));
		}
		else {
			SYSLOG(LOG_ERR,"###qmimux### failed to write (written=%d,muxlen=%d,errno=%d) - %s",written,muxlen,errno,strerror(errno));
		}
		goto err;
	}

	return 0;

err:
	return -1;
}

int qmimux_process_read_event(struct qmimux_t* m)
{
	int read_len;
	int written_len;
	int rx_q_free;

	/* overflow control - do not read any more if no spae in rx q */
	rx_q_free=binqueue_get_free_len(m->rx_q);
	if(rx_q_free<sizeof(m->rx_buf)) {
		SYSLOG(LOG_ERROR,"###qmimux### rx q overflow (rx_buf=%d,rx_q_free=%d)",sizeof(m->rx_buf),rx_q_free);
		goto err;
	}

	/* read qmux packets from dev */
	read_len=read(m->qcqmi_dev,m->rx_buf,sizeof(m->rx_buf));
	/* bypass if no packet */
	if(read_len<=0)
		goto fini;

	/* dump */
	_dump(LOG_DUMP,__FUNCTION__,m->rx_buf,read_len);

	/* queue packets to rx q */
	written_len=binqueue_write(m->rx_q,m->rx_buf,read_len);

	/* check overflow */
	if(read_len!=written_len) {
		SYSLOG(LOG_ERROR,"###qmimux### failed to write to rx q (read_len=%d,written_len=%d)",read_len,written_len);
		goto err;
	}

fini:
	return 0;
err:
	return -1;
}

void qmimux_waste_sdu(struct qmimux_t* m,int byte_to_waste)
{
	return binqueue_waste(m->rx_q,byte_to_waste);
}

int qmimux_peek_sdu(struct qmimux_t* m,int* service_id,void** sdu,int* sdu_len)
{
	int len;
	struct qmi_phys_mux_hdr* mhdr;

	#if 0
	int client_id;
	#endif	

	char* sdu_hdr;
	unsigned char sdu_ctl_flags;
	unsigned short sdu_tran_id;

	int byte_to_waste;
	int muxlen;

	struct qmi_phys_tran_hdr* tran_hdr;

	byte_to_waste=0;

	/* read from queue */
	while(1) {
		len=binqueue_peek(m->rx_q,m->usr_buf,sizeof(m->usr_buf));

		/* break if too small */
		if(len<sizeof(struct qmi_phys_mux_hdr))
			goto err;

		/* convert to mhdr */
		mhdr=(struct qmi_phys_mux_hdr*)m->usr_buf;

		/* bypass if incorrect if type */
		if(mhdr->if_type!=1) {
			SYSLOG(LOG_ERR,"###qmimux### incorrect interface type detected (if_type=0x%02x)",mhdr->if_type);
			binqueue_waste(m->rx_q,1);
			continue;
		}

		/* bypass if incorrect sender type */
		if(mhdr->cflag!=0x80) {
			SYSLOG(LOG_ERR,"###qmimux### incorrect sender type detected (cflag=0x%02x)",mhdr->cflag);
			binqueue_waste(m->rx_q,1);
			continue;
		}

		/* get mux length */
		muxlen=read16_from_little_endian(mhdr->len)+1;

		/* waste if too big */
		if(muxlen>sizeof(m->usr_buf)) {
			SYSLOG(LOG_ERR,"###qmimux### too big mux message detected (muxlen=%d,max=%d)",muxlen,sizeof(m->usr_buf));
			binqueue_waste(m->rx_q,1);
			continue;
		}

		/* break if incomplete body */
		if(len<muxlen) {
			goto err;
		}

		#if 0
		/* get client id - not in use now */
		client_id=mhdr->cid;
		#endif

		// device side service type maynot be same as user-side service id.
		// look up service type using client id
		int sid = qmimux_get_serv_id(m, mhdr->stype, mhdr->cid);
		if (sid > 0) {
			*service_id = sid;
		} else {
			// if client key is not valid (eg. start-up case)
			// then return information from QMUX header
			*service_id = mhdr->stype;
		}

		*sdu_len=muxlen-sizeof(struct qmi_phys_mux_hdr);

		/* get transaction layer header - sdu mhdr */
		sdu_hdr=(char*)(mhdr+1);
		tran_hdr=(struct qmi_phys_tran_hdr*)sdu_hdr;

		/*
			convert physical QMICTL sdu packet into Qualcomm device driver sdu packet
			## physical QMICTL has 1-byte tran_id while Qualcomm device driver packet has 2-bytes tran_id ##
		*/
		if(*service_id==QMICTL) {

			/* 2 bytes QMICTL header - ctl_flags and tran_id */
			sdu_ctl_flags=*sdu_hdr;
			sdu_tran_id=*(sdu_hdr+1);

			/* convert 2 bytes SDU header to 3 bytes SDU header */
			tran_hdr=(struct qmi_phys_tran_hdr*)((sdu_hdr+2)-sizeof(*tran_hdr));
			tran_hdr->ctrl_flag=sdu_ctl_flags;
			write16_to_little_endian(sdu_tran_id,tran_hdr->tran_id);

			/* increase tran_id size difference */
			*sdu_len+=sizeof(*tran_hdr)-2;
		}

		/* return SDU */
		*sdu=tran_hdr;

		/* return byte to waste */
		byte_to_waste=muxlen;

		break;
	}


	return byte_to_waste;

err:
	return -1;
}

void qmimux_destroy(struct qmimux_t* m)
{
	/* bypass if NULL */
	if(!m)
		return;

	qmimux_close(m);

	/* destroy rx_q */
	if(m->rx_q)
		binqueue_destroy(m->rx_q);

	free(m);
}

struct qmimux_t* qmimux_create(const char* qcqmidev_name)
{
	struct qmimux_t* m;

	/* alloc. obj. */
	m=_malloc(sizeof(struct qmimux_t));
	if(!m) {
		SYSLOG(LOG_ERROR,"###qmimux### failed to allocate qmimux_t - size=%d",sizeof(struct qmimux_t));
		goto err;
	}

	/* create rx_q */
	m->rx_q=binqueue_create(QMIMUX_RX_QUEUE_SIZE);
	if(!m->rx_q) {
		SYSLOG(LOG_ERROR,"###qmimux### failed to allocate rx_q in qmimux_t");
		goto err;
	}

	/* init. members */
	m->qcqmi_dev=-1;

	/* set members */
	m->qcqmidev_name=qcqmidev_name;

	return m;
err:
	qmimux_destroy(m);
	return NULL;
}
