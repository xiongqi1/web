/*!
 * Copyright Notice:
 * Copyright (C) 2013 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "term.h"

/* Version Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1
	
#define STO STDOUT_FILENO
#define STI STDIN_FILENO

#define APPLICATION_NAME "metering emulator"

//#define PACKET_DEBUG

#define MAX_BUF_SIZE	2048
unsigned char response[MAX_BUF_SIZE] = {0, };
static char *sport_name = NULL;
static char *dest_no = NULL;
int server_mode = 0;
int echo_mode = 0;
int direct_mode = 0;
volatile int running = 1;
static int tty_fd = -1;
int default_baudrate;
unsigned char test_packet1[] = { "This is test packet for metering device with NTC-1101." };
unsigned char test_packet2[] = { 0xAC, 0x00, 0x00, 0x74, 0xAA };
unsigned char test_packet3[] = { 0xAC, 0x00, 0x00, 0x01, 0x02, 0x03 };

static void print_pkt(char* pbuf, int len)
{
	unsigned char* buf = (unsigned char *)pbuf;
	unsigned char buf2[256] = {0x0,};
	unsigned char buf3[16] = {0x0,};
	int i, j = len/16, k = len % 16;
	fprintf(stderr, "print_pkt %d\n\r", len);
	for (i = 0; i < j; i++)	{
		(void) memset(buf3, 0x00, 16);
		(void) memcpy(buf3, &buf[i*16], 16);
		fprintf(stderr, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ; %s\n\r",
                buf[i*16],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+6],buf[i*16+7],
                buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15], buf3);
	}
	j = i;
	for (i = 0; i < k; i++) {
		sprintf((char *)buf2, "%s%02x ", buf2, buf[j*16+i]);
	}
	(void) memset(buf3, 0x00, 16);
	(void) memcpy(buf3, &buf[j*16], k);
	fprintf(stderr, "%s ; %s\n\r", buf2, buf3);
}

static int open_port(const char *s_port)
{
	struct stat port_stat;
	int r;

	/* port open */
	if (stat(s_port, &port_stat) < 0) {
		fprintf(stderr, "can not find port %s (%s)\n\r", s_port, strerror(errno)); return -1;
	}
	fprintf(stderr, "opening serial port '%s'...\n\r", s_port);
	if((tty_fd = open(s_port, O_RDWR | O_NONBLOCK)) < 0) {
		fprintf(stderr, "failed to open '%s' (%s)\n\r", s_port, strerror(errno));
		return -1;
	}

	/* initialize terminal */
	if (term_lib_init() < 0) {
		fprintf(stderr, "term_lib_init failed (%s)\n\r", term_strerror(term_errno, errno));
		return -1;
	}
	
	if (server_mode)
		//default_baudrate = 115200;
		default_baudrate = 9600;
	else
		//default_baudrate = 115200;
		default_baudrate = 9600;
	
	fprintf(stderr, "\t-raw mode\n\r");
	fprintf(stderr, "\t-%d bps\n\r", default_baudrate);
	fprintf(stderr, "\t-no parity bit\n\r");
	fprintf(stderr, "\t-8 data bits\n\r");
	fprintf(stderr, "\t-no flow control\n\r");
	r = term_set(tty_fd,
				 1,					/* raw mode */
				 default_baudrate,	/* baudrate */
				 P_NONE,			/* parity : P_EVEN/P_ODD/P_NONE */
				 8,					/* data bits */
				 FC_NONE,			/* flow control : FC_XONXOFF/FC_RTSCTS/FC_NONE */
				 1,					/* local or modem */
				 1); 				/* hup-on-close */
	if (r < 0) {
		fprintf(stderr, "failed to add device %s: %s", s_port, term_strerror(term_errno, errno));
		return -1;
	}
	if (term_apply(tty_fd) < 0) {
		fprintf(stderr, "failed to config device %s: %s", s_port, term_strerror(term_errno, errno));
		return -1;
	}
	fprintf(stderr, "Terminal ready\n\r");
	return 0;
}

static void close_port(void)
{
	term_erase(tty_fd);
	if(tty_fd > 0) {
		if(tcflush(tty_fd, TCIFLUSH) != 0) {
			fprintf(stderr, "tcflush() of gps port failed (%s)\n\r", strerror(errno));
		}
		fprintf(stderr, "closing serial port...\n\r");
		close(tty_fd);
		tty_fd = -1;
		fprintf(stderr, "serial port closed\n\r");
	}
}

