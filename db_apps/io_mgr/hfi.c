/*!
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 * This file contains high frequency interface code
 */

#include "xgpio.h"
#include "hfi.h"
#include "binqueue.h"
#include "dq.h"
#include "atcp.h"
#include "tick64.h"

#include "iir_lpfilter.h"

#include "commonIncludes.h"

#define IO_MGR_HFI_COL_COUNT	10

extern struct iir_lpfilter_t * lpfilters;
extern int phys_sam_freq;

void on_iio_dev(void * iio_dev,int ch_index,unsigned int* ch_samples,int sample_count,unsigned long long ms64);

/* input queue struct */
struct inq_t {
	unsigned long long tm;
	double v;
};

/* convert socket server to name */
const char* sockserver_str_name[]={
	[sockserver_output]="output",
	[sockserver_input]="input",
};

/* socket servers */
struct sockserver_t sservers[sockserver_count];

void sockserver_disconnect_client(struct sockserver_t* ss);
void enable_input_stream_mode(int en);

void ch_queue_clear()
{
	struct io_cfg_t* cfg;
	int i;

	for(i=0;i<io_cnt;i++) {
		cfg=&(io_cfg[i]);
		if(!cfg->out_vq)
			continue;

		/* clear queue */
		dq_clear(cfg->out_vq);
	}
}

void ch_queue_fini()
{
	struct io_cfg_t* cfg;
	int i;

	for(i=0;i<io_cnt;i++) {
		cfg=&(io_cfg[i]);

		/* destroy output queue per channel */
		if(cfg->out_vq)
			dq_destroy(cfg->out_vq);
		cfg->out_vq=NULL;

		/* destroy input queue per channel */
		if(cfg->in_vq)
			dq_destroy(cfg->in_vq);
		cfg->in_vq=NULL;

	}
}

int ch_queue_init()
{
	struct io_cfg_t* cfg;
	int i;

	for(i=0;i<io_cnt;i++) {
		cfg=&(io_cfg[i]);

		/* create output queue per channel */
		cfg->out_vq=dq_create(IO_MGR_HFI_COL_COUNT,sizeof(int));
		if(!cfg->out_vq) {
			DBG(LOG_ERR,"failed to create an output queue (idx=%d)",i);
			goto err;
		}

		/* create input queue per channel */
		cfg->in_vq=dq_create(IO_MGR_HFI_COL_COUNT*2+1,sizeof(struct inq_t));
		if(!cfg->in_vq) {
			DBG(LOG_ERR,"failed to create an input queue (idx=%d)",i);
			goto err;
		}

	}

	return 0;

err:
	return -1;
}

int sockserver_is_input_mode_running()
{
	struct sockserver_t* ss;
	int i;
	struct io_cfg_t* cfg;

	/* block iio when injecting mode is enabled */
	ss=&sservers[sockserver_input];

	/* if client is online */
	if(ss->client!=0)
		return 1;

	/* clear per-channel input queue */
	for(i=0;i<io_cnt;i++) {
		cfg=&(io_cfg[i]);

		if(cfg->in_vq) {
			if(!dq_is_empty(cfg->in_vq))
				return 1;
		}
	}

	/* clear session queue */
	if(ss->in_sq) {
		if(!binqueue_is_empty(ss->in_sq))
			return 1;
	}

	return 0;
}

void sockserver_collect_dead_connections()
{
	struct sockserver_t* ss;
	int i;

	char dummy;
	int stat;

	struct atcp_client_t* sc;

	for(i=0;i<COUNTOF(sservers);i++) {
		ss=&sservers[i];

		/* check if client is online */
		sc=ss->client;
		if(!sc || sc->csock<0)
			continue;

		/* check to write */
		stat=write(sc->csock,&dummy,0);
		if(stat<0) {
			sockserver_disconnect_client(ss);

			DBG(LOG_INFO,"socket %s disconnected (detected by writing)",sockserver_str_name[i]);
		}

		/* check to read */
		stat=read(sc->csock,&dummy,0);
		if(stat<0) {
			sockserver_disconnect_client(ss);

			DBG(LOG_INFO,"socket %s disconnected (detected by reading)",sockserver_str_name[i]);
		}

	}
}

