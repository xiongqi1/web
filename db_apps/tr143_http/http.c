/* ----------------------------------------------------------------------------
Ping main program

Lee Huang<leeh@netcomm.com.au>

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include "log.h"


#include "comms.h"
#include "http.h"
#include "rdb_event.h"
#include "utils.h"

// check whether the download has been canceled
// 1 -- canceled
// 0 -- not
int is_download_canceled(TParameters *pParam)
{
	rdb_get_i_str(Diagnostics_Download_Changed, pParam->m_session_id, NULL, 0);
	if(pParam->m_console_test_mode)
	{
		if (get_download_starttest(pParam->m_session_id) ==0) return 1;
	}
	else
	{
		if(get_download_state(pParam->m_session_id) == DiagnosticsState_Code_Cancelled) return 1;
	}
	return 0;
}

// check whether the download has been canceled
// 1 -- canceled
// 0 -- not
int is_upload_canceled(TParameters *pParam)
{
	rdb_get_i_str(Diagnostics_Upload_Changed, pParam->m_session_id, NULL, 0);
	if(pParam->m_console_test_mode)
	{
		if (get_upload_starttest(pParam->m_session_id) ==0) return 1;
	}
	else
	{
		if(get_upload_state(pParam->m_session_id) == DiagnosticsState_Code_Cancelled) return 1;
	}
	return 0;
}


static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{

    TDownloadSession*pSession = (TDownloadSession*)stream;
    // if request changed, stop
    if(pSession)
    {
    	if(poll_rdb(0, 0) >0)
		{
			if(is_download_canceled(pSession->m_parameters))
			{
				pSession->m_connection->m_stop_by_rdb_change =1;
				NTCLOG_DEBUG("RDB changed");
				return 0; // stop http session
			}

		}

		if(pSession->m_parameters->m_running ==0)
		{
			NTCLOG_INFO("Stop by user");
			return 0; // stop http session
		}
		if(pSession->m_TestBytesReceived ==0)
		{
			gettimeofday(&pSession->m_connection->m_BOMTime, NULL);
			pSession->m_connection->m_last_sample_time= tv_xs(&pSession->m_connection->m_BOMTime);
		}
		pSession->m_TestBytesReceived+= nmemb;
    }

	return nmemb;
}


static size_t upload_write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	//NTCLOG_DEBUG("(reponse msg %s, nmemb=%d)",  ptr,  nmemb);

	return nmemb;
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    TUploadSession*pSession = (TUploadSession*)stream;
    if(pSession)
	{
		// if request changed, stop
		if(poll_rdb(0, 0) >0)
		{
			if(is_upload_canceled(pSession->m_parameters))
			{
				pSession->m_connection->m_stop_by_rdb_change =1;
				NTCLOG_DEBUG("RDB changed");
				return CURL_READFUNC_ABORT;
			}

		}
		// if user stop it


		if(pSession->m_parameters->m_running ==0)
		{
			NTCLOG_INFO("Stop by user");

			return CURL_READFUNC_ABORT;
		}
		int left_byte = pSession->m_TestFileLength -pSession->m_BytesSent;

		if(pSession->m_BytesSent ==0)
		{
			gettimeofday(&pSession->m_connection->m_BOMTime, NULL);
			pSession->m_connection->m_last_sample_time= tv_xs(&pSession->m_connection->m_BOMTime);
		}
		nmemb = nmemb > left_byte? left_byte: nmemb;
		pSession->m_BytesSent+= nmemb;
    }
	//NTCLOG_DEBUG("read_callback, size=%d, nmemb=%d", size, nmemb);

    return nmemb;
}

int progress_callback(void *clientp,
                      double dltotal,
                      double dlnow,
                      double ultotal,
                      double ulnow)
{
	TConnectSession*	pConnection = (TConnectSession*)clientp;
	int64_t now = gettimeofdayXS();
	int64_t duration = now - pConnection->m_last_sample_time;
	int64_t lnow = dlnow;
	int64_t ltotal = dltotal;

	if(pConnection->m_parameters->m_enabled_function & RDB_GROUP_UPLOAD)
	{
		lnow = ulnow;
		ltotal = ultotal;
	}
	// ignore it if the period is smaller than 500ms
	if (lnow == 0 || duration < 500) return 0;

	if(duration >= pConnection->m_sample_interval || ltotal == lnow)
	{
		if(lnow  > pConnection->m_last_sample_transfer_bytes )
		{
			int64_t 	throughput = -1;
#if 1
			int64_t rx, tx;
			int64_t lnow_raw =lnow;
			int rx_pkts, tx_pkts;
			if (get_if_rx_tx(pConnection->m_if_name, &rx, &tx,&rx_pkts, &tx_pkts ))
			{
				if(pConnection->m_parameters->m_enabled_function & RDB_GROUP_UPLOAD)
				{
					lnow_raw = tx -  pConnection->m_if_tx;

				}
				else
				{
					lnow_raw = rx -  pConnection->m_if_rx;
				}
				if(lnow_raw  > pConnection->m_last_sample_raw_transfer_bytes)
				{
					//throughput unit is bps(bits/s)
					throughput =  (lnow_raw -pConnection->m_last_sample_raw_transfer_bytes)*XSPES/duration*8;

					pConnection->m_last_sample_raw_transfer_bytes = lnow_raw;

					NTCLOG_DEBUG("(lnow=%lld, ltotal=%lld), throughput=%lld, time=%lld",  lnow_raw,  ltotal, throughput, duration);
				}
			}
			if(throughput < 0)
			{
				throughput =  (lnow -pConnection->m_last_sample_transfer_bytes)*XSPES/duration*8;
				NTCLOG_DEBUG("(*lnow=%lld, ltotal=%lld), throughput=%lld, time=%lld",  lnow,  ltotal, throughput, duration);
			}
#endif
			if(pConnection->m_sample_count < 1)
			{
				pConnection->m_min_throughput = (int)throughput;
				pConnection->m_max_throughput = (int)throughput;
			}
			else
			{
				if(throughput < pConnection->m_min_throughput )pConnection->m_min_throughput = throughput;
				if(throughput > pConnection->m_max_throughput)pConnection->m_max_throughput = throughput;

			}
			pConnection->m_last_sample_time = now;
			pConnection->m_last_sample_transfer_bytes= (int)lnow;
			pConnection->m_sample_count ++;
		}
	}

	return 0;
}

int curl_error_convert(int error)
{
    if(error )
    {
        switch(error)
        {
        case CURLE_URL_MALFORMAT:
            error = DiagnosticsState_Error_Invalid_URL;
            break;
		case CURLE_HTTP_RETURNED_ERROR:
		case CURLE_GOT_NOTHING:
			error =DiagnosticsState_Error_NoResponse;
			break;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_REMOTE_ACCESS_DENIED:
		case CURLE_OUT_OF_MEMORY:
		case CURLE_INTERFACE_FAILED:
            error =DiagnosticsState_Error_InitConnectionFailed;
            break;
		case CURLE_RECV_ERROR:
		case CURLE_SEND_ERROR:
			error =DiagnosticsState_Error_TransferFailed;
			break;
        case CURLE_PARTIAL_FILE:
        case CURLE_FILESIZE_EXCEEDED:
        case CURLE_WRITE_ERROR:
            error =DiagnosticsState_Error_IncorrectSize;
            break;
        case CURLE_OPERATION_TIMEDOUT:
            error = DiagnosticsState_Error_Timeout;
            break;
		case CURLE_ABORTED_BY_CALLBACK:
			error = DiagnosticsState_Error_Cancelled;
			break;

        default:
            error =DiagnosticsState_Error_NoTransferMode;
            break;
        }
    }
    return error;
}

// download file from url
// $0 -- success
// $<0 -- error code
int http_download(TConnectSession*pConnection, TDownloadSession *pSession)
{
  //  double tmp_d;
    //long tmp_int;

    int error =0;

    NTCLOG_INFO("start http_download");

    error = make_connection(pConnection);
    if(error ) return error;

    //1) prepare http request

    /* Define our callback to get called when there's data to be written */
    curl_easy_setopt(pConnection->m_curl, CURLOPT_WRITEFUNCTION, write_data);

    curl_easy_setopt(pConnection->m_curl, CURLOPT_WRITEDATA, (void *)pSession);// we pass our 'chunk' struct to the callback function

