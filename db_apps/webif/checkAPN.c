/*!
 * Copyright Notice:
 * Copyright (C) 2009 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include "rdb_ops.h"

#ifdef PLATFORM_AVIAN
#define APNLIST_DIR "/system/cdcs/www/cgi-bin"
#else
#define APNLIST_DIR "/www/cgi-bin"
#endif


int getXmlTagVal(char *mybuf, char *mytag, char *tagval, int taglen, char *myval, char *valval, int vallen) {
	char *pos = strstr(mybuf, mytag);
	char *val;

	if (!pos) return 0;
	pos += strlen(mytag);
	while (*pos) {
		if (*pos++ == '=') {
			if (*pos++ == '"') {
				val = pos;
				while (*pos++ != 0) {
					if (*pos == '"') {
						*pos = 0;
						strncpy(tagval, val, taglen);
						if (!vallen || !valval || !myval) return 1;
						pos = strstr(pos + 1, myval);
						if (!pos) return 0;
						pos += strlen(myval);
						strncpy(valval, pos, vallen + 2);
						while (*valval++) {
							if (*valval == '"') {
								*valval = 0;
								return 1;
							}
							else if (!isdigit(*valval) && !(*valval == ',' || *valval == ' '))
								return 0;
						}
						return 0;
					}
				}
			}
		}
		else if (*pos > ' ') return 0;
	}
	return 0;
}

char *getXmlNodeVal(char *mybuf, char *checkval, char *nodeval, int len) {
	char *pos = strstr(mybuf, checkval);
	if (pos) {
		strncpy(nodeval, pos + strlen(checkval), len + 1);
		pos = nodeval;
		while (*pos) {
			if (*pos == '<' && *(pos + 1) == '/') {
				*pos = 0;
				return nodeval;
			}
			pos++;
		}
	}
	return 0;
}

int checkMNC(char *mncStr, int mnc) {
	char *pos;

	while (*mncStr) {
		if (mnc == atoi(mncStr)) return 1;
		pos = (strstr(mncStr, ","));
		if (pos)
			mncStr = pos + 1;
		else
			return 0;
	}
	return 0;
}

int is_exclude_apn( FILE *fp2, long int platform_country_exclude_pos, char *carrier, char *apn) {
char buf[128];
char mycarrier[64], myapn[32];

	if ( !fp2 || !platform_country_exclude_pos || *carrier==0 || *apn==0 ) return 0;
	fseek ( fp2, platform_country_exclude_pos, SEEK_SET );

	while(fgets(buf, sizeof(buf), fp2)) {
		if( strstr(buf, "<Carrier ")) {
			if( getXmlTagVal( buf+8, "carrier", mycarrier, 63, NULL, NULL, 0) ) {
				if( *carrier==0 || strcmp(carrier, mycarrier)==0 ) {
					while(fgets(buf, sizeof(buf), fp2)) {
						if( strstr(buf, "<APN ") ) {
							if( getXmlTagVal( buf+4, "apn", myapn, 31, NULL, NULL, 0) ) {
								if( strcmp(apn, myapn)==0 ) {
									return 1;
								}
							}
						}
						else if( strstr(buf, "</APN>") || strstr(buf, "</Carrier>") ) {
							break; 
						}
					}
				}
			}
		}
		else if( strstr(buf, "</Country>"))
			break;
	}
	return 0;
}

long int get_platform_include_pos( FILE *fp2 ) {
char buf[128];

	fseek ( fp2, 0, SEEK_SET );
	while(fgets(buf, sizeof(buf), fp2)) {
		if( strstr(buf, "<Include>"))
			return ftell(fp2);	
	}
	return 0;
}

long int get_platform_country_exclude_pos( FILE *fp2, char *country ) {
char buf[128];
char mycountry[64];

	fseek ( fp2, 0, SEEK_SET );
	while(fgets(buf, sizeof(buf), fp2)) {
		if( strstr(buf, "<Exclude>")) {
			while(fgets(buf, sizeof(buf), fp2)) {
				if( strstr(buf, "<Country ")) {
					if( getXmlTagVal( buf+8, "country", mycountry, 63, NULL, NULL, 0) ) {
						if( strcmp(country, mycountry)==0 ) {
							return ftell(fp2);
						}
					}
				}
				else if( strstr(buf, "</Exclude>"))
					break;
			}
		}	
	}
	return 0;
}


long int findHint( FILE *fp, char *country, char *mcc, int mnc ) {
char hint[32];
char buff[128];
char mycountry[64], mycarrier[64];
char mymcc[6], mymnc[64];
char mystatus=0;

	if ( (rdb_get_single("wwan.0.system_network_status.hint", hint, sizeof(hint)) < 0 ) ||  strlen(hint)==0 )
		return 0;
//syslog(LOG_INFO, "find hint: %s", hint);
	fseek ( fp, 0, SEEK_SET );
	while(fgets(buff, sizeof(buff), fp)) {
		switch(mystatus) {
		case 0:
			if (strstr(buff, "<Country ")) {
				if (getXmlTagVal(buff + 8, "country", mycountry, 63, "mcc=\"", mymcc, 3)) {
					if (strcmp(mcc, mymcc) == 0) {
						mystatus++;
						break;
					}
				}
			}
		break;
		case 1: //find Career
			if( strstr(buff, "<Carrier ")) {
				if( getXmlTagVal( buff+8, "carrier", mycarrier, 63, "mnc=\"", mymnc, 62) ) {
					if( checkMNC( mymnc, mnc) ) {
						if ( *mycarrier && strstr( mycarrier, hint ) )
							return ftell(fp);
						break;
					}
				}
			}
			else if (strstr(buff, "</Country>")) {
				mystatus = 0;
			}
		break;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
void printUsage(char *path)
{
	
	fprintf(stderr, "Usage>\n\t %s [index]\n", path);
	fprintf(stderr, "\t %s [-h|--help]\n", path);
	fprintf(stderr, "\n");

	fprintf(stderr, "Options>\n\t index \t\t Index of APN list started from 1 (default: value of rdb variable link.profile.profilenum) \n");
	fprintf(stderr, "\t -h, --help \t Print this help information\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Return>\n\t APN='apn';USER='username';PASS='password';AUTH='authentication type[chap|pap]';\n");
	fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
	char apn[32], username[64], pass[64], auth[8];
	FILE *fp = 0, *fp1 = 0, *fp2 = 0;
	char buf[128];
	char currentmcc[6], mcc[6], mnc[128], buff[32];
	int imnc;
	char fp_rdb_name[32];
	char autoapn_flag;

	char country[64], carrier[64];
	int status = 0, retry=0;
	int profilenum;
	long int platform_include_pos=0;
	long int platform_country_exclude_pos=0;
	long int l_pos;
	char *pos;
	int i=0;

	if (rdb_open_db() < 0) {
		exit(-1);
	}
	if (argc == 1) {
		if (rdb_get_single("link.profile.profilenum", buf, sizeof(buf)) == 0 )
			profilenum = atoi(buf);
		else
			exit(-1);
	}
	else if (argc == 2) {
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 )
		{
			printUsage(argv[0]);
			goto err;
		}

		profilenum = atoi(argv[1]);
	}
	else {
		exit(-1);
	}

	*apn=0;
	*username=0;
	*pass=0;
	*auth=0;
	*carrier=0;
	if(profilenum <= 0) goto err;
	
	/*check auto apn enabled*/
	/*for the system use globle autoapn rdb varible "webinterface.autoapn", the connectscripts will not specify the profilenum (argc == 1)
	  in this case we can use the condition (argc == 2) to check the system that using individu autoapn settings.
	*/
	autoapn_flag=-1;
	if (argc == 2) {
		sprintf(buf,"link.profile.%u.autoapn", profilenum );
		if ( (rdb_get_single(buf, buff, sizeof(buff)) ==0 ) &&  strlen(buff) ) {
			autoapn_flag=atoi(buff);
		}
	}
	else {
		if ( (rdb_get_single("webinterface.autoapn", buff, sizeof(buff)) ==0 ) &&  strlen(buff) ) {
			autoapn_flag=atoi(buff);
		}
	}

	sprintf(buf,"link.profile.%u.apn", profilenum );
	if ( rdb_get_single(buf, apn, sizeof(apn)) != 0 )
		exit(-1);