static void send_escape_sequence(void)
{
	(void)write(tty_fd, "+", 1);
	(void)write(tty_fd, "+", 1);
	(void)write(tty_fd, "+", 1);
}

static int write_port(char *buf, int blen, int echo)
{
	unsigned char obuf[MAX_BUF_SIZE] = {0, };
	unsigned char* p = &obuf[0];
	int selected, wlen = 0, len = blen;
	fd_set fdw;
	(void) memcpy(obuf, buf, len);
	if (strcmp(obuf, "+++") && obuf[0] != 0xAC && !echo) {
		obuf[len] = '\r'; len += 1;
	}
	while(running) {
		struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
		FD_ZERO(&fdw);
		FD_SET(tty_fd, &fdw);
		selected = select(tty_fd + 1, NULL, &fdw, NULL, &timeout);
		if(selected < 0) {
			fprintf(stderr, "select() failed with error %d (%s)", selected, strerror(errno));
			return -1;
		} else if(selected > 0) {
#ifdef PACKET_DEBUG
			fprintf(stderr, "--> ");
			/* leading 0xAC means metering device protocol */
			if (obuf[0] == 0xAC || direct_mode)
				print_pkt(p, len);
			else
				fprintf(stderr, "%s\n\r", p);
#endif			
			wlen = write(tty_fd, p, len);
			if (wlen != len) {
				fprintf(stderr, "failed to write %d / %d bytes discarded: err '%s'; trying to flush", wlen, len, strerror(errno));
				if(tcflush(tty_fd, TCOFLUSH) != 0) {
					fprintf(stderr, "tcflush failed (%s)", strerror(errno));
				}
				return -1;
			} else {
				fprintf(stderr, "sent %d bytes\n\r", wlen);
				return 0;
			}
		} else {
			return -1;
		}
	}
}

static int read_port(int timeout_sec, int clen, char *expect)
{
	unsigned char* p = &response[0];
	int len = 0;
	int selected, tout = timeout_sec;
	fd_set fdr;
	int t = tout > 0 ? 1 : 0;
	(void) memset(p, 0x00, MAX_BUF_SIZE);
	while(running) {
		struct timeval timeout = { .tv_sec = t, .tv_usec = 100000 };
		FD_ZERO(&fdr);
		FD_SET(tty_fd, &fdr);
		selected = select(tty_fd + 1, &fdr, NULL, NULL, &timeout);
		if(selected < 0) {
			fprintf(stderr, "select() failed with error %d (%s)\n\r", selected, strerror(errno));
			if (strcmp(expect, "CONNECT") == 0) {
				write(tty_fd, "ATH", 3);
			}
			return -1; 
		} else if(selected > 0) {
			if(read(tty_fd, p, 1) < 0) {
				fprintf(stderr, "failed to read from %d (%s)\n\r", tty_fd, strerror(errno));
				return -1;
			}
			++len; ++p;
		} else {
			if (strlen(response) > clen &&
				(strstr(&response[clen], "OK") || strstr(&response[clen], "ERROR") ||
				strstr(&response[clen], "RING") || strstr(&response[clen], "NO CARRIER") ||
				(expect && strstr(&response[clen], expect)))) {
				goto ret_len;
			}
			if(tout-- > 0)
				continue;
			goto ret_len;
		}
	}
ret_len:	
#ifdef PACKET_DEBUG
	fprintf(stderr, "<-- ");
	/* leading 0xAC means metering device protocol */
	if (response[0] == 0xAC || direct_mode)
		print_pkt(&response[0], len);
	else
		fprintf(stderr, "%s\n\r", response);
	fprintf(stderr, "    read %d bytes\n\r", len);
#endif	
	return len;
}

static int send_command_with_timeout(char *cmd, int timeout, char *expect)
{
 	if (write_port(cmd, strlen(cmd), 0) != 0) {
		fprintf(stderr, "failed to write command\n\r");
		return -1;
	}
	if (read_port(timeout, strlen(cmd)+1, expect) < 0) {
		fprintf(stderr, "failed to read command\n\r");
		return -1;
	}
	return 0;
}