void sockserver_disconnect_client(struct sockserver_t* ss)
{
	int port;
	const char* ipaddr;
	struct sockaddr_in* sa;
	struct atcp_client_t* sc;

	const char* sname;

	int last_errno;

	int i;

	/* get sockname */
	sname=sockserver_str_name[ss->idx];
	/* get last error code */
	last_errno=errno;
	/* get client */
	sc=ss->client;

	/* bypass if no client is connected */
	if(!sc) {
		DBG(LOG_ERR,"no client connected");
		return;
	}

	/* get client addr */
	sa=&sc->client_addr;
	port=ntohs(sa->sin_port);
	ipaddr=inet_ntoa(sa->sin_addr);

	/* clear output client */
	ss->client=NULL;

	if(last_errno==0) {
		/* do not print */
	}
	else if(last_errno==EPIPE) {
		/* close the client */
		DBG(LOG_INFO,"client disconnection detected (sname=%s,ip=%s,src_port=%d)",sname,ipaddr,port);
	}
	else {
		DBG(LOG_ERR,"data transfer failure detected (sname=%s,ip=%s,src_port=%d) - %s",sname,ipaddr,port,strerror(errno));
	}

	atcp_close_client(ss->t,sc);

	/* clear lowpass filters */
	if(ss->idx==sockserver_input) {
		for(i=0;i<io_cnt;i++) {
			iir_lpfilter_clear(&lpfilters[i]);
		}
	}
}

int sockserver_reinit(struct sockserver_t* ss)
{
	struct sockaddr_in* saddr;

	saddr=&ss->saddr;

	/* bypass if not ready */
	if(!ss->port_valid || !ss->ipaddr_valid) {
		DBG(LOG_DEBUG,"server setting is not ready (server=%s)",sockserver_str_name[ss->idx]);
		return -1;
	}

	/* close server */
	atcp_close_server(ss->t);

	if(!saddr->sin_port) {
		DBG(LOG_INFO,"disable(close) socket server (server=%s)",sockserver_str_name[ss->idx]);
		goto fini;
	}

	DBG(LOG_INFO,"reinit. socket server (server=%s,addr=%s,port=%d)",sockserver_str_name[ss->idx],inet_ntoa(saddr->sin_addr),saddr->sin_port);

	/* open server */
	if(atcp_open_server(ss->t,&saddr->sin_addr,saddr->sin_port,ss->idx==sockserver_output)<0) {
		DBG(LOG_ERR,"failed to open server");
		goto err;
	}

fini:
	return 0;
err:
	return -1;
}

int dump_to_high_freq_interface( int ch_index,unsigned int* ch_samples,int sample_count,unsigned long long ms64)
{
	char line[1024];

	int len;

	const struct io_info_t* io;

	int i;
	int j;

	double adc_raw;
	double adc;

	struct sockserver_t* ss;
	struct atcp_client_t* sc;

	int stat;

	/* get current io */
	io=&io_info[ch_index];

	unsigned long long ms64_s;
	unsigned long long ms64_m;

	struct io_cfg_t* cfg;

	/*
		IO name,
		IO type [ain,aout,din,dout],
		time stamp (msec since the HFI session starts),
		frequency : value, value, value, ... (max 16 entries)
	*/

	/* get sockserver */
	ss=&sservers[sockserver_output];
	sc=ss->client;
	if(!sc)
		return 0;

	/* get cfg */
	cfg=&io_cfg[ch_index];

	if(!cfg->out_vq) {
		DBG(LOG_INFO,"no vq on channel %d",ch_index);
		return 0;
	}

	/* get start-time of packet */
	ms64_s=ms64-sample_count*1000/phys_sam_freq;

	/* init. session */
	if(!ss->session_valid) {
		ss->session_valid=1;
		/* update session base time */
		ss->session_tm=ms64_s;
		/* start (clear) output queues */
		ch_queue_clear();
	}

	for(j=0;j<sample_count;j++) {
		/* get tick */
		ms64_m=ms64_s-ss->session_tm+(j*1000/phys_sam_freq);

		/* update out_vq time */
		if(dq_is_empty(cfg->out_vq) ) {
			cfg->out_tm=ms64_m;
		}

		/* push to out_vq */
		dq_push(cfg->out_vq,&ch_samples[j]);

		/* when queue is full */
		if(dq_get_free_space(cfg->out_vq)==0) {

			/* peek out_vq */
			unsigned int* vals;
			int count;
			vals=(unsigned int*)dq_peek(cfg->out_vq, &count);

			/* print prefix */
			len=0;
			len+=snprintf(line+len,sizeof(line)-len,"%s,%s,%llu:",io->io_name,"ain",cfg->out_tm);

			/* print pay-load */
			for(i=0;i<count;i++) {
				/* get adc_raw and adc */
				convert_adc_hw_to_value(ch_index,vals[i],&adc_raw,&adc);
				len+=snprintf(line+len,sizeof(line)-len,(i==0)?"%0.2f":",%0.2f",adc);
			}
			len+=snprintf(line+len,sizeof(line)-len,"\n");

			/* clear out_vq */
			dq_clear(cfg->out_vq);

			#warning [TODO] check buffer size before writing to make sure the whole single line is sent
			/* dump to HFI */
			stat=write(sc->csock,line,len);

			/* check errors */
			if(stat<0) {
				sockserver_disconnect_client(ss);
				DBG(LOG_INFO,"socket output disconnected");
				break;
			}
		}
	}

	return 0;
}


