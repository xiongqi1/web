#include <syslog.h>

#include "../minilib.h"
#include "../abstractcmd.h"

int generic_module_info(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param)
{
	const char* manufacture="netcomm";
	const char* model="generic";
	
	struct abstractcmd_param_out_info_t* out;
	
	if(param->out_len<sizeof(*out)) {
		SYSLOG(LOG_ERROR,"insufficent out buffer - cur=%d,need=%d",param->out_len,sizeof(*out));
		goto err;
	}
	
	out=__get_out_param(param,struct abstractcmd_param_out_info_t*);
	out->model=model;
	out->manufacture=manufacture;
			
	return 0;
err:
	return -1;			
}

int generic_module_detect(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param)
{
	return 0;
}
