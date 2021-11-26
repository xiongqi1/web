#ifndef __commoninc_H__28112015
#define __commoninc_H__28112015

/*
 * common include files and macros for this project
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#pragma GCC diagnostic ignored "-Wvariadic-macros"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* include everything here - less headach */
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <regex.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/times.h>

#define STRNCPY(dst,src) \
	strncpy(dst,src,sizeof(dst)); \
	dst[sizeof(dst)-1]=0;

#define STRDUPA(str) \
	strcpy(alloca(strlen(str)+1),str)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define COUNTOF(a)		((int)(sizeof((a))/sizeof((a)[0])))

#define safeFree(p) do {\
	if(p){\
		free((void*)p);(p)=NULL;\
	}\
} while (0)

#ifdef TEST
#define DBG(level,fmt,...) fprintf(level<LOG_ERR?stderr:stdout,"<%d:%s:%s> " fmt "\n",level,__FILE__,__FUNCTION__,##__VA_ARGS__)
#else
#define DBG(level,fmt,...)	syslog(level,"<%s:%s> " fmt,__FILE__,__FUNCTION__,##__VA_ARGS__)
#endif


#endif
