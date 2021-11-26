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

#include "g.h"
#include "rdb.h"

static char _val[RDB_VARIABLE_MAX_LEN];
static char _var[RDB_VARIABLE_NAME_MAX_LEN];
static char enumbuf[RDB_ENUM_MAX_LEN];

static struct rdb_session* rdb=NULL;

int rdb_get_handle()
{
	if(!rdb)
		return -1;
	
	return rdb_fd(rdb);
}

struct rdb_session* rdb_get_struc()
{
	return rdb;
}

char* rdb_enum(const char* name,int flags)
{
	int len;
	
	len=sizeof(enumbuf)-1;
	
	*enumbuf=0;
	if(rdb_getnames(rdb,name,enumbuf,&len,flags)<0)
		return NULL;
	
	if(len<0)
		return NULL;
	
	enumbuf[len]=0;
	
	return enumbuf;
}

int rdb_regex_enum(const char* name,const char* regex,rdb_regex_enum_callback cb,int ref)
{
	char* names;
	char* sp;
	char* token;
	int r;
	
	regex_t re;
	
	/* compile regex */
	if (regcomp(&re,regex,REG_NOSUB)!=0) {
		DBG(LOG_ERR,"failed to compile regex (wildcard=%s) - %s",regex,strerror(errno));
		goto err;
	}
	
	/* get name */
	names=rdb_enum(name,0);
	
	/* bypass if no RDB */
	if(!names) {
		DBG(LOG_DEBUG,"no hwdev RDBs found (name=%s)",name);
		goto fini;
	}
	
	DBG(LOG_DEBUG,"got RDBs (name=%s,names=%s)",name,names);
	
	token=strtok_r(names,";&",&sp);
	while(token) {
	
		DBG(LOG_DEBUG,"got token (token=%s)",token);
		
		r=regexec(&re,token,0,NULL,0);
		
		/* call callback if matching */
		if(r==0) {
			DBG(LOG_DEBUG,"call callback function (token=%s,ref=%d)",token,ref);
			if(cb(token,ref)<0)
				break;
		}
	
		/* get token */
		token=strtok_r(NULL,";&",&sp);
	}
	
fini:	
	return 0;
err:
	return -1;	
}

int rdb_del(const char* var)
{
	return rdb_delete(rdb,var);
}

const char* rdb_var_printf(const char* fmt,...)
{
	static char var[RDB_VARIABLE_NAME_MAX_LEN];
	
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(var,sizeof(var),fmt,ap);

	va_end(ap);

	return var;
}

int rdb_set_printf(const char* var,const char* fmt,...)
{
	int stat;

	va_list ap;

	va_start(ap, fmt);

	vsnprintf(_val,sizeof(_val),fmt,ap);
	stat=rdb_set_value(var, _val, 0);

	va_end(ap);

	return stat;
}


char* rdb_get_printf(const char* fmt,...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);

	vsnprintf(_var,sizeof(_var),fmt,ap);
	len=sizeof(_val);
	
	if(rdb_get(rdb,_var,_val,&len)<0) {
		*_val=0;
	}
	
	va_end(ap);

	return _val;
}

int rdb_set_value(const char* var,const char* val, int persist)
{
	int rc;
	
	int flags=0;
	
	DBG(LOG_DEBUG,"rdb_set (var=%s,val=%s,persist=%d)",var,val,persist);
	
	if(persist)
		flags|=PERSIST;

	if( (rc=rdb_set_string(rdb, var, val))<0 ) {
		if(errno==ENOENT) {
			rc=rdb_create_string(rdb, var, val, flags, 0);
		}
	}

	if(rc<0) {
		DBG(LOG_ERR,"failed to write to rdb(var=%s,val=%s) - %s",var,val,strerror(errno));
	}

	return rc;
}

const char* rdb_get_value(const char* var)
{
	int len;
	
	len=sizeof(_val);
	
	if(rdb_get(rdb,var,_val,&len)<0) {
		return NULL;
	}
	
	_val[sizeof(_val)-1]=0;
	
	return _val;
}


int rdb_init(void)
{
	// open rdb database
	if( rdb_open(NULL,&rdb)<0 ) {
		DBG(LOG_ERR,"failed to open rdb driver - %s",strerror(errno));
		goto err;
	}
	
	return 0;
err:
	return -1;	
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
	
	if( rdb_set_value("rdb_test","abc")<0 ) {
		printf("rdb_set_value() failed\n");
		goto fini;
	}
	
	if(!(p=rdb_get_value("rdb_test"))) {
		printf("rdb_get_value() failed\n");
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
