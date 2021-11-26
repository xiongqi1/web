#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h> 
#include <errno.h>
#include <string.h>

#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 

#include <sys/ioctl.h>
#include <sys/socket.h>

// define kernel types
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include "rdb_ops.h"

static struct rdb_session *g_rdb_session;

#define MAX_RDB_VAR_LEN	256
#define MAX_RDB_VAL_LEN 256
#define MAX_PORT	6
#define POLL_PERIOD	1

const char* default_ethtool_if="eth2";
const char* default_proc_gmac="/proc/rt3352/gmac";


void usage(FILE* sock)
{
	fprintf(sock,
		"ethmon - raeth ethernet switch monitor (RT version of switchd)\n"
		"\tdescription:\n"
		"\traether driver provides lame ethtool control that is not multi-process-free\n"
		"\tethmon centeralizes this lame ethtool access and populates rdb variables - hw.switch.port.x.xxxxx\n"
		"\n"
		"Usage: ethmon [options]\n"
				"Options:\n"
				"\t-i specify network interface (default: %s)\n"
				"\t-p specify proc file - accept multiple proc files (default: %s)\n"
				"\n",
				default_ethtool_if, default_proc_gmac
	);
}

static char rdb_var[MAX_RDB_VAR_LEN];
static char rdb_buf[MAX_RDB_VAL_LEN];

static int rdb_set_str(const char* var, const char* val)
{
	return rdb_set_string(g_rdb_session, var, val);
}

