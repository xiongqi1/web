/*
 * Dumps the collected console information from a
 * hardware console logger. If logger is not fitted,
 * the operation is a no-op.
 *
 * Iwo.Mergler@netcommwireless.com
 */
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BAUDRATE B115200
const static char device[] = "/dev/console";
const static char magic[]  = "\nLogger Dump\n";

const static char logfile[] = "/tmp/console.log"; /* TODO: Use cmdline */

#define BUFFERSIZE 1024
static char buffer[BUFFERSIZE] = { 0 };

int main(int argc, char *argv[])
{
	struct termios oldtio;
	struct termios newtio = {0};
	fd_set rfds;
	struct timeval tv;
	int fd, rval;
	int tf = -1; /* Target file descriptor */
	ssize_t nr;

	/*
	Open tty device for reading and writing, but not as controlling tty
	because we don't want to get killed if it sends CTRL-C.
	*/
	fd = open(device, O_RDWR | O_NOCTTY);
	if (fd <0) {perror(device); exit(-1); }

	tcgetattr(fd,&oldtio); /* save current tty settings */

	/* 
	Set bps rate and hardware flow control and 8n1 (8bit,no parity,1 stopbit).
	Also don't hangup automatically and ignore modem status.
	Finally enable receiving characters.
	*/
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;

	/*
	Ignore bytes with parity errors and make terminal raw and dumb.
	*/
	newtio.c_iflag = IGNPAR;

	/*
	Raw output.
	*/
	newtio.c_oflag = 0;

	/*
	Don't echo characters. Don't generate signals.
	*/
	newtio.c_lflag = 0;

	/* blocking read until at least 1 char arrives (we use select, anyway). */
	newtio.c_cc[VMIN]=1;
	newtio.c_cc[VTIME]=0;

	/* now clean the modem line and activate the settings for modem */
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&newtio);

	/* Send magic */
	write(fd, magic, sizeof(magic)-1);

	/* Collect responses */
	while (1) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		/* The logger responds immediately and dumps at high speed.
		 * If we get nothing for 100ms, the logger is missing or has
		 * finished dumping. */
		tv.tv_sec = 0;
		tv.tv_usec = 0.1 * 1000 * 1000;

		rval = select(fd+1, &rfds, NULL, NULL, &tv);
		if (rval == -1) {
			perror("select()");
		} else if (rval) {
			/* Got data */
			nr = read(fd, buffer, BUFFERSIZE);
			if (nr == -1) {perror("console read"); exit(-1); }
			if (tf == -1) {
				printf("Log dump started\n");
				tf = open(logfile, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
				if (tf < 0) {perror(logfile); exit(-1); }
			}
			write(tf, buffer, nr);
		} else {
			/* Timeout */
			if (tf == -1) {
				/* No Logger response */
				printf("No Logger\n");
				break;
			} else {
				/* End of log dump */
				printf("Logger dump saved\n");
				break;
			}
		}
	}

	if (tf >= 0) close(tf);

	/* restore original console setting */
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd,TCSANOW,&oldtio);

	close(fd);
	return 0;
}
