/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "cdcs_syslog.h"

#include "./at.h"
#include "./queue.h"
#include "../model/model.h"
#include "../util/scheduled.h"
#include "rdb_ops.h"


#define STRLEN( CONST_CHAR ) ( sizeof( CONST_CHAR ) - 1 )
extern volatile int running; // quick and dirty
static int read_(int fd, char* buf, int len, int timeout);
static const char* at_read(int timeout);
extern volatile BOOL sms_disabled;

struct at_t
{
	int fd;
	struct at_queue_t queue;
	struct at_queue_t noti_queue;
	notify_callback_t notify;
	struct
	{
		int fd;
		char name[64];
		BOOL is_open;
	} port;
};

static struct at_t at;

int at_write_raw(const char* buf,int buflen)
{
	return write(at.port.fd,buf,buflen);
}

int at_init(const char* port, notify_callback_t notify)
{
	at_queue_clear(&at.queue);
	at_queue_clear(&at.noti_queue);
	strncpy(at.port.name, port, sizeof(at.port.name) - 1);
	at.port.fd = -1;
	at.notify = notify;
	return 0;
}

#define O_DIRECT	0x8000
int at_open_int(int retry_cnt)
{
	char buf[128];
	struct termios oldtio, newtio;

	if ((at.port.fd = open(at.port.name, O_RDWR | O_NOCTTY | O_TRUNC)) <= 0)
	{
		SYSLOG_ERR("failed to open '%s'! - %s", at.port.name, strerror(errno));
		goto cleanup;
	}
	SYSLOG_DEBUG("opened '%s'", at.port.name);
	if (tcgetattr(at.port.fd, &oldtio) < 0)
	{
		SYSLOG_ERR("tcgetattr() on '%s' failed! (%s)", at.port.name, strerror(errno));
		goto cleanup;
	}
	memcpy(&newtio, &oldtio, sizeof(struct termios));
	cfmakeraw(&newtio);
	newtio.c_cc[VMIN] = 1;
	newtio.c_cc[VTIME] = 0;

/* Ntc-1121 has built-in uarts interface for AT/appl and baudrate of ACS1
 * is fixed to 57600 by default before changed by +IPR command.
 * First setup to 57600 and send +IPR=115200 then change to 115200. */
#ifdef V_BUILTIN_UART_y
	if (retry_cnt%2) {
		cfsetispeed(&newtio, B57600);
		cfsetospeed(&newtio, B57600);
	} else {
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
	}
	newtio.c_cflag |= CRTSCTS;
#else
	cfsetospeed(&newtio, 9600);
#endif

	if (tcsetattr(at.port.fd, TCSAFLUSH, &newtio) < 0)
	{
		SYSLOG_ERR("tcsetattr() on '%s' failed! (%s)", at.port.name, strerror(errno));
		goto cleanup;
	}
	SYSLOG_DEBUG("configured '%s'", at.port.name);
	while (read_(at.port.fd, buf, 128, 1) > 0);

	return at.port.fd;
cleanup:
	at_close();
	return -1;
}


typedef enum
{
	CSAVE,
	CRESTORE
} sms_conf_type;
void sms_config_set(sms_conf_type config)
{
	int fOK, stat;
	int nRetry=1;
    static BOOL csave_cmd_failed = FALSE;

    /* return without trying to run CRES command to reduce
     * init time for some modules which does not support CSAS/CRES commands like Ericsson.
     */
    if (config == CRESTORE && csave_cmd_failed == TRUE)
        return;
	while(nRetry-- > 0)
	{
		if (config == CSAVE)
			stat = at_send_with_timeout("AT+CSAS=0", NULL, "", &fOK, 2, 0);
		else
			stat = at_send_with_timeout("AT+CRES=0", NULL, "", &fOK, 2, 0);
		if (stat >= 0 && fOK)
		{
			SYSLOG_INFO("SMS config (%s) succeeded", (config == CSAVE)? "save":"restore");
			return;
		}
	}
	SYSLOG_INFO("SMS config (%s) failed", (config == CSAVE)? "save":"restore");
    if (config == CSAVE)
        csave_cmd_failed = TRUE;
}

