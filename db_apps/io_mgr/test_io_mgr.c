/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
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
 */

#include "commonIncludes.h"
#include "cdcs_shellfs.h"
#include "iio.h"
#include "iir_lpfilter.h"
#include "atcp.h"

/*
	test shellfs
*/

int test_shellfs() 
{
	struct shellfs_t* sfs;
	struct shellfs_dirent_t* de;

	int dir;
	int file;
	int symlink;

	const char* msg;

	/* create */
	sfs=shellfs_create();
	if(!sfs) {
		DBG(LOG_ERR,"failed in creating shellfs object");
		goto err;
	}

	/* select directory */
	if(shellfs_set_base_dir(sfs,"./shellfs_dir")<0) {
		DBG(LOG_ERR,"failed in settting base directory");
		goto err;
	}

	/* init scan */
	if(shellfs_scandir_init(sfs,"")<0) {
		DBG(LOG_ERR,"failed in opening directory");
		goto err;
	}

	dir=0;
	file=0;
	symlink=0;

	/* get first */
	de=shellfs_scandir_get_first(sfs);

	while(de) {

		DBG(LOG_DEBUG,"file => %s",de->name);

		if(!strcmp(de->name,"dir")) {
			if(!S_ISDIR(de->mode)) {
				DBG(LOG_ERR,"failed to indentify a directory");
				goto err;
			}

			dir++;
		}
		else if(!strcmp(de->name,"file")) {
			if(!S_ISREG(de->mode)) {
				DBG(LOG_ERR,"failed to indentify a file");
				goto err;
			}

			file++;
		}
		else if(!strcmp(de->name,"symlink")) {
			if(!S_ISLNK(de->mode)) {
				DBG(LOG_ERR,"failed to indentify a symlink");
				goto err;
			}

			symlink++;
		}

		/* get next */
		de=shellfs_scandir_get_next(sfs);
	}

	/* check file count */
	if(file!=1) {
		DBG(LOG_ERR,"file count not matching");
		goto err;
	}

	/* check file count */
	if(dir!=1) {
		DBG(LOG_ERR,"dir count not matching");
		goto err;
	}

	/* check file count */
	if(symlink!=1) {
		DBG(LOG_ERR,"symlink count not matching");
		goto err;
	}


	/* fini scan */
	shellfs_scandir_fini(sfs);

	/* echo */
	#define MSG	"I need C++"
	if(shellfs_echo(sfs,"file","I need c++")<0) {
		DBG(LOG_ERR,"failed in echoing to file");
		goto err;
	}

	/* cat */
	msg=shellfs_cat(sfs,"file");
	if(!msg) {
		DBG(LOG_ERR,"failed in catting from file");
		goto err;
	}

	/* check msg */
	if(strcmp(msg,MSG)) {
		DBG(LOG_ERR,"cat and echo not matching");
		goto err;	
	}

	/* cat a file not existing */
	if(shellfs_cat(sfs,"not_existing_file")) {
		DBG(LOG_ERR,"failed to check errors in catting a file not existing");
		goto err;
	}

	/* echo to a file not existing */
	if(shellfs_echo(sfs,"not_existing_file",MSG)>=0) {
		DBG(LOG_ERR,"failed to check errors in echoing to a file not existing");
		goto err;
	}

	/* destroy */
	shellfs_destroy(sfs);

	return 0;

err:
	return -1;	
}

/*
	test iio
*/

const char* ch_names[]={
	"voltage0",
	"voltage2",
	"voltage3",
	"voltage4",
	"voltage1",
	"voltage5",
	"voltage6",
	NULL
};
	
void proc_func_on_iio_dev(struct iio_device_t* iio_dev,int ch_index,unsigned int* ch_samples,int sample_count)
{
	time_t now;
	int i;
	
	static unsigned long long j=0;
	
	j++;
	
	time(&now);
	
	for(i=0;i<sample_count;i++) {
		printf("j=%08llu, ch=%02d i=%08d, size=%08d ch=%s, v=%d - %s",j,ch_index,i,sample_count,ch_names[ch_index],ch_samples[i],ctime(&now));
	}
}

static int running=0;