//    curl_easy_setopt(pConnection->m_curl, CURLOPT_NOSIGNAL, 1L);


    curl_easy_setopt(pConnection->m_curl, CURLOPT_URL, pConnection->m_url);    //url

	//   NTCLOG_DEBUG("enable_sample_window=%d, sample_interval=%d", pConnection->m_parameters->m_enable_sample_window, pConnection->m_sample_interval);
	if(pConnection->m_parameters->m_enable_sample_window &&pConnection->m_sample_interval >0 )
	{

		curl_easy_setopt(pConnection->m_curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
		/* pass the struct pointer into the progress function */
		curl_easy_setopt(pConnection->m_curl, CURLOPT_PROGRESSDATA, pConnection);
		curl_easy_setopt(pConnection->m_curl, CURLOPT_NOPROGRESS, 0L);
	}
	gettimeofday(&pConnection->m_ROMTime, NULL);

	gettimeofday(&pConnection->m_BOMTime, NULL);

	pConnection->m_last_sample_time= tv_xs(&pConnection->m_BOMTime);

//	get_if_rx_tx("eth1", &if_rx, &if_tx);

	//3 start download
	/* get it! */
	error = curl_easy_perform(pConnection->m_curl);

	if(error)
	{
		error = curl_error_convert(error);
	}
	gettimeofday(&pConnection->m_EOMTime, NULL);
	// because we don't know when the last progress_callback is called, we may miss the last throughput calculation.
	// calculate it now
	if(pConnection->m_sample_count > 0)
	{
		progress_callback(pConnection, pSession->m_TestBytesReceived, pSession->m_TestBytesReceived, 0, 0);
	}
	NTCLOG_INFO("Download error=%d, rdb_changed=%d", error, pSession->m_connection->m_stop_by_rdb_change);
	//if download stopped and caused by user request
	if(error && pSession->m_connection->m_stop_by_rdb_change){
		error = DiagnosticsState_Error_Cancelled;
	}

	close_connection(pConnection);

	return error;
}