#ifdef V_MODULE_ERROR_RECOVERY_y
#define RDB_AT_PORT_OPEN_FAIL_RECOVER_CNT	"debug.at_port_open_fail_recover"
#endif

int at_open(void)
{
	int ok, stat;
	int nRetry=3;
	int local_cnt;
	const char buf[2] = {0x1B, 0x00};
#ifdef V_MODULE_ERROR_RECOVERY_y
	static int at_open_failure_cnt = 0;
	int fail_recover_cnt;
	char temp_buf[128] = {0, };
#endif	

	if (at.port.fd >= 0)
	{
		return at.port.fd;
	}

	while(nRetry-->0)
	{
		SYSLOG_INFO("opening serial port.... %d",3-nRetry);

		if(at_open_int(nRetry)<0) {
			sleep(1);
			continue;
		}

/* Ntc-1121 has built-in uarts interface for AT/appl and baudrate of ACS1
 * is fixed to 57600 by default before changed by +IPR command.
 * First setup to 57600 and send +IPR=115200 then change to 115200. */
#ifdef V_BUILTIN_UART_y
		if (nRetry%2) {
			at_send_with_timeout("AT+IPR=115200", NULL, "", &ok, 1, 0);
			if(at_open_int(0)) {
				sleep(1);
				continue;
			}
		}
#endif

		/* send ESC to exit from SMS text mode for safe */
		if((at_send_with_timeout("AT", NULL, "", &ok, 1, 0)<0) || !ok)
			at_send_with_timeout(buf, NULL, "", &ok, 1, 0);

		at_send_with_timeout("ATE1", NULL, "", &ok, 5, 0);

		if (!sms_disabled)
			sms_config_set(CSAVE);

		/* Skip this for all model after this command, previous newwork band selection is
		 * initialized to 'automatic' even though Sierra Wireless modules keeps its previous
		 * setting to its NV memory and restore at next boot time. More unknown parameters would
		 * be initialized by this command so it is better not to do factory reset here.
		 */
		// do not do factory reset if it is a dongle-based variant - the dongle may have user-specified configuration
		//#if ! defined(MODULETYPE_removable)
		//at_send_with_timeout("AT&F", NULL, "", &ok, 5, 0);
		//#endif

		if (!sms_disabled)
			sms_config_set(CRESTORE);

		// fixme: V_MODULE not available in Platypus2 now - not sending this command for MHS by request
		#if ! defined(BOARD_nhd1w)
		stat = at_send_with_timeout("AT+CFUN=1", NULL, "", &ok, 5, 0);
		#endif
		#if defined(MODULE_VZ20Q)
		// give 2 seconds before sending next AT commands unless
		// some responses could be messed up
        sleep(2);
		#endif
		
         for(local_cnt=0; local_cnt < 5 && ok == 0 ; local_cnt++)
         {
            if ( !stat && ok) {
               break;
            }
            sleep(2);
            at_send_with_timeout("AT+CFUN=1", NULL, "", &ok, 5, 0);
         }

		if(at_send_with_timeout("ATE1", NULL, "", &ok, 5, 0)>=0)
		{
			#if defined(MODULE_VZ20Q)
			// just send dummy 'AT' command to clean up garbage responses following
			// ATE1 command
			(void)at_send_with_timeout("AT", NULL, "", &ok, 1, 0);
			#endif
			if(at_send_with_timeout("ATV1", NULL, "", &ok, 5, 0)>=0)
				break;
		}

		at_close();
	}

	if(at.port.fd<0) {
		SYSLOG_ERR("failed to open serial port - %s : '%s'",at.port.name, strerror(errno));
#ifdef V_MODULE_ERROR_RECOVERY_y
		at_open_failure_cnt++;
		SYSLOG_ERR("at_open_failure_cnt : %d",at_open_failure_cnt);
		if (at_open_failure_cnt >= 3) {
			SYSLOG_ERR("at_open_failure_cnt exceeded limit, reboot router to recover");
			system("reboot");
			if(rdb_get_single(RDB_AT_PORT_OPEN_FAIL_RECOVER_CNT, temp_buf, sizeof(temp_buf))>=0) {
				fail_recover_cnt = atoi(temp_buf);
				fail_recover_cnt++;
				sprintf(temp_buf, "%d", fail_recover_cnt);
				rdb_set_single(RDB_AT_PORT_OPEN_FAIL_RECOVER_CNT, temp_buf);
				SYSLOG_ERR("at_open_failure_recover_cnt : %d", fail_recover_cnt);
			} else {
				SYSLOG_ERR("failed to read '%s'", RDB_AT_PORT_OPEN_FAIL_RECOVER_CNT);
			}
		}
#endif		
	}

	return at.port.fd;
}