static int configure_modem(void)
{
	send_escape_sequence();
	if (write_port("ATH", 3, 0) != 0) {
		fprintf(stderr, "failed to write 'ATH' command\n\r");
		return -1;
	}
	if (send_command_with_timeout("AT", 1, NULL) != 0) {
		fprintf(stderr, "failed to write 'AT' command\n\r");
		return -1;
	}
	if (send_command_with_timeout("ATI", 1, NULL) != 0) {
		fprintf(stderr, "failed to write 'ATI' command\n\r");
		return -1;
	}
#if (0)	
	if (send_command_with_timeout("AT+CGMI", 1, NULL) != 0) {
		fprintf(stderr, "failed to write 'AT+CGMI' command\n\r");
		return -1;
	}
	if (send_command_with_timeout("AT+CGMM", 1, NULL) != 0) {
		fprintf(stderr, "failed to write 'AT+CGMM' command\n\r");
		return -1;
	}
	if (send_command_with_timeout("AT+CGMR", 1, NULL) != 0) {
		fprintf(stderr, "failed to write 'AT+CGMR' command\n\r");
		return -1;
	}
#endif	
	return 0;
}

static int connect_remote(int server_mode)
{
	char cmd_buf[MAX_BUF_SIZE] = {0, };
	if (server_mode) {
		sprintf(cmd_buf, "ATD%s", dest_no);
		fprintf(stderr, "dialing to %s...\n\r", dest_no);
		if (send_command_with_timeout(cmd_buf, 60, "CONNECT") != 0) {
			fprintf(stderr, "failed to write '%s' command\n\r", cmd_buf);
			return -1;
		}
		if (strstr(response, "CONNECT") == 0)  {
			fprintf(stderr, "failed to connect to '%s' \n\r", dest_no);
			return -1;
		} else {
			fprintf(stderr, "connected\n\r");
		}
	} else {
		fprintf(stderr, "waiting for incoming RING...\n\r");
		if (read_port(600, 0, "RING") < 0)  {
			fprintf(stderr, "failed to receive ring\n\r");
			return -1;
		} else {
			fprintf(stderr, "received RING\n\r");
		}
		fprintf(stderr, "send ATA and wait for connecting...\n\r");
		if (send_command_with_timeout("ATA", 30, "CONNECT") != 0) {
			fprintf(stderr, "failed to write '%s' command\n\r", cmd_buf);
			return -1;
		}
	}
	return 0;
}

static void print_reponse(int len)
{
	if (response[0] == 0xAC || direct_mode) {
		fprintf(stderr, "\n\r\n\rreceived %d bytes: ", len);
		print_pkt(&response[0], len);
	} else {
		fprintf(stderr, "\n\r\n\rreceived %s\n\r", response);
	}
}

static int packet_exchange(int server_mode)
{
	char cmd_buf[MAX_BUF_SIZE] = {0, };
	int rlen, retry_cnt = 5, result = 0;
	fprintf(stderr, "=================================================================\n\r");
	fprintf(stderr, "         PACKET EXCHANGE START\n\r");
	fprintf(stderr, "=================================================================\n\r");
	if (server_mode) {
retry_send:	
		fprintf(stderr, "send test packet and wait for response...\n\r");
		if (echo_mode) {
			/* send normal ASCII string test packet */
			strcpy(cmd_buf, test_packet1);
			if (write_port(cmd_buf, strlen(cmd_buf), 0) != 0) {
				fprintf(stderr, "failed to write test packet[0]\n\r");
				result = -1;
				goto ret;
			}
		} else {
			/* send hexa decimal string test packet */
			if (write_port(test_packet2, 5, 0) != 0) {
				fprintf(stderr, "failed to write test packet\n\r");
				result = -1;
				goto ret;
			}
		}
		while (running) {
			rlen = read_port(1, 0, NULL);
			if (rlen > 0) {
				print_reponse(rlen);
				break;
			}
			if (retry_cnt-- <= 0) {
				retry_cnt = 5;
				goto retry_send;
			}
		}
	} else {
		while (running) {
			rlen = read_port(1, 0, NULL);
			if (rlen > 0) {
				//print_reponse(rlen);
				fprintf(stderr, "received %d bytes: \n\r%s\n\r", rlen, response);
				// echo check
				if (strlen(cmd_buf)) {
					memset(cmd_buf, 0x00, MAX_BUF_SIZE);
					fprintf(stderr, "got local echo packet, ignore & waiting for next incoming packet...\n\r");
					fprintf(stderr, "============================================================\n\r");
					continue;
				}
				if (echo_mode) {
					memset(cmd_buf, 0x00, MAX_BUF_SIZE);
					strcpy(cmd_buf, response);
					fprintf(stderr, "send echo packet %d bytes...\n\r", strlen(cmd_buf));
					if (write_port(cmd_buf, strlen(cmd_buf), 1) != 0) {
						fprintf(stderr, "failed to echo data\n\r");
						result = -1;
						goto ret;
					}
					fprintf(stderr, "sent echo packet...\n\r");
					continue;
				} else {
					break;
				}
			}
		}
		if (rlen > 0) {
			fprintf(stderr, "send response packet...\n\r");
			if (response[0] == 0xAC) {
				if (write_port(test_packet3, 6, 0) != 0) {
					fprintf(stderr, "failed to write test packet\n\r");
					result = -1;
					goto ret;
				}
			} else {
				if (write_port(response, strlen(response), 0) != 0) {
					fprintf(stderr, "failed to echo data\n\r");
					result = -1;
					goto ret;
				}
			}
			while (running) {
				rlen = read_port(1, 0, NULL);
				if (rlen > 0)
					print_reponse(rlen);
				else
					break;
			}
		}
 	}
	result = 0;
ret:	
	fprintf(stderr, "=================================================================\n\r");
	fprintf(stderr, "         PACKET EXCHANGE END\n\r");
	fprintf(stderr, "=================================================================\n\r");
	return result;
}