//int curl_setsockopt(void *clientp, curl_socket_t curlfd, curlsocktype purpose)
//{
//	int len= 4096;
//	int error = setsockopt(curlfd, SOL_SOCKET, SO_SNDBUF, &len, sizeof len);
//	NTCLOG_DEBUG("set SO_SNFBUF to %d, fd=%d, err=%d", len, curlfd, error);
//	return error;
//}

// upload file to url
// $0 -- success
// $<0 -- error code
int http_upload(TConnectSession*pConnection, TUploadSession *pSession)
{

	double tmp_d;

	int error =0;

	NTCLOG_INFO("start http_upload");


	error = make_connection(pConnection);
	if(error ) return error;


	/* Define our callback to get called when there's data to be written */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_WRITEFUNCTION, upload_write_data);

	/* Define our callback to get called when there's data to be written */

	curl_easy_setopt(pConnection->m_curl, CURLOPT_WRITEDATA, (void *)NULL);// we pass our 'chunk' struct to the callback function

	/* specify target URL, and note that this URL should include a file
		name, not only a directory */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_URL, pConnection->m_url);




	/* we want to use our own read function */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_READFUNCTION, read_callback);

	/* now specify which file to upload */
	/* enable uploading */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_UPLOAD, 1L);


	/* HTTP PUT please */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_PUT, 1L);

	if(pConnection->m_parameters->m_enable_sample_window &&pConnection->m_sample_interval >0 )
	{
		curl_easy_setopt(pConnection->m_curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
		/* pass the struct pointer into the progress function */
		curl_easy_setopt(pConnection->m_curl, CURLOPT_PROGRESSDATA, pConnection);
		curl_easy_setopt(pConnection->m_curl, CURLOPT_NOPROGRESS, 0L);
	}

	/* provide the size of the upload, we specicially typecast the value
		to curl_off_t since we must be sure to use the correct data size
		the read_callback may read more than this 'size'
		*/
	curl_easy_setopt(pConnection->m_curl, CURLOPT_INFILESIZE_LARGE,
						(curl_off_t)pSession->m_TestFileLength);

	/* now specify which file to upload */
	curl_easy_setopt(pConnection->m_curl, CURLOPT_READDATA,  (void *)pSession);



	//NTCLOG_DEBUG("upload FileSize=%d", pSession->m_TestFileLength);
	/* Now run off and do what you've been told! */


	gettimeofday(&pConnection->m_ROMTime, NULL);

	gettimeofday(&pConnection->m_BOMTime, NULL);


	pConnection->m_last_sample_time = tv_xs(&pConnection->m_BOMTime); //gettimeofdayMS();


	error = curl_easy_perform(pConnection->m_curl);

	gettimeofday(&pConnection->m_EOMTime, NULL);
	// because we don't know when the last progress_callback is called, we may miss the last throughput calculation.
	// calculate it now
	if(pConnection->m_sample_count > 0)
	{
		progress_callback(pConnection, 0, 0, pSession->m_BytesSent, pSession->m_BytesSent);
	}
	NTCLOG_INFO("upload error=%d, rdb_changed=%d", error, pSession->m_connection->m_stop_by_rdb_change);
	if(error)
	{
		error = curl_error_convert(error);
	}
	else
	{
		///Pass a pointer to a double to receive the total amount of bytes that were uploaded.
		curl_easy_getinfo(pConnection->m_curl,CURLINFO_SIZE_UPLOAD, &tmp_d);// grabbing it from pConnection->m_curl
		//NTCLOG_DEBUG("upload size=%d", (int)tmp_d);
		if(tmp_d < pSession->m_TestFileLength)
		{
			error =DiagnosticsState_Error_IncorrectSize;
		}
	}

	//if upload stopped and caused by user request
	if(error && pSession->m_connection->m_stop_by_rdb_change){
		error = DiagnosticsState_Error_Cancelled;
	}

	close_connection(pConnection);

	return error;
}



