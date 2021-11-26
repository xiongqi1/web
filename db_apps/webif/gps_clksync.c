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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#include "rdb_util.h"


void gps_clock_sync(void)
{
	char buf[256], *tmp;
	int temp, hour, min, sec, yy, mm, dd;

	/* read date & time */
	tmp = get_single("sensors.gps.0.common.date");		/* ex) 230394 */
	if (!strncmp(tmp, "N/A", 3))
	{
		printf( "can't synchronize date & time\n");
		return;
	}
	temp = atoi(tmp);
	dd = temp / 10000;
	mm = (temp % 10000) / 100;
	yy = temp % 100;

	tmp = get_single("sensors.gps.0.common.time");		/* ex) 123519 */
	if (!strncmp(tmp, "N/A", 3))
	{
		printf( "can't synchronize date & time\n");
		return;
	}
	temp = atoi(tmp);
	hour = temp / 10000;
	min = (temp % 10000) / 100;
	sec = (temp % 100);
    sprintf(buf, "date -u -s \"%02d%02d%02d%02d%02d.%02d\"", yy, mm, dd, hour, min, sec);
	system(buf);
	printf("%s;\n", buf);
}

int main(int argc, char* argv[], char* envp[])
{
	char *p_str, *p_str2;
	p_str=getenv("SESSION_ID");
	p_str2=getenv("sessionid");
	if( p_str==0 || p_str2==0 || strcmp(p_str, p_str2) ) {
		exit(0);
	}

	printf("Content-Type: text/html \r\n\r\n");

	if(rdb_start())
	{
		printf( "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
	}
	else
	{
		gps_clock_sync();
	}
	rdb_end();
	exit(0);
}
