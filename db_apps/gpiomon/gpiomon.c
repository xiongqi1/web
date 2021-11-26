#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h> 
#include <errno.h>
#include <string.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/socket.h>

#include "libgpio.h"


#include "rdb_ops.h"


#define MAX_RDB_VAR_LEN	256
#define MAX_RDB_VAL_LEN 256


static int term=0; // set when we catch SIGTERM
static int gpio_no=-1; // gpio number to monitor
static const char* gpio_script=0; // script to run when gpio is changed
static const char* gpio_rdb=0; // rdb variable name to change when gpio is changed
static int gpio_polarity_invert=0; // gpio polarity
static int gpio_max_time=1; // gpio read period



void usage(FILE* fp)
{
	fprintf(fp,
		"gpiomon - gpio input monitor\n\n"
		"description>\n"
		"\tgpiomon connects a single GPIO input pin directly to an RDB\n"
		"\tvariable. gpiomon is very similar to sysmonitor, but while\n"
		"\tsysmonitor only watches rising edges, gpiomon keeps tracking\n"
		"\tstatus all the time.\n"
		"\n"
		"Usage#1>\n"
		"\tgpiomon -i <gpio_num> -r <RDB variable> [-v] [-t <period>]\n"
		"\t* rdb variable will be set to 0 or 1 (reflecting the current\n"
		"\t  status of the GPIO).\n"
		"\n"
		"Usage#2>\n"
		"\tgpiomon -i <gpio_num> -s <script> [-v] [-t <period>]\n"
		"\t* gpiomon calls the script and gives 3 parameters - gpio number,\n"
		"\t  time past, and gpio status (0 or 1).\n"
		"\n"
		"Options>\n"
		"\t -v \t invert the GPIO polarity\n"
		"\t -t \t specify the interval of time (in seconds) between gpio reads"
		"\n"
		"\n"
	);
}

static int rdb_set_str(const char* var, const char* val)
{
	return rdb_set_single(var,val);
}

#if 0
//Unused functions

static char rdb_buf[MAX_RDB_VAL_LEN];

static const char* rdb_get_str(const char* var)
{
	if(rdb_get_single(var,rdb_buf,sizeof(rdb_buf))<0) {
		syslog(LOG_ERR,"failed to read %s - %s",var,strerror(errno));
		return 0;
	}

	return rdb_buf;
}

static int rdb_set_str_if_chg(const char* var,const char* val) 
{
	const char* prev_val;

	prev_val=rdb_get_str(var);
	if(!prev_val) {
		syslog(LOG_ERR,"failed to read %s - %s",var,strerror(errno));
		return -1;
	}

	// bypass if identical
	if(!strcmp(prev_val,val)) {
		return -1;
	}

	if( rdb_set_str(var,val)<0 ) {
		syslog(LOG_ERR,"failed to write %s - %s",var,strerror(errno));
		return -1;
	}

	return 0;
}
#endif

static int rdb_create_str(const char* var) 
{
	int stat;

	if( (stat=rdb_create_variable(var, "", CREATE, ALL_PERM, 0, 0))<0 ) {
		if(errno!=EEXIST) {
			syslog(LOG_ERR,"failed to create %s - %s",var,strerror(errno));
			stat=-1;
		}
		else {
			stat=rdb_set_single(var,"");
		}
	}

	return stat;
}

static void fini_gpio(void)
{
	gpio_free_pin(gpio_no);
}

#if 0
//Unused functions
static void gpio_handler_time(void *arg, unsigned time)
{
	syslog(LOG_INFO,"gpio handler called #1 - %d",gpio_no);

}

static void gpio_handler(void *arg)
{
	syslog(LOG_INFO,"gpio handler called #2 - %d",gpio_no);
}
#endif

