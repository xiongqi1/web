
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <syslog.h>
#include <string.h>
#include <errno.h>


#include "port.h"

int port_is_open(struct port_t* port)
{
	return port->fd>=0;
}

void port_register_callbacks(struct port_t* port, port_callback_noti port_on_noti, port_callback port_on_recv)
{
	port->port_on_noti=port_on_noti;
	port->port_on_recv=port_on_recv;
}

int port_write(struct port_t* port,void* buf,int buf_len)
{
	return write(port->fd,buf,buf_len);
}

int port_read(struct port_t* port,void* buf,int buf_len)
{
	return read(port->fd,buf,buf_len);
}

static int port_load_wlineq_to_wq(struct port_t* port)
{
	int cmd_len;
	int written;
	char atcmd[PORT_MAX_SEND_LINE_LEN+1];

	struct strqueue_element_t* el;

	// get cmd from buf
	el=strqueue_walk_first(port->wlineq);
	if(!el) {
		SYSLOG(LOG_ERROR,"internal error - broken wlineq");
		goto err;
	}

	SYSLOG(LOG_COMM,"sending command(%d) - %s",el->tran_id,el->str);

	snprintf(atcmd,sizeof(atcmd),"%s\r",el->str);

	// do first flush
	cmd_len=strlen(atcmd);
	written=binqueue_write(port->wq,atcmd,cmd_len);
	if(written!=cmd_len) {
		SYSLOG(LOG_ERROR,"failed to write from line q to binq");
		goto err;
	}

	// get start time
	port->start_time=_get_current_sec();

	return 0;

err:
	return -1;
}

int port_queue_command(struct port_t* port,const char* cmd,int timeout,unsigned short* tran_id)
{
	int first;

	struct strqueue_element_t el;

	SYSLOG(LOG_DEBUG,"start");

	if(tran_id)
		*tran_id=0;

	// increase transaction id
	port->tran_id++;
	if(!port->tran_id)
		port->tran_id++;

	first=strqueue_is_empty(port->wlineq);

	// build a new el
	el.str=(char*)cmd;
	el.tran_id=port->tran_id;
	el.timeout=timeout;

	// add
	SYSLOG(LOG_DEBUG,"add to wlineq");
	if(!strqueue_add(port->wlineq,&el)) {
		SYSLOG(LOG_ERROR,"failed to add wlineq - cmd=%s",cmd);
		goto err;
	}

	SYSLOG(LOG_COMM,"command queued - %s(%d)",cmd,port->tran_id);

	// bypass if in process
	if(!first) {
		SYSLOG(LOG_DEBUG,"exit after queuening");
		return 0;
	}

	SYSLOG(LOG_DEBUG,"trigger to write");

	if(port_load_wlineq_to_wq(port)<0) {
		SYSLOG(LOG_ERROR,"failed to load command");
		goto err;
	}

	SYSLOG(LOG_DEBUG,"done");

	if(tran_id)
		*tran_id=port->tran_id;

	return 0;

err:
	return -1;
}

int port_flush_wq(struct port_t* port)
{
	char buf[PORT_WRITE_BLOCK];
	int len;
	int written;

	// bypass if nothing in wq
	if(binqueue_is_empty(port->wq))
		goto fini;

	// flush
	while( (len=binqueue_peek(port->wq,buf,sizeof(buf)))>0 ) {

		//_dump("at write port",buf,len);

		written=port_write(port,buf,len);

		if(written==-EAGAIN) {
			SYSLOG(LOG_COMM,"failed in port_write() - EAGAIN");
			break;
		}
		else if(written==0) {
			SYSLOG(LOG_COMM,"zero-byte written");
			break;
		}
		else if(written<0) {
			SYSLOG(LOG_ERROR,"failed in port_write() - %d",written);
			goto err;
		}

		binqueue_waste(port->wq,written);
	}

	// if not complete
	if(!binqueue_is_empty(port->wq)) {
		SYSLOG(LOG_DEBUG,"wq not empty yet");
		goto fini;
	}
	else {
		SYSLOG(LOG_DEBUG,"wq empty");
	}


	return 1;

fini:
	return 0;

err:
	return -1;
}

