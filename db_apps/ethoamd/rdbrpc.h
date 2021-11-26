#ifndef __RDBRPC_H
#define __RDBRPC_H

#define REQUEST_ID_NONE				0
#define REQUEST_ID_ADD_RMP			1 // (objectid)
#define REQUEST_ID_DEL_RMP			2 // (objectid)
#define REQUEST_ID_SET_RMP			3 // (objectid, rmpid)
#define REQUEST_ID_GET_RMP			4 // (rmpid)=>objectid
#define REQUEST_ID_LIST_RMP			5 // objectid .... rmpid
#define REQUEST_ID_UPDATE_RMP		6


void rpc_set_status(const char* status, const char*prefix, const char*message);
void rpc_reset_idle();
void rpc_set_response_msg(const char* msg);
void rpc_set_response_ok(int result);

// get request id
//$ >0 new request
//$ ==0 no request
//$<0 -- error code
int rpc_get_request(int *request_id, int * objectid, int *rmpid);


#define RDBRPC_TEST		0x8000001

#endif //__RDBRPC_H