void at_close(void)
{
	if (at.port.fd < 0)
	{
		return;
	}
	close(at.port.fd);
	at.port.fd = -1;
}

int at_get_fd(void)
{
	return at.port.fd;
}

int at_is_open(void)
{
	return !(at.port.fd<0);
}

static void delayed_noti_on_schedule(void* ref)
{
	const char* r;
	SYSLOG_INFO("process delayed notification");
	scheduled_clear("delayed_noti_on_schedule");
	while (running && !at_queue_empty(&at.noti_queue))
	{
		r = at_queue_readline(&at.noti_queue);
		if (r == NULL)
		{
			SYSLOG_INFO("<-- NULL");
			continue;
		}
		SYSLOG_INFO("<-- [%s]", r);
		at.notify(r);
	}
}

// For diagnosing errors.  If the modem returns an error the text is copied to this buffer.
// It is then readable via at_error_text();
#define ERROR_TEXT_BUFFER_SIZE 20
static char error_text_buffer[ERROR_TEXT_BUFFER_SIZE];

// Return the details of an error reported by the modem.  Valid after at_send_with_timeout() sets
// *status to 0.
const char* at_error_text(void)
{
    return error_text_buffer;
}


BOOL sms_msg_body_command = FALSE;
int sms_msg_body_len = 0;
int at_send_with_timeout(const char* cmd, char* response, const char* response_prefix, int* status, int timeout, int max_len)
{
	const char* r;
	const char* rr;
    const char* rt;
	int len = 0, written_bytes = 0;
	char outBuf[AT_RESPONSE_MAX_SIZE];
	int outBufLen;
	char *command=(char *)cmd;

	size_t prefix_size = strlen(response_prefix);
	BOOL got_response = FALSE;
	BOOL got_echo = !command || strncasecmp(command, "ATE1", 4) == 0;   // if previously it was set to ATE0, there will be no echo

	BOOL got_ericsson_cmgr_cmd = FALSE;
    BOOL ericsson_cmgr_cmd_cms_err = FALSE;
    
   	char zerolen_cmd[1]={0,};
	int result = 0;

	// "erase" any previous error text.
	error_text_buffer[0] = '\0';

	/* pretent that we have zero-length command */
	if(!cmd) {
		cmd=zerolen_cmd;
		command=zerolen_cmd;
	}
	
	if (at.port.fd < 0)
	{
		SYSLOG_INFO("AT port %s closed", at.port.name);
		result = -1;
		goto ret_error;
	}
	if (strcasecmp(command, "ATE0") == 0)
	{
		SYSLOG_ERR("forbidden command ATE0, we rely on echo");
		result = -1;
        goto ret_error;
	}

	if (max_len == 0)
		max_len = AT_RESPONSE_MAX_SIZE;

	if (response)
	{
		response[0] = 0;
	}
	if (memcmp(command, "AT+VTS", STRLEN("AT+VTS")) == 0)
		SYSLOG_ERR("--> [%s] (timeout = %d sec)", command, timeout);
	else
		SYSLOG_DEBUG("--> [%s] (timeout = %d sec)", command, timeout);

    if (is_ericsson && memcmp(command, "AT+CMGR=", STRLEN("AT+CMGR=")) == 0)
        got_ericsson_cmgr_cmd = TRUE;

    if (sms_msg_body_command) {
        memcpy(outBuf, command, sms_msg_body_len);
        outBufLen=sms_msg_body_len;
    } else {
        // put carriage return at the end
        if(command)
            sprintf(outBuf,"%s\r",command);
        outBufLen=strlen(outBuf);
    }

    written_bytes = write(at.port.fd, outBuf, outBufLen);
    if (written_bytes < 0)
    {
        SYSLOG_ERR("failed to write '%s'", command);
		result = -1;
        goto ret_error;
    }
    if (written_bytes < outBufLen)
    {
        SYSLOG_ERR("failed to write '%s', %d bytes of total %d bytes were written!", command, written_bytes, strlen(command));
		result = -1;
        goto ret_error;
    }

	while (1)
	{
		if ((r = at_read(timeout)) == NULL)
		{
			result = -1;
            goto ret_error;
		}

		// append for SMS send function.
		// check if this command is second part of AT+CMGW command which ends with ctrl-Z.
		// if then, strip out leading "> " string from response and compare with command after
		// removing trailing ctrl-Z
		// additional modification for sms send command : Sierra modem returns echo string
		// after removing carriage returns in the command string. To compare this, command string
		// should be removed its carriage returns.
		if (!got_echo && strncmp(r, "> ", 2) == 0 && strlen(r) >= 2)
		{
			char *cp = (char *)cmd;
			int match = 1;

			/* Option module sends with multi line reponse for long UCS2 message */
			if (!strcmp(model_name(),"Option Wireless") && (strlen(cmd) > strlen(r) + 2) )
			{
				SYSLOG_ERR("Option module may responses with multi line, need to merge");
				/* reuse outBuf */
				(void) memset(outBuf, 0x00, AT_RESPONSE_MAX_SIZE);
				strcpy(outBuf, r);
				SYSLOG_ERR("outBuf = %s", outBuf);
				while ((rr = at_read(timeout)) != NULL && strncmp(rr, "+CM", 3))
				{
					(void) strcat(outBuf, rr);
					SYSLOG_ERR("outBuf = %s", outBuf);
				}
				r = (const char *)&outBuf[0];
			}

            rt = r;
			rt+=2;
			while (*cp)
			{
				if (*cp == 0x0D || *cp == 0x0A || *cp == 0x1A)
				{
					cp++;
					continue;
				}
				if (*cp != *rt )
				{
					match = 0;
					break;
				}
				cp++;
				rt++;
			}
			/* MC7804/7304/7354 modem returns "< " only without original text */
			/* Quanta modem returns "< " only without original text */
			/* Cinterion modem returns "< " only without original text when message length is less than about 120 characters and
			 * returns "< " + message echo but returned message echo string length is shorter than original ones. Therefore
			 * comparison should be done for "> " only */
			if (match || (!match && !strncmp(r, "> ", 2) &&
			   (!strcmp(model_name(),"4031") ||
			    !strncmp(model_name(),"cinterion",9) ||
			    !strncmp(model_variants_name(), "MC7804", 6) ||
			    !strncmp(model_variants_name(), "MC7354", 6) ||
			    !strncmp(model_variants_name(), "MC7304", 6) )))
			{
				got_echo = TRUE;
				continue;
			}
		}

        /* Option Wireless modems return "< " and following command echo */
        if (!strcmp(r, "> ") && !strcmp(model_name(),"Option Wireless"))
        {
            continue;
        }

		if (!got_echo && !strcasecmp(r, command))
		{
			got_echo = TRUE;
			continue;
		}
        /* compare again without last trailing ctrl-z */
		else if (!got_echo && (!strncasecmp(r, command, strlen(command)-1) && command[strlen(command)-1] == 0x1A))
		{
			got_echo = TRUE;
			continue;
		}
		/* 
			As PHP-8 occationally returns ERROR without echo the command, we accept ERROR as a result even when we do not see echo.
		*/
		else if(!got_echo &&  !strcmp(r,"ERROR") ) {
			strcpy(error_text_buffer, "no echo error");
			if (status)
				*status = 0;
			break;
		}
		else if (!got_echo || (!strcmp(r, "NO CARRIER") && strncasecmp(command,"atd",3)) )
		{
			SYSLOG_ERR("caught noti in AT cmd proc - r : %s #1, push to noti queue", r);
			if (at_queue_write(&at.noti_queue, r, strlen(r)) != 0)
			{
				SYSLOG_ERR("failed to write to the noti queue");
			}
			at_queue_write(&at.noti_queue,"\n",1);
			
			scheduled_clear("delayed_noti_on_schedule");
    		scheduled_func_schedule("delayed_noti_on_schedule",delayed_noti_on_schedule,1);
			//at.notify(r);
			continue;
		}

        /* Bloody stupid Ericsson modules never send OK after +CMS ERROR response */
        if (got_ericsson_cmgr_cmd && memcmp(r, "+CMS ERROR:", STRLEN("+CMS ERROR:")) == 0)
            ericsson_cmgr_cmd_cms_err = TRUE;

		if (strcmp(r, "OK") == 0)
		{
			if (status)
				*status = 1;

			got_response = TRUE;

            	if (response) {
                  	len = strlen(response);

                  	if((len != 0) && (response[len-1] == '\n'))
                  	{
                  	   response[len-1] = '\0';
                  	}
            	}
			break;
		}
		else if (strcmp(r, "ERROR") == 0
		         || !strcmp(r, "NO CARRIER")
		         || memcmp(r, "+CME ERROR:", STRLEN("+CME ERROR:")) == 0
		         || memcmp(r, "+CMS ERROR:", STRLEN("+CMS ERROR:")) == 0
                 || ericsson_cmgr_cmd_cms_err)
		{
			snprintf(error_text_buffer, ERROR_TEXT_BUFFER_SIZE, "%s", r);
			if (status)
				*status = 0;

			got_response = TRUE;

            	if (response) {
                  	len = strlen(response);

                  	if((len != 0) && (response[len-1] == '\n'))
                  	{
                  	   response[len-1] = '\0';
                  	}
            	}

			break;
		}
		else if (prefix_size == 0 && !got_response)
		{
			/*
						oh, my god. it doesn't accept multi-lined result at all.

						TODO : currently, patched to accept multi-line result but it can die for not having OK or ERROR.

						if (response)
						{
							strcpy(response + len, r);
						}

						got_response = TRUE;
			*/
			if (response) {
				if (strlen(response) + strlen(r) <= max_len) {
					strcat(response, r);
					strcat(response, "\n");
				} else {
					SYSLOG_ERR("response len(%d) for '%s' cmd exceeded max limit(%d)", strlen(response) + strlen(r), command, max_len);
					result = -255;
				}
			}
		}
		else if (prefix_size > 0 && memcmp(r, response_prefix, prefix_size) == 0)
		{
			if (response)
			{
				if (strlen(response) + strlen(r) <= max_len) {
					if (*response)
					{
						response[len++] = '\n';
					}
					strcpy(response + len, r);
					len += strlen(r);
				} else {
					SYSLOG_ERR("response len(%d) for '%s' cmd exceeded max limit(%d)", strlen(response) + strlen(r), command, max_len);
					result = -255;
				}
			}
		}
		else
		{
			SYSLOG_ERR("caught noti in AT cmd proc - r : %s #2", r);
			if (at_queue_write(&at.noti_queue, r, strlen(r)) != 0)
			{
				SYSLOG_ERR("failed to write to the noti queue");
			}
			at_queue_write(&at.noti_queue,"\n",1);
			
			scheduled_clear("delayed_noti_on_schedule");
    		scheduled_func_schedule("delayed_noti_on_schedule",delayed_noti_on_schedule,1);
			//at.notify(r);
		}
	}
	
	return result;
ret_error:
    sms_msg_body_command = FALSE;
    
    
    return result;
}

