/* ----------------------------------------------------------------------------
main loop program

Lee Huang<leeh@netcomm.com.au>

*/

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include "log.h"

#include "rdb_event.h"
#include "luaCall.h"

#define PROTOCOL_NAME "ping" // will be used in ifup, ifdown script
/// state machine

TConnectSession g_session;


// prepare ping test
//$0 -- success
//$<0 -- error code
int prepare_ping_test(TParameters *pParameters, TConnectSession *pSession);

int start_ping_test(TParameters *pParameters, TConnectSession *pSession, int stop_if);

int stop_ping_test(TParameters *pParameters, TConnectSession *pSession);


////////////////////////////////////////////////////////////////////////////////
int main_loop(TParameters *pParameters)
{
    int retValue;
    int start_time= time(NULL);
    int current_time=start_time;

    TConnectSession *pSession = &g_session;


    pParameters->m_running =1;
    /// 0 -- start main loog
	NTCLOG_INFO("start main loog");
	while (pParameters->m_running)
	{


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
				NTCLOG_INFO("system call detected");
				continue;
			}

			NTCLOG_INFO("select() punk - error#%d(str%s)",errno,strerror(errno));
			break;
		}
		else if (selected > 0 ){ // detect fd
		    /// 3 --- found RDB changed
			/* process rdb port read */
			rdb_get_i_str(Diagnostics_UDPEcho_Changed, pParameters->m_session_id, NULL, 0);

			NTCLOG_DEBUG("getting RDB event");
			if(pParameters->m_console_test_mode)
			{
				retValue = get_ping_starttest(pParameters->m_session_id);
				if(retValue >0)
				{
					prepare_ping_test(pParameters, pSession);
					retValue = start_ping_test(pParameters, pSession, 0);
					if(pSession->m_stop_by_rdb_change)
					{
						stop_ping_test(pParameters, pSession);
					}
				}
				else if(retValue==0)
				{
					stop_ping_test(pParameters, pSession);
				}

			}
			else
			{
				/// 4 -- LED in RDB changed
				retValue = get_ping_state(pParameters->m_session_id);
				switch (retValue) {
				case DiagnosticsState_Code_Request:
					prepare_ping_test(pParameters, pSession);
					start_ping_test(pParameters, pSession, 1);
					break;
				case DiagnosticsState_Code_Cancelled:
				case DiagnosticsState_Code_None:
					set_ping_state(pParameters->m_session_id, DiagnosticsState_Code_None);
					break;
				default:;
				} //switch(retValue)
			}
			if(pParameters->m_cmd_line_mode){
				pParameters->m_running = 0;
			}
		}//if (selected > 0 ){ // detect fd
	}//while (pParameters->m_running)

	stop_ping_test(pParameters, pSession);

	return 0;
}//int main_loop(TParameters *pParameters)



// prepare ping test
//$0 -- success
//$<0 -- error code
int prepare_ping_test(TParameters *pParameters, TConnectSession *pSession)
{

    int retValue = 0;
	NTCLOG_INFO("prepare_ping_test, ready=%d", pSession->m_test_ready);
    if( pSession->m_test_ready ==0)
    {

		init_connection(pSession, pParameters, PROTOCOL_NAME);
		retValue = load_session_rdb(pSession);
		if(retValue)
		{

			NTCLOG_DEBUG("load_session_rdb, retValue=%d", retValue);
			update_all_session(pSession, 0);
			set_ping_state(pParameters->m_session_id, retValue);

			return retValue;
		}
	
		#ifdef _KERNEL_UDPECHO_TIMESTAMP_
			if (pSession->m_PacketSize < sizeof(TEchoPack))
			{
				pSession->m_PacketSize = sizeof(TEchoPack);
			}
		#endif


		retValue = lua_ifup(pParameters->m_ifup_script, pSession, RDB_VAR_SET_IF);
		if(retValue < 0)
		{
			 NTCLOG_NOTICE("ifup failed");
			 set_ping_state(pParameters->m_session_id, DiagnosticsState_Error_InitConnectionFailed);
		}
		else
		{
			// free exist buffer
			if(pSession->m_send_recv_buf)
			{
				free(pSession->m_send_recv_buf);
				pSession->m_send_recv_buf =NULL;
			}
			if(pSession->m_statData)
			{
				free(pSession->m_statData);
				pSession->m_statData=NULL;
			}
			// allocate new buffer
			pSession->m_send_recv_buf= malloc(pSession->m_PacketSize+sizeof(TEchoPack));
			if(pSession->m_send_recv_buf ==NULL)
			{
				retValue = DiagnosticsState_Error_InitConnectionFailed;
				return retValue;
			}
			pSession->m_sendbuf = pSession->m_send_recv_buf;
			pSession->m_recvbuf = pSession->m_send_recv_buf + sizeof(TEchoPack);
			memset(pSession->m_send_recv_buf, 0, pSession->m_PacketSize+sizeof(TEchoPack));

			pSession->m_statData =malloc( (pSession->m_PacketCount+2)*sizeof(TStatData) );
			//NTCLOG_INFO("allocat m_statData:%p %d\b",pSession->m_statData, (pSession->m_PacketCount+2)*sizeof(TStatData));
			if(pSession->m_statData ==NULL)
			{
				retValue=  DiagnosticsState_Error_InitConnectionFailed;
				return retValue;
			}
			memset(pSession->m_statData, 0 , (pSession->m_PacketCount+2)*sizeof(TStatData));


			pSession->m_test_ready =1;
		}
    }

    return retValue;
}


// console test mode
int start_ping_test(TParameters *pParameters, TConnectSession *pSession, int stop_if)
{
	int retValue =-1;
	NTCLOG_INFO("start_ping_test, ready=%d", pSession->m_test_ready);
	if(pSession->m_test_ready)
	{
		retValue = ping(pParameters, pSession);

		// Set Diagnosticstate: 0=>Completed, Cancelled => None or others
		if(retValue == 0) {
			set_ping_state(pParameters->m_session_id, DiagnosticsState_Code_Completed);
		}else if(retValue == DiagnosticsState_Error_Cancelled) {
			set_ping_state(pParameters->m_session_id, DiagnosticsState_Code_None);
		}else{
			set_ping_state(pParameters->m_session_id, retValue);
		}


		if(stop_if)
		{
			 stop_ping_test(pParameters, pSession);
		}
		

	}///if(pSession->m_test_ready)
	return retValue;
}//int start_ping_test(TParameters *pParameters, TConnectSession *pSession)


int stop_ping_test(TParameters *pParameters, TConnectSession *pSession)
{
	NTCLOG_INFO("stop_ping_test, ready=%d\n", pSession->m_test_ready);
	if(pSession->m_test_ready)
	{
		lua_ifdown(pParameters->m_ifdown_script, pSession, pParameters->m_if_ops);
		pSession->m_test_ready =0;
		if(pSession->m_send_recv_buf)
		{
			free(pSession->m_send_recv_buf);
			pSession->m_send_recv_buf =NULL;
		}
		if(pSession->m_statData)
		{
			free(pSession->m_statData);
			pSession->m_statData=NULL;
		}

	}
	return 0;
}

