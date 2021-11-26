
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

#include "abstractcmd.h"

#include "minilib.h"
#include "modules/modules.h"


int abstractcmd_call(struct abstractcmd_t* abstract, unsigned int idx,struct abstractcmd_param_t* param)
{
	abstractcmd_func func;

	// find func
	func=(abstractcmd_func)generictree_find(abstract->cmdtree,idx);
	if(!func) {
		SYSLOG(LOG_ERROR,"function not found - idx=%d",idx);
		goto err;
	}

	return func(abstract,param);

err:
	return -1;
}

int abstractcmd_detect(struct abstractcmd_t* abstract, struct abstractcmd_param_in_detect_t* param_in_detect)
{
	// create the object
	abstract=(struct abstractcmd_t*)_malloc(sizeof(struct abstractcmd_t));
	if(!abstract) {
		SYSLOG(LOG_ERROR,"failed to allocate abstractcmd_t - size=%d",sizeof(struct abstractcmd_t));
		goto err;
	}

	int i;

	SYSLOG(LOG_OPERATION,"looking for a module - model=%s,manufacture=%s",param_in_detect->model,param_in_detect->manufacture);

	__create_param(param_info,0,sizeof(struct abstractcmd_param_t));
	__create_param(param_detect,sizeof(struct abstractcmd_param_in_detect_t),0);

	// check
	if(!param_info || !param_detect) {
		SYSLOG(LOG_ERROR,"failed to allocate params - abstractcmd_param_t or abstractcmd_param_in_detect_t");
		goto err;
	}

	struct abstractcmd_element_t* cmd_info;
	struct abstractcmd_element_t* cmd_detect;

	// copy param detect
	memcpy(__get_in_param(param_detect,void*),param_in_detect,sizeof(*param_in_detect));

	// internal params
	struct abstractcmd_param_out_info_t* param_out_info;
	struct abstractcmd_element_t* cmds;

	i=0;
	while( (cmds=modules[i])!=0 ) {
		cmd_info=&cmds[ABSTRACTCMD_CMD_IDX_INFO];
		cmd_detect=&cmds[ABSTRACTCMD_CMD_IDX_DETECT];

		// get information
		if(cmd_info->func(abstract,param_info)<0) {
			SYSLOG(LOG_ERROR,"failed in func(ABSTRACTCMD_CMD_IDX_INFO)");
			goto err;
		}

		// log information
		param_out_info=__get_out_param(param_info,struct abstractcmd_param_out_info_t*);
		SYSLOG(LOG_OPERATION,"idx=%d, model=%s, manufacture=%s",i,param_out_info->model,param_out_info->manufacture);

		// detect
		if( cmd_detect->func(abstract,param_detect)>=0 ) {
			SYSLOG(LOG_OPERATION,"supported - idx=%d, model=%s",i,param_out_info->model);
			// store
			abstract->cmds=cmds;
			break;
		}
		else {
			SYSLOG(LOG_OPERATION,"not supported - idx=%d, model=%s",i,param_out_info->model);
		}

		i++;
	}

	// if not found
	if(abstract->cmds) {
		SYSLOG(LOG_OPERATION,"not found - model=%s",param_in_detect->model);
		goto err;
	}

	i=0;

	// build command io function tree
	cmds=abstract->cmds;
	while(cmds->idx && cmds->func) {
		generictree_add(abstract->cmdtree,cmds->idx,cmds->func);
		// go next
		cmds++;
	}

	SYSLOG(LOG_OPERATION,"total %d command io function added - model=%s",i,param_in_detect->model);

	__destroy_param(param_info);
	__destroy_param(param_detect);

	return 0;
err:
	__destroy_param(param_info);
	__destroy_param(param_detect);

	return -1;
}

void abstractcmd_destroy(struct abstractcmd_t* abstract)
{
	if(!abstract)
		return;

	generictree_destroy(abstract->cmdtree);
	_free(abstract);
}

struct abstractcmd_t* abstractcmd_create(void)
{
	struct abstractcmd_t* abstract;

	// create the object
	abstract=(struct abstractcmd_t*)_malloc(sizeof(struct abstractcmd_t));
	if(!abstract) {
		SYSLOG(LOG_ERROR,"failed to allocate abstractcmd_t - size=%d",sizeof(struct abstractcmd_t));
		goto err;
	}

	// create tree
	abstract->cmdtree=generictree_create();
	if(!abstract->cmdtree) {
		SYSLOG(LOG_ERROR,"failed to create cmdtree");
		goto err;
	}

	return abstract;

err:
	abstractcmd_destroy(abstract);
	return NULL;
}
