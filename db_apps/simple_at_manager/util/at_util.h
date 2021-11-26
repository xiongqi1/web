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

#ifndef SIMPLE_AT_MANAGER_AT_UTIL_H_
#define SIMPLE_AT_MANAGER_AT_UTIL_H_

#include <string.h>

#define __zeroMem(p,len)			memset(p,0,len)
#define __goToError()				{ goto error; }
#define __goToErrorIfFalse(c)		{ if(!(c)) __goToError(); }

BOOL is_at_command( const char* command );
int tokenize_at_response( char* response );
int tokenize_with_semicolon(char* response);
const char* get_token( const char* tokenized, unsigned int index );
int get_rid_of_newline_char(char * input_string);
void str_replace(char *st, const char *orig, const char *repl);
void* __alloc(int cbLen);
void __free(void* pMem);
const char* get_str_block(const char* in);

#endif /* SIMPLE_AT_MANAGER_AT_UTIL_H_ */
