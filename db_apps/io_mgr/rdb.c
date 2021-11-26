/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include "rdb.h"
#include "commonIncludes.h"

static char val[RDB_VARIABLE_MAX_LEN];
static char enumbuf[RDB_ENUM_MAX_LEN];

#ifdef RDB_SINGLE_SESSION
#else
static struct rdb_session* rdb=NULL;
#endif

int rdb_get_handle()
{
	return rdb_fd(rdb);
}

struct rdb_session* rdb_get_struc()
{
	return rdb;
}

char* rdb_enum(int flags)
{
	int len;
	
	len=sizeof(enumbuf)-1;
	
	*enumbuf=0;
	if(rdb_getnames(rdb,"",enumbuf,&len,flags)<0)
		return NULL;
	
	if(len<0)
		return NULL;
	
	enumbuf[len]=0;
	
	return enumbuf;
}

int rdb_setVal(const char* var,const char* val, int persist)
{
	int rc;
	int flags=0;

//	DBG(LOG_DEBUG,"rdb_set (var=%s,val=%s,persist=%d)",var,val,persist);

	if(persist)
		flags|=PERSIST;

	if( (rc=rdb_update_string(rdb, var, val, flags, 0))<0 ) {
		syslog(LOG_ERR,"failed to write to rdb(%s) - %s",var,strerror(errno));
	}

	return rc;
}

const char* rdb_getVal(const char* var)
{
	int len;
	
	len=sizeof(val);
	
	if(rdb_get(rdb,var,val,&len)<0) {
		return NULL;
	}
	
	return val;
}


int rdb_init(void)
{
	// open rdb database
	if( rdb_open(NULL,&rdb)<0 ) {
		syslog(LOG_ERR,"failed to open rdb driver - %s",strerror(errno));
		return -1;
	}
	
	return 0;
}


void rdb_fini(void)
{		
	if(rdb)
		rdb_close(&rdb);
}

#ifdef MODULE_TEST
int main(int argc,char* argv[])
{
	const char* p;
	rdb_init();
	
	if( rdb_setVal("rdb_test","abc")<0 ) {
		printf("rdb_set() failed\n");
		goto fini;
	}
	
	if(!(p=rdb_getVal("rdb_test"))) {
		printf("rdb_get() failed\n");
		goto fini;
	}
	
	if(strcmp(p,"abc")) {
		printf("rdb value not matching\n");
		goto fini;
	}
	
	printf("module test ok\n");
	
fini:	
	rdb_fini();
	
	return 0;
}
		
		
#endif
