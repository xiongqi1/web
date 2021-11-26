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


#ifndef AT_H_20090320_
#define AT_H_20090320_

typedef enum { TRUE = 1 == 1, FALSE = 1 != 1 } BOOL;
typedef void ( *notify_callback_t )( const char* notification );

#define AT_RESPONSE_MAX_SIZE (1024*4)

int at_write_raw(const char* buf,int buflen);

int at_init( const char* port, notify_callback_t notify );
int at_send_with_timeout( const char* command, char* response, const char* response_prefix, int* status, int timeout, int max_len );
int at_send( const char* command, char* response, const char* response_prefix, int* status, int max_len );
const char* at_readline( void );
int at_get_fd( void );
int at_open( void );
void at_close( void );
char* at_bufsend(const char* command, const char* response_prefix);
char* at_bufsend_with_timeout(const char* command, const char* response_prefix, int to);
int at_send_and_forget(const char* command, int* status);
int at_wait_notification(int timeout);

const char* direct_at_read(int timeout);
int at_is_open(void);
// Returns the error text output from the modem.  Can be checked following a call to
// at_send_with_timeout() in which *status is set to 0.
const char* at_error_text(void);


#define TIMEOUT_NETWORK_QUERY		30
#define TIMEOUT_CFUN_SET			15
#define MAX_TIMEOUT_CNT	5		/* maximum timeout count 5 seconds for default timeout (0) */


#endif /* AT_H_20090320_ */
