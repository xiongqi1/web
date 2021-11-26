
/* ----------------------------------------------------------------------------
RDB/Indoor event handling program

Lee Huang<leeh@netcomm.com.au>

*/

#ifndef __RDB_EVENT_H
#define __RDB_EVENT_H
#include "rdb_comms.h"
#include "http.h"



// requrest
static inline int  open_rdb(int session_id, int rdb_group, int subs_group)
{
    int retValue = rdb_open(RDB_DEVNAME, &g_rdb_session);
    if(retValue <0) return retValue;

    retValue = subscribe(session_id, rdb_group, subs_group);
    if(retValue <0) return retValue;

    return retValue;
}




static inline void close_rdb()
{
    rdb_close(&g_rdb_session);
    g_rdb_session=NULL;
}
///    * "None" - service is idle waiting for a request.
///   * "Requested" - set by ACS after configuring test to request CPE to begin the test.
///    * "Completed" - set by CPE to indicate successful test completion.
///    * "Error_<description>" - set by CPE to indicate failure of test.

#define DiagnosticsState_Code_None 					0	//"None"
#define DiagnosticsState_Code_Request 				1//"Requested"
#define DiagnosticsState_Code_Completed 			2 //"Completed"
#define DiagnosticsState_Code_Cancelled				3 //"Cancelled"

#define DiagnosticsState_Error_InitConnectionFailed -1 //"Error_InitConnectionFailed"
#define DiagnosticsState_Error_NoResponse 			-2	//"Error_NoResponse
#define DiagnosticsState_Error_TransferFailed 		-3	//"Error_TransferFailed"
#define DiagnosticsState_Error_PasswordRequestFailed -4	//"Error_PasswordRequestFailed"
#define DiagnosticsState_Error_LoginFailed 			-5//"Error_LoginFailed"
#define DiagnosticsState_Error_NoTransferMode 		-6//"Error_NoTransferMode"
#define DiagnosticsState_Error_NoPASV 				-7//"Error_NoPASV"
#define DiagnosticsState_Error_IncorrectSize 		-8//"Error_IncorrectSize"
#define DiagnosticsState_Error_Timeout 				-9//"Error_Timeout"
#define DiagnosticsState_Error_Invalid_URL			-10//"Error_Invalid_URL"
#define DiagnosticsState_Error_Cancelled			-11//"Error_Canceled"


// get state code of Download.DiagnositicsState
int get_download_state(int session_id);

// get state code of Upload.DiagnositicsState
int get_upload_state(int session_id);

// set  Download.DiagnositicsState by state code
void set_download_state(int session_id, int state);

// set  Upload.DiagnositicsState by state code
void set_upload_state(int session_id, int state);

// get download.starttest state
int get_download_starttest(int session_id);

// get upload.starttest state
int get_upload_starttest(int session_id);


//$ ==0 update
//$<0 -- error code
int load_download_rdb(TConnectSession* pConnection, TDownloadSession *pDownloadSession);

//$ ==0 update
//$<0 -- error code
int load_upload_rdb(TConnectSession* pConnection, TUploadSession *pUploadSession);


//$ ==0 update
//$<0 -- error code
int update_download_session(TDownloadSession*TSession);

//$ ==0 update
//$<0 -- error code
int update_upload_session(TUploadSession*TSession);



/// save download rdb variable
static inline void url_download2rdb( unsigned session_id, const char*url)
{
	if(url)
		rdb_set_i_str(Diagnostics_Download_DownloadURL, session_id, url);

}
static inline void ifd_ip2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Download_InterfaceAddress, session_id, ip);
}


static inline void ifd_mask2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Download_InterfaceNetmask, session_id, ip);
}

static inline void ifd_routr2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Download_SmartEdgeAddress, session_id, ip);
}

static inline void ifd_MPLSTag2rdb(unsigned session_id, int tag)
{
	if(tag >= 0)
		rdb_set_i_uint(Diagnostics_Download_MPLSTag, session_id, tag);
}

static inline void ifd_Cos2rdb(unsigned session_id, int cos)
{
	if(cos >=0)
		rdb_set_i_uint(Diagnostics_Download_CoS, session_id, cos);
}

static inline void if_sample_interval(unsigned session_id, int sample_interval)
{
	if(sample_interval> 0)
		rdb_set_uint(Diagnostics_HttpMovingAverageWindowSize, sample_interval);
}

/// save upload rdb variable
static inline  void url_upload2rdb(unsigned session_id,  const char*url)
{
	if(url)
		rdb_set_i_str(Diagnostics_Upload_UploadURL, session_id, url);

}

static inline void ifu_ip2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Upload_InterfaceAddress, session_id, ip);
}


static inline void ifu_mask2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Upload_InterfaceNetmask, session_id, ip);
}

static inline void ifu_routr2rdb(unsigned session_id, const char*ip)
{
	if(ip)
		rdb_set_i_str(Diagnostics_Upload_SmartEdgeAddress, session_id, ip);
}
static inline void ifu_size2rdb(unsigned session_id, int size)
{
	if(size >0)
		rdb_set_i_uint(Diagnostics_Upload_TestFileLength, session_id, size);
}
static inline void ifu_MPLSTag2rdb(unsigned session_id, int tag)
{
	if(tag >=0)
		rdb_set_i_uint(Diagnostics_Upload_MPLSTag, session_id, tag);
}

static inline void ifu_Cos2rdb(unsigned session_id, int cos)
{
	if(cos >=0)
		rdb_set_i_uint(Diagnostics_Upload_CoS, session_id, cos);
}

//static inline void ifu_sample_interval(unsigned session_id, int sample_interval)
//{
//	if(sample_interval> 0)
//		rdb_set_i_uint(Diagnostics_Upload_SampleInterval, session_id, sample_interval);
//}

#endif