static const char* rdb_get_str(const char* var)
{
	if(rdb_get_string(g_rdb_session,var,rdb_buf,sizeof(rdb_buf))<0) {
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

static int rdb_create_str(const char* var)
{
	int stat;

	if( (stat=rdb_create_string(g_rdb_session, var, "", CREATE, ALL_PERM))<0 ) {
		if(errno!=EEXIST) {
			syslog(LOG_ERR,"failed to create %s - %s",var,strerror(errno));
			stat=-1;
		}
		else {
			stat=rdb_set_string(g_rdb_session, var, "");
		}
	}

	return stat;
}

static int init_rdb()
{
	int i;

	rdb_create_str("hw.switch.port.x.link");

	for(i=0;i<MAX_PORT;i++) {
		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.changed",i);
		rdb_create_str(rdb_var);

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.status",i);
		rdb_create_str(rdb_var);

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.link",i);
		rdb_create_str(rdb_var);

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.speed",i);
		rdb_create_str(rdb_var);

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.duplex",i);
		rdb_create_str(rdb_var);
	}

	return 0;
}

static void fini_rdb()
{
	int i;

	rdb_set_str("hw.switch.port.x.link","");

	for(i=0;i<MAX_PORT;i++) {
		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.changed",i);
		rdb_set_str(rdb_var,"");

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.status",i);
		rdb_create_str(rdb_var);

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.link",i);
		rdb_set_str(rdb_var,"");

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.speed",i);
		rdb_set_str(rdb_var,"");

		snprintf(rdb_var,sizeof(rdb_var),"hw.switch.port.%d.duplex",i);
		rdb_set_str(rdb_var,"");
	}
}

int main(int argc,char* argv[])
{
	int sock;

	int port;

	const char* ethtool_if=default_ethtool_if;
	const char* proc_gmac=default_proc_gmac;

	struct stat fstat;

	int opt;

	int chg;
	int anyport_chg;
	const char* duplex;

	struct ifreq ifr;

	struct ethtool_cmd ecmd;
	struct ethtool_value edata;

	char buf[MAX_RDB_VAL_LEN];

	while ((opt = getopt(argc, argv, "i:p:?")) != EOF) switch(opt) {
		case 'i':
			ethtool_if=optarg;
			break;

		case 'p':
			if(stat(optarg,&fstat)<0) {
				fprintf(stderr,"failed to open %s - %s\n",proc_gmac,strerror(errno));
			}
			else {
				proc_gmac=optarg;
			}

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
	openlog("ethmon", LOG_PID | LOG_PERROR, LOG_DEBUG);

	// open socket
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR,"failed to open socket - %s",strerror(errno));
		return -1;
	}

	// open rdb
	syslog(LOG_INFO,"opening database");

	if (rdb_open(NULL, &g_rdb_session) < 0) {
		syslog(LOG_ERR,"failed to open rdb - %s",strerror(errno));
		return -1;
	}

	init_rdb();

	// start monitoring
	while(1) {
		anyport_chg=0;

		for(port=0;port<MAX_PORT;port++) {

			// switch to the port
			FILE *fp=fopen(proc_gmac,"w");
			if(!fp) {
				syslog(LOG_ERR,"failed to open %s - %s",proc_gmac,strerror(errno));
				break;
			}
			fprintf(fp,"%d\n",port);
			fclose(fp);

			// build SIOCETHTOOL command for ETHTOOL_GSET
			strcpy(ifr.ifr_name, ethtool_if);
			ecmd.cmd = ETHTOOL_GSET;
			ifr.ifr_data = (caddr_t)&ecmd;

			// send SIOCETHTOOL
			if( ioctl(sock, SIOCETHTOOL, &ifr)<0 ) {
				syslog(LOG_ERR,"failed to send SIOCETHTOOL(ETHTOOL_GSET) to %s - %s",ethtool_if,strerror(errno));
				break;
			}

			// build SIOCETHTOOL command for ETHTOOL_GLINK
			strcpy(ifr.ifr_name, ethtool_if);
			edata.cmd = ETHTOOL_GLINK;
			ifr.ifr_data = (caddr_t)&edata;

			// send SIOCETHTOOL
			if(ioctl(sock, SIOCETHTOOL, &ifr)<0) {
				syslog(LOG_ERR,"failed to send SIOCETHTOOL(ETHTOOL_GLINK) to %s - %s",ethtool_if,strerror(errno));
				break;
			}

			// set speed
			sprintf(rdb_var,"hw.switch.port.%d.speed",port);
			sprintf(buf,"%d",ecmd.speed);
			chg=rdb_set_str_if_chg(rdb_var,buf)==0;

			// set duplex
			sprintf(rdb_var,"hw.switch.port.%d.duplex",port);
			switch (ecmd.duplex) {
				case DUPLEX_HALF:
					duplex = "H";
					break;

				case DUPLEX_FULL:
					duplex = "F";
					break;

				default:
					duplex = "U";
					break;
			};
			chg=rdb_set_str_if_chg(rdb_var,duplex)==0;

			// set link
			sprintf(rdb_var,"hw.switch.port.%d.link",port);
			sprintf(buf,"%d",edata.data?1:0);
			chg=rdb_set_str_if_chg(rdb_var,buf)==0;
			anyport_chg=anyport_chg || chg;

			//
			// let's do the awsome previous format that doesn't even have 1G! eww, I don't know we use this or not
			// FIXME: resolved status always returns r
			sprintf (buf,"%s%s%s%s",
				(edata.data)?"u":"d",
				(1)?"r":"-",
				(ecmd.speed>=SPEED_100)?"h":"l",
				(ecmd.duplex&DUPLEX_FULL)?"f":"-"
			);
			sprintf(rdb_var,"hw.switch.port.%d.status",port);
			rdb_set_str_if_chg(rdb_var,buf);

			// set trigger if changed
			if(chg) {
				sprintf(rdb_var,"hw.switch.port.%d.changed",port);
				rdb_set_str(rdb_var,"1");
				syslog(LOG_INFO,"port status changed = %d - spd=%d,duplex=%d,link=%d",port,ecmd.speed,ecmd.duplex,edata.data);
			}
		}

		if(anyport_chg) {
			syslog(LOG_INFO,"global link port changed");
			rdb_set_str("hw.switch.port.x.link","1");
		}

		// wait for a second and break if sleep fails - assume interrupted by signals
		if(sleep(POLL_PERIOD)!=0) {
			syslog(LOG_ERR,"punk - %s",strerror(errno));
			break;
		}
	}

	syslog(LOG_INFO,"terminating...");

	// finialize
	close(sock);

	fini_rdb();

	syslog(LOG_INFO,"closing rdb");
	rdb_close(&g_rdb_session);

	syslog(LOG_INFO,"closing log");
	closelog();

	return 0;
}

