#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

static int sig_term=0;

void signal_handler(int signal)
{
	switch(signal) {
		case SIGTERM:
			syslog(LOG_ERR,"SIGTERM caught");
			sig_term=1;
			break;
			
		default:
			//syslog(LOG_ERR,"signal caught - %d\n",signal);
			break;
	}
}

int main(int argc,char* argv[])
{
	char* fname=basename(argv[0]);
	char* cmd_to_run;
	char** cmd_arg;
	long sec_to_wait;
	int stat;
	char* timer_name;
	
	struct timeval tv;
	
	clock_t clock_start;
	clock_t clock_now;
	clock_t past_sec;
	
	int period;
	pid_t pid;
	int len;
	
	struct tms tmsbuf;
	
	if(argc<4) {
		fprintf(stderr, "%s - run on time\n",fname);
		fprintf(stderr, "\nUsage:\n");
		fprintf(stderr, "\t %s <timer name> <seconds to wait> <command line "
                        "to run on time>\n\n", fname);
		fprintf(stderr, "Options:\n");
        fprintf(stderr, "\t seconds to wait: use suffix 'p' to periodically"
                        " run the command\n");
		exit(1);
	}
	
	// get argv
	timer_name=argv[1];
	sec_to_wait=atol(argv[2]);
	cmd_to_run=argv[3];
	cmd_arg=&argv[3];
	
	len=strlen(argv[2]);
	period=len && (argv[2][len-1]=='p');
	
	openlog("runontime",LOG_PID,LOG_DEBUG);

	// override handlers
	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);

	// current time
	clock_start=clock_now=times(&tmsbuf);
	// wait for the period of time
	while(!sig_term) {
		past_sec=(clock_now-clock_start)/sysconf(_SC_CLK_TCK);
		
		if(sec_to_wait<=past_sec)  {
			if(period)
				clock_start=clock_now;
				
			syslog(LOG_INFO,"[%s] timer (%ld sec.) expired - run %s",timer_name, sec_to_wait,cmd_to_run);
			
			pid=0;
			if(period) {
				pid=vfork();
				if(pid<0)
					syslog(LOG_ERR,"failed to vfork() - %s",strerror(errno));
			}
			
			if(pid==0) {
				execvp(cmd_to_run,cmd_arg);
				syslog(LOG_ERR,"command launch failure - %s",strerror(errno));
				exit(-1);
			}
		}

		memset(&tv,0,sizeof(tv));
		tv.tv_sec=sec_to_wait-past_sec;

		// wait
		stat=select(0,0,0,0,&tv);
		// get the time
		clock_now=times(&tmsbuf);

		if(stat<0) {
			if(errno!=EINTR) {
				syslog(LOG_ERR,"punk! - %s",strerror(errno));
				break;
			}
		}
	}
	
	syslog(LOG_INFO,"[%s] timer cancelled",timer_name);

	return 0;
}
