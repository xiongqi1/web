#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "log.h"
#include "rdb_event.h"
#include "utils.h"


struct DiagnosticsState_Code
{
	int 	m_code;
	char*	m_desc;
};
static const struct DiagnosticsState_Code g_DiagnosticsState_Code[]=
{
	{DiagnosticsState_Code_None, "None"},
	{DiagnosticsState_Code_Request, "Requested"},
	{DiagnosticsState_Code_Completed, "Completed"},
	{DiagnosticsState_Code_Cancelled, "Cancelled"},
	{DiagnosticsState_Error_InitConnectionFailed, "Error_InitConnectionFailed"},
	{DiagnosticsState_Error_NoResponse, "Error_NoResponse"},
	{DiagnosticsState_Error_TransferFailed, "Error_TransferFailed"},
	{DiagnosticsState_Error_PasswordRequestFailed, "Error_PasswordRequestFailed"},
	{DiagnosticsState_Error_LoginFailed, "Error_LoginFailed"},
	{DiagnosticsState_Error_NoTransferMode, "Error_NoTransferMode"},
	{DiagnosticsState_Error_NoPASV, "Error_NoPASV"},
	{DiagnosticsState_Error_IncorrectSize, "Error_IncorrectSize"},
	{DiagnosticsState_Error_Timeout, "Error_Timeout"},
	{DiagnosticsState_Error_Invalid_URL, "Error_Invalid_URL"},
	{DiagnosticsState_Error_Cancelled, "Error_Cancelled"},
	{0, NULL},

};

int Diagnostics_msg2code(char *msg)
{
	int i = strlen(msg);
    if(i < 2) return DiagnosticsState_Code_None;

    for(i =0;g_DiagnosticsState_Code[i].m_desc;i++)
    {
    	if(strcmp(msg, g_DiagnosticsState_Code[i].m_desc) ==0)
    	{
    		return g_DiagnosticsState_Code[i].m_code;
    	}
    }
    return DiagnosticsState_Code_None;
}


const char *Diagnostics_code2msg(int code)
{
	int i =0;
	for(i =0; g_DiagnosticsState_Code[i].m_desc; i++)
    {
    	if(g_DiagnosticsState_Code[i].m_code == code)
    	{
    		return g_DiagnosticsState_Code[i].m_desc;
    	}
    }
	return 0;
}
// get state code of Download.DiagnositicsState
int get_download_state(int session_id)
{
	char buf[60];

    int retValue;
	retValue = rdb_get_i_str(Diagnostics_Download_DiagnosticsState, session_id, buf,60);
    if(retValue)
    {
        return retValue;
    }
    return Diagnostics_msg2code(buf);
}

// get state code of Upload.DiagnositicsState
int get_upload_state(int session_id)
{
	char buf[60];

    int retValue;
	retValue = rdb_get_i_str(Diagnostics_Upload_DiagnosticsState, session_id, buf,60);

    if(retValue)
    {
        return retValue;
    }
    return Diagnostics_msg2code(buf);

}

// set  Download.DiagnositicsState by state code
void set_download_state(int session_id, int state)
{
	const char *p = Diagnostics_code2msg(state);
	if(p)
	{
		rdb_set_i_str(Diagnostics_Download_DiagnosticsState, session_id, p);

	}
}

// set  Upload.DiagnositicsState by state code
void set_upload_state(int session_id, int state)
{
	const char *p = Diagnostics_code2msg(state);
	if(p)
	{
		rdb_set_i_str(Diagnostics_Upload_DiagnosticsState, session_id, p);

	}
}


// get download.starttest state
int get_download_starttest(int session_id)
{

    int retValue;
	int err = rdb_get_i_boolean(Diagnostics_Download_StartTest, session_id, &retValue);
	if(err) return err;
	return retValue;
}

// get upload.starttest state
int get_upload_starttest(int session_id)
{

    int retValue;
	int err = rdb_get_i_boolean(Diagnostics_Upload_StartTest, session_id, &retValue);
	if(err) return err;
	return retValue;
}




