#define _GNU_SOURCE
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

#include "../at/at.h"

static char atcmd[128];
static char atresp[2048];

static int toint(char digit)
{
	if (('0' <= digit) && (digit <= '9'))
		return digit -'0';

	return ((digit | ('a' ^ 'A')) - 'a' + 0x0a) & 0x0f;
}


#define SIMCMD_READ_BINARY	0xB0
#define SIMCMD_READ_RECORD	0xB2
#define SIMCMD_UPDATE_BINARY	0xD6
#define SIMCMD_UPDATE_RECORD	0xDC

#define SIMSW_NORMAL_ENDING	0x9000
#define SIMSW_RECORD_NOT_FOUND	0x6a83

static int send_sim_command(int cmd, int fileid, int p1, int p2, int p3, int* sw, char* buf, int buflen)
{
	int stat;
	int ok;
	char* p;
	char *val;
	int len;
	int i;

	int sw1;
	int sw2;

	// build CRSM command
	sprintf(atcmd, "AT+CRSM=%u,%u,%02u,%02u,%02u", cmd, fileid, p1, p2, p3);

	// send CRSM command
	stat = at_send(atcmd, atresp, "", &ok, sizeof(atresp));

	// check result
	if (stat!=0) {
		syslog(LOG_ERR,"failed to send AT command - %s",atcmd);
		return -1;
	}

	// check resp
	if(!ok) {
		syslog(LOG_ERR,"got failure response - %s",atcmd);
		return -1;
	}


	// +CRSM: 144,0,"1E4A172C27FE05004000"

	// duplicate atresp to print the orignal atresp
	p=strdupa(atresp);
	// get SW1
	p=strtok(p,",");
	if(!p) {
		syslog(LOG_ERR,"incorrect resp format (SW1 not found) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}
	sw1 = atoi(p);

	// get SW2
	p=strtok(0,",");
	if(!p) {
		syslog(LOG_ERR,"incorrect resp format (SW2 not found) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}
	sw2 = atoi(p);

	// set sw
	if(sw)
		*sw=(sw1<<8) | sw2;

	// get value
	p=strtok(0,",");
	if(!p) {
		syslog(LOG_ERR,"incorrect resp format (VAL not found) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}

	val=p;

	// search double quotation mark
	p=strchr(p,'"');
	if(!p) {
		syslog(LOG_ERR,"incorrect resp format (VAL does not have staring double quotation mark) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}

	// get val content
	val=p+1;

	// put null
	p=strchr(val,'"');
	if(!p) {
		syslog(LOG_ERR,"incorrect resp format (VAL does not have ending double quotation mark) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}
	*p=0;

	// check val validation - digit
	len=strlen(val);

	if(len%2) {
		syslog(LOG_ERR,"incorrect resp foramt (VAL does not have even number of hex-digit) - cmd=%s,resp=%s",atcmd,atresp);
		return -1;
	}

	if(buflen < (len/2)) {
		syslog(LOG_ERR,"insufficient buffer size [hex] = buflen=%d,len=%d,cmd=%s,resp=%s",buflen,len,atcmd,atresp);
		return -1;
	}

	// convert ascii hex to hex
	i=0;
	while (*val && (i<buflen)) {
		buf[i++]=(char)(toint(*val)<<4 | toint(*(val+1)));
	}

	return i;
}

int read_simfile_record(int fileid,int recordno,char* buf,int buflen)
{
	int sw;
	int stat;

	stat=send_sim_command(SIMCMD_READ_RECORD,fileid,recordno,4,0,&sw,buf,buflen);

	if(sw==SIMSW_RECORD_NOT_FOUND)
		return -1;
	else if(sw!=SIMSW_NORMAL_ENDING) {
		syslog(LOG_ERR,"SIM file read-record access failure - fileid=0x%04x, sw=0x%04x",fileid,sw);
		return -1;
	}

	return 0;
}

/*
static int read_simfile_binary(int fileid,char* buf,int buflen)
{
	int sw;
	int stat;

	stat=send_sim_command(SIMCMD_READ_BINARY,fileid,0,0,0,&sw,buf,buflen);
	if(stat<0)
		return -1;

	if(sw!=SIMSW_NORMAL_ENDING) {
		syslog(LOG_ERR,"SIM file read-binary access failure - fileid=0x%04x, sw=0x%04x",fileid,sw);
		return -1;
	}

	return 0;
}
*/