int at_send_and_forget(const char* command, int* status)
{
	const char* r;
	int written_bytes = 0;
	char outBuf[AT_RESPONSE_MAX_SIZE];
	int outBufLen;

	BOOL got_echo = strncasecmp(command, "ATE1", 4) == 0;   // if previously it was set to ATE0, there will be no echo

	if (at.port.fd < 0)
	{
		SYSLOG_INFO("AT port %s closed", at.port.name);
		return -1;
	}
	if (strcasecmp(command, "ATE0") == 0)
	{
		SYSLOG_ERR("forbidden command ATE0, we rely on echo");
		return -1;
	}

	// put carridge return at the end
	sprintf(outBuf,"%s\r",command);
	outBufLen=strlen(outBuf);

	SYSLOG_DEBUG("--> [%s]", command);
	written_bytes = write(at.port.fd, outBuf, outBufLen);
	if (written_bytes < 0)
	{
		SYSLOG_ERR("failed to write '%s'", command);
		return -1;
	}
	if (written_bytes < outBufLen)
	{
		SYSLOG_ERR("failed to write '%s', %d bytes of total %d bytes were written!", command, written_bytes, strlen(command));
		return -1;
	}

	while (1)
	{
		if ((r = at_read(0)) == NULL)
		{
			return -1;
		}
		if (!got_echo && !strcasecmp(r, command))
		{
			got_echo = TRUE;
			if (status)
				*status = 1;
			break;
		}
	}
	return 0;
}