/// load  download rdb variable
///$ ==0 update
///$<0 -- error code
int load_download_rdb(TConnectSession* pConnection, TDownloadSession *pSession)
{
	int retValue;
	char buffer[MAX_URL_LEN];

	unsigned int session_id = pConnection->m_parameters->m_session_id;

	memset(pSession, 0, sizeof(TDownloadSession));
	pSession->m_connection= pConnection;
	pSession->m_parameters= pConnection->m_parameters;

///Diagnostics.Download.DownloadURL 	string 	readwrite 	none 			URL to download for the test. Host part must be resolvable by the WNTD DNS configuration or an IP address specified. HTTP server must be reachable via the test interface.
///#define Diagnostics_Download_DownloadURL "Diagnostics.Download.DownloadURL"

	retValue = rdb_get_i_uint(Diagnostics_Download_CoS, session_id, &pConnection->m_if_cos);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;

	retValue = rdb_get_i_str(Diagnostics_Download_DownloadURL, session_id, pConnection->m_url, MAX_URL_LEN);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;

	retValue = split_url(pConnection->m_url, buffer, MAX_URL_LEN, &pConnection->m_ServerPort);
	if(retValue <0) return DiagnosticsState_Error_Invalid_URL;


	retValue = lookup_hostname(buffer, &pConnection->m_ServerAddress);

	//NTCLOG_DEBUG("host=%s, port=%d, ip=%x, ip=%x", buffer, pConnection->m_ServerPort, pConnection->m_ServerAddress, inet_addr(buffer));

	if(retValue <0) return DiagnosticsState_Error_Invalid_URL;


//	retValue = rdb_get_i_uint(Diagnostics_Download_SampleInterval, session_id, (unsigned int*)&pConnection->m_sample_interval);
	retValue = rdb_get_uint(Diagnostics_HttpMovingAverageWindowSize, (unsigned int*)&pConnection->m_sample_interval);
	if(retValue <0) return DiagnosticsState_Error_IncorrectSize;

	if(pConnection->m_sample_interval==0) pConnection->m_sample_interval = Default_HttpMovingAverageWindowSize;
	pConnection->m_sample_interval *=XSPES;
	//NTCLOG_DEBUG("HttpMovingAverageWindowSize=%dms", pConnection->m_sample_interval);
	return 0;
}

/// load upload rdb variable
///$ ==0 update
///$<0 -- error code
int load_upload_rdb(TConnectSession* pConnection, TUploadSession *pSession)
{
	int retValue;

	char buffer[MAX_URL_LEN];
	unsigned int session_id = pConnection->m_parameters->m_session_id;

	memset(pSession, 0, sizeof(TUploadSession));
	pSession->m_connection = pConnection;
	pSession->m_parameters= pConnection->m_parameters;

	retValue = rdb_get_i_uint(Diagnostics_Upload_CoS, session_id, &pConnection->m_if_cos);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;


///Diagnostics.Upload.UploadURL 	string 	readwrite 	none 			URL to upload for the test. Host part must be resolvable by the WNTD DNS configuration or an IP address specified. HTTP server must be reachable via the test interface.
	retValue = rdb_get_i_str(Diagnostics_Upload_UploadURL, session_id, pConnection->m_url, MAX_URL_LEN);
	if(retValue <0) return DiagnosticsState_Error_Invalid_URL;

	retValue = split_url(pConnection->m_url, buffer, MAX_URL_LEN, &pConnection->m_ServerPort);
	if(retValue <0) return DiagnosticsState_Error_Invalid_URL;


	retValue = lookup_hostname(buffer, &pConnection->m_ServerAddress);
	//NTCLOG_DEBUG("host=%s, port=%d, retValue=%d", inet_ntoa(*((struct in_addr*)&pConnection->m_ServerAddress)), pConnection->m_ServerPort, retValue);

	if(retValue <0) return DiagnosticsState_Error_Invalid_URL;


///Diagnostics.Upload.TestFileLength 	uint 	readwrite 	none 	0 		Size of test file to generate in bytes.
///#define Diagnostics_Upload_TestFileLength "Diagnostics.Upload.TestFileLength"
	retValue = rdb_get_i_uint(Diagnostics_Upload_TestFileLength, session_id, &pSession->m_TestFileLength);
	if(retValue <0) return DiagnosticsState_Error_InitConnectionFailed;



	//retValue = rdb_get_i_uint(Diagnostics_Upload_SampleInterval, session_id, (unsigned int*)&pConnection->m_sample_interval);

	retValue = rdb_get_uint(Diagnostics_HttpMovingAverageWindowSize, (unsigned int*)&pConnection->m_sample_interval);
	if(retValue <0) return DiagnosticsState_Error_IncorrectSize;

	if(pConnection->m_sample_interval==0) pConnection->m_sample_interval = Default_HttpMovingAverageWindowSize;
	pConnection->m_sample_interval *=XSPES;

	return 0;
}



