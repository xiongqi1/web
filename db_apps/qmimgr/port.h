#ifndef __PORT_H__
#define __PORT_H__

#include <termios.h>

#include "minilib.h"

#include "binqueue.h"
#include "strqueue.h"

#define PORT_READ_BUF 4096

// result + CR + LF
#define PORT_MAX_RECV_LINE_LEN	(STRQUEUE_MAX_STR_LEN+1+1)
#define PORT_MAX_SEND_LINE_LEN  (STRQUEUE_MAX_STR_LEN+1)

#define PORT_READ_BLOCK	512
#define PORT_WRITE_BLOCK 512

struct port_t;

typedef void (*port_callback)(struct port_t* port,unsigned short tran_id,struct strqueue_t* resultq,int timeout);
typedef void (*port_callback_noti)(struct port_t* port,const char* noti);

struct port_t {
	int fd;

	int oldtio_valid;
	struct termios oldtio;

	struct binqueue_t* wq;	// write buffer directly connected to port
	struct binqueue_t* rq;  // read buffer directly connected from port

	struct strqueue_t* wlineq; // queue of AT commands to wq
	struct strqueue_t* rlineq; // queue of AT commands from rq

	struct strqueue_t* notiq; // queue of AT commands from rq

	struct strqueue_t* immeidateq;

	fd_set* readfds;
	fd_set* writefds;

	unsigned short cur_tran_id;

	unsigned short tran_id;

	int start_time;

	port_callback port_on_recv;
	port_callback_noti port_on_noti;
};


int port_write(struct port_t* port,void* buf,int buf_len);
int port_read(struct port_t* port,void* buf,int buf_len);
int port_open(struct port_t* port,const char* dev);
void port_close(struct port_t* port);
struct port_t* port_create();
void port_destroy(struct port_t* port);

int port_process_select(struct port_t* port,unsigned short tran_id,struct strqueue_t** ret_q);
int port_setfds(struct port_t* port, int* maxfd, fd_set* readfds, fd_set* writefds);
void port_register_callbacks(struct port_t* port, port_callback_noti port_on_noti, port_callback port_on_recv);

int port_queue_command(struct port_t* port,const char* cmd,int timeout,unsigned short* tran_id);

int port_is_open(struct port_t* port);

#endif
