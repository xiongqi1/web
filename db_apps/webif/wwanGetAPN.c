/*!
 * Copyright Notice:
 * Copyright (C) 2002-2009 Call Direct Cellular Solutions
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of CDCS
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CDCS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CDCS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/msg.h>
#include <ctype.h>
#include "string.h"
#include "ajax.h"
#include <fcntl.h>
#include <signal.h>

#include "rdb_util.h"

#ifdef PLATFORM_AVIAN
#define APNLIST_DIR "/system/cdcs/www/cgi-bin"
#else
#define APNLIST_DIR "/www/cgi-bin"
#endif

int getXmlTagVal( char *mybuf, char *mytag, char *tagval, int taglen, char *myval, char *valval, int vallen )
{
char *pos=strstr( mybuf, mytag );
char *val;

	if( !pos ) return 0;
	pos+=strlen(mytag);
	while( *pos )
	{
		if( *pos++=='=' )
		{
			if( *pos++=='"' )
			{
				val=pos;
				while( *pos++ != 0 )
				{
					if( *pos == '"' )
					{
						*pos=0;
						strncpy( tagval, val, taglen );				
						if( !vallen || !valval || !myval) return 1;
						pos=strstr( pos+1, myval );
						if( !pos ) return 0;
						pos+=strlen(myval);
						strncpy( valval, pos, vallen+2 );
						while(*valval++) 
						{
							if(*valval=='"')
							{
								*valval=0; 
								return 1; 
							}else if( !isdigit(*valval)&&!(*valval==','||*valval==' ') )
								return 0;
						}
						return 1;
					}
				}
			}
		}
		else if( *pos > ' ' ) return 0;
	}
	return 0;
}

char *getXmlNodeVal( char *mybuf, char *checkval, char *nodeval, int len )
{
	char *pos = strstr(mybuf, checkval);
	if( pos )
	{
		strncpy( nodeval, pos+strlen( checkval ), len+1);
		pos = nodeval;
		while( *pos )
		{
			if( *pos=='<' && *(pos+1)=='/' )
			{
				*pos=0;
				return nodeval;
			}
			pos++;
		}
	}
	return 0;
}

int checkMNC( char *mncStr, int mnc )
{
	char *pos;

	while(*mncStr)
	{
		if(mnc == atoi(mncStr)) return 1;
		pos=(strstr(mncStr, ","));
		if( pos )
			mncStr = pos+1;
		else
			return 0;
	}
	return 0;
}

int is_exclude_apn( FILE *fp2, long int platform_country_exclude_pos, char *carrier, char *apn)
{
char buf[128];
char mycarrier[64], myapn[32];
	if ( !fp2 || !platform_country_exclude_pos || *carrier==0 || *apn==0 ) return 0;
	fseek ( fp2, platform_country_exclude_pos, SEEK_SET );

	while(fgets(buf, sizeof(buf), fp2))
	{
		if( strstr(buf, "<Carrier "))
		{
			if( getXmlTagVal( buf+8, "carrier", mycarrier, 63, NULL, NULL, 0) )
			{
				if( strcmp(carrier, mycarrier)==0 )
				{
					while(fgets(buf, sizeof(buf), fp2))
					{
						if( strstr(buf, "<APN ") )
						{
							if( getXmlTagVal( buf+4, "apn", myapn, 31, NULL, NULL, 0) )
							{
								if( strcmp(apn, myapn)==0 )
								{
									return 1;
								}
							}
						}
						else if( strstr(buf, "</APN>") || strstr(buf, "</Carrier>") )
						{
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

long int get_platform_include_pos( FILE *fp2 )
{
char buf[128];

	fseek ( fp2, 0, SEEK_SET );
	while(fgets(buf, sizeof(buf), fp2))
	{
		if( strstr(buf, "<Include>"))
			return ftell(fp2);	
	}
	return 0;
}

long int get_platform_country_exclude_pos( FILE *fp2, char *country )
{
char buf[128];
char mycountry[64];

	fseek ( fp2, 0, SEEK_SET );
	while(fgets(buf, sizeof(buf), fp2))
	{
		if( strstr(buf, "<Exclude>"))
		{
			while(fgets(buf, sizeof(buf), fp2))
			{
				if( strstr(buf, "<Country "))
				{
					if( getXmlTagVal( buf+8, "country", mycountry, 63, NULL, NULL, 0) )
					{
						if( strcmp(country, mycountry)==0 )
						{
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

static void wwanGetAPN(char *query)
{
FILE *fp1 = fopen( APNLIST_DIR"/apnList.xml", "r");
FILE *fp2 = fopen( APNLIST_DIR"/platform_apn.xml", "r");
FILE *fp;
long int temp_pos=0;
long int current_country_pos=0;
long int platform_include_pos=0;
long int platform_country_exclude_pos=0;
char buf[128];
char currentmcc[6], mcc[6], mnc[128], apn[32];
int imnc;
char country[64], carrier[64];
char  *pos=query;
char username[64], pass[64], auth[8], alertstr[128], dial[64];
int status=0;
int selectedIdx=0, countryCounter=0;

	if(!fp1)
	{
		printf("var errstr='open apnList.xml error';");		
		return;
	}
	fp = fp1;
	if(fp2)
	{
		platform_include_pos = get_platform_include_pos( fp2 );
	}
	if( *query==0 )
	{
		//getSingle("wwan.0.system_network_status.MCC", currentmcc);
		strncpy(currentmcc, get_single("wwan.0.imsi.plmn_mcc"), 5);
		if(strcmp(currentmcc, "N/A")==0)
			strcpy( currentmcc, "505"); //default to Australia
	}
	else
	{
		while( (pos=strstr(query, "%20")) )
		{
			*pos=' ';
			while( *(pos+2) )
			{
				pos++;
				*pos=*(pos+2);
			}
		}
	}
	//getSingle("wwan.0.imsi.plmn_mnc", buf);
	imnc = -1; //do not check the mnc
	printf( "var st=[''" );
	while(1)
	{
		if(!fgets(buf, sizeof(buf), fp)) break;
		if( strstr(buf, "<Country "))
		{
			if( getXmlTagVal( buf+8, "country", country, 63, "mcc=\"", mcc, 3) )
			{
				if( strcmp(country, "test") )
				{
					if( *query )
					{
						if( strcmp(country, query)==0 )
						{
							selectedIdx = countryCounter;
							current_country_pos = temp_pos;
							strcpy(currentmcc,mcc);
							if( current_country_pos < 0 )
								current_country_pos = 0;
						}
						else
							temp_pos = ftell(fp);
					}
					else if( *currentmcc )
					{
						if( strcmp(currentmcc, mcc)==0 )
						{
							selectedIdx = countryCounter;
							current_country_pos = temp_pos;
							if( current_country_pos < 0 )
								current_country_pos = 0;
						}
						else
							temp_pos = ftell(fp);
					}
					countryCounter++;
					printf( ",'%s'", country);
				}
			}
		}
	}
	printf( "];var selectedIdx=%d;\n", selectedIdx );
	fseek ( fp, current_country_pos, SEEK_SET );
	printf( "document.getElementById('nav2').innerHTML=\"" );
#ifdef PLATFORM_AVIAN
	printf( "<li class='top'><b style='color:#00FF00; text-decoration:underline;' id='chooseAPN'>Choose an APN here</b>" );
#else
	printf( "<li class='top'><b id='chooseAPN'>Choose an APN here</b>" );
#endif
	printf( "<ul class='sub'>" );
	while(fp)
	{
		for( countryCounter=0;; )
		{
			if( !fp || !fgets(buf, sizeof(buf), fp))
			{
				fp=0;
				break;//status=-1;	
			}
			switch(status)
			{
				case 0:
					if( strstr(buf, "<Country ")) //find Country
					{
						if( getXmlTagVal( buf+8, "country", country, 63, "mcc=\"", mcc, 3) )
						{
							if( current_country_pos || countryCounter==selectedIdx )
							{
								if( fp!=fp2 && current_country_pos && fp2 )
								{
									platform_country_exclude_pos=get_platform_country_exclude_pos( fp2, country );
								}
								if( strcmp(currentmcc, mcc)==0 || *currentmcc==0)
									status++;
							}
							++countryCounter;
						}
						else
							printf("apnList.xml format error (no mcc)\n");		
					}
					else if( strstr(buf, "</Include>") )
					{
						status=-1;
					}
				break;
				case 1: //find Career
					if( strstr(buf, "<Carrier "))
					{
						*username=0;
						*pass=0;
						*auth=0;
						if( getXmlTagVal( buf+8, "carrier", carrier, 63, "mnc=\"", mnc, 126) )
						{
							if( *currentmcc==0 || imnc==-1 || checkMNC(mnc, imnc) )
							{
								if(!fgets(buf, sizeof(buf), fp)) break;
								if( getXmlTagVal( buf+4, "apn", apn, 31, NULL, NULL, 0) )
								{
									printf("<li><span>%s</span><ul>", carrier);
									//printf("<li><span onClick=setAPN('%s')>%s</span><ul>", apn, carrier);
									//printf("<li><span onClick=setAPN('%s')>%s</span></li>", apn, apn );
									status=3;//++;	
								}
							}
						}
					}
					else if (strstr(buf, "</Country>"))
					{
						if(fp==fp2 || !fp2)
							status = 0;
						else
							status=-1;
					}
					else if( strstr(buf, "</Include>") )
					{
						status=-1;
					}
				break;
				case 2: //find APN
					if( strstr(buf, "<APN ") )
					{
						if( getXmlTagVal( buf+4, "apn", apn, 31, NULL, NULL, 0) )
						{
//printf("apn=%s\n", apn );
							//printf("<li><span onClick=setAPN('%s')>%s</span></li>", apn, apn );
							status++;	
						}
					}
					else if( strstr(buf, "</Carrier>"))
					{
						if( *currentmcc==0 || imnc==-1 || checkMNC(mnc, imnc) )
							printf( "</ul></li>" );
						status=1;
					}
					else if( strstr(buf, "</Include>") )
					{
						status=-1;
					}
				break;
				case 3:
					if( strstr(buf, "</Include>") )
					{
						status=-1;
					}
					else if( getXmlNodeVal(buf, "<UserName>", username, 63))
					{
						;//	printf("username=%s\n", username );
					}
					else if( getXmlNodeVal(buf, "<Password>", pass, 63) )
					{
						;//	printf("pass=%s\n", pass );
					}
					else if( getXmlNodeVal(buf, "<Auth>", auth, 7) )
					{
						;//	printf("auth=%s\n", auth );
					}
					else if( getXmlNodeVal(buf, "<Alert>", alertstr, 127) )
					{
					//	printf("alertstr=%s\n", alertstr );
						status++;
					}
					else if( getXmlNodeVal(buf, "<Dial>", dial, 63) )
					{
					//	printf("dial=%s\n", dial );
					}
					else if( strstr(buf, "</APN>") )
					{
						if( fp==fp2 || !platform_country_exclude_pos || !is_exclude_apn( fp2, platform_country_exclude_pos, carrier, apn) )
							printf("<li><span onClick=setAPN('%s','%s','%s','%s')>%s</span></li>", apn, username, pass, auth, apn );
						status=2;
					}
					else
					{
						printf("apnList.xml format error (apn miss item) -- %s\n", buf);
						status=-1;
					}
				break;
				case 4:
					if( strstr(buf, "</APN>") )
					{
						status=2;
					}
					else if( strstr(buf, "</Carrier>"))
					{
						status=1;
						if( *currentmcc==0 || imnc==-1 || checkMNC(mnc, imnc) )
							printf( "</ul></li>" );
					}
					else if( strstr(buf, "</Country>"))
					{
						if(fp==fp2 || !fp2)
							status = 0;
						else
							status=-1;
					}
				break;
				default:
					if( (fp == fp1) && fp2 && platform_include_pos)
					{
						fp=fp2;
						platform_country_exclude_pos=0;
						fseek ( fp, platform_include_pos, SEEK_SET );
					}
					else
					{
						fp=0;
					}
					status=0;
				break;
			}
		}
	}
	printf( "</ul></li>\";" );
	if(fp1)
		fclose(fp1);
	if(fp2)
		fclose(fp2);

}

int main(int argc, char* argv[], char* envp[])
{
	char *query;
	char *p_str, *p_str2;

// On Platypus2, this cgi can be called by wizard.html without SESSION_ID
#if ( !defined PLATFORM_Platypus2 )
	p_str=getenv("SESSION_ID");
	p_str2=getenv("sessionid");
	if( p_str==0 || p_str2==0 || strcmp(p_str, p_str2) ) {
		exit(0);
	}
#endif
	printf("Content-Type: text/html \r\n\r\n");
	query = getenv("QUERY_STRING");
	if (!query)
	{
		query="\0";
	}
	if(rdb_start())
	{
		printf( "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
	}
	else
	{
		wwanGetAPN( query );
	}
	rdb_end();
	exit(0);
}

