/* ----------------------------------------------------------------------------
main loop program

Lee Huang<leeh@netcomm.com.au>

*/

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "log.h"

#include "rdb_event.h"
#include "luaCall.h"
#include "http.h"
#include "utils.h"


/// state machine

TDownloadSession g_downloadSession;
TUploadSession g_uploadSession;

TConnectSession g_connectSession;


/// start download test
// prepare download test
// $0 -- success
// $ <0 -- error code
int prepare_download_test(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession);

/// start download test
/// auto_stop -- stop interface after finish
/// 0 -- success
/// $<0 error
int start_download_test(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession,
				int stop_if);

/// stop console download test
int stop_download_test(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession);

/// get stats of  console dowload test
int download_test_stats(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession);


/// start upload test
// prepare upload test
// $0 -- success
// $ <0 -- error code
int prepare_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession);

int start_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession,
					int stop_if);

int stop_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession);



// update interface stats
int update_http_test_stats(TConnectSession *pSession);

////////////////////////////////////////////////////////////////////////////////
int main_loop(TParameters *pParameters)
{
    int retValue;
    int start_time= time(NULL);
    int current_time=start_time;
	TDownloadSession * pDownloadSession =&g_downloadSession;
	TUploadSession *pUploadSession = &g_uploadSession;
	TConnectSession*pConnectSession = &g_connectSession;

	memset(&g_connectSession, 0, sizeof(TConnectSession));
	memset(pDownloadSession, 0, sizeof(TDownloadSession));
	memset(pUploadSession, 0, sizeof(TUploadSession));


	curl_global_init(CURL_GLOBAL_DEFAULT);


    pParameters->m_running =1;

    /// 0 -- start main loop
	NTCLOG_INFO("start main loop");
	while (pParameters->m_running)
	{
		// poll rdb with 1 second timeout
		int selected = poll_rdb(1, 0);


        /// 1 -- stop running, extern request
		if (!pParameters->m_running)
		{
			NTCLOG_INFO("running flag off");
			break;
		}
		else if (selected < 0) /// 2 ---faulty condition
		{

			// if system call
			if (errno == EINTR)
			{
				NTCLOG_NOTICE("system call detected");
				continue;
			}

			NTCLOG_DEBUG("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		else if (selected > 0) { // detect fd
		    /// 3 --- found RDB changed
			/* process rdb port read */
			NTCLOG_DEBUG("getting RDB event, selected=%d", selected);

			if( pParameters->m_enabled_function &RDB_GROUP_DOWNLOAD)
			{
				NTCLOG_DEBUG("getting Download RDB event, selected=%d", selected);
				rdb_get_i_str(Diagnostics_Download_Changed, pParameters->m_session_id, NULL, 0);

				if(pParameters->m_console_test_mode)
				{
					retValue = get_download_starttest(pParameters->m_session_id);
					if( retValue >0) // start test
					{
						prepare_download_test(pConnectSession, pParameters, pDownloadSession);
						start_download_test(pConnectSession, pParameters, pDownloadSession, 0);
						if(pConnectSession->m_stop_by_rdb_change)
						{
							stop_download_test(pConnectSession, pParameters, pDownloadSession);
						}
					}
					else if (retValue ==0)
					{
						stop_download_test(pConnectSession, pParameters, pDownloadSession);
					}
				}
				else
				{
					/// 4 -- check download state
					retValue = get_download_state(pParameters->m_session_id);
					switch (retValue) {
					case DiagnosticsState_Code_Request:
						prepare_download_test(pConnectSession, pParameters, pDownloadSession);
						start_download_test(pConnectSession, pParameters, pDownloadSession, 1);
						break;
					case DiagnosticsState_Code_Cancelled:
					case DiagnosticsState_Code_None:
						set_download_state(pParameters->m_session_id, DiagnosticsState_Code_None);
						break;
					default:;
					} //switch(retValue)

				}///if(pParameters->m_console_test_mode)
			}///if( pParameters->m_enabled_function &RDB_GROUP_DOWNLOAD)
			else if( pParameters->m_enabled_function &RDB_GROUP_UPLOAD)
			{
				/// 5-- check upload state
				NTCLOG_DEBUG("getting Upload RDB event");
				rdb_get_i_str(Diagnostics_Upload_Changed, pParameters->m_session_id, NULL, 0);

				if(pParameters->m_console_test_mode)
				{
					retValue = get_upload_starttest(pParameters->m_session_id);
					if( retValue >0) // start test
					{
						prepare_upload_test(pConnectSession, pParameters, pUploadSession);
						start_upload_test(pConnectSession, pParameters, pUploadSession, 0);
						if(pConnectSession->m_stop_by_rdb_change)
						{
							stop_upload_test(pConnectSession, pParameters, pUploadSession);
						}
					}
					else if (retValue ==0)
					{
						stop_upload_test(pConnectSession, pParameters, pUploadSession);
					}
				}
				else
				{

					retValue = get_upload_state(pParameters->m_session_id);
					switch (retValue) {
					case DiagnosticsState_Code_Request:
						prepare_upload_test(pConnectSession, pParameters, pUploadSession);
						start_upload_test(pConnectSession, pParameters, pUploadSession, 1);
						break;
					case DiagnosticsState_Code_Cancelled:
					case DiagnosticsState_Code_None:
						set_download_state(pParameters->m_session_id, DiagnosticsState_Code_None);
						break;
					default:;
					} //switch(retValue)
				}///if(pParameters->m_console_test_mode)
			}
			if(pParameters->m_cmd_line_mode){
				pParameters->m_running = 0;
			}
			} // if(select ==0


	}//while (pParameters->m_running)

	if( pParameters->m_enabled_function &RDB_GROUP_DOWNLOAD)
	{
		stop_download_test(pConnectSession, pParameters, pDownloadSession);
	}
	else if( pParameters->m_enabled_function &RDB_GROUP_DOWNLOAD)
	{
		stop_upload_test(pConnectSession, pParameters, pUploadSession);
	}

	curl_global_cleanup();


	return 0;
}//int main_loop(TParameters *pParameters)





// prepare download test
// $0 -- success
// $ <0 -- error code
int prepare_download_test(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession)
{
    int retValue =0;
    if(pConnectSession->m_test_ready ==0)
    {
		// 1)
		init_connection(pConnectSession, pParameters, PROTOCOL_NAME_DOWNLOAD);

		//2) collect rdb variable into TParameter
		retValue = load_download_rdb(pConnectSession, pSession);
		if(retValue)
		{
			update_download_session(pSession);
			set_download_state(pParameters->m_session_id, retValue);

			return retValue;
		}
		//3)

		retValue = lua_ifup(pParameters->m_ifup_script, pConnectSession, RDB_VAR_DOWNLOAD_IF);
		if(retValue < 0)
		{
			 NTCLOG_NOTICE("ifup failed");
			 set_download_state(pParameters->m_session_id, DiagnosticsState_Error_InitConnectionFailed);
		}
		else
		{
			pConnectSession->m_test_ready =1;
		}
    }
    else
    {
    	pSession->m_TestBytesReceived =0;
    	pConnectSession->m_stop_by_rdb_change =0;
    	pSession->m_TotalBytesReceived =0;
    	pSession->m_TotalBytesSent =0;

    }

	return retValue;

}//int prepare_download_test(TConnectSession *pConnectSession,


/// start console download test
/// 0 -- success
/// $<0 error
int start_download_test(TConnectSession *pConnectSession,
						TParameters *pParameters,
						TDownloadSession*pSession,
						int stop_if)
{
    int retValue =-1;

	if( pConnectSession->m_test_ready )
	{
		retValue = http_download(pConnectSession, pSession);
		update_http_test_stats(pConnectSession);

		/// the reason put lua_ifdown here is:
		/// lua_ifdown would retrieve tx/rx from interface, this data then update into rbd
		if(stop_if)
		{
			//lua_ifdown(pParameters->m_ifdown_script, pConnectSession);
			//pConnectSession->m_test_ready =0;
			stop_download_test(pConnectSession, pParameters, pSession);

		}
	
		update_download_session(pSession);

		if(retValue ==0) {
			set_download_state(pParameters->m_session_id, DiagnosticsState_Code_Completed);
		}else if(retValue == DiagnosticsState_Error_Cancelled) {
			set_download_state(pParameters->m_session_id, DiagnosticsState_Code_None);
		}else {
			set_download_state(pParameters->m_session_id, retValue);
		}


	}
	return retValue;
}///int start_download_test(TConnectSession *pConnectSession,TParameters *pParameters, TDownloadSession*pSession)



// stop console dowload test
int stop_download_test(TConnectSession *pConnectSession,
				TParameters *pParameters,
				TDownloadSession*pSession)
{

	if( pConnectSession->m_test_ready )
	{
		lua_ifdown(pParameters->m_ifdown_script, pConnectSession, pParameters->m_if_ops);
	}
	pConnectSession->m_test_ready =0;

	return 0;

}///int stop_download_test(TConnectSession *pConnectSession,


// prepare upload test
// $0 -- success
// $ <0 -- error code

int prepare_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession)
{

    int retValue =0;
    if(pConnectSession->m_test_ready ==0)
    {
		//1)
		 init_connection(pConnectSession, pParameters, PROTOCOL_NAME_UPLOAD);
		//2collect rdb variable into TParameter
		retValue = load_upload_rdb(pConnectSession, pSession);
		if(retValue)
		{
			update_upload_session(pSession);
			set_upload_state(pParameters->m_session_id, retValue);

			return retValue;
		}
		//3)

		retValue = lua_ifup(pParameters->m_ifup_script, pConnectSession, RDB_VAR_UPLOAD_IF);
		if(retValue < 0)
		{
			 NTCLOG_NOTICE("ifup failed");
			 set_upload_state(pParameters->m_session_id, DiagnosticsState_Error_InitConnectionFailed);
		}
		else
		{
			pConnectSession->m_test_ready =1;
		}
    }
    else
    {
    	pSession->m_BytesSent =0;
    	pConnectSession->m_stop_by_rdb_change =0;
    	pSession->m_TotalBytesSent =0;

    }
	return retValue;
}///int prepare_upload_test(TConnectSession *pConnectSession,



