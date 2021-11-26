/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/timeb.h>
#include <sys/types.h>

#include "cdcs_syslog.h"
#include "rdb_ops.h"

#include "./rdb_util.h"
#include "../rdb_names.h"

#define SUCCESS 0
char wwan_prefix[64];
char gps_prefix[64];

const char* rdb_name(const char* path, const char* name);

const char* rdb_name(const char* path, const char* name)
{
	enum { size = 10 };
	static unsigned int i = 0;
	static char names[size][256];
	const char* first_dot = path && *path ? "." : "";
	const char* second_dot = name && *name ? "." : "";
	char* n = names[i];
	if (strcmp(path, RDB_GPS_PREFIX) == 0) {
    	sprintf(n, "%s%s%s", gps_prefix, second_dot, name);
	} else if (strcmp(path, RDB_SYSLOG_PREFIX) == 0) {
    	sprintf(n, "%s%s%s", path, second_dot, name);
	} else {
    	sprintf(n, "%s%s%s%s%s", wwan_prefix, first_dot, path, second_dot, name);
    }
	if (++i == size)
	{
		i = 0;
	}
	return n;
}


////////////////////////////////////////////////////////////////////////////////
int rdb_set_single_int(const char* name, int value)
{
	char str[64];
	sprintf(str,"%d",value);

	return rdb_set_single(name,str);
}

////////////////////////////////////////////////////////////////////////////////
void rdb_create_and_get_single(const char* name, char *value, int vlen)
{
	(void) memset(value, 0x00, vlen);
	if (rdb_get_single(name, value, vlen) != 0) {
		if(rdb_create_variable(name, "", CREATE | PERSIST, ALL_PERM, 0, 0)<0)
		{
			SYSLOG_ERR("failed to create '%s'", name);
		}
	}
}

