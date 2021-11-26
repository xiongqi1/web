
#include <stdio.h> 
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/select.h>
#include <syslog.h>
#include <errno.h>

#include "rdb_ops.h"

#define DEBUG

#ifdef DEBUG
#define SYSLOG(...) syslog(LOG_INFO, __VA_ARGS__)
#else
static inline int NOLOG(const char *fmt, ...)
{
	(void)fmt; return 0;
}
#define SYSLOG NOLOG
#endif


#define DEFAULT_POLLINGINTERVAL   2

enum power_type {
  	power_none,
	power_vin_mon,
 	power_poe_mon,
};

struct adc_table_tag {
	enum power_type pt;
	
	const char* name;
	const char* sys;
	
	double scale;
	double correction;
};


#define SYS_VOLTAGE(dev,vtg)	"/sys/bus/iio/devices/iio:device" #dev "/in_voltage" #vtg "_raw"
#define RESISTOR_DIV(up,bottom)	( ( ((double)(up))+(bottom) ) / (bottom) )

struct adc_table_tag adc_table[]={
	
#if defined(ADCMAP_falcon)
	/* NWL10, NWL11 */
	{power_none,	"sysmon.router.vtemp",		SYS_VOLTAGE(0,0),1,1},
	{power_vin_mon,	"sysmon.router.vin_mon",	SYS_VOLTAGE(0,1),RESISTOR_DIV(300,30),0.929166666667},
	{power_poe_mon,	"sysmon.router.3v8poe_mon",	SYS_VOLTAGE(0,3),RESISTOR_DIV(30,10),0.9875},
			
	{power_none,	"sysmon.router.3v3_mon",	SYS_VOLTAGE(0,4),RESISTOR_DIV(30,10),0.993},
	{power_none,	"sysmon.router.3v8_mon",	SYS_VOLTAGE(0,2),RESISTOR_DIV(30,10),0.9915},
			
	#if 0
	/* resistors are put yet */
	{power_none,	"sysmon.router.5v_mon",		SYS_VOLTAGE(0,5),RESISTOR_DIV(30,10),1},
	#endif
#elif defined(ADCMAP_ioext4)
	/* NWL12 */
	#if 0
	/* resistors are put yet */
	{power_none,	"sysmon.router.vtemp",		SYS_VOLTAGE(0,0),1,1},
	#endif
	{power_vin_mon,	"sysmon.router.vin_mon",	SYS_VOLTAGE(0,1),RESISTOR_DIV(300,20),1.01149328859},
	{power_poe_mon,	"sysmon.router.3v8poe_mon",	SYS_VOLTAGE(0,3),RESISTOR_DIV(30,10),0.999175},
			
	{power_none,	"adc.ign_mon",			SYS_VOLTAGE(0,6),RESISTOR_DIV(300,20),1},
	{power_none,	"adc.xgpio_mon1",		SYS_VOLTAGE(0,2),1,1},
	{power_none,	"adc.xgpio_mon2",		SYS_VOLTAGE(0,4),1,1},
	{power_none,	"adc.xgpio_mon3",		SYS_VOLTAGE(0,5),1,1},
#endif		
	{power_none,	NULL,},
};


/*
	* rdb structure example
	
	# read rdb variables
	sysmon.router.vin_mon			<scaled value>
	sysmon.router.vin_mon.raw		<adc raw value>
	
	# configuration rdb variables
	sysmon.router.vin_mon.scale		<scale value to scale "raw value" to "scaled value">
	sysmon.router.vin_mon.correction	<yet another optional scale to correct the scaled value>
*/


// Possible Value: DCJack, PoE, DCJack+PoE
#define RDB_ROUTER_POWSRC	"sysmon.router.powersource"

static char* cmdname=0;
static volatile int active=1;

void print_usage(FILE* fp) {
	fprintf(fp,
		"Usage:\n"
		"\t %s -i PollingInterval\n"
		"\n"
		"Description:\n"
		"\t Monitor DC input Voltage\n"
		"\n"
		"Options:\n"
		"\t -i \t Polling Interval in seconds. Default: 1.(0 is not valid)\n"
		"\n", 
  		cmdname);
}

void sighandler(int sig)
{
	SYSLOG("Caught signal %d", sig);
	active=0;
}

char* get_rdb_suffix(const char* name,const char* suffix)
{
	static char rdb[256];
	
	snprintf(rdb,sizeof(rdb),"%s.%s",name,suffix);
	return rdb;
}

int write_adc_rdb_var(const char* name,const char* var,const char* val)
{
	const char* rdb;
	char prev_val[256];
	
	if(var && var[0])
		rdb=get_rdb_suffix(name,var);
	else
		rdb=name;
		 
	/* get prev val */
	if(rdb_get_single(rdb,prev_val,sizeof(prev_val)) != 0) {
		if(rdb_create_variable(rdb, val, CREATE, ALL_PERM, 0, 0)!=0) {
			syslog(LOG_ERR,"rdb_create_variable() failed (rdb=%s,val=%s) - %s",rdb,val,strerror(errno));
			goto err;
		}
	}
	
	/* set only when the value is changed */
	if(strcmp(prev_val,val)) {
		if( rdb_set_single(rdb,val) != 0) {
			syslog(LOG_ERR,"rdb_set_single() failed (rdb=%s,val=%s) - %s",rdb,val,strerror(errno));
			goto err;
		}
	}
	
	return 0;
err:
	return -1;	
}