int port_charge_rq(struct port_t* port)
{
	char buf[PORT_READ_BLOCK];
	int len;
	int written;

	while( (len=port_read(port,buf,sizeof(buf)))>0 ) {

		//_dump("at read port",buf,len);

		written=binqueue_write(port->rq,buf,len);
		SYSLOG(LOG_DEBUG,"write to rq - %d",written);

		if(written<len) {
			SYSLOG(LOG_ERROR,"write buffer overflow - len=%d,written=%d",len,written);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

/*
	port_setfds(...);

	while(1) {
		select(...);

		port_queue_command("AT");

		port_check(...);
	}
*/

static int __is_noti(const char* result)
{
	const char* noti_strs[]={"RING"};
	int i;

	for(i=0;i<__countof(noti_strs);i++) {
		if(!strcmp(noti_strs[i],result))
			return 1;
	}

	return 0;
}

int __is_end_of_resp(const char* result)
{
	const char* result_end_strs[]={ "OK", "ERROR" };
	int i;

	for(i=0;i<__countof(result_end_strs);i++) {
		if(!strcmp(result_end_strs[i],result))
			return 1;
	}

	return 0;
}

int port_charge_rlineq(struct port_t* port)
{
	char buf[PORT_MAX_RECV_LINE_LEN+1];
	char cmd[PORT_MAX_SEND_LINE_LEN+1];
	int cmd_len;

	int buf_len;

	int read;

	const char* queued;

	struct strqueue_element_t* el;
	struct strqueue_element_t new_el;

	// build rlineq
	buf_len=binqueue_peek(port->rq,buf,PORT_MAX_RECV_LINE_LEN); // result + CR + LF
	if(buf_len<0) {
		SYSLOG(LOG_ERROR,"internal error - binqueue broken");
		goto err;
	}
	if(!buf_len) {
		SYSLOG(LOG_DEBUG,"no data received");
		goto err;
	}

	buf[buf_len]=0;

	// compare to current transaction
	el=strqueue_walk_first(port->wlineq);
	if(el) {
		SYSLOG(LOG_DEBUG,"current transaction = %s",el->str);

		snprintf(cmd,sizeof(cmd),"%s\r",el->str);
		cmd_len=strlen(cmd);

		if(buf_len>=cmd_len && !strncmp(buf,cmd,cmd_len) ) {
			port->cur_tran_id=el->tran_id;
			SYSLOG(LOG_DEBUG,"command echo received - %s",cmd);
		}
	}


	char* eols[]={ "\r\r\n", "\r\n" };
	int i;
	char* eol;
	char* str=buf;

	// search EOL
	eol=NULL;
	for(i=0;i<__countof(eols);i++) {
		str=strstr(buf,eols[i]);
		if(str) {
			*str=0;
			eol=eols[i];
			break;
		}
	}

	if(!eol) {
		if(buf_len<PORT_MAX_RECV_LINE_LEN)
			goto err;

		read=PORT_MAX_RECV_LINE_LEN;
		SYSLOG(LOG_COMM,"too long line detected - single line without CRLF (line length=%d,tran_id=%d)",buf_len,port->cur_tran_id);
	}
	else {
		read=(int)(str-buf)+strlen(eol);
		SYSLOG(LOG_COMM,"recieving result(%d) - %s",port->cur_tran_id,buf);
	}

	// waste
	SYSLOG(LOG_DEBUG,"removed from rq - read=%d",read);
	binqueue_waste(port->rq,read);

	SYSLOG(LOG_DEBUG,"received - tran_id=%d",port->cur_tran_id);


	// build el
	memset(&new_el,sizeof(new_el),0);
	new_el.tran_id=port->cur_tran_id;
	new_el.str=buf;

	if(port->cur_tran_id && !__is_noti(new_el.str)) {
		// add to line queue
		queued=strqueue_add(port->rlineq,&new_el);
		if(!queued) {
			SYSLOG(LOG_ERROR,"failed to add a new line - len=%d (wasting?)",read);
			goto err;
		}

		if(__is_end_of_resp(buf)) {
			SYSLOG(LOG_DEBUG,"end of command received - %s",buf);

			port->cur_tran_id=0;
			return 1;
		}
	}
	else {
		// add to line queue
		queued=strqueue_add(port->notiq,&new_el);
		if(!queued) {
			SYSLOG(LOG_ERROR,"failed to add a new line - len=%d (wasting?)",read);
			goto err;
		}
	}

	return 0;

err:
	return -1;
}


/*

	ret < 0  : error
	ret == 0 : normal
	ret == 1 : tran_id received
	ret == 2 : tran_id timeout
*/
int port_process_select(struct port_t* port,unsigned short tran_id,struct strqueue_t** ret_q)
{
	int stat;
	int cur;

	int tran_done;
	int timeout;

	unsigned short result_tran_id;

	struct strqueue_element_t* el;
	struct strqueue_t* resultq;

	int result_lines;

	char noti[STRQUEUE_MAX_STR_LEN];

	if(port->fd<0) {
		SYSLOG(LOG_DEBUG,"at port not opened");
		return 0;
	}

	cur=_get_current_sec();

	// collect the memory back
	strqueue_eat(port->rlineq,port->immeidateq);

	// process read
	if(FD_ISSET(port->fd,port->readfds)) {

		SYSLOG(LOG_DEBUG,"got read event");

		// fill up the rq
		if( port_charge_rq(port)<0 )
			goto err;
	}

	// process write
	if(FD_ISSET(port->fd,port->writefds)) {
		SYSLOG(LOG_DEBUG,"got write event");

		stat=port_flush_wq(port);

		// if err
		if(stat<0)
			goto err;
	}

	// process rq
	tran_done=timeout=0;
	while(1) {
		// fill up rlineq
		stat=port_charge_rlineq(port);

		// run if any noti
		el=strqueue_walk_first(port->notiq);
		if(el) {
			strcpy(noti,el->str);

			strqueue_remove(port->notiq,1);

			SYSLOG(LOG_DEBUG,"noti found - %s",noti);

			if(port->port_on_noti) {
				port->port_on_noti(port,noti);
			}

			el=strqueue_walk_first(port->notiq);
		}

		// break if done
		tran_done=stat==1;
		if(tran_done) {
			SYSLOG(LOG_DEBUG,"got transaction result");
			break;
		}

		// check timeout
		el=strqueue_walk_first(port->wlineq);
		timeout=el && (cur-port->start_time>el->timeout);

		// break if timeout
		if(timeout) {
			SYSLOG(LOG_ERROR,"transaction timeout(%d)",el->tran_id);

			port->cur_tran_id=0;
			break;
		}

		if(stat<0)
			goto fini;
	}

	// get information of current transaction
	el=strqueue_walk_first(port->wlineq);
	if(el) {
		result_tran_id=el->tran_id;
		SYSLOG(LOG_DEBUG,"current tran_id=%d",result_tran_id);
	}
	else {
		result_tran_id=0;
		SYSLOG(LOG_DEBUG,"guess notification? - no transaction found");
	}

	// clear current transaction
	if( strqueue_remove(port->wlineq,1) != 1 ) {
		SYSLOG(LOG_ERROR,"internal error - queue broken");
		goto err;
	}

	// load next transaction
	el=strqueue_walk_first(port->wlineq);
	if(el) {
		SYSLOG(LOG_DEBUG,"load the next wlineq to wq");
		// load the next command
		port_load_wlineq_to_wq(port);
	}
	else {
		SYSLOG(LOG_DEBUG,"no next command found");
	}

	if(tran_id && (tran_id==result_tran_id) && ret_q) {
		SYSLOG(LOG_DEBUG,"before vomitting");

		*ret_q=port->immeidateq;
		result_lines=strqueue_vomit(port->rlineq,port->immeidateq);

		SYSLOG(LOG_DEBUG,"after vomitting");

		SYSLOG(LOG_DEBUG,"vomited to parameter - %d",result_lines);
	}
	else {
		// notify
		resultq=strqueue_create();
		if(!resultq) {
			SYSLOG(LOG_DEBUG,"failed to create resultq");
			goto err;
		}

		result_lines=strqueue_vomit(port->rlineq,resultq);

		SYSLOG(LOG_DEBUG,"vomited to local variable - %d",result_lines);

		if(port->port_on_recv) {
			port->port_on_recv(port,result_tran_id,resultq,timeout);
		}

		strqueue_eat(port->rlineq,resultq);
		strqueue_destroy(resultq);
	}


	if(tran_id==result_tran_id) {

		if(timeout)
			return 2;
		else
			return 1;
	}

fini:
	return 0;

err:
	return -1;
}

int port_setfds(struct port_t* port, int* maxfd, fd_set* readfds, fd_set* writefds)
{
	if(port->fd<0) {
		SYSLOG(LOG_COMM,"port not open");
		goto err;
	}

	if(maxfd) {
		if(*maxfd<port->fd)
			*maxfd=port->fd;

		// set write fds
		if(!binqueue_is_empty(port->wq))
			FD_SET(port->fd,writefds);
		// set read fds
		FD_SET(port->fd,readfds);
	}

	port->readfds=readfds;
	port->writefds=writefds;

	return 0;

err:
	return -1;
}

void port_close(struct port_t* port)
{

	if(port->fd<0)
		return;

	// restore termio attribte
	if(port->oldtio_valid) {
		if (tcsetattr(port->fd, TCSAFLUSH, &port->oldtio) < 0)
			SYSLOG(LOG_ERROR,"tcsetattr failed - %s",strerror(errno));
		port->oldtio_valid=0;
	}

	close(port->fd);
	port->fd=-1;

	binqueue_destroy(port->rq);
	binqueue_destroy(port->wq);

	strqueue_destroy(port->notiq);
	strqueue_destroy(port->immeidateq);

	strqueue_destroy(port->wlineq);
	strqueue_destroy(port->rlineq);

	port->notiq=NULL;
	port->rq=NULL;
	port->wq=NULL;
	port->wlineq=NULL;
	port->rlineq=NULL;
	port->immeidateq=NULL;
}

int port_open(struct port_t* port,const char* dev)
{
	struct termios newtio;

	// open
	port->fd=open(dev,O_RDWR | O_NONBLOCK | O_NOCTTY);
	if(port->fd<0) {
		SYSLOG(LOG_ERROR,"open failed - %s\n",strerror(errno));
		goto err;
	}

	// set attribute
	if (tcgetattr(port->fd, &port->oldtio) < 0) {
		SYSLOG(LOG_ERROR,"tcgetattr failed - %s",strerror(errno));
		goto err;
	}
	port->oldtio_valid=1;

	memcpy(&newtio, &port->oldtio, sizeof(struct termios));
	cfmakeraw(&newtio);
	newtio.c_cc[VMIN] = 0;
	newtio.c_cc[VTIME] = 0;

	if (tcsetattr(port->fd, TCSAFLUSH, &newtio) < 0) {
		SYSLOG(LOG_ERROR,"tcsetattr failed - %s",strerror(errno));
		goto err;
	}

	// create write buffer
	port->wq=binqueue_create(PORT_MAX_SEND_LINE_LEN); // at command + CR
	if(!port->wq) {
		SYSLOG(LOG_ERROR,"failed to create wq");
		goto err;
	}

	// create read buffer
	port->rq=binqueue_create(PORT_READ_BUF);
	if(!port->wq) {
		SYSLOG(LOG_ERROR,"failed to create rq");
		goto err;
	}

	// create rlineq buffer
	port->rlineq=strqueue_create();
	if(!port->rlineq) {
		SYSLOG(LOG_ERROR,"failed to alloc rlineq");
		goto err;
	}

	// create wlineq buffer
	port->wlineq=strqueue_create();
	if(!port->wlineq) {
		SYSLOG(LOG_ERROR,"failed to alloc wlineq");
		goto err;
	}

	// create noti buffer
	port->notiq=strqueue_create();
	if(!port->notiq) {
		SYSLOG(LOG_ERROR,"failed to alloc notiq");
		goto err;
	}

	// create noti buffer
	port->immeidateq=strqueue_create();
	if(!port->immeidateq) {
		SYSLOG(LOG_ERROR,"failed to alloc immeidateq");
		goto err;
	}

	return 0;

err:
	return -1;
}

struct port_t* port_create()
{
	struct port_t* port;

	// create the object
	port=(struct port_t*)_malloc(sizeof(struct port_t));
	if(!port) {
		SYSLOG(LOG_ERROR,"failed to allocate port_t - size=%d",sizeof(struct port_t));
		goto err;
	}

	port->fd=-1;

	return port;

err:
	port_destroy(port);
	return NULL;
}

void port_destroy(struct port_t* port)
{
	if(!port)
		return;

	port_close(port);


	_free(port);
}

