#define __USE_POSIX
#define __USE_POSIX2
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <alloca.h>
#include <linux/limits.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>

#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>

#include "rdb_ops.h"
#include "cbuf.h"
#include "rules.h"

#ifdef PLATFORM_BOVINE
#define SYSLOG_FILENAME     "/opt/messages"
#else
#define SYSLOG_FILENAME     "/etc/messages"
#endif
#define SYSLOG_LOGTOFILE_DB_NAME    "service.syslog.option.logtofile"
#define SYSLOG_LASTREBOOT_DB_NAME    "service.syslog.lastreboot"
#define MAX_LINE   512
#define MAX_BUFFER 2048

/* rdb variable for heartbeat timeout in minutes for simple_at_manager, qmimgr, cnsmgr */
#define PORTMGR_HEARTBEAT_TIMEOUT    "supervisor.portmgr.heartbeat_timeout"
/* default value replaced from static.c */
#define HEARTBEAT_TIMEOUT	(2*60) /* In lowest power mode, the module does one hearbeat per minute */

/* heartbeat timeout for simple_at_manager, qmimgr, cnsmgr */
int portmgr_heartbeat_timeout = 0;

char Exit_Reason[128]="Unknown (bug in supervisor)";
long tick_per_second;

/* reset heart beat informaiton - exists in static.c */
extern void reset_mgr_heart_beat();


/* Append messages to supervisor log file. Rotate log file
if size exceeds LOGSIZE bytes. */
#define LOGSIZE 1024
void logfile_append(const char *fname, const char *fmt, ...)
{
	va_list ap;
	struct stat s;
	FILE *lf = NULL;
	if (!fname) return;
	if (stat(fname, &s) == -1) return;
	if (s.st_size > LOGSIZE) {
		char *nname;
		nname = alloca(strlen(fname) + 4);
		strcpy(nname, fname);
		strcat(nname, ".old");
		rename(fname, nname);
	}
	lf=fopen(fname, "a");
	if (!lf) return;
	va_start(ap, fmt);
	vfprintf(lf, fmt, ap);
	va_end(ap);
	fclose(lf);
}

#include <sys/mman.h>
/* Register access. */
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)
#define MAP_BASE 0xfffff000;
#define RSTC     0xD00
#define RSTC_STA (RSTC+0x4)
static const off_t   ioregs = 0xfffff000;
static char* volatile io_base;
static int io_fd=-1;
#define REG(ofs) *((unsigned long*) &io_base[ofs])

/* Map a page of IO memory */
static void io_init(void)
{
	void* map_base;
	if ((io_fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
		perror("/dev/mem");
		return;
	}
	/* Map one page */
	map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, io_fd, ioregs & ~MAP_MASK);
	if (map_base == (void *) -1)
	{
		perror("mmap");
		close(io_fd);
		io_fd = -1;
		return;
	}

	/* Calculate page offset */
	io_base = (char *volatile)((unsigned long)map_base + (ioregs & MAP_MASK));
}

/* Returns last reset reason for at91sam processors */
/* Power, Wakeup, Watchdog, Software, User */

/* TODO: Use the sys script for cross-platform access, get rid of register access. */
static const char *reset_reason(void)
{
	unsigned long status;
	if (io_fd == -1) return "-";
	status=REG(RSTC_STA);
	switch ((status >> 8) & 0x7) {
		case 0 : return "Power";
		case 1 : return "Wakeup";
		case 2 : return "Watchdog";
		case 3 : return "Software";
		case 4 : return "Button";
		default: return "???";
	}
}

#define READ 0
#define WRITE 1