int write_adc_rdb_var_int(const char* name,const char* var,int val)
{
	char val_str[256];
	
	snprintf(val_str,sizeof(val_str),"%d",val);
	return write_adc_rdb_var(name,var,val_str);
}

double read_adc_rdb_var_dbl(const char* name,const char* var)
{
	char* rdb;
	char scale[256];
	
	rdb=get_rdb_suffix(name,var);
		 
	if( rdb_get_single(rdb,scale,sizeof(scale)) != 0) {
		return 0;
	}
	
	/* use 1 if it is blank */
	if(!scale[0])
		return 0;
	
	/* do we have to allow zero or not? currently zero is allowed */
	return strtod(scale,NULL);
}

int get_ADCValue(const char * path) {
	FILE * stream = NULL;
	char adc_value[128]={0,};

	if (path == NULL)
		return -1;

	if ((stream = fopen(path, "r")) == NULL) {
		SYSLOG("File Open ERROR: %s", path);
		exit(-1);
	}

	if(!fgets(adc_value, sizeof(adc_value), stream)) {
		fclose(stream);
		return -1;
	}

	fclose(stream);
	return atoi(adc_value);
}
int main(int argc,char **argv)
{
	int ret;
	time_t pollingInterval=0;
	struct timeval timeout;
	char bufV[32];
	char bufR[32];
	int cnt=0;
	int adc_value=0, adc_VINmon=0, adc_PoEmon=0;
	
	double scale;
	double correction;

	if (rdb_open_db()==-1) {
		SYSLOG("Failed to open rdb");
		return 1;
	}

	/* Catch signals */
	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	signal(SIGTERM, sighandler);

	
	cmdname=(char *)basename(strdup(argv[0]));

	while ((ret = getopt(argc, argv, "i:h")) != EOF)
	{
		switch (ret)
		{
			case 'i':
				pollingInterval = atoi(optarg);
				break;
			case 'h':
				print_usage(stderr);
				return -1;
			default:
				print_usage(stderr);
				return -1;
		}
	}

	if (pollingInterval == 0) {
		pollingInterval = DEFAULT_POLLINGINTERVAL;
	}


	/* setup configuration rdb */
	for (cnt = 0; adc_table[cnt].name != NULL; cnt++) {
		/* write default scale */
		if(!read_adc_rdb_var_dbl(adc_table[cnt].name,"scale")) {
			sprintf(bufV, "%f", (double)adc_table[cnt].scale);
			write_adc_rdb_var(adc_table[cnt].name,"scale",bufV);
		}
		
		/* write default correction */
		if(!read_adc_rdb_var_dbl(adc_table[cnt].name,"correction")) {
			sprintf(bufV, "%f", (double)adc_table[cnt].correction);
			write_adc_rdb_var(adc_table[cnt].name,"correction",bufV);
		}
	}

	while(active) {
		timeout.tv_sec = pollingInterval;
		timeout.tv_usec =0;
		
		ret = select(0, NULL, NULL, NULL, &timeout);
	/////////////////////////////////////////////////////////////////////////////////////////////////
		adc_VINmon=0;
		adc_PoEmon=0;

		for (cnt = 0; adc_table[cnt].name != NULL; cnt++)
		{
			adc_value = get_ADCValue(adc_table[cnt].sys);
			
			if(adc_value == -1) {
				continue;
			}
				
			/* get user-specific scales */
			scale=read_adc_rdb_var_dbl(adc_table[cnt].name,"scale");
			correction=read_adc_rdb_var_dbl(adc_table[cnt].name,"correction");
			
			/* use the default scale if no user-specific scale is in use */
			if(!scale) {
				scale=adc_table[cnt].scale;
			}
			
			/* use default correction if no user-specific correction is in use */
			if(!correction) {
				correction=adc_table[cnt].correction;
			}
			
			/* ignore small value? */
			if ( adc_value <= 100) {
				adc_value=0;
			}
			
			/* write raw value */
			sprintf(bufV, "%f", (((double)(adc_value) * 1.85) / 4096));
			write_adc_rdb_var(adc_table[cnt].name,"raw",bufV);
			
			/* write scaled value */
			sprintf(bufV, "%0.2f", (((double)adc_value * 1.85 * scale * correction) / 4096));
			write_adc_rdb_var(adc_table[cnt].name,NULL,bufV);
			
			if (adc_table[cnt].pt==power_vin_mon)
				adc_VINmon = adc_value;
			else if (adc_table[cnt].pt==power_poe_mon)
				adc_PoEmon = adc_value;
		}
	/////////////////////////////////////////////////////////////////////////////////////////////////


	/////////////////////////////////////////////////////////////////////////////////////////////////


		bufV[0] = 0;

		if (adc_VINmon > 100)
			strcat(bufV, "DCJack");

		if (adc_VINmon > 100 && adc_PoEmon >100)
			strcat(bufV, "+");

		if (adc_PoEmon >100)
			strcat(bufV, "PoE");

		bufR[0] = 0;

		if(rdb_get_single(RDB_ROUTER_POWSRC,bufR,sizeof(bufR)) != 0) {
			rdb_create_variable(RDB_ROUTER_POWSRC, "", CREATE, ALL_PERM, 0, 0);
		}

		if (strcmp(bufR, bufV)) {
			rdb_set_single(RDB_ROUTER_POWSRC, bufV);
		}
	/////////////////////////////////////////////////////////////////////////////////////////////////
	}

	rdb_close_db();
	return 0;
}