int at_wait_notification(int timeout)
{
	const char* r;
	if (at.port.fd < 0)
	{
		SYSLOG_INFO("AT port %s closed", at.port.name);
		return -1;
	}

	/* process previous queued notifications */
	scheduled_clear("delayed_noti_on_schedule");
	delayed_noti_on_schedule(NULL);

	while (1)
	{
		if ((r = at_read(timeout)) == NULL)
		{
			return -1;
		}
		if (!strcasecmp(r, "ERROR") || !strcasecmp(r, "+CME ERROR")) {
			SYSLOG_INFO("got error while waiting notification - r : %s", r);
			return -1;
		}
		SYSLOG_ERR("caught noti in AT cmd proc - r : %s #3", r);
		break;
	}
	at.notify(r);
	return 0;
}

char* at_bufsend_with_timeout(const char* command, const char* response_prefix, int to)
{
	static char response[AT_RESPONSE_MAX_SIZE];
	int status;

	if( at_send_with_timeout(command,response,response_prefix,&status,to,0) != 0)
		return 0;

	if(!status)
		return 0;

	return response+strlen(response_prefix);
}

char* at_bufsend(const char* command, const char* response_prefix)
{
	static char response[AT_RESPONSE_MAX_SIZE];
	int status;

	if( at_send(command,response,response_prefix,&status,0) != 0)
		return 0;

	if(!status)
		return 0;

	return response+strlen(response_prefix);
}