int init_gpio(void)
{
	if(gpio_init("/dev/gpio")<0) {
		syslog(LOG_ERR,"failed to initialize gpio - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}

	// request
	if(gpio_request_pin(gpio_no)<0) {
		syslog(LOG_ERR,"request failed - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}

	// set to input
	if(gpio_set_input(gpio_no)<0) {
		syslog(LOG_ERR,"input setting failed - gpio=%d,err=%s",gpio_no,strerror(errno));
		return -1;
	}

	return 0;
}

static unsigned convert_gpio(const char *s)
{
	unsigned rval;
#if defined(V_GPIO_STYLE_atmel)
	switch (*s) {
		case 'A':
		case 'a': rval = 0; break;
		case 'B':
		case 'b': rval = 0x20; break;
		case 'C':
		case 'c': rval = 0x40; break;
		case 'D':
		case 'd': rval = 0x60; break;
		case 'E':
		case 'e': rval = 0x80; break;
		default: usage("Please use A0 - A31, B0 - B31, etc. for bovine");
	}
	s++;
	rval |= atoi(s) & 0x1F;
#elif defined(V_GPIO_STYLE_freescale) || defined(V_GPIO_STYLE_ti)
	char *pos;
	// s format: xx:yy (xx=bank number yy port number / or port number (whithout ":")
	rval = atoi(s)*32;
	pos=strchr(s, ':');
	if(pos) {
		rval += atoi(pos+1);
	}
	else {
		rval = atoi(s);
	}
#else
	rval = atoi(s);
#endif
	return rval;
}
void sighandler(int signo)
{
	switch(signo) {
		case SIGTERM:
			syslog(LOG_ERR,"punk - sig=%d",signo);
			term=1;
			break;
			
		default:
			syslog(LOG_ERR,"unknown sig - %d",signo);
			break;
	}
}
int main(int argc,char* argv[])
{
	int opt;
	int stat;
	
	int gpio_stat;
	int gpio_prev_stat;
	
#if !defined(V_GPIO_STYLE_freescale) || defined(V_GPIO_STYLE_ti)
	char cmd[256];
#endif

	struct timeval tv;

	while ((opt = getopt(argc, argv, "i:r:s:t:v?h")) != EOF) switch(opt) {
		case 'i':
			gpio_no=convert_gpio(optarg);
			break;

		case 'r':
			gpio_rdb=optarg;
			break;

		case 's':
			gpio_script=optarg;
			break;

		case 't':
			gpio_max_time=atoi(optarg);
			break;

		case 'v':
			gpio_polarity_invert=1;
			break;

		case ':':
			fprintf(stderr,"missing argument - %c\n",opt);
			usage(stderr);
			exit(-1);
			break;

		case 'h':
			usage(stdout);
			exit(0);
			break;

		case '?':
			fprintf(stderr,"unknown option - %c\n",opt);
			usage(stderr);
			exit(-1);
			break;

		default:
			usage(stderr);
			exit(-1);
			break;
	}

	// openlog
	openlog("gpiomon", LOG_PID | LOG_PERROR, LOG_DEBUG);

	// do validation check - gpio number
	if(gpio_no<0) {
		fprintf(stderr,"gpiomon: Input GPIO number not specified\n");
		usage(stderr);
		return -1;
	}

	// do validation check - script or rdb variable
	if(!gpio_script && !gpio_rdb) {
		fprintf(stderr,"gpiomon: Please specify either script or rdb variable\n");
		usage(stderr);
		return -1;
	}

	// open rdb
	syslog(LOG_INFO,"opening database");
	if(rdb_open_db()<0) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}

	// hook up kill signal
	signal(SIGTERM,sighandler);

	// create rdb variable
	if(gpio_rdb) {
		if(rdb_create_str(gpio_rdb)<0)
			syslog(LOG_ERR,"not able to init rdb variable - %s",gpio_rdb);
	}

	init_gpio();

	gpio_prev_stat=-1;

	// turn to Sleeping beauty waiting for the prince charming
	while(!term) {
		tv.tv_usec=100000; //100ms
		tv.tv_sec=gpio_max_time;

		// select - wait!
		stat=select(0,NULL,NULL,NULL,&tv); 

		// check if it is Charming
		if(stat==-1 && errno!=EINTR) {
			syslog(LOG_ERR,"unknwon result from select() - stat=%d,err=%s",stat,strerror(errno));
			break;
		}

		// read
		gpio_stat=gpio_read(gpio_no);

		if(gpio_polarity_invert)
			gpio_stat=!gpio_stat?1:0;
		else
			gpio_stat=gpio_stat?1:0;

		// if it is first time or changed
		if(gpio_prev_stat<0 || (gpio_prev_stat && !gpio_stat) || (!gpio_prev_stat && gpio_stat)) {
#ifdef V_PRODUCT_ntc_8000c
			syslog(LOG_NOTICE,"gpio changed - gpio=%d,stat=%d",gpio_no,gpio_stat);
#else
			syslog(LOG_INFO,"gpio changed - gpio=%d,stat=%d",gpio_no,gpio_stat);
#endif

			if(gpio_rdb) {
				//syslog(LOG_INFO,"setting gpio rdb - rdb=%s,stat=%d",gpio_rdb,gpio_stat);
				if(rdb_set_str(gpio_rdb,gpio_stat?"1":"0")<0) {
					syslog(LOG_ERR,"failed to set rdb - rdb=%s,err=%s",gpio_rdb,strerror(errno));
				}
			}
#if defined(V_GPIO_STYLE_freescale) || defined(V_GPIO_STYLE_ti)
			// first time not run the script
			if(gpio_prev_stat>=0 && gpio_script) {
				//syslog(LOG_INFO,"calling gpio script - gpio_script=%s,stat=%d",gpio_script,gpio_stat);
				if( system(gpio_script)==-1 ) {
					syslog(LOG_ERR,"failed to start the script - cmd='%s',err=%s",gpio_script,strerror(errno));
				}
			}
#else
			if(gpio_script) {
				snprintf(cmd,sizeof(cmd),"%s %d %d",gpio_script,gpio_no,gpio_stat?1:0);
				//syslog(LOG_INFO,"calling gpio script - cmd=%s,stat=%d",cmd,gpio_stat);
				if( system(cmd)==-1 ) {
					syslog(LOG_ERR,"failed to start the script - cmd='%s',err=%s",cmd,strerror(errno));
				}
			}
#endif
			gpio_prev_stat=gpio_stat;
		}
	}

	syslog(LOG_INFO,"terminating...");

	fini_gpio();

	if(gpio_rdb)
		rdb_set_str(gpio_rdb,"");

	syslog(LOG_INFO,"closing rdb");
	rdb_close_db();

	syslog(LOG_INFO,"closing log");
	closelog();

	return 0;
}

