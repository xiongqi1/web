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
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include "string.h"
#include "rdb_util.h"

int main(int argc, char* argv[], char* envp[])
{
	char *str;
	char buff[256];
	char *pos;
	FILE *fp;
	int i = 0;

	openlog("hotspotWiFi", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	if(rdb_start())
	{
		syslog(LOG_ERR, "can't open RDB driver %i ( %s )\n", -errno, strerror(errno));
	}
	printf("Content-Type: text/html\n\n");
	str = getenv("REMOTE_ADDR");
//printf( "str=%s",str);
	if(str)
	{
		system("iptables -t nat -L --line-numbers -n >/tmp/line-numbers");
		if ((fp = fopen("/tmp/line-numbers", "r")) != 0)
		{
		  while(fgets(buff, sizeof(buff), fp) != NULL)
		  {	
			if( (pos=strstr(buff,"MAC ")) )
			{
				pos += 21; //4+17
				*pos=0;
				for( i=0; i<17; i++, pos--)
				{
					if( isupper(*pos) )
					{
						*pos = tolower(*pos);
					}
				}	
				if( strncmp(pos, get_single( str ), 17)==0 )
				{
					i=0;
					break;
				}
			}
		  }
		  fclose(fp);
		}
		if(i)
		{
			sprintf( buff, "iptables -t nat -I nat_hotspot 1 -m mac --mac-source %s -j ACCEPT", get_single( str ) );
		//	sprintf( buff, "iptables -t nat -A allowed_macs -m mac --mac-source %s -j ACCEPT", get_single( str ) );
		//	sprintf( buff, "rdb_set hotspot.add_mac %s", get_single( str ) ); //use template
			system( buff );
			//sprintf( buff, "iptables -I FORWARD 1 -m mac --mac-source %s -j ACCEPT", get_single( str ) );
			//system( buff );
		}
		system("rm /tmp/line-numbers");
	}
	else
	{
		syslog(LOG_ERR,"can't get the REMOTE_ADDR" );
	}
	rdb_end();
	exit(0);
}