#if defined(PLATFORM_Platypus2) || defined(PLATFORM_Bovine)
	if ( autoapn_flag==0 ) {
#else
	if ( strlen(apn) || autoapn_flag==0 ) {
#endif
		sprintf(buf,"link.profile.%u.user", profilenum );
		rdb_get_single(buf, username, sizeof(username));
		sprintf(buf,"link.profile.%u.pass", profilenum );
		rdb_get_single(buf, pass, sizeof(pass));
		sprintf(buf,"link.profile.%u.auth_type", profilenum );
		rdb_get_single(buf, auth, sizeof(auth));
		goto fini;
	}

	// open
	const char* szXml1=APNLIST_DIR"/apnList.xml";
	const char* szXml2=APNLIST_DIR"/platform_apn.xml";
	fp1 = fopen(szXml1, "r");
	if (!fp1) {
		syslog(LOG_ERR, "failed to open file %s",szXml1);
		goto err;
	}
	fp=fp1;

	fp2 = fopen(szXml2, "r");
	if(fp2) {
		platform_include_pos = get_platform_include_pos( fp2 );
	}

	// get MCC
	//if (rdb_get_single("wwan.0.system_network_status.MCC", currentmcc, sizeof(currentmcc)) < 0)
	*currentmcc=0;
	while (rdb_get_single("wwan.0.imsi.plmn_mcc", currentmcc, sizeof(currentmcc)) < 0  || !strlen(currentmcc)) {
		if( ++i > 10) {
			syslog(LOG_ERR, "failed to get MCC");
			goto err;
		}
		sleep(2);
	}
	// get MNC
	//if (rdb_get_single("wwan.0.system_network_status.MNC", buf, sizeof(buf)) < 0)
	i=0;
	*buf=0;
	while (rdb_get_single("wwan.0.imsi.plmn_mnc", buf, sizeof(buf)) < 0 || !strlen(buf) ) {
		if( ++i > 10) {
			syslog(LOG_ERR, "failed to get MNC");
			goto err;
		}
		sleep(2);
	}
	//printf(	"information MCC=%s,MNC=%s\n",currentmcc,buf);
	imnc = atoi(buf);

	// get the ICCID of current SIM then check the valid apn/user/pass/auth stored in the database
	i=0;
	*buff=0;
	while(rdb_get_single("wwan.0.system_network_status.simICCID", buff, sizeof(buff)) < 0 || !strlen(buff) ) {
		if( ++i > 2 ) {
			syslog(LOG_ERR, "failed to get ICCID");
			strcpy(buff,"0");
			break;
		}
		sleep(2);
	}
#ifdef V_MULTIPLE_WWAN_PROFILES_y
	sprintf(fp_rdb_name, "wwan.%u.fp.%s", profilenum, buff);
	sprintf(buf, "wwan.%u.apn.%s", profilenum, buff);
	if (rdb_get_single( buf, apn, sizeof(apn)) >= 0 && strlen(apn)>0 ) {
		rdb_set_single( buf, "");
		sprintf(buf, "wwan.%u.username.%s", profilenum, buff);
		if (rdb_get_single( buf, username, sizeof(username)) >= 0) {
			sprintf(buf, "wwan.%u.pass.%s", profilenum, buff);
			if (rdb_get_single( buf, pass, sizeof(pass)) < 0)
				*pass=0;
		}
		else
			*username=0;
		sprintf(buf, "wwan.%u.auth.%s", profilenum, buff);
		if (rdb_get_single( buf, auth, sizeof(auth)) < 0)
			*auth=0;
		goto fini;
	}
	sprintf(fp_rdb_name, "wwan.%u.fp.%s", profilenum, buff);
#else
	sprintf(fp_rdb_name, "wwan.0.fp.%s", buff);
	sprintf(buf, "wwan.0.apn.%s", buff);
	if (rdb_get_single( buf, apn, sizeof(apn)) >= 0 && strlen(apn)>0 ) {
		rdb_set_single( buf, "");
		sprintf(buf, "wwan.0.username.%s", buff);
		if (rdb_get_single( buf, username, sizeof(username)) >= 0) {
			sprintf(buf, "wwan.0.pass.%s", buff);
			if (rdb_get_single( buf, pass, sizeof(pass)) < 0)
				*pass=0;
		}
		else
			*username=0;
		sprintf(buf, "wwan.0.auth.%s", buff);
		if (rdb_get_single( buf, auth, sizeof(auth)) < 0)
			*auth=0;
		goto fini;
	}
	else {//need reset fp if the wwan.0.apn.ICCID is empty?
		;/*if(rdb_set_single( fp_rdb_name, "0" )<0)
		{
			syslog(LOG_ERR, "failed to set %s", fp_rdb_name);
		}*/
		//status = 0;
		//fseek ( fp, 0, SEEK_SET );
	}
	sprintf(fp_rdb_name, "wwan.0.fp.%s", buff);
#endif
	if (rdb_get_single( fp_rdb_name, buff, sizeof(buff)) >= 0 ) {
		if( *(buff+1)==':') {
			if(*(buff)=='2' && fp2) {
				fp=fp2;
			}
			pos=strchr(buff+2,':');
			if(pos)
				*pos=0;
			strncpy(carrier, buff+2, sizeof(carrier));
			l_pos = atol(pos+1);
		}
		else
			l_pos=0;
		if(l_pos)
			status = 2; //goto find APN
		else
			status = 0;
	}
	else {
		if(rdb_create_variable( fp_rdb_name, "1::0", CREATE, DEFAULT_PERM, 0, 0)<0) {
			syslog(LOG_ERR, "failed to create %s", fp_rdb_name);
			goto err;
		}
		status = 0;
		l_pos = 0;
	}

	if(fp==fp1) {// need to find the country and platform_country_exclude_pos
		fseek ( fp, 0, SEEK_SET );
		while(fgets(buf, sizeof(buf), fp)) {
			if (strstr(buf, "<Country ")) {
				if (getXmlTagVal(buf + 8, "country", country, 63, "mcc=\"", mcc, 3)) {
					if ( strcmp(currentmcc, mcc) == 0) {
						if ( fp2 )
							platform_country_exclude_pos=get_platform_country_exclude_pos( fp2, country );
						break;
					}
				}
			}
		}
	}

//syslog(LOG_INFO, "l_pos=%lx fp=%s country=%s currentmcc=%s, mcc=%s, imnc=%u", l_pos, (fp==fp1)?"1":"2", country, currentmcc, mcc, imnc);
	if(l_pos==0 && fp==fp1 && *country && *currentmcc && imnc) {
		l_pos=findHint( fp, country, currentmcc, imnc);
		if(l_pos)
			status = 2; //find hint, goto find APN
	}
	fseek ( fp, l_pos, SEEK_SET );
	
	while (fp) {
		// get a line xml
		if (!fgets(buf, sizeof(buf), fp)) {// end of xml file
			if( !retry && fp==fp1) {
				if(rdb_set_single( fp_rdb_name, "1::0" )<0) {
					syslog(LOG_ERR, "failed to set %s", fp_rdb_name);
				}
				retry++;
				fseek ( fp, 0, SEEK_SET );
				status=0;	
			}
			else if( !fp2 || fp==fp2 ) {
				syslog(LOG_ERR, "failed to detect apn");
				goto err;
			}
			else {
				fp=fp1;
				fseek ( fp, 0, SEEK_SET );
				status=0;
			}
			continue;
		}

		switch (status) {
			case 0: //find Country
				if (strstr(buf, "<Country ")) {
					if (getXmlTagVal(buf + 8, "country", country, 63, "mcc=\"", mcc, 3)) {
						if (strcmp(currentmcc, mcc) == 0) {
							if( fp!=fp2 && fp2 ) {
								platform_country_exclude_pos=get_platform_country_exclude_pos( fp2, country );
							}
							status++;
						}
					}
				}
				else if( strstr(buf, "</Include>") ) {
					status=-1;
				}
			break;
			case 1: //find Career
				if (strstr(buf, "<Carrier ")) {
					*username=0;
					*pass=0;
					*auth=0;
					if (getXmlTagVal(buf+8, "carrier", carrier, 63, "mnc=\"", mnc, 126)) {
						if (checkMNC(mnc, imnc)) {
							status++;
						}
					}
				}
				else if (strstr(buf, "</Country>")) {
					if(fp==fp2 || !fp2)
						status = 0;
					else
						status=-1;
				}
				else if( strstr(buf, "</Include>") ) {
					status=-1;
				}
			break;
			case 2: //find APN
				if (strstr(buf, "<APN ")) {
					if (getXmlTagVal(buf + 4, "apn", apn, 31, NULL, NULL, 0)) {
						status++;
					}
				}
				else if (strstr(buf, "</Carrier>")) {
					status = 1;
				}
				else if( strstr(buf, "</Include>") ) {
					status=-1;
				}
			break;
			case 3:
				if( getXmlNodeVal(buf, "<UserName>", username, 63)) {
					;//	printf("username=%s\n", username );
				}
				else if( getXmlNodeVal(buf, "<Password>", pass, 63) ) {
					;//	printf("pass=%s\n", pass );
				}
				else if( getXmlNodeVal(buf, "<Auth>", auth, 7) ) {
					;//	printf("auth=%s\n", auth );
				}
				else if( strstr(buf, "</APN>") ) {
					if( fp==fp2 || !platform_country_exclude_pos || !is_exclude_apn( fp2, platform_country_exclude_pos, carrier, apn) ) {
						syslog(LOG_INFO, "apn detected: %s", apn);
						goto fini2;
					}
					else {
						status=2;
					}
				}
				else {
					syslog(LOG_INFO, "apnList.xml format error (apn miss item) -- %s\n", buf);
					//goto err;
				}
			break;
			default:
//printf("default---%s\n", (fp == fp1)?"fp1":"fp2");
				if( (fp == fp1) && fp2 && platform_include_pos) {
					fp=fp2;
					platform_country_exclude_pos=0;
					fseek ( fp, platform_include_pos, SEEK_SET );
				}
				else if(fp==fp2) {
					fp=fp1;
					fseek ( fp, 0, SEEK_SET );
				}
				status=0;
			break;
		}
	}

fini2:
	l_pos = ftell(fp);
	if(l_pos<0) l_pos=0;
	sprintf( buff, "%s:%s:%lu", (fp==fp1)?"1":"2", carrier, l_pos );
	if (rdb_set_single( fp_rdb_name, buff ) < 0)
		syslog(LOG_ERR, "failed to set %s", fp_rdb_name);
	if(fp1)
		fclose(fp1);
	if(fp2)
		fclose(fp2);

fini:
	printf( "APN='%s';USER='%s';PASS='%s';AUTH='%s';\n", apn, username, pass, auth );
	if(rdb_update_single("wwan.0.apn.current", apn, CREATE, DEFAULT_PERM, 0, 0)<0)
		syslog(LOG_ERR, "failed to set wwan.0.apn.current");
	if(rdb_update_single("wwan.0.username.current", username, CREATE, DEFAULT_PERM, 0, 0)<0)
		syslog(LOG_ERR, "failed to set wwan.0.username.current");
	if(rdb_update_single("wwan.0.pass.current", pass, CREATE, DEFAULT_PERM, 0, 0)<0)
		syslog(LOG_ERR, "failed to set wwan.0.pass.current");
	if(rdb_update_single("wwan.0.auth.current", auth, CREATE, DEFAULT_PERM, 0, 0)<0)
		syslog(LOG_ERR, "failed to set wwan.0.auth.current");
	exit (0);
err:
	if(fp1)
		fclose(fp1);
	if(fp2)
		fclose(fp2);
	exit (-1);
}

/*
* vim:ts=4:sw=4:
*/