///$ ==0 update
///$<0 -- error code
int update_download_session(TDownloadSession*pSession)
{
	unsigned int session_id = pSession->m_parameters->m_session_id;
	//int delta_throughput=0;// the throughput difference betweeen download data and RX on interface
	int throughput =0;

	//char time_string[60];
///Diagnostics.Download.ROMTime 	datetime 	readonly 	none 			Request time.
//#define Diagnostics_Download_ROMTime "Diagnostics.Download.ROMTime"
	rdb_set_i_timestamp(Diagnostics_Download_ROMTime, session_id, &pSession->m_connection->m_ROMTime);

///Diagnostics.Download.BOMTime 	datetime 	readonly 	none 			Beginning of download time.
//#define Diagnostics_Download_BOMTime "Diagnostics.Download.BOMTime"
	rdb_set_i_timestamp(Diagnostics_Download_BOMTime, session_id, &pSession->m_connection->m_BOMTime);

///Diagnostics.Download.EOMTime 	datetime 	readonly 	none 			Completion of download time.
//#define Diagnostics_Download_EOMTime "Diagnostics.Download.EOMTime"
	rdb_set_i_timestamp(Diagnostics_Download_EOMTime, session_id, &pSession->m_connection->m_EOMTime);

///Diagnostics.Download.TestBytesReceived 	uint 	readonly 	none 	0 		Size of the file received.
//#define Diagnostics_Download_TestBytesReceived "Diagnostics.Download.TestBytesReceived"
	rdb_set_i_uint(Diagnostics_Download_TestBytesReceived, session_id, pSession->m_TestBytesReceived);

///Diagnostics.Download.TotalBytesReceived 	uint 	readonly 	none 	0 		Bytes received over interface during download.
//#define Diagnostics_Download_TotalBytesReceived "Diagnostics.Download.TotalBytesReceived"
	pSession->m_TotalBytesReceived = pSession->m_connection->m_if_rx;
	if(pSession->m_TotalBytesReceived <= 0 )
	{
		NTCLOG_NOTICE("Invalid rx Bytes from interface %s", pSession->m_connection->m_if_name);
		pSession->m_TotalBytesReceived = pSession->m_TestBytesReceived;
	}
	rdb_set_i_uint(Diagnostics_Download_TotalBytesReceived, session_id, pSession->m_TotalBytesReceived);


	/// update throught
	{
		int64_t duration = tv_xs(&pSession->m_connection->m_EOMTime)- tv_xs(&pSession->m_connection->m_BOMTime);
	
		if(duration > 0)
		{
			throughput = (int64_t)pSession->m_TotalBytesReceived*XSPES / duration*8;
			NTCLOG_DEBUG("throughput=%d,  xs=%lld",  throughput,  duration);
		}
		else
		{
			duration = tv_us(&pSession->m_connection->m_EOMTime)- tv_us(&pSession->m_connection->m_BOMTime);
			if(duration <= 0) duration=1;
			throughput = (int64_t)pSession->m_TotalBytesReceived*USPES/ duration*8;
			NTCLOG_DEBUG("throughput=%d,  us=%lld", throughput,  duration);

		}
		rdb_set_i_uint(Diagnostics_Download_Throughput, session_id, throughput);
	}


	if(pSession->m_parameters->m_enable_sample_window && pSession->m_connection->m_sample_count > 0)
	{
		// due to some overhead between the end of http session and the finish of curl lib call.
		// the throughput may be outside of the range min, max. This should be avoided.
		if(throughput < pSession->m_connection->m_min_throughput || throughput > pSession->m_connection->m_max_throughput)
		{
			pSession->m_connection->m_min_throughput = throughput;
			pSession->m_connection->m_max_throughput = throughput;
		}
	}
	else
	{
		pSession->m_connection->m_min_throughput = throughput;
		pSession->m_connection->m_max_throughput = throughput;
	}
	rdb_set_i_uint(Diagnostics_Download_MinThroughput, session_id, pSession->m_connection->m_min_throughput);
	rdb_set_i_uint(Diagnostics_Download_MaxThroughput, session_id, pSession->m_connection->m_max_throughput);
	NTCLOG_DEBUG("throughput: min=%d, max=%d", pSession->m_connection->m_min_throughput, pSession->m_connection->m_max_throughput);

	return 0;
}


