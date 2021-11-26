/*!
 * Copyright Notice:
 * Copyright (C) 2002-2008 Call Direct Cellular Solutions
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

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "string.h"
#include "rdb_ops.h"

#define SYSLOG_LOGTOFILE_DB_NAME    "service.syslog.option.logtofile"
int roaming_only=0;

char* quotit(char* outquote, int maxlen, char* inquote, char* qlist)
{
	int i;
	if (maxlen < 3)
	{
		outquote[0] = '\0';
		return outquote;
	}
	outquote[0] = '"';
	i = 1;
	while (*inquote != '\0')
	{
		if (qlist && strchr(qlist, * inquote))
		{
			if (i >= maxlen - 3)
				break;
			outquote[i++] = '\\';
			outquote[i++] = * inquote++;
			continue;
		}
		else
		{
			if (i >= maxlen - 2)
				break;
			outquote[i++] = * inquote++;
			continue;
		}
	}
	outquote[i++] = '"';
	outquote[i] = '\0';
	return outquote;
}

int file_to_array(int some, char* varname, FILE* outfp, char* infname)
{
	FILE* infp;
	char buf[512];
	char qbuf[512];
	int first = 1;
	char* cp;

	if(roaming_only)
	{
		sprintf( buf, "cat %s |grep roaming >%s2; mv %s2 %s",infname, infname, infname, infname);
		system(buf);
	}

	if ((infp = fopen(infname, "r")) != 0)
	{
		while (fgets(buf, sizeof(buf) - 1, infp))
		{
			if(strncmp(buf,"utc =", 5)==0||strncmp(buf,"local =", 7)==0||strncmp(buf,"offset =", 8)==0) continue;
			if ((cp = strchr(buf, '\n')) != NULL)
				* cp = '\0';
			if ((cp = strchr(buf, '\r')) != NULL)
				* cp = '\0';
			if (buf[0] == '\0')
				continue;
			if (some)
				fprintf(outfp, ",\n");
			some = 1;
			if (first)
			{
				first = 0;
				fprintf(outfp, "\"%s\":[\n", varname);
			}
			quotit(qbuf, sizeof(qbuf), buf, "\"\\'");
			fprintf(outfp, "%s", qbuf);
		}
		fclose(infp);
		if (!first)
		{
			fprintf(outfp, "]\n");
		}
	}
	return some;
}

int logfile(int buffer_mode)
{
	int some = 0;
	printf("var logdata = {\n");

#ifdef PLATFORM_AVIAN
#define TEMP_FILE "/var/log/messages"
	if (buffer_mode)
	{
		system("/system/cdcs/bin/logread_localtz  >" TEMP_FILE);
	}
	else {
		system("/system/cdcs/bin/merge_logfile.sh");
	}
#elif defined(PLATFORM_Platypus2)
#define TEMP_FILE "/tmp/messages"
	if (buffer_mode)
	{
		system("logread >" TEMP_FILE);
	}
	else
	{
		system("merge_logfile.sh");
	}
#elif defined(PLATFORM_Serpent)
#define TEMP_FILE "/tmp/messages"
	if (buffer_mode)
	{
		system("logread >" TEMP_FILE);
	}
	else
	{
		system("merge_logfile.sh");
	}
#else
#define TEMP_FILE "/var/log/messages"
	if (buffer_mode)
	{
		system("logread >" TEMP_FILE);
	}
	else
	{
		system("merge_logfile.sh");
	}
#endif
	some = file_to_array(some, "messages", stdout, TEMP_FILE);
	printf(" };\n");
	printf("displayLogData();\n");
	return (0);
}


int main(int argc, char* argv[], char* envp[])
{
	char *p_str;
	//char *p_str2;
	int buffer_mode = 1;
	char temp[10];

/*	p_str=getenv("SESSION_ID");
	p_str2=getenv("sessionid");
	if( p_str==0 || p_str2==0 || strcmp(p_str, p_str2) ) {//|| strcmp( p_str, p_str2 )
		//printf("Content-Type: text/html\n\n");
		printf("SESSION_ID=%s sessionid=%s\n\n",p_str, p_str2);
		exit(0);
	}*/

	openlog("logfile", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if (rdb_open_db() < 0)
	{
		exit(-1);
	}
	/* check if syslogd is running in buffer mode or file mode */
	if (rdb_get_single(SYSLOG_LOGTOFILE_DB_NAME, temp, 10) == 0 && strcmp(temp, "1") == 0)
	{
		//syslog(LOG_DAEMON | LOG_DEBUG, "%s is 1. run supervisor with file mode\n", SYSLOG_LOGTOFILE_DB_NAME);
		buffer_mode = 0;
	}

	p_str = getenv("QUERY_STRING");
	if (p_str && strcmp(p_str,"clearlog")==0)
	{
		system("/usr/bin/killall -9 syslogd");
		printf("Content-Type: text/html\n\n");
		printf("var result='ok';\n");
		if (!buffer_mode)
		{
#if defined(PLATFORM_Platypus2)
			system("rm -f /etc/message* >/dev/null 2>&1");
#elif defined(PLATFORM_Serpent)
			system("rm -f /var/log/message* >/dev/null 2>&1");
#else
			system("rm -f /opt/message* >/dev/null 2>&1");
#endif
		}
		rdb_close_db();
		exit(0);
	}
	else
	{
		if( (argc==2) && (!strcmp(argv[1],"-q" ) || !strcmp(argv[1],"-r" )) )
		{
			if( !strcmp(argv[1],"-r" ) )
			{
				roaming_only=1;
			}
		}
		else
		{
			printf("Content-Type: text/html\n\n");
		}
		//syslog(LOG_ERR, "logfile-1");
		logfile(buffer_mode);
		rdb_close_db();
		exit(0);
	}
}
