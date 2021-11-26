/*!
 * Copyright Notice:
 * Copyright (C) 2010 Call Direct Cellular Solutions Pty. Ltd.
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
#ifndef GPS_COMMON_H_
#define GPS_COMMON_H_

#include "cdcs_types.h"

#define MAX_INVALID_COUNT	1
#define MAX_RMC_INVALID_COUNT	1
#define MAX_TIMEOUT_COUNT	3			/* default 3 secs */
#define MAX_RDB_VAR_SIZE	256
#define DEFAULT_UPDATE_INT	60 * 10		/* default 600 secs = 10 mins */
#define TIMEOUT_FOR_AGPS	180
#define MAX_GPS_RETRY_CNT	5

typedef enum
{
	NORMAL = 0,
	GEN_ERROR,
	PKT_INVALID,
	PKT_VALID
} gps_event_result_type;

typedef enum
{
	CMD_DISABLE = 0,
	CMD_ENABLE,
	CMD_AGPS_MODE
} gps_mode_cmd_type;

typedef enum
{
	STANDALONE = 0,
	AGPS,
	HISTORICAL,
	MAX_SRC	
} gps_data_source_type;

typedef enum
{
	GPGGA = 0,
	GPRMC,
	GPVTG,
	GPGSA	,
	GPGSV,
	MAX_PKT
} gps_packet_type;

#define RDB_SYSLOG_MASK			"service.syslog.option.mask"

extern BOOL change_to_agps_mode;
extern const char* gga_field_names[];
extern const char* rmc_field_names[];
extern const char* vtg_field_names[];
extern const char* gsa_field_names[];
extern const char* gsv_field_names[];
extern const int gga_field_names_size;
extern const int rmc_field_names_size;
extern const int vtg_field_names_size;
extern const int gsa_field_names_size;
extern const int gsv_field_names_size;
extern void update_debug_level( void );
extern void sig_handler( int signum );
extern BOOL gga_valid( const char* data );
extern BOOL rmc_valid( const char* data );
extern BOOL vtg_valid( const char* data );
extern BOOL gsa_valid( const char* data );
extern BOOL gsv_valid( const char* data );
extern gps_event_result_type update_nmea_data( const char* data, const char** names, unsigned int size, BOOL ( *valid )( const char* ) );
extern gps_event_result_type handle_gpgsv( const char* data, const char** names, BOOL ( *valid )( const char* ) );
extern int init_rdb( void );
extern void close_rdb( void );
extern BOOL send_gps_control_cmd( gps_mode_cmd_type cmd );
extern int handle_gps_event( void );
extern int main_loop( void );
extern void change_gps_valid_status( gps_data_source_type source, gps_event_result_type validity );

#endif /* GPS_COMMON_H_ */

/*
* vim:ts=4:sw=4:
*/