pid_t popen2(int *infp, int *outfp,const char* file,...)
{
	int p_stdin[2], p_stdout[2];
	pid_t pid;

	va_list ap;
	const char* a;

	#define MAX_ARG 64
	const char *argv[MAX_ARG];
	int i;

	if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
		return -1;

	va_start(ap, file);

	/* build argument */
	argv[0]=NULL;
	i=0;
	for(i=0;i<MAX_ARG;i++) {
		a = va_arg(ap, const char*);
		argv[i]=a;

		if(!a)
			break;
	}


	pid = fork();

	if (pid < 0)
		goto fini;
	else if (pid == 0)
	{
		close(p_stdin[WRITE]);
		dup2(p_stdin[READ], READ);
		close(p_stdout[READ]);
		dup2(p_stdout[WRITE], WRITE);


		execvp(file,(char* const*)argv);
		perror("execl");
		exit(1);
	}

	if (infp == NULL)
		close(p_stdin[WRITE]);
	else
		*infp = p_stdin[WRITE];

	if (outfp == NULL)
		close(p_stdout[READ]);
	else
		*outfp = p_stdout[READ];

fini:
	va_end(ap);

	return pid;
}

void sig_handler(int sig)
{
	switch(sig) {
		case SIGCONT:
			syslog(LOG_ERR,"supervisor got SIGCONT - resetting heart beat counts");
			/* reset heart beat information - start from scratch */
			reset_mgr_heart_beat();
			break;

		default:
			syslog(LOG_ERR,"sig(%d) caught",sig);
			break;
	}
}

