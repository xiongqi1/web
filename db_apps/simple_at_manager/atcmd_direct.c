#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <stdlib.h>

#include <termios.h>

int main(int argc,char* argv[]) 
{
	int fd;
	
	char cmd[256];
	int written;
	int len;
	int readlen;
	
	char buf[1024];

	struct termios newtio;
	struct termios oldtio;
	
	int delay;

	if(argc<3) {
		fprintf(stderr,"atcmd <port> <cmd> [delay]\n");
		exit(-1);
	}
	
	if(argc==4) {
		delay=atoi(argv[3]);
	}
	else {
		delay=1;
	}
	
	fprintf(stderr,"port=%s,cmd=%s,delay=%d\n",argv[1],argv[2],delay);
	
	/* open */
	fd=open(argv[1], O_RDWR | O_NOCTTY | O_TRUNC);
	if(fd<0) {
		fprintf(stderr,"failed to open - %s",strerror(errno));
		exit(-1);
	}

	/* store attribute */
	if (tcgetattr(fd, &oldtio) < 0) {
		fprintf(stderr,"tcgetattr failed - %s",strerror(errno));
		exit(-1);
	}
	
	/* set attribute */
	memcpy(&newtio, &oldtio, sizeof(struct termios));
	cfmakeraw(&newtio);
	newtio.c_cc[VMIN] = 0;
	newtio.c_cc[VTIME] = 0;
	
	if (tcsetattr(fd, TCSAFLUSH, &newtio) < 0) {
		fprintf(stderr,"tcsetattr failed - %s",strerror(errno));
		exit(-1);
	}

	/* write */
	len=snprintf(cmd,sizeof(cmd),"%s\r\n",argv[2]);
	written=write(fd,cmd,len);
	
	if(written!=len) {
		fprintf(stderr,"failed to write - %s",strerror(errno));
		exit(-1);
	}
	
	fsync(fd);
	
	sleep(delay);
	
	/* read */
	readlen=read(fd,buf,sizeof(buf));
	if(readlen<0) {
		fprintf(stderr,"failed to read - %s",strerror(errno));
		exit(-1);
	}

	/* print to stdout */
	write(STDOUT_FILENO,buf,readlen);
	
	close(fd);
	
	exit(0);
}