///$ ==0 update
///$<0 -- error code
int update_upload_session(TUploadSession*pSession)
{

	unsigned int session_id = pSession->m_parameters->m_session_id;
	//int delta_throughput=0;// the throughput difference betweeen download data and RX on interface
	int throughput =0;

	//char time_string[60];
///Diagnostics.Upload.ROMTime 	datetime 	readonly 	none 			Request time.
///#define Diagnostics_Upload_ROMTime "Diagnostics.Upload.ROMTime"
	rdb_set_i_timestamp(Diagnostics_Upload_ROMTime, session_id,   &pSession->m_connection->m_ROMTime);

///Diagnostics.Upload.BOMTime 	datetime 	readonly 	none 			Beginning of upload time.
///#define Diagnostics_Upload_BOMTime "Diagnostics.Upload.BOMTime"
	rdb_set_i_timestamp(Diagnostics_Upload_BOMTime, session_id, &pSession->m_connection->m_BOMTime);

///Diagnostics.Upload.EOMTime 	datetime 	readonly 	none 			Completion of upload time.
///#define Diagnostics_Upload_EOMTime "Diagnostics.Upload.EOMTime"
	rdb_set_i_timestamp(Diagnostics_Upload_EOMTime, session_id, &pSession->m_connection->m_EOMTime);

///Diagnostics.Upload.TotalBytesSent 	uint 	readonly 	none 	0 		Bytes sent into interface during upload.
///#define Diagnostics_Upload_TotalBytesSent "Diagnostics.Upload.TotalBytesSent"
	pSession->m_TotalBytesSent = pSession->m_connection->m_if_tx;
	if(pSession->m_TotalBytesSent  <= 0 )
	{
		NTCLOG_NOTICE("Invalid tx Bytes from interface %s", pSession->m_connection->m_if_name);
		pSession->m_TotalBytesSent = pSession->m_TestFileLength;
	}

	rdb_set_i_uint(Diagnostics_Upload_TotalBytesSent, session_id, pSession->m_TotalBytesSent);
	/// update throught
	{
		int64_t duration = tv_xs(&pSession->m_connection->m_EOMTime)- tv_xs(&pSession->m_connection->m_BOMTime);
		if(duration > 0 )
		{
			throughput = (int64_t)pSession->m_TotalBytesSent *XSPES / duration*8;	
			NTCLOG_DEBUG("throughput=%d, xs=%lld", throughput,  duration);
		}
		else
		{
			duration = tv_us(&pSession->m_connection->m_EOMTime)- tv_us(&pSession->m_connection->m_BOMTime);
			if(duration <= 0) duration=1;
			throughput = (int64_t)pSession->m_TotalBytesSent*USPES/ duration*8;
			NTCLOG_DEBUG("throughput=%d, us=%lld", throughput,  duration);
		}

		rdb_set_i_uint(Diagnostics_Upload_Throughput, session_id, throughput);
	}


	if(pSession->m_parameters->m_enable_sample_window && pSession->m_connection->m_sample_count > 0)
	{
		// due to some overhead between the end of http session and the finish of curl lib call.
		// the throughput may be outside of the range min, max. This should be avoided.
		if(throughput < pSession->m_connection->m_min_throughput || throughput > pSession->m_connection->m_max_throughput)
		{
			pSession->m_connection->m_min_throughput = throughput;
			pSession->m_connection->m_max_throughput = throughput;
		}
	}
	else
	{
		pSession->m_connection->m_min_throughput = throughput;
		pSession->m_connection->m_max_throughput = throughput;
	}

	rdb_set_i_uint(Diagnostics_Upload_MinThroughput, session_id, pSession->m_connection->m_min_throughput);
	rdb_set_i_uint(Diagnostics_Upload_MaxThroughput, session_id, pSession->m_connection->m_max_throughput);
	NTCLOG_DEBUG("throughput: min=%d, max=%d", pSession->m_connection->m_min_throughput, pSession->m_connection->m_max_throughput);
	return 0;
}

