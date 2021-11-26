#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rdb_comms.h"
#include "rdbrpc.h"



#define CMD_END					"end"

#define CMD_ADD_RMP				"add_rmp" // objid
#define CMD_DEL_RMP				"del_rmp"// objid
#define CMD_SET_RMP				"set_rmp"// objid rmpid
#define CMD_GET_RMP				"get_rmp"// rmpid, return object
#define CMD_LIST_RMP			"list_rmp" //
#define CMD_UPDATE_RMP			"update_rmp"
#define CMD_TEST				"test"

#define PARAM_OBJID				"objid"
#define PARAM_RMPID				"rmpid"

void rpc_set_response3(const char* status, const char*prefix, const char*message)
{
	char buffer[128];
	strcpy(buffer, prefix);
	if( message)
	{
		strcat(buffer, "\"");
		strcat(buffer, message);
		strcat(buffer, "\"");
	}
	rdb_set_string(g_rdb_session, DOT1AG_CMD_MESSAGE, buffer);
	rdb_set_string(g_rdb_session, DOT1AG_CMD_STATUS, status);
	//NTCSLOG_DEBUG("rpc_set_response3 , %s", prefix);
}



int rpc_get_param(const char* name, int *param)
{
	int retValue;
	char param_str[128];
	strcpy(param_str, DOT1AG_CMD_PARAM_);
	strcat(param_str, name);
	retValue =rdb_get_sint(param_str, param);
	if(retValue ==0)
	{
		rdb_delete_variable(param_str);
	}
	else
	{
		//NTCSLOG_DEBUG("rpc_get_param( %s, ) %s", name, param_str);
		rpc_set_response3("error", "Invalid Parameter ", name);
	}
	return retValue;
}

void rpc_clear_params()
{
	char param_str[128];
	strcpy(param_str, DOT1AG_CMD_PARAM_);
	strcpy(param_str, PARAM_OBJID);
	rdb_delete_variable(param_str);

	strcpy(param_str, DOT1AG_CMD_PARAM_);
	strcpy(param_str, PARAM_RMPID);
	rdb_delete_variable(param_str);

}

void rpc_reset_idle()
{
	rpc_clear_params();
	rpc_set_response3("idle", "Ready to process commands.", NULL);
}



void rpc_set_response_msg(const char* msg)
{
	rdb_set_string(g_rdb_session,  DOT1AG_CMD_MESSAGE, msg);
	rdb_set_string(g_rdb_session,  DOT1AG_CMD_STATUS, "success");
}

void rpc_set_response_ok(int result)
{
	char buf[20];
	sprintf(buf, "ok:%d", result);
	rdb_set_string(g_rdb_session,  DOT1AG_CMD_MESSAGE, buf);
	rdb_set_string(g_rdb_session,  DOT1AG_CMD_STATUS, "success");
}

// get request id
//$ >0 new request
//$ ==0 no request
//$<0 -- error code
int rpc_get_request(int *request_id, int * objectid, int *rmpid)
{
    int err;
    char buff[64];
    *request_id =0;
	*objectid =-1;
	*rmpid = -1;

    err = rdb_get_str(DOT1AG_CMD_COMMAND, buff, 64);

    if(err <0)
    {
        return err;
    }
	//NTCSLOG_DEBUG("cmd.command = %s", buff);
	if( strlen(buff) ==0 || strcmp(buff, CMD_END) ==0)
	{
		rpc_reset_idle();
		return 0;
	}
	else if( strcmp(buff, CMD_ADD_RMP) ==0)
	{
		*request_id = REQUEST_ID_ADD_RMP;
		err = rpc_get_param(PARAM_OBJID, objectid); // objid
		if(err) goto lab_err;
	}
	else if (strcmp(buff, CMD_DEL_RMP) ==0)
	{
		*request_id = REQUEST_ID_DEL_RMP;
		err = rpc_get_param(PARAM_OBJID, objectid);// objid
		if(err) goto lab_err;
	}
	else if (strcmp(buff, CMD_SET_RMP) ==0) // objid, rmpid
	{
		*request_id = REQUEST_ID_SET_RMP;
		err = rpc_get_param(PARAM_OBJID, objectid);
		if(err) goto lab_err;
		rpc_get_param(PARAM_RMPID, rmpid);
		if(err) goto lab_err;
	}
	else if (strcmp(buff, CMD_GET_RMP) ==0)// rmpid, return objectid
	{
		*request_id = REQUEST_ID_GET_RMP;
		err = rpc_get_param(PARAM_RMPID, rmpid);
		if(err) goto lab_err;
	}
	else if (strcmp(buff, CMD_LIST_RMP) ==0)
	{
		*request_id = REQUEST_ID_LIST_RMP;
	}
	else if (strcmp(buff, CMD_UPDATE_RMP) ==0)
	{
		*request_id = REQUEST_ID_UPDATE_RMP;
	}
	else if (strcmp(buff, CMD_TEST) ==0)
	{
		*request_id = RDBRPC_TEST;
	}
	else // invalid command
	{
		rpc_set_response3("error",  "No such command ", buff);
		return -1;
	}
lab_err:
	if(err)
	{
		rpc_set_response3("error",  "Invalid parameter", NULL);

	}
    return 1;
}

