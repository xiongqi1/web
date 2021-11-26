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


#ifndef SIMPLE_AT_MANAGER_QUEUE_H_
#define SIMPLE_AT_MANAGER_QUEUE_H_

#include <stdlib.h>
#include "./at.h"

/* max size 10k can contain almost 30 sms msgs list for worst case of UCS2 coding */
#define AT_QUEUE_MAX_SIZE 2048*5

/// queue type
struct at_queue_t
{
	char* head;
	char* tail;
	char* last;
	const char* end;
	char storage[ AT_QUEUE_MAX_SIZE ];
	char response[AT_RESPONSE_MAX_SIZE];
};

/// initialize queue
struct at_queue_t* at_queue_create( void );

/// initialize queue
void at_queue_free( struct at_queue_t* q );

/// write bytes to the queue
int at_queue_write( struct at_queue_t* q, const char* buf, size_t size );

/// read zero-terminated line from the queue (the content will be preserved until the next read)
const char* at_queue_readline( struct at_queue_t* q );

/// clear queue
void at_queue_clear( struct at_queue_t* q );

/// queue empty? (i.e. would read return NULL?)
int at_queue_empty( const struct at_queue_t* q );

/// queue full? (i.e. will the next attempt to write be discarded?)
int at_queue_full( const struct at_queue_t* q );

/// print queue contents to syslog
void at_queue_debug_print( struct at_queue_t* q );


#endif /* SIMPLE_AT_MANAGER_QUEUE_H_ */