int at_send(const char* command, char* response, const char* response_prefix, int* status, int max_len)
{
	SYSLOG_DEBUG("--> [%s]", command);
	return at_send_with_timeout(command, response, response_prefix, status, 0, max_len);
}

const char* at_readline(void)
{
	int len;
	char buf[AT_RESPONSE_MAX_SIZE];
	const char* r;
	fd_set fdr;
	int selected;
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
	if (at.port.fd < 0)
	{
		SYSLOG_INFO("AT port %s closed", at.port.name);
		return NULL;
	}
	FD_ZERO(&fdr);
	FD_SET(at.port.fd, &fdr);
	selected = select(at.port.fd + 1, &fdr, NULL, NULL, &timeout);
	if (selected > 0)
	{
		len = read(at.port.fd, buf, AT_RESPONSE_MAX_SIZE);

		// we do graceful termination
		if ( (len==0) || ((len<0) && ((errno==2) || (errno==5))) )
		{
			//
			// this is weird situation that we get a select result but don't have any byte received!
			// this only happens when the device is disconnected
			//

			SYSLOG_ERR("module disconnection detected!!!");
			running=0;

			return 0;
		}
		else if (len>0)
		{
			if (at_queue_write(&at.queue, buf, len) != 0)
			{
				SYSLOG_ERR("failed to write to the queue");
			}
		}
		else if (len<0)
		{
			SYSLOG_ERR("failed to read from %d (#%d - %s)", at.port.fd, errno, strerror(errno));
		}

	}

	if ((r = at_queue_readline(&at.queue)) != NULL)
	{
		SYSLOG_DEBUG("<-- [%s]", r);
	}
	return r;
}

