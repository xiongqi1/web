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
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include "string.h"
#include "rdb_util.h"

void ifconf_to_DD( void )
{
	FILE *fp;
	char name[128];
	char buff[256];
	char tmp[32];
	char *pos;

	system("ifconfig >/tmp/ifconf");
	if ((fp = fopen("/tmp/ifconf", "r")) != 0)
    	{
      		while(fgets(buff, sizeof(buff), fp) != NULL)
      		{
#ifdef PLATFORM_AVIAN
		if( strncmp(buff,"br0", 3)==0 )
#else
		if( strncmp(buff,"athwlan0", 8)==0 || strncmp(buff,"br0", 3)==0 || strncmp(buff,"eth0", 4)==0 )
#endif
		{
			getword( name, sizeof(buff), buff, ' ' );	
			if( (pos=strstr(buff,"HWaddr"))==0 )
			{
				syslog(LOG_ERR, "ifconfig format not match!" );
				return;
			}
			strcpy( tmp, name );
			strcat( tmp, ".HWaddr");
			set_single( tmp, pos+7 );
			fgets(buff, sizeof(buff), fp);
			if( (pos=strstr(buff,"Mask:"))==0 )
			{
				syslog(LOG_ERR, "ifconfig format not match!" );
				return;
			}
			strcpy( tmp, name );
			strcat( tmp, ".mask");
			*(pos+strlen(pos)-1) = 0;
			set_single( tmp, pos+5 );
			if( (pos=strstr(buff,"inet addr:"))==0 )
			{
				syslog(LOG_ERR, "ifconfig format not match!" );
				return;
			}
			getword( tmp, 16, pos+10, ' ' );
			strcat( name, ".ip");
			set_single( name, tmp );
			break;
		}
      }
      fclose(fp);
    }
	system("rm /tmp/ifconf");
}

int main(int argc, char* argv[], char* envp[])
{
	FILE *fp;
	char name[128];
	char buff[256];
	char *pos;
	int len;


	openlog("hotspotInit", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	printf("Content-Type: text/html\n\n");
//syslog(LOG_ERR, "wifiInit-1");
	if(rdb_start())
	{
		syslog(LOG_ERR, "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
	}
	ifconf_to_DD();
	system("cat /proc/net/arp >/tmp/netarp");
	if ((fp = fopen("/tmp/netarp", "r")) != 0)
    {
      while(fgets(buff, sizeof(buff), fp) != NULL)
      {
		if( (pos=strstr(buff,"athwlan0"))||(pos=strstr(buff,"br0"))||(pos=strstr(buff,"eth0"))||(pos=strstr(buff,"wlan0")) )
		{
			len = strlen(buff);
			getword( name, 128, buff, ' ' );
			for( pos=buff+strlen(name); *pos!=':' && pos<(buff+len); pos++);
			pos -= 2;
			*(pos+17)=0;
			set_single( name, pos );
//printf("new='%s---%s';\n", name, pos);
		}
      }
      fclose(fp);
    }
	system("rm /tmp/netarp");
	if( strcmp("N/A",get_single("br0.ip")) )
	{
		printf("var newurl='http://%s/status.html';", get_single("br0.ip"));
	}
	else if( strcmp("N/A",get_single("athwlan0.ip")) )
	{
		printf("var newurl='http://%s/status.html';", get_single("athwlan0.ip"));
	}
	else
	{
		printf("var newurl='http://%s/status.html';", get_single("eth0.ip"));
	}
	rdb_end();
	exit(0);
}


