#include <stdio.h>

#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/un.h>
#include <signal.h> 
#include <unistd.h>
#include <sys/socket.h> 
#include <sys/types.h> 
#include <sys/wait.h> 
#include <sys/times.h>

#include "queue_buffer.h"
#include "sock_list.h"

#define CGIPIPE_BUF		(64*1024)
#define GCIPIPE_DSOCK_BUF	(64*1024)
#define CGIPIPE_RW_BLOCK_BUF_LEN	(4*1024)

// delay - 5 seconds when data in stdout exists
#define CGIPIPE_TERM_TIMEOUT		60
#define CGIPIPE_MAX_CLIENT_NUMBER	64

#define MAX(a,b) ( ((a)>(b))?(a):(b) )
#define MIN(a,b) ( ((a)>(b))?(b):(a) )

clock_t tick_per_sec;

const char* pipe_names[]={"pipe-stdin","pipe-stdout","pipe-ctrl"};

void print_usage(FILE* fp)
{
	fprintf(fp,"Usage :cgipipe [functions] [arguments] \n");
	fprintf(fp,"       functions:\n");
	fprintf(fp,"\t\tcgipipe_read  pipeid\n");
	fprintf(fp,"\t\tcgipipe_write pipeid\n");
	fprintf(fp,"\t\tcgipipe_check  pipeid\n");
	fprintf(fp,"\t\tcgipipe_server pipeid\n");
	fprintf(fp,"\t\tcgipipe_ctrl  pipeid\n");
	fprintf(fp," ");
}

int build_socket_name(int id,const char* name, char* buf,int buf_len)
{
	return snprintf(buf,buf_len,"/tmp/cgipipe-%s-%d",name,id);
}

int open_unix_domain_client_socket(int id,const char* name)
{
	struct sockaddr_un addr;
	int fd=-1;

	// open socket
	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0))<0 ) {
		fprintf(stderr,"socket open failure - %s\n",strerror(errno));
		goto err;
	}

	// build address
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	build_socket_name(id,name,addr.sun_path,sizeof(addr.sun_path));

	// connect
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr))<0) {
		fprintf(stderr,"socket connect failure - %s\n",strerror(errno));
		goto err;
	}

	return fd;
	
err:	
	if(fd>=0)
		close(fd);
	
	return -1;
}

int delete_unix_domain_server_socket(int id,const char* name)
{
	struct sockaddr_un addr;
	
	memset(&addr, 0, sizeof(addr));
	build_socket_name(id,name,addr.sun_path,sizeof(addr.sun_path));
	
	// unlink any previous name
	return unlink(addr.sun_path);
}

int open_unix_domain_server_socket(int id,const char* name)
{
	struct sockaddr_un addr;
	int fd=-1;
	
	// open socket
	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0))<0 ) {
		fprintf(stderr,"socket open failure - %s\n",strerror(errno));
		goto err;
	}

	// build address
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	build_socket_name(id,name,addr.sun_path,sizeof(addr.sun_path));
	
	// bind
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		fprintf(stderr,"socket bind failure - %s\n",strerror(errno));
		goto err;
	}
	
	// listen
	if (listen(fd, 5) == -1) {
		fprintf(stderr,"socket listen failure - %s\n",strerror(errno));
		goto err;
	}

	return fd;

err:
	if(fd>=0)
		close(fd);
	
	return -1;
}



int create_pipe(int fds[2])
{
	fds[0]=-1;
	fds[1]=-1;
	
	return pipe(fds);
}

void delete_pipe(int fds[2])
{
	if(fds[0]>=0)
		close(fds[0]);
	
	if(fds[1]>=0)
		close(fds[1]);
	
	fds[0]=-1;
	fds[1]=-1;
}

int appterm=0;

void sig_handler(int sig_no)
{
	switch(sig_no) {
		case SIGTERM:
			fprintf(stderr,"SIGTERM caught!\n");
			appterm=1;
			break;
			
		case SIGPIPE:
			fprintf(stderr,"SIGPIPE caught!\n");
			break;
			
		case SIGCHLD:
			fprintf(stderr,"SIGCHLD caught!\n");
			break;
			
		default:
			fprintf(stderr,"signal caught - %d\n",sig_no);
			break;
	}
}

int close_down_socket_if_broken_pipe(int* sock)
{
	if(errno==EPIPE) {
		// close down the socket
		if(*sock>=0)
			close(*sock);
		*sock=-1;
		
		return 0;
	}
	
	return -1;
}


