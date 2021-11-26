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
#include "stridx.h"
#include "commonIncludes.h"

/* ardb */
struct ardb_t {
	struct stridx_t* rbt;
};

/* singleton object of ardb */
static struct ardb_t ardb;

int ardb_get_reference(const char* rdb_var)
{
	return stridx_find(ardb.rbt,rdb_var);
}

int ardb_add_reference(const char* rdb_var,int ref)
{
	return stridx_add(ardb.rbt,rdb_var,ref);
}

int ardb_subscribe(const char* rdb_var,int ref, int persist)
{
	int resubscribe=0;
	#ifdef RDB_SINGLE_SESSION
	#else
	struct rdb_session* rdb;
	#endif
	
	int flags=0;
	
	/* use persist flags if required */
	if(persist)
		flags|=PERSIST;
	
	DBG(LOG_DEBUG,"rdb_set (var=%s,ref=0x%08x,persist=%d)",rdb_var,ref,persist);
	
	/* add reference */
	if(ardb_add_reference(rdb_var,ref)<0)
		return -1;
	
	subscribe_again:
	{
		/* subscribe */
		rdb=rdb_get_struc();
		if(rdb_subscribe(rdb,rdb_var)<0) {
		
			/* return error if failed */
			if(resubscribe || errno!=ENOENT) {
				DBG(LOG_ERR,"failed to subscribe (rdb=%s) - %s",rdb_var,strerror(errno));
				goto err;
			}
			
			/* create rdb */
			if(rdb_create_string(rdb,rdb_var,"",flags,0)<0) {
				DBG(LOG_ERR,"failed to create rdb to subscribe (rdb=%s) - %s",rdb_var,strerror(errno));
				goto err;
			}
			
			resubscribe=1;
			goto subscribe_again;
		}
	}
	
	return 0;
err:
	return -1;	
}

static char* triggered_rdbs;
static char* triggered_rdbs_sp;

int ardb_get_next_triggered()
{
	int ref;
	char* token;
	
	ref=-1;
	
	while(1) {
		/* get token */
		token=strtok_r(triggered_rdbs,";&",&triggered_rdbs_sp);
		if(!token)
			break;
		
		triggered_rdbs=NULL;
		
		/* get reference */
		ref=ardb_get_reference(token);
		if(ref>=0)
			break;
		
		DBG(LOG_ERR,"unknown subscribed value found - '%s'",token);
	}
	
	return ref;
}

int ardb_get_first_triggered()
{
	/* get triggered dbs */
	triggered_rdbs=rdb_enum(TRIGGERED);
	if(!triggered_rdbs)
		goto err;
	
	return ardb_get_next_triggered();
	
err:
	return -1;	
	
}

void ardb_fini()
{
	stridx_destroy(ardb.rbt);
	rdb_fini();
}

int ardb_init()
{
	/* memset ardb obj. */
	memset(&ardb,0,sizeof(ardb));
	
	/* init. rdb */
	if(rdb_init()<0) {
		DBG(LOG_ERR,"failed to init. rdb");
		goto err;
	}
	
	/* init. rbt */		
	ardb.rbt=stridx_create();
	if(!ardb.rbt) {
		DBG(LOG_ERR,"failed to create rbt obj");
		goto err;
	}
	
	return 0;
	
err:
	ardb_fini();
	return -1;	
}