void sig_handler(int signo)
{
	DBG(LOG_NOTICE,"punk! (signo=%d)",signo);
	
	if(signo==SIGTERM || signo==SIGINT) 	
		running=0;
}

int test_iio() 
{
	
	struct iio_t* iio;
	
	int count;
	struct iio_device_t* iio_dev;
	int h;
	int stat;
	
	/* create iio */
	iio=iio_create();
	if(!iio) {
		DBG(LOG_ERR,"failed to create iio");
		goto err;
	}
	
	/* get iio device count */
	count=iio_get_device_count(iio);
	DBG(LOG_NOTICE,"iio device count = %d",count);
	if(!count) {
		DBG(LOG_ERR,"no iio device found");
		goto err;
	}
    
	/* get first iio device */
	iio_dev=iio_get_device(iio,0);
	if(!iio_dev) {
		DBG(LOG_ERR,"iio device not found");
		goto err;
	}
	
	/* init buffering */
	iio_device_init_buffering(iio_dev,ch_names,COUNTOF(ch_names));
	
	/* setup device */
	iio_device_set_config_integer(iio_dev,"configs/sampling_freq",100);
	iio_device_set_ch_config_integer(iio_dev,"voltage1","hardwaregain",0);
	
	/* start buffering */
	iio_device_start_buffering(iio_dev,proc_func_on_iio_dev);
	
	/* hook signals */
	signal(SIGINT,sig_handler);
	signal(SIGTERM,sig_handler);
	
	/* get handle */
	h=iio_device_get_buffer_handle(iio_dev);
	
	fd_set readfds;
	struct timeval tv;
			
	/* process */
	running=1;
	while(running) {
		FD_ZERO(&readfds);
		FD_SET(h,&readfds);
		
		tv.tv_sec=1;
		tv.tv_usec=0;
		
		/* select */
		stat=select(h+1,&readfds,NULL,NULL,NULL);
		if(stat<0)
			break;
		
		if(stat==0) {
			printf("timeout\n");
		}
		/* process */
		else if(FD_ISSET(h,&readfds)) {
			iio_device_proc_buffering(iio_dev);
		}
	}
	
	/* stop buffering */
	iio_device_stop_buffering(iio_dev);
	
	iio_device_fini_buffering(iio_dev);
	
	iio_destroy(iio);
	
	return 0;
err:
	return -1;	
}

int test_lowpassfilter()
{
	int input_wave[1000*60];
	int output_wave[1000*60];
	int i;
	int j;
	struct tms s;
	struct tms e;
	clock_t ticks_per_sec;
	
	void* lpf[8];
			
	DBG(LOG_INFO,"testing low pass filter");
	
	/* build 10 seconds 10 Hz noise */
	for(i=0;i<COUNTOF(input_wave);i++) {
		input_wave[i]=1<<12;
		
		if(i%2) 
			input_wave[i]=1<<12;
		else
			input_wave[i]=0;
	}

	/* create lowpassfilter */
	for(i=0;i<COUNTOF(lpf);i++) {
		lpf[i]=iir_lpfilter_create();
		
		if(!lpf[i]) {
			DBG(LOG_ERR,"failed to create lpf object");
			goto err;
		}
		
		/* setup filter */
		//iir_lpfilter_setup(lpf[i],20,5);
	}
		
	times(&s);
	
	/* apply filter */
	for(j=0;j<COUNTOF(lpf);j++) {
		for(i=0;i<COUNTOF(input_wave);i++) {
			output_wave[i]=iir_lpfilter_filter(lpf[j],input_wave[i]);
		}
	}
	
	times(&e);
	
	/* destroy lowpassfilter */
	for(i=0;i<COUNTOF(lpf);i++) {
		iir_lpfilter_destroy(lpf[i]);
	}
	
	/* print output */
	for(i=0;i<COUNTOF(input_wave);i++) {
		printf("wavef %d : %04d ==> %04d\n",i,input_wave[i],output_wave[i]);
	}
	
	ticks_per_sec=sysconf(_SC_CLK_TCK); 
	clock_t tick_period;
	double sam_per_sec;
	
	tick_period=e.tms_utime-s.tms_utime;
	sam_per_sec=COUNTOF(input_wave)*COUNTOF(lpf)*ticks_per_sec/(tick_period);
	
	printf("ticks = %ld, samples per sec = %0.2f\n",tick_period,sam_per_sec);
	
	return 0;
err:
	return -1;	
}