int cgppipe_server_main(int argc,char* argv[])
{
	int pipe_stdin[2]={-1,-1};
	int pipe_stdout[2]={-1,-1};
	
	int process_stdio[2];

	int stat=-1;
	
	int pipe_id;
	
	const char* program;
	char** program_params;
	
	int server_socket[3]={-1,-1,-1};
	
	int client_stdio[3]={-1,-1,-1};
	
	char block_buf[CGIPIPE_RW_BLOCK_BUF_LEN];
	int read_len;
	int write_len;
	int queue_free;
	pid_t pid;
	
	int i;
	int j;
	
	int max_fd;
	
	struct queue_t process_queue[3];
	
	fd_set rfds;
	fd_set wfds;
	
	int child_stat;
	
	int child_term;
	clock_t child_term_clock;
	
	int written_to_client;
	int read_from_client;
	int read_from_ctrl_client;
	
	struct timeval tv;
	int sel;
	
	char line_buf[1024];
	int waste;
	char ch;
	
	struct tms tmsbuf;
		
		
	// check parameter validation
	if(argc<2) {
		print_usage(stderr);
		return -1;
	}
	
	
	pipe_id=atoi(argv[0]);
	
	
	signal(SIGTERM,sig_handler);
	signal(SIGPIPE,sig_handler);
	signal(SIGCHLD,sig_handler);
	
	// create pipe_stdin and pipe_stdout pipes
	if( (create_pipe(pipe_stdin)<0) || (create_pipe(pipe_stdout)<0) ) {
		fprintf(stderr,"pipe creation failure - %s\n",strerror(errno));
		goto fini;
	}
	
	// get process pipe_stdin and pipe_stdout
	process_stdio[0]=pipe_stdout[0];
	process_stdio[1]=pipe_stdin[1];
	
	child_term=0;
	
	// get pipe id
	pid = vfork();
	
	if(pid<0) {
		fprintf(stderr,"cannot fork a new process - %s\n",strerror(errno));
		goto fini;
	}
	else if(pid==0) {
		dup2(pipe_stdin[0],0); 	// process pipe_stdin
		dup2(pipe_stdout[1],1);	// process pipe_stdout
		dup2(pipe_stdout[1],2);	// process stderr
		
		// close all other handles
		for(i=3;i<255;i++) 
			close(i);
		
		program=argv[1];
		program_params=&argv[1];
		execv(program,program_params);
		
		fprintf(stderr,"cannot exec a process - %s\n",strerror(errno));
		exit(-1);
	}
	else {
		// delete any previous doman sockets
		for(i=0;i<3;i++) {
			delete_unix_domain_server_socket(pipe_id,pipe_names[i]);
				
			// open domain pipe_stdin and pipe_stdout sockets
			if( (server_socket[i]=open_unix_domain_server_socket(pipe_id,pipe_names[i]))<0 ) {
				fprintf(stderr,"%s domain socket creation failure - pipe_id=%d\n",pipe_names[i],pipe_id);
					goto fini;
			}
		}
		
		// init queues
		for(i=0;i<3;i++)
			init_queue(&process_queue[i],CGIPIPE_BUF);
		
		while(!appterm) {
			tv.tv_sec=10;
			tv.tv_usec=0;

			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			

			max_fd=-1;
			
			written_to_client=0;
			read_from_client=0;
			read_from_ctrl_client=0;
			
			
			// put process stdout (read) to rfds if queue is free
			if(get_queue_free(&process_queue[0])) {
				FD_SET(process_stdio[0],&rfds);
				max_fd=MAX(max_fd,process_stdio[0]);
			}
			
			// put server sockets to rfds
			for(i=0;i<3;i++) {
				FD_SET(server_socket[i],&rfds);
				max_fd=MAX(max_fd,server_socket[i]);
			}
			
			// put stdin client (read) to rfds
			if(client_stdio[0]>=0) {
				FD_SET(client_stdio[0],&rfds);
				max_fd=MAX(max_fd,client_stdio[0]);
			}
			
			// put control client (read) to rfds
			if(client_stdio[2]>=0) {
				FD_SET(client_stdio[2],&rfds);
				max_fd=MAX(max_fd,client_stdio[2]);
			}
			
			// put stdout client (write) to wfds only when stdout process queue has traffic
			if( (client_stdio[1]>=0) && get_queue_content_len(&process_queue[0]) ) {
				FD_SET(client_stdio[1],&wfds);
				max_fd=MAX(max_fd,client_stdio[1]);
			}
			
			// put stdin process (write) to wfds only when stdin process queue has traffic
			if( get_queue_content_len(&process_queue[1]) ) {
				FD_SET(process_stdio[1],&wfds);
				max_fd=MAX(max_fd,process_stdio[1]);
			}
			
			// select
			sel=select(max_fd+1,&rfds,&wfds,0,&tv);
			if(sel<0) {
				if(errno!=EINTR) {
					fprintf(stderr,"punk!");
					break;
				}
			}
			else if(sel==0) {
				// timeout
			}
			else {
				// accept server socket if any
				for(i=0;i<3;i++) {
					// TODO: should not accept if any client socket is already running
					if(FD_ISSET(server_socket[i],&rfds)) {
						if( (client_stdio[i]=accept(server_socket[i], (struct sockaddr *)0, (socklen_t *)0))<0 ) {
							fprintf(stderr,"socket accept failure - %s\n",strerror(errno));
						}
					}
				}
				
				// pipe - process stdin (write) queue ==> process stdin (write)
				if( FD_ISSET(process_stdio[1],&wfds) ) {
					read_len=pull_from_queue(&process_queue[1],block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
					if(read_len>0) {
						write_len=write(process_stdio[1],block_buf,read_len);
						if(write_len<0) {
							if( close_down_socket_if_broken_pipe(&client_stdio[0])<0 ) {
								fprintf(stderr,"process stdin write failure - %s\n",strerror(errno));
							}
						}
						else {
							remove_from_queue(&process_queue[1],write_len);
						}
					}
					else {
						fprintf(stderr,"process stdin (write) queue empty\n");
					}
				}
				
				// pipe - process stdout (read) ==> process (read) queue
				if( FD_ISSET(process_stdio[0],&rfds) ) {
					queue_free=get_queue_free(&process_queue[0]);
					read_len=read(process_stdio[0],block_buf,MIN(CGIPIPE_RW_BLOCK_BUF_LEN,queue_free));
					if(read_len<0) {
						fprintf(stderr,"process stdout read failure - %s\n",strerror(errno));
					}
					else {
						write_len=push_into_queue(&process_queue[0],block_buf,read_len);
						
						if(read_len!=write_len) {
							fprintf(stderr,"internal structure failure - incorrect queue free size calculation");
						}
					}
				}
				
				// pipe - stdin client (read) socket socket ==> process stdin (write) queue
				if( (client_stdio[0]>=0) && FD_ISSET(client_stdio[0],&rfds) ){
					
					read_len=read(client_stdio[0],block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
					
					if(read_len<0) {
						fprintf(stderr,"client socket read failure - %s\n",strerror(errno));
					}
					else {
						push_into_queue(&process_queue[1],block_buf,read_len);
						if(read_len)
							read_from_client=1;
					}
				}
				
				// pipe - control client (read) socket socket ==> process stdin (write) queue
				if( (client_stdio[2]>=0) && FD_ISSET(client_stdio[2],&rfds) ){
					
					read_len=read(client_stdio[2],block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
					
					if(read_len<0) {
						fprintf(stderr,"client socket read failure - %s\n",strerror(errno));
					}
					else {
						push_into_queue(&process_queue[2],block_buf,read_len);
						if(read_len)
							read_from_ctrl_client=1;
					}
				}
				
				
				// pipe - process stdout (read) queue ==> stdout client (write) socket
				if( (client_stdio[1]>=0) && FD_ISSET(client_stdio[1],&wfds) ) {
					read_len=pull_from_queue(&process_queue[0],block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
					if(read_len<0) {
						fprintf(stderr,"process stdout (read) queue empty\n");
					}
					else {
						write_len=write(client_stdio[1],block_buf,read_len);
						
						if(write_len<0) {
							if( close_down_socket_if_broken_pipe(&client_stdio[1])<0 ) {
								fprintf(stderr,"stdout client socket write failure - %s\n",strerror(errno));
							}
						}
						else {
							remove_from_queue(&process_queue[0],write_len);
						}
						
						written_to_client=1;
					}
				}
			}
			
			
			// wait for child
			if( waitpid(pid,&child_stat,WNOHANG) == pid ) {
				child_term=1;
				child_term_clock=times(&tmsbuf);
				fprintf(stderr,"child terminated\n");
			}
			
			// if child terminates
			if(child_term) {
				
				tv.tv_sec=0;
				tv.tv_usec=0;
				FD_SET(process_stdio[0],&rfds);
				
				// check the process stdout
				sel=select(process_stdio[0]+1,&rfds,0,0,&tv);
				if(sel<0)
					continue;
			
				// if no traffic in stdout (read) queue or in tty queue
				if((!get_queue_content_len(&process_queue[0]) && !sel)) {
					break;
				}
				// timeout break even if we have traffic
				else if( (((times(&tmsbuf)-child_term_clock)/tick_per_sec) >= CGIPIPE_TERM_TIMEOUT ) ) {
					fprintf(stderr,"queueing traffic in stdout but pipe timed out.\n");
					break;
				}
					
			}
			
			// check stdout client connection
			if(client_stdio[1]>=0 && !written_to_client) {
				if( write(client_stdio[1],0,0)<0 ) {
					if( close_down_socket_if_broken_pipe(&client_stdio[1])<0 ) {
						fprintf(stderr,"stdout client socket write failure - %s\n",strerror(errno));
					}
				}
			}
			
			// check stdin client connection
			if(client_stdio[0]>=0 && !read_from_client) {
				if( write(client_stdio[0],0,0)<0 ) {
					if( close_down_socket_if_broken_pipe(&client_stdio[0])<0 ) {
						fprintf(stderr,"stdin client socket write failure - %s\n",strerror(errno));
					}
				}
			}
			
			// check control client connection
			if(client_stdio[2]>=0 && !read_from_ctrl_client) {
				if( write(client_stdio[2],0,0)<0 ) {
					if( close_down_socket_if_broken_pipe(&client_stdio[2])<0 ) {
						fprintf(stderr,"control client socket write failure - %s\n",strerror(errno));
					}
				}
			}
			
			// parse the control socket
			if(read_from_ctrl_client) {
				while(1) {
					
					read_len=pull_from_queue(&process_queue[2],block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
					if(read_len<=0)
						break;
					
					line_buf[0]=0;
							
					// remove until CRLF
					j=0;
					i=0;
					waste=0;
					while( (i<read_len) && (j<sizeof(line_buf)) ) {
						ch=block_buf[i++];
						
						if(ch=='*') {
							line_buf[0]=0;
							waste=1;
							break;
						}
						
						if(ch=='\n') {
							line_buf[j++]=0;
							waste=1;
							break;
						}
							
						line_buf[j++]=ch;
					}
					
					line_buf[sizeof(line_buf)-1]=0;
					
					// remove parsed part from queue
					if(waste || (read_len==CGIPIPE_RW_BLOCK_BUF_LEN)) {
						remove_from_queue(&process_queue[2],i);
					}
					
					if(line_buf[0]==0) {
					}
					// control command - term
					else if(!strcmp(line_buf,"term")) {
						const char* crlf="\n";
						
						kill(pid,SIGTERM);
						
						write(process_stdio[1],crlf,sizeof(crlf));
					}
				}
			}
		}
	}
	
	
	stat=0;
	
fini:
		
	// finialize queues
	for(i=0;i<3;i++) {
		fini_queue(&process_queue[i]);
		
		if(server_socket[i])
			close(server_socket[i]);
		
		if(client_stdio[i])
			close(client_stdio[i]);
		
		delete_unix_domain_server_socket(pipe_id,pipe_names[i]);
	}
	
	delete_pipe(pipe_stdin);
	delete_pipe(pipe_stdout);

	return stat;
}

int cgppipe_check_main(int argc,char* argv[])
{
	int stat=-1;
	
	int fd_stdin=-1;
	
	int pipe_id;
	
	
	// check parameter validation
	if(argc<1) {
		print_usage(stderr);
		goto fini;
	}
	
	pipe_id=atoi(argv[0]);
	
	// open socket
	if( (fd_stdin=open_unix_domain_client_socket(pipe_id,pipe_names[0]))<0 ) {
		fprintf(stderr,"stdin domain client socket creation failure\n");
		goto fini;
	}
	
	stat=0;
	
fini:
	if(fd_stdin>=0)
		close(fd_stdin);
		
	return stat;	
}

int cgppipe_readwritectrl_main(int argc,char* argv[], int readwritectrl)
{
	int stat=-1;
	
	fd_set rfds;
	
	int fd;
	int sel;
	
	struct timeval tv;
	
	char block_buf[CGIPIPE_RW_BLOCK_BUF_LEN];
	
	int read_len;
	int write_len;
	int len_to_read;
	int queue_free_len;
	
	int pipe_id;
	
	int fd_in;
	int fd_out;
	
	struct queue_t sock_queue={0,};
	
	// check parameter validation
	if(argc<1) {
		print_usage(stderr);
		goto fini;
	}
	
	pipe_id=atoi(argv[0]);
	
	// open socket
	if( (fd=open_unix_domain_client_socket(pipe_id,pipe_names[readwritectrl]))<0 ) {
		fprintf(stderr,"stdin domain client socket creation failure\n");
		goto fini;
	}
	
	switch(readwritectrl) {
		// pipe-stdin
		case 0:
		// pipe-ctrl
		case 2:
			fd_in=0;
			fd_out=fd;
			break;
		
		// pipe-stdout
		case 1:
			fd_in=fd;
			fd_out=1;
			break;
	}
			
	init_queue(&sock_queue,GCIPIPE_DSOCK_BUF);
	
	// fill up queue
	while(1) {
		FD_ZERO(&rfds);
		FD_SET(fd_in,&rfds);
		
		// build tv
		tv.tv_sec=1;
		tv.tv_usec=0;
		
		// select
		sel=select(fd_in+1,&rfds,0,0,&tv);
		if(sel<0) {
			fprintf(stderr,"select failure - %s\n",strerror(errno));
			goto fini;
		}
		else if(sel==0) {
			fprintf(stderr,"no traffic\n");
			break;
		}
		
		// get length to read
		queue_free_len=get_queue_free(&sock_queue);
		if(!queue_free_len)
			break;
			
		len_to_read=MIN(CGIPIPE_RW_BLOCK_BUF_LEN,queue_free_len);
		
		// read
		read_len=read(fd_in,block_buf,len_to_read);
		if(read_len<=0)
			break;
			
		// write to queue
		write_len=push_into_queue(&sock_queue,block_buf,read_len);
		if(write_len!=read_len) {
			fprintf(stderr,"internal structure failure - incorrect queue free size calculation");
		}
	}
	
	// flush queue to stdout
	while(get_queue_content_len(&sock_queue)) {
		
		read_len=pull_from_queue(&sock_queue,block_buf,CGIPIPE_RW_BLOCK_BUF_LEN);
		write_len=write(fd_out,block_buf,read_len);
		if(write_len<0)
			break;
		
		remove_from_queue(&sock_queue,write_len);
	}
				
	stat=0;
	
fini:
	fini_queue(&sock_queue);
	if(fd>=0)
		close(fd);
	return stat;	
}


int main(int argc,char* argv[])
{
	const char* cgipipe_cmdname;
	int stat=-1;
	
	char** sub_argv;
	int sub_argc;
	
	sub_argv=&argv[1];
	sub_argc=argc-1;
	
	// get command name
	cgipipe_cmdname=basename(argv[0]);
	if(!strcmp(cgipipe_cmdname,"cgipipe")) {

		if(argc<2) {
			print_usage(stderr);
			exit(-1);
		}
		if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
			print_usage(stderr);
			exit(0);
		}

		cgipipe_cmdname=argv[1];

		sub_argv=&argv[2];
		sub_argc=argc-2;
	}

	if (sub_argc > 0) {
		if (!strcmp(sub_argv[0], "--help") || !strcmp(sub_argv[0], "-h")) {
			print_usage(stderr);
			exit(0);
		}
	}

	// get tick per sec
	tick_per_sec=sysconf(_SC_CLK_TCK);
	
	if(!strcmp(cgipipe_cmdname,"cgipipe_server")) {
		stat=cgppipe_server_main(sub_argc,sub_argv);
	}
	else if(!strcmp(cgipipe_cmdname,"cgipipe_read")) {
		stat=cgppipe_readwritectrl_main(sub_argc,sub_argv,1);
	}
	else if(!strcmp(cgipipe_cmdname,"cgipipe_write")) {
		stat=cgppipe_readwritectrl_main(sub_argc,sub_argv,0);
	}
	else if(!strcmp(cgipipe_cmdname,"cgipipe_check")) {
		stat=cgppipe_check_main(sub_argc,sub_argv);
	}
	else if(!strcmp(cgipipe_cmdname,"cgipipe_ctrl")) {
		stat=cgppipe_readwritectrl_main(sub_argc,sub_argv,2);
	}
	
	return stat;
}