static int read_(int fd, char* buf, int len, int timeout)
{
	fd_set fdr;
	int selected;
	int nfds = fd + 1;
	int timeout_count = 0;

	if (fd < 0)
	{
		SYSLOG_INFO("AT port %s closed", at.port.name);
		return -1;
	}
	while (running)
	{
		struct timeval t = { .tv_sec = timeout == 0 ? 1 : timeout, .tv_usec = 0 };
		FD_ZERO(&fdr);
		FD_SET(fd, &fdr);
		
		/* immeidate return */
		if(t.tv_sec<0)
			t.tv_sec=0;
		
		selected = select(nfds, &fdr, NULL, NULL, &t);
		/*
				horrible code. it doesn't even think about signal

				TODO: when it gets a signal, finishes this function properly not to effect the at_send()

				if (selected < 0)
				{
					SYSLOG_ERR("select() failed with error %d (%s)", selected, strerror(errno));
					return -1;
				}

		*/
		if (selected > 0)
		{
			if (FD_ISSET(fd, &fdr))
			{
				int stat=read(fd, buf, len);

				if(stat<0)
					SYSLOG_ERR("failed to read from %d (#%d %s) - stat=%d", fd, errno, strerror(errno),stat);

				return stat;
			}
		}
		else if (selected == 0)
		{
			if (timeout)
			{
				SYSLOG_DEBUG("timed out");
				return -1;
			}
			/* Add below to prevent infinite loop when timeout variable is 0 and there is no response. */
			else
			{
				if (++timeout_count > MAX_TIMEOUT_CNT)
				{
					SYSLOG_DEBUG("%d timed out with default timer set", MAX_TIMEOUT_CNT);
					return -1;
				}
				else
				{
					SYSLOG_DEBUG("time out counter = %d", timeout_count);
				}
			}
		}
	}
	SYSLOG_DEBUG("shutting down");
	return 0;
}

static const char* at_read(int timeout)
{
	int len;
	char buf[AT_RESPONSE_MAX_SIZE];
	const char* r;
	while (1)
	{
		while (running && at_queue_empty(&at.queue))
		{
			if ((len = read_(at.port.fd, buf, AT_RESPONSE_MAX_SIZE, timeout)) <= 0)
			{
				return NULL;
			}
			if (at_queue_write(&at.queue, buf, len) != 0)
			{
				return NULL;
			}
		}
		r = at_queue_readline(&at.queue);
		if (r == NULL)
		{
			SYSLOG_ERR("<-- NULL");
			return NULL;
		}
		if (strstr(r, "AT+VTS"))
			SYSLOG_ERR("<-- [%s]", r);
		else
			SYSLOG_DEBUG("<-- [%s]", r);
		return r;
	}
}

//

/*
		Gee, notification doesn't support multi-line either.
		This is the only way to handle multi-line notification without in the structure,
*/
const char* direct_at_read(int timeout)
{
	return at_read(timeout);
}


