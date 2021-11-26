
#include <stdio.h>


#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <rdb_ops.h>
#include <sys/times.h>

#define __USE_XOPEN
#include <time.h>
#include <cdcs_syslog.h>
#include <sys/select.h>
#include <ctype.h>
#include <sys/timex.h>

#include "rbtreehash-dlist.h"

#define TIME_SOURCE_NAME_MAX_LEN		64
#define TIME_SOURCE_CONFIG_MAX_LINE_LEN		256
#define TIME_SOURCE_RDB_VALUE_MAX_LEN		256

#define INT_MAX 	( ~((int)1<<( sizeof(int)*8-1)) )

#define SAFE_STRNCPY(d,s,l)	{ strncpy((d),(s),(l)); (d)[(l)-1]=0; } while(0)
#define ABS(c)			(((c)>=0)?(c):(-(c)))
#define GET_SEC_ACCURACY(x)	( ((x)+999)/1000 )

#define MAX_GAP_SEC_FROM_BASE_TIME		60

static char SAVED_DATETIME_NAME[] = "system.saved.timeDate";
static char LOG_FILE_PATH[] = "/tmp/timedaemon.log";

struct time_src_t;

// local functions
int time_convert_func_date(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_mobile_time(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_network_time(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_gps_time(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_brower_time(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_ntp_time(struct time_src_t* time_src,const char* str,time_t* time_new);
int time_convert_func_saved_time(struct time_src_t* time_src,const char* str,time_t* time_new);

typedef int (*time_convert_func_t)(struct time_src_t* time_src,const char* str,time_t* time_new);

// time source
struct time_src_t {
	char name[TIME_SOURCE_NAME_MAX_LEN];
	char rdbs[TIME_SOURCE_CONFIG_MAX_LINE_LEN];

	int accuracy;					// accuracy (ms)
	time_convert_func_t time_conv_func;		// convert

	int time_read_cnt;
	int time_min_read_cnt;

	time_t time_diff;		// real-time difference average (different from current time)

	int priority;			// tne lower the higher priority
	int incorrect_clock;

	clock_t time_last_updated;
};

// time convert info
struct time_conv_func_info_t {
	const char* name;
	time_convert_func_t time_conv_func;
};

// opt struc
static struct option long_options[] = {
	{"config", 1, 0, 'f'},
	{"verbose", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{"execute", 1, 0, 'e'},
	{0, 0, 0, 0}
};

// time convert function list
static struct time_conv_func_info_t time_convert_func_list[]={
	{"date",time_convert_func_date},
	{"mobile",time_convert_func_mobile_time},
	{"network", time_convert_func_network_time},
	{"ntp",time_convert_func_ntp_time},
 	{"gps",time_convert_func_gps_time},
	{"browser",time_convert_func_brower_time},
	{"saved",time_convert_func_saved_time},
	{0,0}
};


#define TIMEDAEMON		"timedaemon"
#define TIMEDAEMON_CONF		"/etc/timedaemon.conf"

// command line options
static char* config_file=TIMEDAEMON_CONF;
static int verbose=0;

// running parameters
static struct rdb_session* rdb=NULL;
static struct strbt_t* rdb_rbt=NULL;
static int term=0;
static int minimum_read=10;

static const int clock_error_period=24*60*60;
static const int clock_error_increase=1000;


static time_t convert_utc_to_local(struct tm* tm_utc)
{
	time_t utc;
	
	tzset();
	
	// make utc
	utc=timegm(tm_utc);
	
	return utc;
}

static int is_after_design_time(time_t time_now)
{
	struct tm tm_design;
	time_t time_design;

	memset(&tm_design,0,sizeof(tm_design));
	tm_design.tm_year=2012-1900;
	tm_design.tm_mon=5-1;
	tm_design.tm_mday=18-0;

	time_design=mktime(&tm_design);

	return time_design<=time_now;
}

int time_convert_func_brower_time(struct time_src_t* time_src,const char* str, time_t* time_new)
{
	struct tm tm_utc;

	// * java browser time
	// ex) 2012.05.17-05:33

	memset(&tm_utc,0,sizeof(tm_utc));
	if(!strptime(str,"%Y.%m.%d-%H:%M",&tm_utc))
		return -1;

	// convert localtime
	*time_new=convert_utc_to_local(&tm_utc);

	return 0;
}

int time_convert_func_network_time(struct time_src_t* time_src,const char* str, time_t* time_new)
{
	struct tm tm_utc;

	// * network time like 2015-11-11 05:33:20

	memset(&tm_utc,0,sizeof(tm_utc));
	if(!strptime(str,"%Y-%m-%d %H:%M:%S",&tm_utc))
		return -1;

	// convert localtime
	*time_new=convert_utc_to_local(&tm_utc);

	return 0;
}

int time_convert_func_gps_time(struct time_src_t* time_src,const char* str,time_t* time_new)
{
	struct tm tm_utc;

	// * gps time - ddmmyy hhmmss
	// ex) 311215 235959
	if(!strptime(str,"%d%m%y %H%M%S",&tm_utc))
		return -1;

	*time_new=convert_utc_to_local(&tm_utc);

	return 0;
}

int time_convert_func_ntp_time(struct time_src_t* time_src,const char* str,time_t* time_new)
{
	double diff;

	if( sscanf(str,"%lf",&diff)!=1 )
		return -1;

	*time_new=time(NULL)+(time_t)diff;
	return 0;
}

int time_convert_func_mobile_time(struct time_src_t* time_src,const char* str,time_t* time_new)
{
	// * cnsmgr or at manager time - different from system clock
	// ex) -226396sec

	*time_new=atol(str)+time(NULL);

	return 0;
}

int time_convert_func_date(struct time_src_t* time_src,const char* str,time_t* time_new)
{
	struct tm tm_date;

	// * standard date time format - YYYY/MM/DD HH:MM:SS
	// ex) 2010/05/10 23:59:59

	memset(&tm_date,0,sizeof(tm_date));
	if( !strptime(str,"%Y/%m/%d %H:%M:%S",&tm_date) )
		return -1;

	// convert localtime
	tm_date.tm_isdst=-1;
	*time_new=mktime(&tm_date);

	return 0;
}

int time_convert_func_saved_time(struct time_src_t* time_src,const char* str,time_t* time_new)
{
	struct tm tm_saved;

	// * saved date time format - MMDDHHmmYYYY.SS
	// ex) 080612192015.21

	memset(&tm_saved,0,sizeof(tm_saved));
	if( !strptime(str,"%m%d%H%M%Y.%S",&tm_saved) )
		return -1;

	// convert localtime
	tm_saved.tm_isdst=-1;
	*time_new=mktime(&tm_saved);

	return 0;
}



void print_usage(FILE* fp) {

	fprintf(fp,
		TIMEDAEMON " v1.0\n"
		"\n"
		"USAGE\n"
		"\t " TIMEDAEMON "[OPTION]...\n"
		"\n"
		"OPTIOIN\n"
		"\t -f, --config,   \t specify the configuration file (default:" TIMEDAEMON_CONF ")\n"
		"\t -h, --help,     \t print this usage\n"
		"\t -v, --verbose,  \t enable verbose mode\n"
		"\t -e, --execute,  \t run a script when the system time is synchronized\n"

		"\n"
	);
}

void fini_locals()
{
	if(rdb_rbt)
		strbt_destroy(rdb_rbt);

	if(rdb)
		rdb_close(&rdb);

}

int init_locals()
{
	unlink(LOG_FILE_PATH);
	FILE* loggingFile = 0;
	int level = 0xFFFFFFFF; // LOG_USER|LOG_EMERG|LOG_ALERT|LOG_CRIT|LOG_ERR|LOG_WARNING|LOG_NOTICE|LOG_INFO|LOG_DEBUG;

	if (verbose) {
		loggingFile = fopen(LOG_FILE_PATH, "a");
		int rc = errno;

		if (loggingFile == 0) {
			fprintf(stderr, "Unable to open logging file:%d\n", rc);
			perror(0);
			fflush(stderr);
			goto err;
		}
	}

	NTCLOG_INIT_FULL("timedaemon", LOG_PID, LOG_LOCAL5, level, level, loggingFile);

	// open rdb database
	if( rdb_open(NULL,&rdb)<0 ) {
		NTCLOG_ERR("failed to open rdb driver - %s",strerror(errno));
		goto err;
	}

	if(verbose)
		NTCLOG_DEBUG("Starting");

	// create rdb_rbt
	if(!(rdb_rbt=strbt_create(sizeof(struct time_src_t)))) {
		NTCLOG_ERR("failed to create rdb_rbt");
		goto err;
	}

	return 0;
err:

	return -1;
}


void sig_handler(int sig_no)
{
	switch(sig_no) {
		case SIGTERM:
			term=1;
			NTCLOG_ERR("SIG_TERM caught");
			break;

		default:
			NTCLOG_ERR("signal caught - sig_no=%d",sig_no);
	}
}

static int is_blank_line(const char* str)
{
	while(*str) {
		if(!isspace(*str++))
			return 0;
	}

	return 1;
}

static int update_rdb_str(const char* var,const char* val)
{
	int rc;

	if( (rc=rdb_set_string(rdb, var, val))<0 ) {
		if(errno==ENOENT) {
			rc=rdb_create_string(rdb, var, val, 0, 0);
		}
	}

	if(rc<0) {
		NTCLOG_ERR("failed to update rdb(%s) - %s",val,strerror(errno));
	}

	return rc;
}

static int update_rdb_int(const char* var,long long val)
{
	char val_str[256];

	snprintf(val_str,sizeof(val_str),"%lld",val);
	return update_rdb_str(var,val_str);
}

static const char* get_rdb_values(const char* rdbs)
{
	char rdb_vars[TIME_SOURCE_CONFIG_MAX_LINE_LEN];

	static char value[TIME_SOURCE_CONFIG_MAX_LINE_LEN];
	int len;
	int value_len;

	char* saveptr;
	char* token;
	int valid;
	
	// assume time is valid
	valid=1;

	SAFE_STRNCPY(rdb_vars,rdbs,sizeof(rdb_vars));

	token=strtok_r(rdb_vars,":",&saveptr);
	
	// check "xxx.valid" rdb value - TODO: we need to integrate this to the config file
	if(token) {
		char* dot=0;
		
		char mrdb_valid[TIME_SOURCE_CONFIG_MAX_LINE_LEN];
		char mrdb[TIME_SOURCE_CONFIG_MAX_LINE_LEN];
		
		strcpy(mrdb,token);
		
		// search last dot
		if( (dot=strrchr(mrdb,'.'))!=0 )
			*dot=0;
		
		snprintf(mrdb_valid,sizeof(mrdb_valid),"%s.valid",mrdb);
		
		len=sizeof(value);
		
		if(rdb_get(rdb,mrdb_valid,value,&len)<0) {
			value[0]=0;
		}
		
		// return 
		if(!strcmp(value,"invalid"))
			valid=0;
	}
	
	// init
	value_len=0;
	value[0]=0;

	while(token) {

		// we need 1 space (null termination) for the first lap and 2 spaces (space + null termination) for the following lap
		if((value[0] && (sizeof(value)-value_len<2)) || (sizeof(value)-value_len<1) ) {
			NTCLOG_ERR("too big merging value - %s",rdbs);
			goto err;
		}

		// append a space if this lap is appending
		if(value[0]) {
			value[value_len++]=' ';
			value[value_len]=0;
		}

		// read
		len=sizeof(value)-value_len-1;
		if( rdb_get(rdb,token,&value[value_len],&len)<0 ) {
			// if not first
			if(value[0])
				NTCLOG_ERR("failed to read rdb (%s) - %s",token,strerror(errno));
			valid=0;
		}

		// put null termination at the end
		value[value_len+len]=0;
		// we cannot rely on the len because the value from rdb_get_single() may have NULL termination or may not!
		value_len=strlen(value);

		token=strtok_r(NULL,":",&saveptr);
	}

	// do not take value if not valid
	if(!valid)
		goto err;
	
	return value;

err:
	return NULL;
}

int main(int argc,char* argv[])
{
	int c;
	int i;

	FILE* fp=NULL;

	char* line_buf;
	char* time_src_name;
	char* time_source_rdb_var;
	char* conv_func_name;

	int accuracy;
	int min_read_cnt;

	int line_no;
	int opt_idx;

	struct strbt_t* convert_rbt=NULL;

	clock_t now;
	time_t time_now;
	long ticks_per_sec;

	struct time_src_t temp_time_src;
	struct time_conv_func_info_t* time_conv_info;

	const char* rdb_rbt_var;
	const char* blink_str="";

	fd_set rfds;
	struct timeval tv;
	int retval;

	char names[1024];
	int names_len;

	struct time_src_t* clk_src;

	char* token;
	char* token2;
	char* saveptr;
	const char* value;
	time_t src_time, base_time;

	int rdbfd;

	time_t time_diff;
	time_t time_diff_deviation;

	struct time_src_t builtin_src;

	int cond_error_free;
	int cond_minimum_read;
	int cond_better_accuracy;
	int cond_out_of_accuracy;

	int clock_src_accuracy_sec;
	int incorrect_clock;
	
	struct tms tms_buf;
	
	const char* sync_script=NULL;
	
	//////////////////////////////////////////
	// take command line options
	//////////////////////////////////////////

	// interpret options
	while( (c=getopt_long(argc,argv,"f:vhe:",long_options,&opt_idx))>=0 ) switch (c) {
		case 'f':
			config_file=optarg;
			break;

		case 'h':
			print_usage(stdout);
			exit(0);
			break;

		case '?':
			print_usage(stderr);
			exit(-1);
			break;

		case 'v':
			verbose=1;
			break;
			
		case  'e':
			sync_script=optarg;
			break;
			

		default:
			print_usage(stderr);
			exit(-1);
			break;
	}

	//////////////////////////////////////////
	// initialize
	//////////////////////////////////////////

	// remap signals
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, sig_handler);

	// get tick per sec
	ticks_per_sec=sysconf(_SC_CLK_TCK);
	if (ticks_per_sec <= 0) {
		NTCLOG_ERR("initialization failure");
		goto fini;
	}

	// set current time source
	memset(&builtin_src,0,sizeof(builtin_src));
	SAFE_STRNCPY(builtin_src.name,"default",sizeof(builtin_src.name));
	builtin_src.accuracy=INT_MAX;

	// initialize
	if( init_locals()<0 ) {
		NTCLOG_ERR("initialization failure");
		goto fini;
	}


	//////////////////////////////////////////
	// build rdb_rbt
	//////////////////////////////////////////

	// allocate buffers
	line_buf=malloc(TIME_SOURCE_CONFIG_MAX_LINE_LEN);
	time_src_name=malloc(TIME_SOURCE_CONFIG_MAX_LINE_LEN);
	time_source_rdb_var=malloc(TIME_SOURCE_CONFIG_MAX_LINE_LEN);
	conv_func_name=malloc(TIME_SOURCE_CONFIG_MAX_LINE_LEN);

	if(!line_buf || !time_src_name || !time_source_rdb_var || !conv_func_name ) {
		NTCLOG_ERR("memory allocation failure - %s",strerror(errno));
		goto fini;
	}

	// create convert rbt
	if(!(convert_rbt=strbt_create(sizeof(struct time_conv_func_info_t)))) {
		NTCLOG_ERR("failed to create convert_rbt");
		goto fini;
	}


	// build convert_rbt
	for(time_conv_info=time_convert_func_list;time_conv_info->name;time_conv_info++) {
		if( strbt_add(convert_rbt,time_conv_info->name,time_conv_info)<0 ) {
			NTCLOG_ERR("failed to add convert func");
			goto fini;
		}
	}


	// open configuration file
	fp=fopen(config_file,"r");
	if(!fp) {
		NTCLOG_ERR("failed to open configuration file (%s) - %s\n",config_file,strerror(errno));
		goto fini;
	}

	// read configuration file
	line_no=0;
	while( fgets(line_buf,TIME_SOURCE_CONFIG_MAX_LINE_LEN,fp) ) {

		line_no++;

		// cut off comment
		i=0;
		while(line_buf[i] && (line_buf[i]!='#'))
			i++;
		line_buf[i]=0;

		// bypass blink line
		if(is_blank_line(line_buf))
			continue;

		//
		// time source configuration format
		//
		// time_src_name time_rdb_variable accuracy time_rdb_variable_type
		//

		c=sscanf(line_buf,"%s %s %d %s %d",time_src_name,time_source_rdb_var,&accuracy,conv_func_name,&min_read_cnt);
		if(c<4) {
			NTCLOG_ERR("incorrect format found in line %d (%s)",line_no,config_file);
			continue;
		}

		if(c<5) {
			min_read_cnt=minimum_read;
		}

		// find convert function
		if( !(time_conv_info=strbt_find(convert_rbt,conv_func_name)) ) {
			NTCLOG_ERR("incorrect convert function name in line %d (%s) - ignored",line_no,config_file);
			continue;
		}

		// build time source
		memset(&temp_time_src,0,sizeof(temp_time_src));
		SAFE_STRNCPY(temp_time_src.name,time_src_name,sizeof(temp_time_src.name));
		SAFE_STRNCPY(temp_time_src.rdbs,time_source_rdb_var,sizeof(temp_time_src.rdbs));
		temp_time_src.accuracy=accuracy;
		temp_time_src.time_conv_func=time_conv_info->time_conv_func;
		temp_time_src.time_min_read_cnt=min_read_cnt;
		temp_time_src.priority=line_no;

		NTCLOG_INFO("src=%s,accuracy=%d,type=%s,rdbs=%s",temp_time_src.name,temp_time_src.accuracy,conv_func_name,temp_time_src.rdbs);

		// get last rdb
		token=NULL;
		token2=strtok_r(time_source_rdb_var,":",&saveptr);
		while(token2) {
			token=token2;
			token2=strtok_r(NULL,";",&saveptr);
		}

		// add to rdb_rbt
		if( strbt_add(rdb_rbt,token,&temp_time_src)<0 ) {
			NTCLOG_ERR("failed to add a time source in line %d (%s)",line_no,config_file);
		}
	}

	free(line_buf);
	free(time_src_name);
	free(time_source_rdb_var);
	free(conv_func_name);

	fclose(fp);

	strbt_destroy(convert_rbt);

	//////////////////////////////////////////
	// subscribe rdb
	//////////////////////////////////////////

	// get first time source
	if(!(clk_src=strbt_get_first(rdb_rbt,&rdb_rbt_var))) {
		NTCLOG_ERR("no time source found in %s",config_file);
		goto fini;
	}

	// subscribe loop
	while(clk_src) {
		if (strcmp(clk_src->name, "saved") != 0) {
			if(verbose) {
				NTCLOG_DEBUG("subscribing to %s", clk_src->name);
			}

			// create rdb variable
			if(rdb_create_string(rdb,rdb_rbt_var,blink_str,0,0)<0) {
				if(errno!=EEXIST)
					NTCLOG_ERR("failed to create rdb variable(%s) - %s",rdb_rbt_var,strerror(errno));
			}
			
			// subscribe
			if( rdb_subscribe(rdb,rdb_rbt_var)<0 ) {
					NTCLOG_ERR("failed to subscribe rdb variable(%s) - %s",rdb_rbt_var,strerror(errno));
			}
		}

		clk_src=strbt_get_next(rdb_rbt,&rdb_rbt_var);
	}

	// If there is a value in system.saved.timeDate, and this is the first timeout of the loop,
	// set the time from system.saved.timeDate.
	// In the loop, system.saved.timeDate will be checked for validity and used if it is suitable.

	char savedTimeDate[16]; // Allow space for "MMDDHHmmYYYY.SS" - 15 characters + terminating '\0'.
	int len = sizeof(savedTimeDate)-1;
	int firstTime = 
		(rdb_get(rdb,SAVED_DATETIME_NAME,savedTimeDate,&len) == 0) &&
		(len != 0);

	//////////////////////////////////////////
	// select loop
	//////////////////////////////////////////

	rdbfd = rdb_fd(rdb);

	while(!term) {
		FD_ZERO(&rfds);
		FD_SET(rdbfd, &rfds);

		// wait one sec
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		retval = select(rdbfd+1, &rfds, NULL, NULL, &tv);

		// get current tick and time
		now=times(&tms_buf);
		time_now=time(NULL);

		if (retval<0) {
			if(errno==EINTR) {
				NTCLOG_ERR("punk!");
				continue;
			}
			else {
				NTCLOG_ERR("select() failed - %s",strerror(errno));
				break;
			}
		}

		if (!retval) {
			if (firstTime) {
				if(verbose) {
					NTCLOG_DEBUG("firstTime, using %s",savedTimeDate);
				}
				token2=SAVED_DATETIME_NAME;
				firstTime=0;
			} else {
				token2=NULL;
			}
		}
		else {
			// get triggered rdb names
			names_len=sizeof(names) - 1;
			int rc = rdb_getnames(rdb,blink_str,names,&names_len,TRIGGERED);
			if( rc<0 ) {
				
				NTCLOG_ERR("rdb_get_names() failed - %s",strerror(errno));
				
				/* ignore incorrect behaviour of RDB driver */
				names_len=0;
				// goto fini;
			}

			names[names_len]=0;

			if(verbose) {
				NTCLOG_DEBUG("rdb notify list - '%s'",names);
			}

			// update time source
			token2=strtok_r(names,"&",&saveptr);
		}

		// read all rdb variables
		while( (token=token2)!=NULL ) {
			token2=strtok_r(NULL,"&",&saveptr);

			// get clock source
			if( !(clk_src=strbt_find(rdb_rbt,token)) ) {
				NTCLOG_CRIT("internal structure error - cannot find rdb(%s) in rbt",token);
				continue;
			}

			if(verbose) {
				NTCLOG_DEBUG("looking for clock source - rdb=%s",token);
			}

			// bypass if failure
			if( (value=get_rdb_values(clk_src->rdbs))==NULL ) {
				continue;
			}

			// bypass if blank
			if(!value[0]) {
				if(verbose) {
					NTCLOG_INFO("bypassing source [%s] - blank trigger",clk_src->name);
				}
				continue;
			}

			// bypass if converting failure
			if(clk_src->time_conv_func(clk_src,value,&src_time)<0 ) {
				NTCLOG_ERR("clock time convert failure - rdbs=%s,value=%s",clk_src->rdbs,value);
				continue;
			}

			// bypass if not valid
			if(!is_after_design_time(src_time)) {
				// print only for verbose mode
				if(verbose) {
					NTCLOG_ERR("invalid time from source [%s] - %s",clk_src->name,ctime(&src_time));
				}
				continue;
			}

			if (!strcmp(clk_src->name, "ntp")) {
				if( (value=get_rdb_values("system.ntp.time.base")) != NULL && value[0]) {
					base_time=atoi(value);
					NTCLOG_DEBUG("base time = %ld, time_now = %ld",base_time,time_now);

					if(ABS(base_time - time_now) > MAX_GAP_SEC_FROM_BASE_TIME) {
						NTCLOG_ERR("error: The gap between ntp base and current time(%d) is too big",(int)(ABS(base_time - time_now)));
						continue;
					}
				}
			}

			// update time source
			clk_src->time_last_updated=now;


			// get time difference from current system time to the source time
			time_diff=src_time-time_now;

			if(clk_src->time_read_cnt<INT_MAX)
				clk_src->time_read_cnt++;

			// get deviation
			time_diff_deviation=ABS(time_diff-clk_src->time_diff);
			clk_src->time_diff=time_diff;

			/*

			to update the system clock, we have the following conditions

			1. standard deviation has to be zero - no errors (or averaged enough)

			2. the time source has to have read more than minimum read count
			3. the time source's accuracy has to be smaller than the current time source
			4. the time source's actual averaged difference has to exceed its own accuracy

			*/

			clock_src_accuracy_sec=GET_SEC_ACCURACY(clk_src->accuracy);

			cond_error_free=0;
			cond_minimum_read=0;
			cond_better_accuracy=0;
			cond_out_of_accuracy=0;


			// better readable than better performance here
			cond_minimum_read=clk_src->time_min_read_cnt<=clk_src->time_read_cnt;
			cond_error_free=(clk_src->time_read_cnt>1)?time_diff_deviation<=clock_src_accuracy_sec:1;
			if (clk_src->time_read_cnt>1) {
				NTCLOG_DEBUG("cond_error_free(%d) : time_diff_deviation(%d) <= clock_src_accuracy_sec(%d)?", cond_error_free, (int)time_diff_deviation, clock_src_accuracy_sec);
			} else {
				NTCLOG_DEBUG("cond_error_free(1) : clk_src->time_read_cnt(%d)", clk_src->time_read_cnt);
			}

            cond_better_accuracy=(builtin_src.accuracy>clk_src->accuracy) || ((builtin_src.accuracy==clk_src->accuracy) && (builtin_src.priority>=clk_src->priority));
			NTCLOG_DEBUG("cond_better_accuracy(%d) : ", cond_better_accuracy);
			NTCLOG_DEBUG("           builtin_src.accuracy(%d) > clk_src->accuracy(%d)? or", builtin_src.accuracy, clk_src->accuracy);
			NTCLOG_DEBUG("          (builtin_src.accuracy(%d) = clk_src->accuracy(%d) and", builtin_src.accuracy, clk_src->accuracy);
			NTCLOG_DEBUG("           builtin_src.priority(%d) = clk_src->priority(%d))?", builtin_src.priority, clk_src->priority);

			cond_out_of_accuracy=ABS(clk_src->time_diff)>clock_src_accuracy_sec;
			NTCLOG_DEBUG("cond_out_of_accuracy(%d) : ABS(clk_src->time_diff)(%d) > clock_src_accuracy_sec(%d)", cond_out_of_accuracy, (int)ABS(clk_src->time_diff), clock_src_accuracy_sec);

			if(verbose) {
				NTCLOG_DEBUG("clock source name = %s",clk_src->name);

				// display the triggered clock source
				NTCLOG_DEBUG("      clock cur = %ld (%s)",time_now,ctime(&time_now));
				NTCLOG_DEBUG("      clock src = %ld (%s)",src_time,ctime(&src_time));

				// display the triggered clock source
				NTCLOG_DEBUG("      read count = %d",clk_src->time_read_cnt);
				NTCLOG_DEBUG("      accuracy = %d",clk_src->accuracy);
				NTCLOG_DEBUG("      current time diff = %ld (sec)",time_diff);
				NTCLOG_DEBUG("      clock src diff avg = %ld (sec)",clk_src->time_diff);

				// display the builtin clock
				if(builtin_src.name[0]) {
					NTCLOG_DEBUG("builtin clock source = %s",builtin_src.name);
					NTCLOG_DEBUG("builtin source accuracy = %d",builtin_src.accuracy);
				}

				NTCLOG_DEBUG("min_read=%d,zero_devi=%d,accuracy=%d,out_of_accuracy=%d",cond_minimum_read,cond_error_free,cond_better_accuracy,cond_out_of_accuracy);
			}

			// read from the begining if we see any error
			if(!cond_error_free) {
				if(verbose) {
					NTCLOG_ERR("out of accuracy in time source [%s] - diff=%ld(sec),acc=%d(ms)",clk_src->name,time_diff_deviation,clk_src->accuracy);
				}
				NTCLOG_DEBUG("set clk_src->time_read_cnt=0");
				clk_src->time_read_cnt=0;
				continue;
			}

			// bypass if not read enough
			if(!cond_minimum_read) {
				NTCLOG_DEBUG("cond_minimum_read not reached, continue...");
				continue;
			}

			// too much long - only print when we get a different source
			{
				static char prev_clk_src[TIME_SOURCE_NAME_MAX_LEN]={0,};

				if(strcmp(prev_clk_src,clk_src->name)) {
					if(verbose)
						NTCLOG_INFO("new time source : [%s] --> [%s] %s (%ld sec)", prev_clk_src, clk_src->name,ctime(&src_time),clk_src->time_diff);
					SAFE_STRNCPY(prev_clk_src,clk_src->name,sizeof(prev_clk_src));
				}
			}

			// correct the clock source if we are using any better clock now
			if((builtin_src.accuracy<clk_src->accuracy) || ((builtin_src.accuracy==clk_src->accuracy) && (builtin_src.priority<clk_src->priority))) {
				NTCLOG_DEBUG("correct the clock source if we are using any better clock now...");
				incorrect_clock=ABS(clk_src->time_diff)>clock_src_accuracy_sec;
				if(!clk_src->incorrect_clock && incorrect_clock && verbose)
					NTCLOG_ERR("time source [%s] is incorrect by current time source [%s]",clk_src->name,builtin_src.name);

				clk_src->incorrect_clock=incorrect_clock;
				NTCLOG_DEBUG("clk_src->incorrect_clock = %d", clk_src->incorrect_clock);
			}

			// update builtin time source if needed
			if(cond_better_accuracy) {
				NTCLOG_DEBUG("cond_better_accuracy is 1, update builtin time source if needed...");
				if(clk_src->incorrect_clock) {
					NTCLOG_ERR("not using time source (incorrect clock) - [%s]",clk_src->name);
				}
				else {
					if((builtin_src.priority!=clk_src->priority) || (builtin_src.accuracy!=clk_src->accuracy) )
						NTCLOG_INFO("system clock changed time source from [%s] to [%s] ",builtin_src.name,clk_src->name);

					if(verbose)
						NTCLOG_INFO("candidate - name=%s,accuracy=%d,read_cnt=%d,diff=%ld",clk_src->name,clk_src->accuracy,clk_src->time_read_cnt,clk_src->time_diff);

					// update builtin time source
					memcpy(&builtin_src,clk_src,sizeof(builtin_src));
					builtin_src.time_last_updated=now;
					NTCLOG_DEBUG("updated built-in time source with [%s]",clk_src->name);
				}
			}
		}


		// increase clock_error_increase if clock_error_period past
		if( (builtin_src.accuracy<(INT_MAX-clock_error_increase)) && (now-builtin_src.time_last_updated)/ticks_per_sec > clock_error_period ) {
			builtin_src.accuracy+=clock_error_increase;
			NTCLOG_INFO("setting new system clock accuracy - %d (ms)",builtin_src.accuracy);

			// change updated time
			builtin_src.time_last_updated=now;
		}


		// update local time if needed
		if(builtin_src.time_read_cnt) {
			time_t time_new;

			// setting system time if out of accuracy
			if(ABS(builtin_src.time_diff)>GET_SEC_ACCURACY(builtin_src.accuracy)) {

				// print compensation information
				if(verbose) {
					NTCLOG_INFO("applying clock new / time source=%s,accuracy=%d,read_cnt=%d,diff=%ld",builtin_src.name,builtin_src.accuracy,builtin_src.time_read_cnt,builtin_src.time_diff);
				}

				// set new system time
				// TODO: use adjtime() - to speed up or slow down the system clock in order to make a gradual adjustment.
				time_new=time_now+builtin_src.time_diff;
				if(verbose) {
					NTCLOG_INFO("clock cur = %s",ctime(&time_now));
					NTCLOG_INFO("clock new - %s",ctime(&time_new));
					NTCLOG_INFO("clock diff. = %ld (sec)",builtin_src.time_diff);
				}

				NTCLOG_INFO("syncing system clock to [%s]",builtin_src.name);

				if( stime(&time_new)<0 ) {
					NTCLOG_ERR("stime() failed - %s",strerror(errno));
				}
				else {
					char savedTimeDate[200];
					struct tm* currentLocalTime = localtime(&time_new);

					savedTimeDate[0] = '\0';
					
					update_rdb_str("system.time.source",builtin_src.name);
					update_rdb_int("system.time.accuracy",builtin_src.accuracy);
					update_rdb_int("system.time.updated",now/ticks_per_sec);

					if (currentLocalTime != NULL) {
						strftime(savedTimeDate, sizeof(savedTimeDate), "%m%d%H%M%Y.%S", currentLocalTime);
					}

					if(verbose) {
						NTCLOG_INFO("setting system.saved.timeDate to %s",savedTimeDate);
						fprintf(stderr, "setting system.saved.timeDate to %s\n", savedTimeDate);
					}
					update_rdb_str(SAVED_DATETIME_NAME, savedTimeDate);

					/* run a sync script */
					if(sync_script) {
						NTCLOG_INFO("calling sync script - %s",sync_script);
						system(sync_script);
					}
				}

				// reset all time sources
				clk_src=strbt_get_first(rdb_rbt,&rdb_rbt_var);
				while(clk_src) {
					clk_src->time_read_cnt=0;
					clk_src=strbt_get_next(rdb_rbt,&rdb_rbt_var);
				}
			}

			builtin_src.time_read_cnt=0;
		}

	}

fini:
	// finalize
	fini_locals();

	return 0;
}