static int disconnect(void)
{
	int retry_cnt = 5;
	while (retry_cnt-- > 0) {
		send_escape_sequence();
		if (read_port(2, 0, NULL) < 0)
			fprintf(stderr, "failed to write command\n\r");
		if (strstr(response, "NO CARRIER"))
			break;
		if (write_port("ATH", 3, 0) != 0)
			fprintf(stderr, "failed to write 'ATH' command\n\r");
 		if (read_port(2, 0, NULL) < 0)  
 			fprintf(stderr, "failed to write command\n\r");
		if (strstr(response, "NO CARRIER"))
			break;
	}
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "\n\rUsage: metering_emul [options]\n\r");
	fprintf(stderr, "\n\r Options:\n\r");
	fprintf(stderr, "\t-p port to attach\n\r");
	fprintf(stderr, "\t-n destination number (for server mode)\n\r");
	fprintf(stderr, "\t-d direct mode\n\r");
	fprintf(stderr, "\t-s server mode (default metering device)\n\r");
	fprintf(stderr, "\t-e text echo test mode\n\r");
	fprintf(stderr, "\t-V version information\n\r");
	fprintf(stderr, "\n\r");
	exit(-1);
}

static void sig_handler(int signum)
{
	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notfication via SIGHUP. So we don't whinge
	on SIGHUP. */
	if (signum!=SIGHUP)
		fprintf(stderr, "caught signal %d\n\r", signum);

	switch(signum)
	{
		default:
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			running = 0;
 			break;
	}
}

int main(int argc, char **argv)
{
	int	ret = 0;
	int	verbosity = 0;
	int be_daemon = 1;

	while((ret = getopt(argc, argv, "Vhp:?sdn:e")) != EOF) {
		switch(ret) {
			case 'p': sport_name = optarg; break;
			case 'd': direct_mode = 1; break;
			case 'n': dest_no = optarg; break;
			case 's': server_mode = 1; break;
			case 'e': echo_mode = 1; break;
			case 'V': fprintf(stderr, "%s Version %d.%d.%d\n\r", argv[0], VER_MJ, VER_MN, VER_BLD); break;
			case 'h':
			case '?': usage();
			default: break;
		}
	}

	if(!sport_name | (server_mode && !direct_mode && !dest_no)) { usage(); }
	fprintf(stderr,  "\n\n\nstart %s %d.%d.%d in 'metering %s' mode\n\r",  APPLICATION_NAME, VER_MJ, VER_MN, VER_BLD,
		(server_mode? "server":"client"));
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	if((ret = open_port(sport_name)) != 0) {
		fprintf(stderr, "failed to open port\n\r");
		exit(ret);
	}
	if (direct_mode == 0) {
		if((ret = configure_modem()) != 0) {
			fprintf(stderr, "failed to configure modem\n\r");
			goto exit_emul;
		}
		if((ret = connect_remote(server_mode)) != 0) {
			fprintf(stderr, "failed to connect remote\n\r");
			goto exit_emul;
		}
	}
	// give 5 seconds delay before sending command packet for more stable operation
	//sleep(5);
	if((ret = packet_exchange(server_mode)) != 0) {
		fprintf(stderr, "failed to exchange test packet\n\r");
		goto exit_emul;
	}
	if (direct_mode == 0) {
		if((ret = disconnect()) != 0) {
			fprintf(stderr, "failed to exchange test packet\n\r");
			goto exit_emul;
		}
	}
exit_emul:	
	close_port();
	fprintf(stderr,  "exit (%s)\n\r\n\n\n", ret == 0 ? "normal" : "failure");
	exit(ret);
}

/*
* vim:ts=4:sw=4:
*/
