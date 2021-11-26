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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "cdcs_syslog.h"
#include "./gps.h"

BOOL gps_checksum_valid( const char* line )
{
	const char* buf = line;
	char sum;
	char checksum[3];
	if( *buf == '*' || *buf == 0 ) { SYSLOG_ERR( "too a short line [%s]", line ); return FALSE; }
	++buf;
	for( sum = *buf++; *buf && *buf != '*'; sum ^= *buf++ );
	if( *buf != '*' ) { SYSLOG_ERR( "expected line with checksum after '*', got [%s]", line ); return FALSE; }
	sprintf( checksum, "%02X", sum );
	//fprintf( stderr, "expected checksum = %s\n", checksum );
	if ( memcmp( checksum, buf + 1, 2 ) != 0 )
	{
		SYSLOG_ERR("checksum err : curr : %c%c, expected = %s\n", *(buf + 1), *(buf + 2), checksum );
	}
	return memcmp( checksum, buf + 1, 2 ) == 0;
}

const char* gps_field( const char* line, unsigned int field )
{
	const char* f = line;
	unsigned int i;
	for( i = 0; i < field; ++i, ++f )
	{
		for( ; *f != ',' && *f != '*'; ++f )
		{
			if( *f == 0 || *f == '\n' || *f == '\r' ) { SYSLOG_ERR( "unexpected terminator 0x%02X in [%s]", *f, line ); return NULL; }
		}
	}
	return f;
}

const char* gps_field_copy( char* d, const char* s )
{
	const char *t=d;
	if( !d || !s ) { SYSLOG_ERR( "got NULL pointer" ); return NULL; }
	while( *s != ',' && *s != '*' )
	{
		if( *s == 0 || *s == '\n' || *s == '\r' ) { SYSLOG_ERR( "unexpected terminator 0x%02X", *s ); return NULL; }
		*d++ = *s++;
	}
	*d = 0;
	return t;
}

unsigned int gps_field_count( const char* line )
{
	const char* f = line;
	unsigned int i;
	for( i = 0; ; ++i, ++f )
	{
		for( ; *f != ',' && *f != '*'; ++f )
		{
			if( *f == 0 || *f == '\n' || *f == '\r' ) return i;
		}
	}
	return i;
}