struct atcp_client_t* client=NULL;

void t_on_accept(struct atcp_t* t,struct atcp_client_t* c)
{
	int port;
	const char* ipaddr;
	
	struct sockaddr_in* sa;
	
	/* get client addr */
	sa=&c->client_addr;
	port=ntohs(sa->sin_port);
	ipaddr=inet_ntoa(sa->sin_addr);
	
	DBG(LOG_INFO,"client connection requested (ip=%s,src_port=%d)",ipaddr,port);
	
	/* close - allow a single client */
	if(client) {
		DBG(LOG_ERR,"multiple connections not allowed. disconnect client (ip=%s,src_port=%d)",ipaddr,port);
		atcp_close_client(t,c);
		return;
	}
		
	DBG(LOG_INFO,"client connected");
	
	client=c;
}

void t_on_read(struct atcp_t* t,struct atcp_client_t* c)
{
}

void sig_handler2(int signo)
{
	DBG(LOG_ERR,"punk! (signo=%d)",signo);
}

int test_atcp()
{
	struct atcp_t* t;
	int max_fd;
	int stat;
	
	struct timeval tv;
	
	int i;
	char msg[256];
	int len;
	
	fd_set readfds;
	
	signal(SIGPIPE,sig_handler2);
	
	/* create a tcp object */
	t=atcp_create();
	if(!t) {
		DBG(LOG_ERR,"failed to create an object of atcp");
		goto err;
	}
	
	/* set up callbacks */
	atcp_set_callbacks(t,t_on_accept,t_on_read);
	
	/* open server */
	if(atcp_open_server(t,INADDR_ANY,10000,1)<0) {
		DBG(LOG_ERR,"failed to open server");
		goto err;
	}
	
	i=0;
	while(1) {
		FD_ZERO(&readfds);
		
		/* set fds */
		max_fd=atcp_set_fds(t,&readfds);
		if(max_fd<0) {
			DBG(LOG_ERR,"failed in atcp_set_fds()");
		}
		
		tv.tv_sec=1;
		tv.tv_usec=0;
		
		/* select */
		stat=select(max_fd+1,&readfds,NULL,NULL,&tv);
		if(stat<0) {
			DBG(LOG_ERR,"punk! - %s",strerror(errno));
			break;
		}
		
		/* process tcp */
		atcp_do_process(t,&readfds);
		
		if(client) {
			i++;
			
			len=snprintf(msg,sizeof(msg),"are you dead yet? %d\n",i);
			
			if(write(client->csock,msg,len)<0) {
				
				if(errno==EPIPE) {
					int port;
					const char* ipaddr;
	
					struct sockaddr_in* sa;
	
					/* get client addr */
					sa=&client->client_addr;
					port=ntohs(sa->sin_port);
					ipaddr=inet_ntoa(sa->sin_addr);
	
					DBG(LOG_INFO,"disconnection detected (ip=%s,src_port=%d)",ipaddr,port);
					atcp_close_client(t,client);
					client=NULL;
				}
				else {
					DBG(LOG_ERR,"failed in write() - %s",strerror(errno));
					break;
				}
			}
		}
		
#if 0
		{
			char buf[16];
			
			if(read(client->csock,buf,0)<0) {
				DBG(LOG_ERR,"failed in read() - %s",strerror(errno));
				break;
			}
		}
#endif							    
	}
	
	DBG(LOG_ERR,"closing");
	
	/* close server */
	atcp_close_server(t);
	
	/* destroy tcp object */
	atcp_destroy(t);
	
	return 0;
	
err:	
	return -1;
}

int main(int argc,char* argv[])
{
	int stat_shellfs;
	int stat_iio;
	int stat_lpf;
	int stat_atcp;
	
	stat_atcp=test_atcp();
	exit(-1);
	
	stat_lpf=test_lowpassfilter();
	
	stat_shellfs=test_shellfs();
	stat_iio=test_iio();
	

	return stat_shellfs || stat_iio || stat_lpf;
}