/// console upload test
/// 0 -- success
/// $<0 error
int start_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession,
					int stop_if)
{
    int retValue = -1;
	if(pConnectSession->m_test_ready)
	{

		retValue = http_upload(pConnectSession, pSession);
		update_http_test_stats(pConnectSession);
		

		/// the reason put lua_ifdown here is:
		/// lua_ifdown would retrieve tx/rx from interface, this data then update into rbd
		if(stop_if)
		{
			stop_upload_test(pConnectSession, pParameters, pSession);

		}


		update_upload_session(pSession);

		// Set Diagnosticstate: 0=>Completed, Cancelled => None or others
		if(retValue == 0) {
			set_upload_state(pParameters->m_session_id, DiagnosticsState_Code_Completed);
		}else if(retValue == DiagnosticsState_Error_Cancelled) {
			set_upload_state(pParameters->m_session_id, DiagnosticsState_Code_None);
		}else {
			set_upload_state(pParameters->m_session_id, retValue);
		}

	}
	return retValue;
}///int start_upload_test(TConnectSession *pConnectSession,

int stop_upload_test(TConnectSession *pConnectSession,
					TParameters *pParameters,
					TUploadSession*pSession)
{
	if(pConnectSession->m_test_ready)
	{
		// drop down if, before set DiagnosticsState, this will make sure user read correct value
		// call ifdown anyway to retrieve total bytes
		lua_ifdown(pParameters->m_ifdown_script, pConnectSession, pParameters->m_if_ops);
	}
	pConnectSession->m_test_ready =0;
	return 0;
}


// update interface stats
int update_http_test_stats(TConnectSession *pSession)
{
	int64_t rx, tx;
	int rx_pkts, tx_pkts;
	int error=0;
	if(pSession->m_test_ready)
	{
		if(get_if_rx_tx(pSession->m_if_name, &rx,&tx, &rx_pkts, &tx_pkts))
		{
			pSession->m_if_rx = rx - pSession->m_if_rx;
			pSession->m_if_tx = tx - pSession->m_if_tx;
			pSession->m_if_rx_pkts = rx_pkts - pSession->m_if_rx_pkts;
			pSession->m_if_tx_pkts = tx_pkts - pSession->m_if_tx_pkts;
			error=-1;
		}
	}
	return error;
}