int main(int argc, char *argv[])
{
	int  logfid = -1;
	char Line[MAX_LINE];
	fd_set rfds;
	struct timeval tv;
	int rval,readb;
	struct cbuf *cb=NULL;
	int opt;
	/* Options */
	int reboot=0;
	const char *logfile=NULL;
	int buffer_mode = 1;
	char temp[10];

	ino_t pino=0;
	struct stat sb;
	pid_t popen_pid = -1;
	int popen_stat;

	/* get tick per second */
	tick_per_second=sysconf(_SC_CLK_TCK);

	rdb_open_db();

	io_init();

	/* TODO: Catch signals */

	while ((opt=getopt(argc, argv, "hrl:")) != -1) {
		switch (opt) {
			case 'h':
				fprintf(stderr, "Usage: supervisor [-h] [-r] [-l logfile]\n"
						"	-h = this stuff\n"
						"	-r = enable auto-reboot\n"
						"	-l = use logfile\n");
				return -1;
				break;
			case 'r':
				reboot=1;
				break;
			case 'l':
				logfile=optarg;
				break;
			default :
				fprintf(stderr,"Unknown option %c, exiting\n",opt);
				return -1;
				break;
		}
	}

	cb = cbuf_alloc(MAX_BUFFER);
	if (!cb) {
		fprintf(stderr,"Can't allocate %d byte buffer\n",MAX_BUFFER);
		return -1;
	}

	syslog(LOG_DAEMON | LOG_INFO, "starting, reboot=%d, log=%s\n", reboot, logfile);
	logfile_append(logfile,"START(R:%s)\n", reset_reason());
	printf("Supervisor started\n");

	/* catch SIGCONT - to re-apply timeout after sleeping for a while */
	signal(SIGCONT,sig_handler);

	/* check if syslogd is running in buffer mode or file mode */
	if (rdb_get_single(SYSLOG_LOGTOFILE_DB_NAME, temp, 10) == 0 && strcmp(temp, "1") == 0) {
		syslog(LOG_DAEMON | LOG_DEBUG, "%s is 1. run supervisor with file mode\n", SYSLOG_LOGTOFILE_DB_NAME);
		buffer_mode = 0;
	}

	/* set heartbeat timeout for portmanagers*/
	if ((rdb_get_single_int(PORTMGR_HEARTBEAT_TIMEOUT, &portmgr_heartbeat_timeout) != 0)
		|| portmgr_heartbeat_timeout < 2) {
		portmgr_heartbeat_timeout = HEARTBEAT_TIMEOUT;
		syslog(LOG_DAEMON | LOG_DEBUG, "heartbeat timeout is set to default (%d seconds)\n", portmgr_heartbeat_timeout);
	} else {
		portmgr_heartbeat_timeout *= 60;
		syslog(LOG_DAEMON | LOG_DEBUG, "heartbeat timeout is set to (%d seconds)\n", portmgr_heartbeat_timeout);
	}

	/* fix a compiling warning - caused by the unusual initialization of logfd
	 * inside of the following loop
	 */

	while(1) {
		/* monitor node id */
		if(!buffer_mode) {
			if(stat(SYSLOG_FILENAME,&sb)>=0) {
				if(sb.st_ino!=pino) {
					if(logfid>=0) {
						printf("Supervisor: logfile changed\n");

						/* clear logfile */
						kill(popen_pid,SIGKILL);
					}
				}

				pino=sb.st_ino;
			}
		}

		/* monitor if child is running */
		if(logfid>=0) {
			if(waitpid(popen_pid,&popen_stat,WNOHANG)>0) {
				close(logfid);
				logfid=-1;
			}
		}

		/* open log */
		if(logfid<0) {
			if (buffer_mode) {
				popen_pid = popen2(NULL,&logfid,"logread","logread","-f",NULL);
			} else {
				popen_pid = popen2(NULL,&logfid,"tail","tail","-fn0",SYSLOG_FILENAME,NULL);
			}
			if (popen_pid<0) {
				fprintf(stderr,"popen2() failed (buffer_mode=%d)\n",buffer_mode);
				return -1;
			}
			syslog(LOG_DAEMON | LOG_DEBUG, "Opened log pipe.\n");
		}

		FD_ZERO(&rfds);

		if(logfid>=0)
			FD_SET(logfid, &rfds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		rval = select(logfid+1, &rfds, NULL, NULL, &tv);

		if (rval == -1) {
			/* continue if interrupted by a signal */
			if(errno==EINTR) {
				#if 0
				syslog(LOG_ERR,"punk!");
				#endif
				continue;
			}

			perror("select()");
			return -1;
		} else if (rval) {
			readb = read(logfid, Line, MAX_LINE);
			if(readb < 1) {
				syslog(LOG_DAEMON | LOG_INFO, "Read from log pipe failed.\n");
				printf("Supervisor: Exit %d\n", __LINE__);
				if(logfid>=0)
					kill(popen_pid,SIGKILL);
			} else {
				rval = cbuf_push(cb,Line,readb);
			}
		} else {
			rval = rules_poll();
			if (rval != RULES_OK) {
				syslog(LOG_DAEMON | LOG_INFO, "Exit: %d\n", __LINE__);
				printf("Supervisor: Exit %d\n", __LINE__);
				goto exit;
			}
		}

		do {
			rval = cbuf_getline(cb,Line,MAX_LINE);
			if (rval) {
				rval = rules_logcheck(Line);
				if (rval != RULES_OK) {
					syslog(LOG_DAEMON | LOG_INFO, "Exit: %d\n", __LINE__);
					printf("Supervisor: Exit %d\n", __LINE__);
					goto exit;
				}
			}
		} while (rval);
	}

exit:

    /* write last reboot reason so we can put it in the log at next restart */
	if (reboot) {
		(void)rdb_update_single(SYSLOG_LASTREBOOT_DB_NAME, Exit_Reason, 0, 0, 0, 0);
	}

	rdb_close_db();

	syslog(LOG_DAEMON | LOG_INFO, "Supervisor exiting (Exit_Reason=%s)\n",Exit_Reason); /* Without this, pclose blocks. */
	if(logfid>=0) {
		kill(popen_pid,SIGKILL);
		waitpid(popen_pid,&popen_stat,0);
		close(logfid);
	}
	cbuf_free(cb);

	logfile_append(logfile,"%s\n",Exit_Reason);

	printf("supervisor: reboot, reason %s\n",Exit_Reason);
	if (reboot) system("reboot");
	return 0;
}