int feed_ch_queue()
{
	unsigned int adc_hw;
	int i;

	unsigned long long now;
	unsigned long long past;

	struct inq_t* dq_entity;

	struct sockserver_t* ss;

	int triggered;

	/* block iio when injecting mode is enabled */
	ss=&sservers[sockserver_input];

	/* skip if off-line */
	if(!ss->input_stream_mode)
		return 0;

	/* turn off on-line flag */
	if(!sockserver_is_input_mode_running()) {
		ss->input_stream_mode=0;
		/* turn off input stream mode */
		enable_input_stream_mode(0);

		DBG(LOG_INFO,"switching to operation mode (accepting hardware input stream)");
		return 0;
	}

	/* get current time */
	now=tick64_get_ms();

	triggered=1;
	while(triggered) {

		triggered=0;

		for(i=0;i<io_cnt;i++) {
			struct io_cfg_t* cfg = &io_cfg[i];
			int count;
			/* peek the first entity */
			dq_entity=(struct inq_t*)dq_peek(cfg->in_vq, &count);
			/* bypass if no entity */
			if(!dq_entity)
				continue;

			/* init. session */
			if(!ss->session_valid) {
				ss->session_valid=1;
				/* update session base time */
				ss->session_tm=now;
			}

			/* wait until the time past */
			past=now-ss->session_tm;
			if(past<dq_entity->tm)
				continue;

			triggered=1;

			/* convert v to hw v */
			adc_hw=convert_value_to_adc_hw(i,dq_entity->v);

			/* emulate iio push */
			on_iio_dev(NULL,i,&adc_hw,1,dq_entity->tm);

			/* waste the one */
			dq_waste(cfg->in_vq);
		}
	}

	return 0;
}

int dump_session_queue_to_ch_queue()
{
	char buf[1024+1];
	struct sockserver_t* ss;
	int len;

	char* saveptr_colon;
	char* saveptr_comma;

	char* p;
	char* p2;
	char* info;
	char* payload;
	char* crlf;

	char* io_name;
	char* io_type;
	char* msec_str;

	unsigned long long msec;

	int i;

	int fs;


	struct inq_t dq_entity;

	/* get input socketserver */
	ss=&sservers[sockserver_input];

	/* bypass if empty */
	if(binqueue_is_empty(ss->in_sq))
		return 0;

	while(1) {
		/* peek stream from queue */
		len=binqueue_peek(ss->in_sq,buf,sizeof(buf)-1);
		buf[len]=0;

		/* search crlf */
		crlf=strstr(buf,"\n");
		/* break if no crlf */
		if(!crlf)
			break;

		/* get the first line only */
		*crlf=0;

		/* get info part */
		info=strtok_r(buf,":",&saveptr_colon);
		if(!info) {
			DBG(LOG_ERR,"colon(:) not found");
			goto read_next_line;
		}

		/* parse info part */
		io_name=strtok_r(info,",",&saveptr_comma);
		io_type=strtok_r(NULL,",",&saveptr_comma);
		msec_str=strtok_r(NULL,",",&saveptr_comma);

		int idx = getIo_Index(io_name);
		if(idx < 0) {
			DBG(LOG_ERR,"missing or unknown io name detected (buf=%s)",buf);
			goto read_next_line;
		}
		struct io_cfg_t* cfg = &io_cfg[idx];

		/* give up if no enough space */
		fs=dq_get_free_space(cfg->in_vq);
		if(fs<IO_MGR_HFI_COL_COUNT) {
			DBG(LOG_DEBUG,"per-channel input buffer queue full - piping procedure suspended (io_name=%s,fs=%d)",io_name,fs);
			break;
		}

		/* check validation - io type */
		if(!io_type || strcmp(io_type,"ain")) {
			DBG(LOG_ERR,"missing or unknown io type detected (buf=%s)",buf);
			goto read_next_line;
		}

		/* check validation - msec_str */
		if(!msec_str) {
			DBG(LOG_ERR,"missing msec detected (buf=%s)",buf);
			goto read_next_line;
		}
		msec=atoll(msec_str);

		/* parse payload part */
		payload=strtok_r(NULL,":",&saveptr_colon);
		if(!payload) {
			DBG(LOG_ERR,"payload partition not found (buf=%s)",buf);
			goto read_next_line;
		}

		/* add payload to dqueue */
		i=0;
		p=strtok_r(payload,",",&saveptr_colon);
		while(p) {
			/* convert payload to double */
			dq_entity.v=strtod(p,&p2);
			if(p2==p) {
				DBG(LOG_ERR,"error found in the payload (idx=%d)",i+1);
			}

			/* get tm */
			dq_entity.tm=msec+i*1000/phys_sam_freq;

			/* push */
			dq_push(cfg->in_vq,&dq_entity);

			p=strtok_r(NULL,",",&saveptr_colon);

			i++;
		}


	read_next_line:
		#if 0
		printf("%llu\n",tick64_get_ms()-ss->session_tm);
		len=binqueue_peek(ss->in_sq,buf,crlf+1-buf);
		write(1,buf,len);
		#endif

		/* throw out the first line */
		binqueue_waste(ss->in_sq,crlf+1-buf);
	}



	return 0;
}

