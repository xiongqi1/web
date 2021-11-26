#include <string.h>
#include "log.h"

#include "rdb_event.h"
#include "comms.h"

// initilize TConnectSession Struct
void init_connection(TConnectSession *pConnection, TParameters *pParameter, const char*pProtocolName)
{
	memset(pConnection, 0, sizeof(TConnectSession));
	pConnection->m_connect_timeout_ms = pParameter->m_connect_timeout_ms;
	pConnection->m_session_timeout_ms = pParameter->m_session_timeout_ms;
	pConnection->m_protocol_name = pProtocolName;
	pConnection->m_parameters = pParameter;

}



/// connect to sever address
///$ 0 -- success
///$ <0-- error code
int make_connection(TConnectSession *pConnection)
{
	///1) close previous
	if(pConnection->m_curl)
	{
		close_connection(pConnection);
	}
	///2) open new socket
    pConnection->m_curl= curl_easy_init();
    if(pConnection->m_curl ==NULL)
    {
        NTCLOG_ERR("cannot open conection");
        return DiagnosticsState_Error_NoTransferMode;
    }
 // Internal CURL progressmeter must be disabled if we provide our own callback
    curl_easy_setopt(pConnection->m_curl, CURLOPT_NOPROGRESS, 1);

    curl_easy_setopt(pConnection->m_curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    //don't verify peer against cert
    curl_easy_setopt(pConnection->m_curl, CURLOPT_SSL_VERIFYPEER, 0);
    //don't verify host against cert
    curl_easy_setopt(pConnection->m_curl, CURLOPT_SSL_VERIFYHOST, 0);
    //disable signals to use with threads
    curl_easy_setopt(pConnection->m_curl, CURLOPT_NOSIGNAL, 1);
	// set buffer size, default:  20K

	// set connection timeout
	if(pConnection->m_connect_timeout_ms>0)
	{
		curl_easy_setopt(pConnection->m_curl, CURLOPT_CONNECTTIMEOUT_MS, pConnection->m_connect_timeout_ms);
	}
	// set session timeout
	if(pConnection->m_session_timeout_ms>0)
	{
		curl_easy_setopt(pConnection->m_curl, CURLOPT_LOW_SPEED_TIME, pConnection->m_session_timeout_ms/1000);
		curl_easy_setopt(pConnection->m_curl, CURLOPT_LOW_SPEED_LIMIT, 1);
	}
	if(NTCLOG_ENABLED(LOG_DEBUG))
	{
		curl_easy_setopt(pConnection->m_curl, CURLOPT_VERBOSE, 1L);
	}
	else
	{
		curl_easy_setopt(pConnection->m_curl, CURLOPT_VERBOSE, 0L);
	}



	pConnection->m_last_sample_time =0;
	pConnection->m_last_sample_transfer_bytes =0;
	//pConnection->m_last_progress_transfer_bytes=0;
	pConnection->m_sample_count =0;
	pConnection->m_min_throughput = 0;
	pConnection->m_max_throughput =0;
	//NTCLOG_DEBUG("make_connection %p", pConnection->m_curl);

	return 0; //success
}



void close_connection(TConnectSession *pSession)
{
	//NTCLOG_DEBUG("close_connection %p", pSession->m_curl);
	if(pSession->m_curl)
	{
		curl_easy_reset(pSession->m_curl);
		curl_easy_cleanup(pSession->m_curl);
	}
	pSession->m_curl=0;
}