void sockserver_on_read(struct atcp_t* t,struct atcp_client_t* c,void* ref)
{
	char buf[1024];
	int len;
	int free_len;
	int written;

	struct sockserver_t* ss;

	/* get sockserver */
	ss=(struct sockserver_t*)ref;

	/* get free len */
	free_len=binqueue_get_free_len(ss->in_sq);
	if(!free_len) {
		DBG(LOG_DEBUG,"stream buffer full - socket read procedure suspended");
	}
	else {
		/* read socket */
		len=read(c->csock,buf,MIN(sizeof(buf),free_len));

		if(len==0) {
			sockserver_disconnect_client(ss);
			DBG(LOG_INFO,"socket input disconnected (EOF)");
			return;
		}

		/* bypass if any error */
		if(len<0) {
			sockserver_disconnect_client(ss);
			DBG(LOG_INFO,"socket input disconnected (read failure) - %s",strerror(errno));
			return;
		}

		/* buffer in in_sq */
		written=binqueue_write(ss->in_sq,buf,len);
		if(written!=len) {
			DBG(LOG_ERR,"input stream buffer queue overflow (len=%d,written=%d,free_len=%d)",len,written,free_len);
		}
	}
}

void sockserver_on_accept(struct atcp_t* t,struct atcp_client_t* c,void* ref)
{
	int port;
	const char* ipaddr;

	struct sockaddr_in* sa;
	struct sockserver_t* ss;

	struct io_cfg_t* cfg;
	int i;

	/* get sockserver */
	ss=(struct sockserver_t*)ref;

	/* get client addr */
	sa=&(c->client_addr);
	port=ntohs(sa->sin_port);
	ipaddr=inet_ntoa(sa->sin_addr);

	/* tear down the connection if any previoius connection is still alive */
	if(ss->client) {
		DBG(LOG_ERR,"client connection rejected - multiple clients not allowed (ip=%s,src_port=%d)",ipaddr,port);
		atcp_close_client(t,c);
		return;
	}

	DBG(LOG_INFO,"client connection accepted (sname=%s,ip=%s,src_port=%d)",sockserver_str_name[ss->idx],ipaddr,port);

	/* store the client socket as output client */
	ss->client=c;
	/* update session time */
	ss->session_valid=0;

	/* update online flag */
	ss->input_stream_mode=1;

	/* enable injectin mode */
	if(ss->idx==sockserver_input) {

		/* set input stream mode on */
		enable_input_stream_mode(1);

		DBG(LOG_INFO,"switching to input stream mode (ignoring hardware input stream)");

		/* clear lowpass filters */
		for(i=0;i<io_cnt;i++) {
			iir_lpfilter_clear(&lpfilters[i]);
		}
	}

	/* clear per-channel input queue */
	for(i=0;i<io_cnt;i++) {
		cfg=&(io_cfg[i]);

		if(cfg->in_vq)
			dq_clear(cfg->in_vq);
	}

	/* clear session queue */
	if(ss->in_sq)
		binqueue_clear(ss->in_sq);
}
