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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "cdcs_types.h"
#include "cdcs_utils.h"
#include "cdcs_syslog.h"

#include <gps.h>

#include "rdb_ops.h"
#include "./util/daemon.h"
#include "./util/gps.h"
#include "./util/rdb_util.h"

#include "gps_common.h"
#include <time.h>

#define REDUCE_RDB_VARIABLES

const char* gga_field_names[] =
{
	NULL
	, "time"
	, "latitude"
	, "latitude_direction"
	, "longitude"
	, "longitude_direction"
	, "fix_quality"
	, "number_of_satellites_tracked"
	, "hdop"
	, "altitude"
	, NULL
	, "height_of_geoid"
	, NULL
	, NULL
	, NULL
};

const char* rmc_field_names[] =
{
	NULL
	, "time"
	, "status"
	, "latitude"
	, "latitude_direction"
	, "longitude"
	, "longitude_direction"
	, "ground_speed_knots"
	, "track_angle"
	, "date"
	, "magnetic_variation"
	, "magnetic_variation_direction"
	, "mode"
};

const char* vtg_field_names[] =
{
	NULL
	, "true_track_made_good"
	, NULL
	, "magnetic_track_made_good"
	, NULL
	, "ground_speed_knots"
	, NULL
	, "ground_speed_kph"
	, NULL
	, "mode"
};

const char* gsa_field_names[] =
{
	NULL
	, "auto_selection"
	, "3d_fix"
	, "satellite_prn_for_fix_1"
	, "satellite_prn_for_fix_2"
	, "satellite_prn_for_fix_3"
	, "satellite_prn_for_fix_4"
	, "satellite_prn_for_fix_5"
	, "satellite_prn_for_fix_6"
	, "satellite_prn_for_fix_7"
	, "satellite_prn_for_fix_8"
	, "satellite_prn_for_fix_9"
	, "satellite_prn_for_fix_10"
	, "satellite_prn_for_fix_11"
	, "satellite_prn_for_fix_12"
	, "pdop"
	, "hdop"
	, "vdop"
};

#ifdef REDUCE_RDB_VARIABLES
const char* gsa_field_names2[] =
{
	NULL
	, "auto_selection"
	, "3d_fix"
	, "pdop"
	, "hdop"
	, "vdop"
};
#endif

const char* gsv_field_names[] =
{
	NULL
	, "number_of_sentences"
	, "sentence_number"
	, "number_of_satellites"
	, "satellite_prn1"
	, "elevation1"
	, "azimuth1"
	, "snr1"
	, "satellite_prn2"
	, "elevation2"
	, "azimuth2"
	, "snr2"
	, "satellite_prn3"
	, "elevation3"
	, "azimuth3"
	, "snr3"
	, "satellite_prn4"
	, "elevation4"
	, "azimuth4"
	, "snr4"
	, "satellite_prn5"
	, "elevation5"
	, "azimuth5"
	, "snr5"
	, "satellite_prn6"
	, "elevation6"
	, "azimuth6"
	, "snr6"
	, "satellite_prn7"
	, "elevation7"
	, "azimuth7"
	, "snr7"
	, "satellite_prn8"
	, "elevation8"
	, "azimuth8"
	, "snr8"
	, "satellite_prn9"
	, "elevation9"
	, "azimuth9"
	, "snr9"
	, "satellite_prn10"
	, "elevation10"
	, "azimuth10"
	, "snr10"
	, "satellite_prn11"
	, "elevation11"
	, "azimuth11"
	, "snr11"
	, "satellite_prn12"
	, "elevation12"
	, "azimuth12"
	, "snr12"
};

#ifdef REDUCE_RDB_VARIABLES
const char* gsv_field_names2[] =
{
	NULL
	, "number_of_sentences"
	, "sentence_number"
	, "number_of_satellites"
};

#define MAX_FIELD_LEN     4
typedef struct
{
	char fixed;
	char prn[MAX_FIELD_LEN];
	char snr[MAX_FIELD_LEN];
	char elev[MAX_FIELD_LEN];
	char azim[MAX_FIELD_LEN];
} satellite_info_type;

#define MAX_SATELLITE     36
typedef struct
{
	int fixed_prn[MAX_SATELLITE];
	satellite_info_type satellite_info[MAX_SATELLITE];
} satellite_info_db_type;
satellite_info_db_type satellite_db;
#endif

/* These fields are shared with standlone and agps both */
#define MAX_COMMON_FIELD_NUMBER		7
#define MAX_COMMON_FIELD_LENGTH		20
const char* common_field_names[MAX_COMMON_FIELD_NUMBER] =
{
	"date"
	, "time"
	, "latitude"
	, "latitude_direction"
	, "longitude"
	, "longitude_direction"
	, "height_of_geoid"
};

char ms_assisted_data[MAX_COMMON_FIELD_NUMBER][MAX_COMMON_FIELD_LENGTH];

const int gga_field_names_size = sizeof(gga_field_names);
const int rmc_field_names_size = sizeof(rmc_field_names);
const int vtg_field_names_size = sizeof(vtg_field_names);
const int gsa_field_names_size = sizeof(gsa_field_names);
const int gsv_field_names_size = sizeof(gsv_field_names);

extern volatile int running;
volatile BOOL test_mode = FALSE;

static BOOL save_to_common_fields( const char** names, unsigned int size );
static BOOL change_gps_data_source( gps_data_source_type source );
static void update_agps_last_valid_time( void );
extern int gps_data_invalid_count;
extern int gps_data_timeout_count;
extern int gps_rmc_data_timeout_count;
extern int gps_fd;
extern int gps_gsv_data_invalid_count;
int agps_update_interval = DEFAULT_UPDATE_INT;
time_t agps_elapsed_time = 0;
time_t agps_last_updated_time = 0;
extern int support_agps_mode;
extern int support_sgps_mode;
gps_event_result_type agps_validity = NORMAL;
BOOL packet_validity[MAX_PKT] = {FALSE, };

void update_debug_level()
{
	char buf[128];
	int error_level;

	if( rdb_get_single(rdb_name("debug", ""), buf, sizeof(buf)) == 0 )
	{
		error_level=atoi(buf);
		setlogmask(LOG_UPTO(error_level + LOG_ERR));
		SYSLOG_INFO("error level changed to %d", error_level);
	}
	else
	{
		SYSLOG_ERR("no debugging database variable exists");
	}
}

static void toggle_test_mode()
{
	test_mode = (test_mode == FALSE);
	SYSLOG_INFO("test mode changed to %d", test_mode);
}

void sig_handler( int signum )
{
	/* The rdb_library and driver have a bug that means that subscribing to
	variables always enables notfication via SIGHUP. So we don't whinge
	on SIGHUP. */
	if (signum!=SIGHUP)
		SYSLOG_DEBUG( "caught signal %d", signum );

	switch( signum )
	{
		default:
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			running = 0;
			break;
		case SIGUSR1:
			update_debug_level();
			break;
		case SIGUSR2:
			toggle_test_mode();
			break;
}
}

#ifdef REDUCE_RDB_VARIABLES
static void init_satellite_db( void )
{
	int i;
	(void) memset((char *)&satellite_db.fixed_prn[0], 0x0, sizeof(satellite_db));
	for (i = 0; i < MAX_SATELLITE; i++)
	{
		satellite_db.satellite_info[i].fixed = '0';
		(void) strcpy(satellite_db.satellite_info[i].prn, "N/A");
		(void) strcpy(satellite_db.satellite_info[i].snr, "N/A");
		(void) strcpy(satellite_db.satellite_info[i].elev, "N/A");
		(void) strcpy(satellite_db.satellite_info[i].azim, "N/A");
	}
}

static void print_satellite_db( void )
{
	int i;
	#define TEST_BUF_SIZE	1024
	char buf[TEST_BUF_SIZE] = {0, };
	SYSLOG_DEBUG( "================ satellite information DB ==============");
	for (i = 0; i < MAX_SATELLITE; i++) {
		sprintf(buf, "%s, %d", buf, satellite_db.fixed_prn[i]);
	}
	SYSLOG_DEBUG( "fixed_prn: %s", buf);
	(void) memset(buf, 0x0, TEST_BUF_SIZE);
	SYSLOG_DEBUG( "index  fixed  prn  snr  elev  azim");
	for (i = 0; i < MAX_SATELLITE; i++) {
		SYSLOG_DEBUG( "[%02d] :  %c     %s   %s   %s   %s", i, satellite_db.satellite_info[i].fixed,
			satellite_db.satellite_info[i].prn, satellite_db.satellite_info[i].snr,
			satellite_db.satellite_info[i].elev, satellite_db.satellite_info[i].azim);
	}
}
#endif

#define GPGSA_PACKET_SIZE       18
#define GPRMC_V22_PACKET_SIZE   12
#define GPRMC_V23_PACKET_SIZE   13
#define GPVTG_V22_PACKET_SIZE   9
#define GPVTG_V23_PACKET_SIZE   10

static BOOL validate_mode( const char mode ) {
	if ( mode == gps_autonomous_mode || mode == gps_differencial_mode || mode == gps_estimated_mode || mode == gps_notvalid_mode || mode == gps_simulated_mode ) return TRUE;
	return FALSE;
}

static gps_event_result_type gps_update( const char* data, const char** names, unsigned int size )
{
	char prefix[64];
	char value[64];
	char type[6];
	unsigned int i, offset = 0;
	BOOL illegal_gpgsa = FALSE;
	int ret = NORMAL;
	gps_packet_type loop_cnt;
	BOOL all_packet_valid = TRUE;

	memcpy( type, data + 1, 5 );
	type[5] = 0;
	sprintf( prefix, "standalone.%s", type );
	//SYSLOG_DEBUG( "\n\n====================    %s     ========================\n", prefix);
	SYSLOG_DEBUG( "%s\n", data);
	if( rdb_set_single( rdb_name( "", prefix ), data ) != 0 ) { SYSLOG_ERR( "failed to set '%s'", rdb_name( "", prefix ) ); ret = GEN_ERROR; }
	sprintf( prefix, "standalone" );

	/* NMEA version check
	* Read the mode field of GPRMC or GPVTG packet. If the result is NULL, process as Ver. 2.2 else Ver.2.3.
	*/
	if( ( memcmp( data, "$GPRMC", 6 ) == 0 && gps_field_count( data ) == GPRMC_V22_PACKET_SIZE ) ||
		( memcmp( data, "$GPVTG", 6 ) == 0 && gps_field_count( data ) == GPVTG_V22_PACKET_SIZE ) )
	{
		//SYSLOG_DEBUG("\n\nPacket size was decremented for NMEA Ver 2.2 packet!\n");
		size--;
	}

/* We need some trick for Sierra module because it sends GPGSA packet with 13 prn fields while standard is 12 !*/
	if( memcmp( data, "$GPGSA", 6 ) == 0 && gps_field_count( data ) > GPGSA_PACKET_SIZE )
	{
		//SYSLOG_DEBUG("\n\nPacket size was incremented for Sierra illegal packet!\n");
		size++;
		illegal_gpgsa = TRUE;
		offset = 1;
	}

	for( i = 0; i < size; ++i )
	{
		const char* field = gps_field( data, i );

		if ( illegal_gpgsa ) {
			if ( i == gsa_pdop )  continue;
			else if ( i > gsa_pdop ) offset = 1;
			else offset = 0;
		}
		//SYSLOG_DEBUG("illeagal_gpgsa = %d, offset = %d", illeagal_gpgsa, offset );

		if ( !names[i - offset] ) { continue; }
		if ( !field ) { SYSLOG_DEBUG( "failed to parse '%s'", names[i - offset] ); ret = PKT_INVALID; break; }

		/* do not write code like 'rdb_set_single( rdb_name( prefix, names[i] ), gps_field_copy( value, field ) ) != 0 '
		* because if call rdb_set_single(xxx, NULL), system will go to deadlock state.
		*/
		if ( ( field = gps_field_copy( value, field ) ) ) {
#ifdef REDUCE_RDB_VARIABLES
			if( memcmp( data, "$GPGSA", 6 ) == 0 &&
				i >= gsa_satellite_prn1 && i <= gsa_satellite_prn12 )
			{
				satellite_db.fixed_prn[i-gsa_satellite_prn1] = atoi(field);
			}
			else
			{
				if( rdb_set_single( rdb_name( prefix, names[i - offset] ), field ) != 0 )
				{
					SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[i - offset] ), strerror( errno ) );
					ret = GEN_ERROR;
				}
			}
#else
			if( rdb_set_single( rdb_name( prefix, names[i - offset] ), field ) != 0 )
			{
				SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[i - offset] ), strerror( errno ) );
				ret = GEN_ERROR;
			}
#endif
			//SYSLOG_DEBUG( "'%s' (%s)", rdb_name( prefix, names[i - offset] ), value );
		}
		else {
			SYSLOG_DEBUG( "field copy failed (%s)", field );
			ret = GEN_ERROR;
		}
	}
	if (ret == NORMAL &&
		(memcmp( data, "$GPRMC", 6 ) == 0 || memcmp( data, "$GPGGA", 6 ) == 0))
	{
		if (memcmp( data, "$GPRMC", 6 ) == 0)
		{
			gps_rmc_data_timeout_count = 0;
		}
		
		/* reset invalid counter & save to common database only when all gps packets are valid */
		for (loop_cnt = GPGGA; loop_cnt < MAX_PKT; loop_cnt++) {
			if (!packet_validity[loop_cnt]) {
				all_packet_valid = FALSE;
				break;
			}
		}
		
		if (all_packet_valid) {
			SYSLOG_DEBUG( "All GPS packets are valid, reset invalid counter & save to common database");
			if (!save_to_common_fields(names, size))
			{
				SYSLOG_ERR( "failed to save gps data to common field");
			}
			ret = PKT_VALID;
			change_gps_valid_status( STANDALONE, PKT_VALID );
		} else {
			ret = PKT_INVALID;
		}
	}
	return ret;
}

#ifdef REDUCE_RDB_VARIABLES
static char check_fixed_prn( char *prn_str )
{
	int i;
	int prn_no;
	if ( prn_str[0] == '\0' || strcmp(prn_str, "N/A") == 0 )
		return '0';
	prn_no = atoi(prn_str);
	for (i = 0; i < MAX_SATELLITE; i++)
		if ( satellite_db.fixed_prn[i] == prn_no )
			return '1';
	return '0';
}

static void save_satellite_info_to_rdb( void )
{
	int i;
	char buf[1024];
	(void) memset(buf, 0x00, 1024);
	for (i = 0; i < MAX_SATELLITE; i++)
	{
		sprintf(buf, "%s%c,%s,%s,%s,%s;", buf,
						check_fixed_prn( satellite_db.satellite_info[i].prn ),
						satellite_db.satellite_info[i].prn,
						satellite_db.satellite_info[i].snr,
						satellite_db.satellite_info[i].elev,
						satellite_db.satellite_info[i].azim );
	}
	if( rdb_set_single( rdb_name( "", "standalone.satellitedata" ), buf ) != 0 )
	{
		SYSLOG_ERR( "failed to update '%s'", rdb_name( "", "standalone.satellitedata" ) );
	}
	else
		SYSLOG_DEBUG( "update = %s", buf );
	init_satellite_db();
}

static void gsv_data_base_reset( void )
{
	init_satellite_db();
	if( rdb_set_single( rdb_name( "", "standalone.satellitedata" ), "" ) != 0 )
	{
		SYSLOG_ERR( "failed to update '%s'", rdb_name( "", "standalone.satellitedata" ) );
	}
}
#endif

#define SATELLITE_INFO_SIZE		4
#define	MAX_SATELLITE_INFO		4
#define	MAX_SENTENCE_NUMBER		3
static gps_event_result_type gps_gsv_update( const char* data, const char** names )
{
	char prefix[64], value[64], type[6];
	unsigned int sentence_number, offset;
	gps_gsv_enum loop, loop_limit;
	unsigned int sentence_no = 0, sentence_cnt = 0, prn_cnt = 0;
	int ret = NORMAL;

	memcpy( type, data + 1, 5 );
	type[5] = 0;
	sprintf( prefix, "standalone.%s", type );
	//SYSLOG_DEBUG( "\n\n====================    %s     ========================\n", prefix);
	SYSLOG_DEBUG( "%s\n", data);
	if( rdb_set_single( rdb_name( "", prefix ), data ) != 0 ) { SYSLOG_ERR( "failed to set '%s'", rdb_name( "", prefix ) ); ret = GEN_ERROR; }
	sprintf( prefix, "standalone" );

	if ( gps_field_copy( value, gps_field( data, gsv_sentence_number ) ) ) {
		sentence_no	= value[0] - '0';
	}
	if ( gps_field_copy( value, gps_field( data, gsv_number_of_sentences ) ) ) {
		sentence_cnt	= value[0] - '0';
	}
	if ( gps_field_copy( value, gps_field( data, gsv_number_of_satellites ) ) ) {
	if (value[1]) 	prn_cnt	= ( value[0] - '0' ) * 10 + ( value[1] - '0' );
	else prn_cnt	= value[0] - '0';
	}
	if ( sentence_no < sentence_cnt ) {
		loop_limit = gsv_snr4;
	}
	else
	{
		loop_limit = gsv_satellite_prn1 + ( prn_cnt - ( sentence_cnt - 1 ) * SATELLITE_INFO_SIZE ) * SATELLITE_INFO_SIZE - 1;
	}

	/* copy validate GPGSV fields only */
	sentence_number = offset = 0;
	for( loop = gsv_type; loop <= loop_limit; ++loop )
	{
		const char* field = gps_field( data, loop );

		if ( loop < gsv_satellite_prn1 ) {
			if( !names[loop] ) { continue; }
			if( !field ) { SYSLOG_DEBUG( "failed to parse '%s'", names[loop] ); ret = PKT_INVALID; break; }
		}
		else
		{
			if( !names[loop + offset] ) { continue; }
			if( !field ) { SYSLOG_DEBUG( "failed to parse '%s'", names[loop + offset] ); ret = PKT_INVALID; break; }
		}

		switch ( loop )
		{
		case gsv_sentence_number:
			if ( gps_field_copy( value, field ) ) {
				sentence_number	= value[0] - '0';
				offset = SATELLITE_INFO_SIZE * MAX_SATELLITE_INFO * ( sentence_number - 1 );
				if( rdb_set_single( rdb_name( prefix, names[loop] ), value ) != 0 )
				{
					SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[loop] ), strerror( errno ) );
					ret = GEN_ERROR;
				}
				//SYSLOG_DEBUG( "'%s' (%s)", rdb_name( prefix, names[loop] ), value );
			}
			else
			{
				SYSLOG_DEBUG( "field copy failed '%s' (%s)", rdb_name( prefix, names[loop] ), value );
				ret = GEN_ERROR;
			}
			break;

		case gsv_type:
		case gsv_number_of_sentences:
		case gsv_number_of_satellites:
			if ( gps_field_copy( value, field ) ) {
				if( rdb_set_single( rdb_name( prefix, names[loop] ), value ) != 0 )
				{
					SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[loop] ), strerror( errno ) );
					ret = GEN_ERROR;
				}
				//SYSLOG_DEBUG( "'%s' (%s)", rdb_name( prefix, names[loop] ), value );
			}
			else
			{
				SYSLOG_DEBUG( "field copy failed '%s' (%s)", rdb_name( prefix, names[loop] ), value );
				ret = GEN_ERROR;
			}
			break;

		default:
			if ( gps_field_copy( value, field ) ) {
#ifdef REDUCE_RDB_VARIABLES
				/* in gsv_field_names[],
						loop + offset       field
					------------------------------------
							4           "satellite_prn1"
							5           "elevation1"
							6           "azimuth1"
							7           "snr1"
							8           "satellite_prn2"
							9           "elevation2"
						.......................
				*/
				int satellite_index;

				satellite_index=(loop + offset)/4 - 1;

				/* ignore extra satellite - 12 satellites standard */
				if(satellite_index<MAX_SATELLITE) {
					if (value[0] == 0 )	strcpy(value, "N/A");
					if ( (loop + offset) % 4 == 0 )
						(void) strncpy( (char *)satellite_db.satellite_info[satellite_index].prn, value, MAX_FIELD_LEN );
					else if ( (loop + offset) % 4 == 1 )
						(void) strncpy( (char *)satellite_db.satellite_info[satellite_index].elev, value, MAX_FIELD_LEN );
					else if ( (loop + offset) % 4 == 2 )
						(void) strncpy( (char *)satellite_db.satellite_info[satellite_index].azim, value, MAX_FIELD_LEN );
					else if ( (loop + offset) % 4 == 3 )
						(void) strncpy( (char *)satellite_db.satellite_info[satellite_index].snr, value, MAX_FIELD_LEN );
				}
#else
				if( rdb_set_single( rdb_name( prefix, names[loop + offset] ), value ) != 0 )
				{
					SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[loop + offset] ), strerror( errno ) );
					ret = GEN_ERROR;
				}
#endif
				//SYSLOG_DEBUG( "'%s' (%s)", rdb_name( prefix, names[loop + offset] ), value );
			}
			else
			{
				SYSLOG_DEBUG( "field copy failed '%s' (%s)", rdb_name( prefix, names[loop + offset] ), value );
				ret = GEN_ERROR;
			}
			break;
		}
	}

#ifdef REDUCE_RDB_VARIABLES
	if ( sentence_no == sentence_cnt ) {
		//print_satellite_db();
		save_satellite_info_to_rdb();
	}
#else
	/* clear non-validate GPGSV fields */
	if ( sentence_no != sentence_cnt || ( loop_limit >= gsv_snr4 && prn_cnt == MAX_SATELLITE_INFO * MAX_SENTENCE_NUMBER ) )
	{
		//SYSLOG_ERR( "err : stn no %d stn cnt %d lp limit %d prn cnt %d", sentence_no, sentence_cnt, loop_limit, prn_cnt );
		return NORMAL;
	}
	for( loop = loop + offset; loop <= gsv_snr4 + MAX_SATELLITE_INFO * SATELLITE_INFO_SIZE * ( MAX_SENTENCE_NUMBER - 1); ++loop )
	{
		if( rdb_set_single( rdb_name( prefix, names[loop] ), "" ) != 0 )
		{
			SYSLOG_ERR( "failed to set '%s' (%s)", rdb_name( prefix, names[loop] ), strerror( errno ) );
			ret = GEN_ERROR;
		}
		//SYSLOG_DEBUG( "'%s' (%s)", rdb_name( prefix, names[loop] ), "" );
	}
#endif
	return PKT_VALID;
}

BOOL gga_valid( const char* data )
{
	const char* fix_quality = gps_field( data, gga_fix_quality );
	packet_validity[GPGGA] = (BOOL)(fix_quality && fix_quality[0] != gps_gga_invalid);
	SYSLOG_DEBUG( "set validity : GGA : %d", packet_validity[GPGGA] );
	return packet_validity[GPGGA];
}

BOOL rmc_valid( const char* data )
{
	const char* rmc_vtg_mode;
	char value[64];
	BOOL result = TRUE;

	/* RMC v2.2 packet has not the valid check field but V2.3 packet has one. */
	if ( gps_field_count( data ) == GPRMC_V23_PACKET_SIZE ) {
		rmc_vtg_mode = gps_field( data, rmc_mode );
		if ( !validate_mode( rmc_vtg_mode[0] ) )
		{
			gps_rmc_data_timeout_count++;
			result = FALSE;
		}
	}
	if ( !gps_field_copy( value, gps_field( data, rmc_time ) ) || strlen(value) < 6 )
	{
		gps_rmc_data_timeout_count++;
		result = FALSE;
	}
	packet_validity[GPRMC] = result;
	SYSLOG_DEBUG( "set validity : RMC : %d", packet_validity[GPRMC] );
	return packet_validity[GPRMC];
}

BOOL vtg_valid( const char* data )
{
	const char* rmc_vtg_mode;
	BOOL result = TRUE;

	/* VTG v2.2 packet has not the valid check field but V2.3 packet has one. */
	if ( gps_field_count( data ) == GPVTG_V23_PACKET_SIZE ) {
		rmc_vtg_mode = gps_field( data, vtg_mode );
		if ( !validate_mode( rmc_vtg_mode[0] ) ) result = FALSE;
	}
	packet_validity[GPVTG] = result;
	SYSLOG_DEBUG( "set validity : VTG : %d", packet_validity[GPVTG] );
	return packet_validity[GPVTG];
}

BOOL gsa_valid( const char* data )
{
	/* GSA packet has not the valid check field */
	packet_validity[GPGSA] = TRUE;
	SYSLOG_DEBUG( "set validity : GSA : %d", packet_validity[GPGSA] );
	return packet_validity[GPGSA];
}

BOOL gsv_valid( const char* data )
{
	/* GSV packet has not the valid check field */
	packet_validity[GPGSV] = TRUE;
	SYSLOG_DEBUG( "set validity : GSV : %d", packet_validity[GPGSV] );
	return packet_validity[GPGSV];
}

gps_event_result_type update_nmea_data( const char* data, const char** names, unsigned int size, BOOL ( *valid )( const char* ) )
{

	gps_event_result_type ret = NORMAL;
#if (0)
	const char gga_buffer[] = "$GPGGA,101202.0,3348.567082,S,15100.894340,E,1,06,3.4,16.0,M,,,,*21";
	const char rmc_buffer[] = "$GPRMC,101202.0,A,3348.567082,S,15100.894340,E,,,270812,,,A*78";	// NMEA 2.2 version
	const char rmc_buffer2[]= "$GPRMC,101202.0,A,3348.567082,S,15100.894340,E,,,270812,,,A*78";	// NMEA 2.3 version
	const char vtg_buffer[] = "$GPVTG,,T,,M,0.0,N,0.0,K*4E";								// NMEA 2.2 version
	const char vtg_buffer2[]= "$GPVTG,,T,,M,0.0,N,0.0,K*4E";								// NMEA 2.3 version
	const char gsa_buffer[] = "$GPGSA,A,3,05,08,10,19,26,28,,,,,,,5.0,3.4,3.6*3F";
	const char gsa_buffer2[]= "$GPGSA,A,3,05,08,10,19,26,28,,,,,,,5.0,3.4,3.6*3F";   // illegal packet from Sierra module
#endif	
#if (1)	
	const char gga_buffer[] = "$GPGGA,123519,3353.623475,S,15111.8049179999,E,1,08,0.9,545.4,M,46.9,M,,*55";
	const char rmc_buffer[] = "$GPRMC,123519,A,3353.623475,S,15111.8049179999,E,022.4,084.4,230394,003.1,W*78";	// NMEA 2.2 version
	const char rmc_buffer2[]= "$GPRMC,023044,A,3353.623475,S,15111.8049179999,E,0.0,156.1,131102,15.3,E,A*30";	// NMEA 2.3 version
	const char vtg_buffer[] = "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48";								// NMEA 2.2 version
	const char vtg_buffer2[]= "$GPVTG,096.5,T,083.5,M,0.0,N,0.0,K,D*22";								// NMEA 2.3 version
	const char gsa_buffer[] = "$GPGSA,A,10,27,08,28,13,31,10,03,29,23,01,,,2.5,1.3,2.1*39";
	const char gsa_buffer2[]= "$GPGSA,A,3,09,21,24,,,,,,,,,,,5.3,5.2,0.9*1A";   // illegal packet from Sierra module
#endif	
	char *gga_buf = (char *)&gga_buffer;
	char *rmc_buf = (char *)&rmc_buffer;
	char *vtg_buf = (char *)&vtg_buffer;
	char *gsa_buf = (char *)&gsa_buffer;
	static BOOL rmc_toggle = TRUE, vtg_toggle = TRUE, gsa_toggle = TRUE;

	/* Turn on this feature to simulate pseudo GPS packet with RDB interface */
	if ( test_mode )
	{
		if( memcmp( data, "$GPGGA", 6 ) == 0 )
		{
			//SYSLOG_DEBUG( "field count %d", gps_field_count( gga_buf ) );
			if( valid( gga_buf ) ) { ret = gps_update( gga_buf, names, size ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
		}
		else if( memcmp( data, "$GPGSA", 6 ) == 0 )
		{
			if ( gsa_toggle ) { gsa_toggle = FALSE; gsa_buf = (char *)&gsa_buffer; }
			else { gsa_toggle = TRUE;  gsa_buf = (char *)&gsa_buffer2; }
			//SYSLOG_DEBUG( "field count %d", gps_field_count( gsa_buf ) );
			if( valid( gsa_buf ) ) { ret = gps_update( gsa_buf, names, size ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
		}
		else if( memcmp( data, "$GPRMC", 6 ) == 0 )
		{
			if ( rmc_toggle ) { rmc_toggle = FALSE; rmc_buf = (char *)&rmc_buffer; }
			else { rmc_toggle = TRUE;  rmc_buf = (char *)&rmc_buffer2; }
			//SYSLOG_DEBUG( "field count %d", gps_field_count( rmc_buf ) );
			if( valid( rmc_buf ) ) { ret = gps_update( rmc_buf, names, size ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
		}
		else if( memcmp( data, "$GPVTG", 6 ) == 0 )
		{
			if ( vtg_toggle ) { vtg_toggle = FALSE; vtg_buf = (char *)&vtg_buffer; }
			else { vtg_toggle = TRUE;  vtg_buf = (char *)&vtg_buffer2; }
			//SYSLOG_DEBUG( "field count %d", gps_field_count( vtg_buf ) );
			if( valid( vtg_buf ) ) { ret = gps_update( vtg_buf, names, size ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
		}
	}
	else
	{
		if( valid( data ) ) { ret = gps_update( data, names, size ); }	else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
	}
	return ret;
}

/* Need separate procedure for GPGSV because it may need to be 3 sentences for the full information.
* and one GSV sentence only can provide data for up to 4 satellites.
*/
gps_event_result_type handle_gpgsv( const char* data, const char** names, BOOL ( *valid )( const char* ) )
{
	int ret = NORMAL;
#if (0)
	const char gsv_buffer1[] = "$GPGSV,3,1,12,03,07,139,,05,16,258,22,07,54,116,,08,56,195,22*7C";
	const char gsv_buffer2[] = "$GPGSV,3,2,12,10,16,312,20,11,14,075,17,13,38,026,15,26,18,219,22*70";
	const char gsv_buffer3[] = "$GPGSV,3,3,12,19,30,122,18,23,06,030,,17,11,348,,28,49,285,29*77";
	const char gsv_buffer4[] = "$GPGSV,3,1,12,03,07,139,,05,16,258,22,07,54,116,,08,56,195,22*7C";
	const char gsv_buffer5[] = "$GPGSV,3,2,12,10,16,312,20,11,14,075,17,13,38,026,15,26,18,219,22*70";
	const char gsv_buffer6[] = "$GPGSV,3,3,12,19,30,122,18,23,06,030,,17,11,348,,28,49,285,29*77";
	const char gsv_buffer7[] = "$GPGSV,3,1,12,03,07,139,,05,16,258,22,07,54,116,,08,56,195,22*7C";
#endif	
#if (1)	
	const char gsv_buffer1[] = "$GPGSV,3,1,10,27,68,048,42,08,63,326,53,28,48,239,75,13,39,154,39*79";
	const char gsv_buffer2[] = "$GPGSV,3,2,10,31,38,069,24,10,23,282,10,03,12,041,,29,09,319,*7C";
	const char gsv_buffer3[] = "$GPGSV,3,3,10,23,07,325,66,01,05,145,80*76";
	const char gsv_buffer4[] = "$GPGSV,3,1,12,27,68,048,30,08,63,326,43,28,48,239,65,13,39,154,29*7F";
	const char gsv_buffer5[] = "$GPGSV,3,2,12,31,38,069,64,10,23,282,70,03,12,041,50,29,09,319,40*7D";
	const char gsv_buffer6[] = "$GPGSV,3,3,12,23,07,325,86,01,05,145,60,14,34,100,35,19,40,200,60*79";
	const char gsv_buffer7[] = "$GPGSV,1,1,02,01,18,009,,30,18,008,*78";
#endif	
	char *gsv_buf = (char *)&gsv_buffer1;
	static unsigned int gsv_sentence = 0;

	if ( test_mode )
	{
		gsv_sentence %= 7;
		switch ( gsv_sentence ) { case 0: gsv_buf = (char *)&gsv_buffer1; break; case 1: gsv_buf = (char *)&gsv_buffer2; break;
								case 2: gsv_buf = (char *)&gsv_buffer3; break; case 3: gsv_buf = (char *)&gsv_buffer4; break;
								case 4: gsv_buf = (char *)&gsv_buffer5; break; case 5: gsv_buf = (char *)&gsv_buffer6; break;
								case 6: gsv_buf = (char *)&gsv_buffer7; break;}
		gsv_sentence++;
		if( valid( gsv_buf ) ) { ret = gps_gsv_update( gsv_buf, names ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
	}
	else
	{
		if( valid( data ) ) { ret = gps_gsv_update( data, names ); } else { SYSLOG_DEBUG( "Invalid Packet : %s", data ); ret = PKT_INVALID; }
	}
	return ret;
}

int create_rdb_standalone_variables( const char** names, unsigned int size )
{
	unsigned int i;
	char prefix[64];
	sprintf( prefix, "standalone" );
	for( i = 0; i < size; ++i )
	{
		if( names[i] )
		{
			const char* name = rdb_name( prefix, names[i] );
			if( rdb_update_single( name, "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { SYSLOG_ERR( "failed to create '%s' ('%s')", name, strerror( errno ) ); return -1; }
		}
	}
	return 0;
}

static int create_rdb_variables( void )
{
	if( rdb_update_single( rdb_name( "", "assisted.valid" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.valid" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.GPGGA" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.GPRMC" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.GPVTG" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.GPGSA" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.GPGSV" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
	if( create_rdb_standalone_variables( gga_field_names, sizeof( gga_field_names ) / sizeof( const char* ) ) != 0 ) { return -1; }
	if( create_rdb_standalone_variables( rmc_field_names, sizeof( rmc_field_names ) / sizeof( const char* ) ) != 0 ) { return -1; }
	if( create_rdb_standalone_variables( vtg_field_names, sizeof( vtg_field_names ) / sizeof( const char* ) ) != 0 ) { return -1; }
#ifdef REDUCE_RDB_VARIABLES
	if( create_rdb_standalone_variables( gsa_field_names2, sizeof( gsa_field_names2 ) / sizeof( const char* ) ) != 0 ) { return -1; }
	if( create_rdb_standalone_variables( gsv_field_names2, sizeof( gsv_field_names2 ) / sizeof( const char* ) ) != 0 ) { return -1; }
	if( rdb_update_single( rdb_name( "", "standalone.satellitedata" ), "", CREATE, ALL_PERM, 0, 0 ) != 0 ) { return -1; }
#else
	if( create_rdb_standalone_variables( gsa_field_names, sizeof( gsa_field_names ) / sizeof( const char* ) ) != 0 ) { return -1; }
	if( create_rdb_standalone_variables( gsv_field_names, sizeof( gsv_field_names ) / sizeof( const char* ) ) != 0 ) { return -1; }
#endif
	return 0;
}

static int read_and_subscribe_rdb_variables( void )
{
	int result = 0;
	char buf[MAX_RDB_VAR_SIZE];
	char *var_p = (char *)rdb_name( "", "assisted.updateinterval" );

	/* subscribe AGPS update interval rdb variable */
	if ( rdb_get_single( var_p, buf, MAX_RDB_VAR_SIZE ) < 0 )
	{
		SYSLOG_ERR("failed to rdb_get_single '%s' (%s)", var_p, strerror(errno));
		if (rdb_update_single(var_p, "600", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", var_p, strerror(errno));
			return -1;
		}
	}
	else
	{
		SYSLOG_INFO( "agps_update_interval = %s", buf );
		agps_update_interval = atoi(buf);
	}
	if (rdb_subscribe_variable(var_p) != 0)
	{
		SYSLOG_ERR("failed to subscribe to '%s', fail : %s", var_p, strerror(errno));
		result = -1;
	}

	/* read AGPS last valid time rdb variable */
	(void) memset(buf, MAX_RDB_VAR_SIZE, 0x00);
	if ( rdb_get_single( rdb_name( "", "assisted.lastvalidtime" ), buf, MAX_RDB_VAR_SIZE ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "", "assisted.lastvalidtime" ) );
		result = -1;
	}
	else
	{
		agps_last_updated_time = atoi(buf);
		/* if current time is older than last valid time, reset time */
		if ( agps_last_updated_time == 0 || agps_last_updated_time > time(NULL) )
		{
			update_agps_last_valid_time();
		}
		else
		{
			agps_elapsed_time = time(NULL) - agps_last_updated_time;
			if (agps_elapsed_time < 0) {
				agps_last_updated_time = time(NULL);
				agps_elapsed_time = 0;
			}
		}
		SYSLOG_INFO( "last valid time %s, agps_elapsed_time = %d", buf, (int) agps_elapsed_time );
	}

	/* subscribe log mask */
	if ( rdb_get_single( RDB_SYSLOG_MASK, buf, MAX_RDB_VAR_SIZE ) < 0 )
	{
		SYSLOG_ERR("failed to rdb_get_single '%s' (%s)", RDB_SYSLOG_MASK, strerror(errno));
		if (rdb_update_single(RDB_SYSLOG_MASK, "", CREATE, ALL_PERM, 0, 0) != 0)
		{
			SYSLOG_ERR("failed to create '%s' (%s)", RDB_SYSLOG_MASK, strerror(errno));
			return -1;
		}
	}
	if (rdb_subscribe_variable(RDB_SYSLOG_MASK) != 0)
	{
		SYSLOG_ERR("failed to subscribe to '%s', fail : %s", RDB_SYSLOG_MASK, strerror(errno));
		result = -1;
	}
	(void) memset(buf, MAX_RDB_VAR_SIZE, 0x00);
	if ( rdb_get_single( RDB_SYSLOG_MASK, buf, MAX_RDB_VAR_SIZE ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", RDB_SYSLOG_MASK );
		result = -1;
	} else {
		(void)change_loglevel_via_logmask(buf);
	}
	
	return result;
}

static int init_rdb_variables( void )
{
	int result = 0;

	if( rdb_set_single( rdb_name( "cmd", "timeout" ), "180" ) != 0 )	/* agps mode change timer 180 sec. */
	{
		SYSLOG_ERR( "failed to set %s (%s)", rdb_name( "cmd", "timeout" ), strerror( errno ) );
		result = -1;
	}
	/* start with historical mode */
	if( rdb_set_single( rdb_name( "standalone", "valid" ), "invalid" ) != 0 )
	{
		SYSLOG_ERR( "failed to set %s (%s)", rdb_name( "standalone", "valid" ), strerror( errno ) );
		result = -1;
	}
	if( rdb_set_single( rdb_name( "assisted", "valid" ), "invalid" ) != 0 )
	{
		SYSLOG_ERR( "failed to set %s (%s)", rdb_name( "assisted", "valid" ), strerror( errno ) );
		result = -1;
	}
	if ( !change_gps_data_source(HISTORICAL) )
	{
		result = -1;
	}
	return result;
}

int init_rdb( void )
{
	int result = 0;

	if( rdb_open_db() < 0 ) { SYSLOG_ERR( "failed to open RDB!" ); return -1; }
	result = create_rdb_variables( );
	result = read_and_subscribe_rdb_variables( );
	result = init_rdb_variables( );
#ifdef REDUCE_RDB_VARIABLES
	init_satellite_db( );
#endif
	return result;
}

void close_rdb( void )
{
	rdb_close_db();
}

int change_loglevel_via_logmask(char *logmask)
{
	char *pToken, *pToken2, *pSavePtr, *pSavePtr2, *pATRes;
	int llevel = LOG_ERR;
	/* bypass if incorrect argument */
	if (!logmask)
	{
		//SYSLOG_ERR("mask value is NULL");
		return -1;
	}
	//SYSLOG_ERR("Got LOGMASK command : '%s'", logmask);

	pATRes = logmask;
	pToken = strtok_r(pATRes, ";", &pSavePtr);
	while (pToken) {
		pToken2 = strtok_r((char *)pToken, ",", &pSavePtr2);
		//SYSLOG_ERR("pToken2 '%s', pSavePtr2 '%s'", pToken2, pSavePtr2);
		if (!pToken2 || !pSavePtr2) {
			SYSLOG_ERR("wrong mask value, skip");
		} else if (strcmp(pToken2, "gps") == 0) {
			//SYSLOG_ERR("found gps logmask");
			llevel = atoi(pSavePtr2);
			if (llevel < LOG_EMERG || llevel > LOG_DEBUG) {
				SYSLOG_ERR("wrong log level value, skip");
			} else {
				//SYSLOG_ERR("set '%s' log level to %d", pToken2, llevel);
				//setlogmask(LOG_UPTO(llevel));
				break;
			}
		}
		pToken = strtok_r(NULL, ";", &pSavePtr);
	}
	setlogmask(LOG_UPTO(llevel));
	return 0;
}

/* trigger subscribed variable only at the moment */
#define MAX_TRIGGER_SIZE 2048
static void handle_rdb_event( void )
{
	char triggered_names[MAX_TRIGGER_SIZE];
	int triggered_size = sizeof(triggered_names);
	char *it, *end;
	char buf[1024];

	rdb_database_lock(0);
	if (rdb_get_names("", triggered_names, &triggered_size, TRIGGERED) < 0)
	{
		SYSLOG_ERR("failed to get triggered variables '%s'", triggered_names);
		goto unlock_return;
	}
	triggered_names[triggered_size] = '\0';
	if (triggered_names[0] == 0)
		goto unlock_return;
	end = triggered_names + triggered_size + 1;
	for (it = triggered_names; it < end; ++it)
	{
		if (*it == '&')
			*it = '\0';
	}
	for (it = triggered_names; it < end; it += (strlen(it)) + 1)
	{
		//SYSLOG_DEBUG( "name '%s', triged_names='%s'", name, it );
		if (strcmp(rdb_name( "", "assisted.updateinterval" ), it) == 0) {
			(void) memset(buf, 1024, 0x00);
			if ( rdb_get_single( rdb_name( "", "assisted.updateinterval" ), buf, 1024 ) < 0 )
			{
				SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "", "assisted.updateinterval" ) );
				goto unlock_return;
			}
			else
			{
				SYSLOG_INFO( "agps_update_interval = %s", buf );
				agps_update_interval = atoi(buf);
			}
		} else if (strcmp(RDB_SYSLOG_MASK, it) == 0) {
			(void) memset(buf, 1024, 0x00);
			if ( rdb_get_single( RDB_SYSLOG_MASK, buf, 1024 ) < 0 )
			{
				SYSLOG_ERR( "rdb_get_single('%s') error ", RDB_SYSLOG_MASK );
				goto unlock_return;
			}
			else
			{
				SYSLOG_ERR( "new logmask value = %s", buf );
				(void)change_loglevel_via_logmask(buf);
			}
		} else {
			goto unlock_return;
		}
	}
unlock_return:
	rdb_database_unlock();
}

char* modestr[] = {"standalone", "agps", "historical", NULL};
static BOOL change_gps_data_source( gps_data_source_type source )
{
	char var_name[MAX_RDB_VAR_SIZE];
	static gps_data_source_type old_src = MAX_SRC;
	char buf[64], buf2[64];

	if ( old_src == source )
		return TRUE;

	SYSLOG_INFO( "change gps source to ('%s')", modestr[source] );
	sprintf(var_name, "%s.source", gps_prefix);
	if ( source == HISTORICAL && old_src == STANDALONE )
	{
		if( rdb_set_single( var_name, "historical-standalone" ) != 0 )
		{
			SYSLOG_ERR( "failed to set '%s'", var_name );
			return FALSE;
		}
	}
	else if ( source == HISTORICAL && old_src == AGPS )
	{
		if( rdb_set_single( var_name, "historical-agps" ) != 0 )
		{
			SYSLOG_ERR( "failed to set '%s'", var_name );
			return FALSE;
		}
	}
	else if ( source == HISTORICAL && old_src == MAX_SRC )
	{
		(void) memset(buf, 64, 0x00);
		if ( rdb_get_single( var_name, buf, 64 ) < 0 )
		{
			SYSLOG_ERR( "rdb_get_single('%s') error ", var_name );
			return FALSE;
		}
		if ( !strcmp(buf, "standalone") || !strcmp(buf, "agps") )
		{
			(void) memset(buf2, 64, 0x00);
			sprintf(buf2, "historical-%s", buf);
			if( rdb_set_single( var_name, buf2 ) != 0 )
			{
				SYSLOG_ERR( "failed to set '%s'", var_name );
				return FALSE;
			}
		}
	}
	else
	{
		if( rdb_set_single( var_name, modestr[source] ) != 0 )
		{
			SYSLOG_ERR( "failed to set '%s'", var_name );
			return FALSE;
		}
	}
	old_src = source;
	return TRUE;
}

char* validstr[] = {NULL, NULL, "invalid", "valid", NULL};
void change_gps_valid_status( gps_data_source_type source, gps_event_result_type validity )
{
	char var_name[MAX_RDB_VAR_SIZE];
	char buf1[16], buf2[16];

	//SYSLOG_DEBUG( "change gps ('%s') valid status to ('%s')", modestr[source], validstr[validity] );
	if (source == AGPS) agps_validity = validity;
	(void) memset(buf1, 16, 0x00);
	(void) memset(buf2, 16, 0x00);
	sprintf(var_name, "%s.%s.valid", gps_prefix, (source == STANDALONE)? "standalone":"assisted");
	if( rdb_set_single( var_name, validstr[validity] ) != 0 )
	{
		SYSLOG_ERR( "failed to set '%s'", var_name );
	}
	if ( validity == PKT_VALID )
	{
		if ( !change_gps_data_source( source ) )
			SYSLOG_ERR( "failed to set gps source to %s", modestr[source] );
	}
	if ( rdb_get_single( rdb_name( "standalone", "valid" ), buf1, 16 ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "standalone", "valid" ) );
		return;
	}
	if ( rdb_get_single( rdb_name( "assisted", "valid" ), buf2, 16 ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "assisted", "valid" ) );
		return;
	}
	/* if standalone and agps data is invalid, mark as historical data use */
	if ( strcmp(buf1, "valid") && strcmp(buf2, "valid") )
	{
		if( !change_gps_data_source(HISTORICAL) )
		{
			SYSLOG_ERR( "failed to set gps source to historical" );
		}
	}
	/* if standalone is invalid but agps is valid, mark as agps data use */
	else if ( strcmp(buf1, "valid") && strcmp(buf2, "valid") == 0 )
	{
		if( !change_gps_data_source(AGPS) )
		{
			SYSLOG_ERR( "failed to set gps source to assisted" );
		}
	}
	/* if agps is invalid but standalone is valid, mark as standalone data use */
	else if ( strcmp(buf1, "valid") == 0 && strcmp(buf2, "valid") )
	{
		if( !change_gps_data_source(STANDALONE) )
		{
			SYSLOG_ERR( "failed to set gps source to standalone" );
		}
	}
#if (0)
	/* if standalone is valid, mark as standalone gps data use */
	else if ( strcmp(buf1, "valid") == 0 )
	{
		if( !change_gps_data_source(STANDALONE) )
		{
			SYSLOG_ERR( "failed to set gps source to standalone" );
		}
	}
	/* if agps is valid, mark as mobile assisted gps data use */
	else
	{
		if( !change_gps_data_source(AGPS) )
		{
			SYSLOG_ERR( "failed to set gps source to agps" );
		}
	}
#endif
}

/* use latest time source */
static void select_latest_time_source( char* stimestr )
{
	int stime, atime;
	char buf[MAX_COMMON_FIELD_LENGTH];
	char var_name[MAX_RDB_VAR_SIZE];
	stime = atoi(stimestr);
	sprintf(var_name, "%s.assisted.time", gps_prefix);
	(void) memset(buf, MAX_COMMON_FIELD_LENGTH, 0x00);
	if ( rdb_get_single( var_name, buf, MAX_COMMON_FIELD_LENGTH ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", var_name );
		return;
	}
	atime = atoi(buf);
	SYSLOG_DEBUG( "stime %d, atime %d", stime, atime );
	if (atime  > stime) {
		SYSLOG_DEBUG( "use latest AGPS time" );
		strcpy(stimestr, buf);
	}
}

static BOOL save_to_common_fields( const char** names, unsigned int size )
{
	int i, j;
	char var_name[MAX_RDB_VAR_SIZE];
	char buf[MAX_COMMON_FIELD_LENGTH];

	/* save MS assisted gps data to common field and set gps source to MS assised */
	for (i = 0; i < MAX_COMMON_FIELD_NUMBER; i++)
	{
		for (j = 0; j < size; j++)
		{
			if (!names[j])
				continue;
			if (strcmp(common_field_names[i], names[j]) == 0)
			{
				//SYSLOG_DEBUG( "found '%s' field to update", names[j] );
				sprintf(var_name, "%s.standalone.%s", gps_prefix, names[j]);
				(void) memset(buf, MAX_COMMON_FIELD_LENGTH, 0x00);
				if ( rdb_get_single( var_name, buf, MAX_COMMON_FIELD_LENGTH ) < 0 )
				{
					SYSLOG_ERR( "rdb_get_single('%s') error ", var_name );
					return FALSE;
				}
				//SYSLOG_DEBUG( "'%s' = %s", var_name, buf );
				if (strcmp( names[j], "time" ) == 0)
				{
					select_latest_time_source( &buf[0] );
				}
				sprintf(var_name, "%s.common.%s", gps_prefix, names[j]);
				if( rdb_set_single( var_name, buf ) != 0 )
				{
					SYSLOG_ERR( "failed to set '%s'", var_name );
					return FALSE;
				}
				break;
			}
		}
	}
	return TRUE;
}

static void update_agps_last_valid_time( void )
{
	char buf[16];

	agps_last_updated_time = time(NULL);
	sprintf(buf, "%d", (int) agps_last_updated_time);
	if( rdb_set_single( rdb_name( "", "assisted.lastvalidtime" ), buf ) != 0 )
	{
		SYSLOG_ERR( "failed to update '%s'", rdb_name( "", "assisted.lastvalidtime" ) );
		return;
	}
	agps_elapsed_time = 0;
	SYSLOG_INFO( "updated '%s' as %s", rdb_name( "", "assisted.lastvalidtime" ), buf );
}

static BOOL read_and_check_agps_data( void )
{
	int i;
	char var_name[MAX_RDB_VAR_SIZE];
	char *buf;

	/* read ms assisted gps data */
	(void) memset((char *)ms_assisted_data, 0x00, MAX_COMMON_FIELD_NUMBER*MAX_COMMON_FIELD_LENGTH);
	for (i = 0; i < MAX_COMMON_FIELD_NUMBER; i++)
	{
		buf = (char *)&ms_assisted_data[i][0];
		sprintf(var_name, "%s.assisted.%s", gps_prefix, common_field_names[i]);
		*buf = 0;
		/* allow NULL height_of_geoid field */
		if ( ( rdb_get_single( var_name, buf, MAX_COMMON_FIELD_LENGTH ) != 0 ||
			strlen(buf) == 0 ) && i != MAX_COMMON_FIELD_NUMBER -1  )
		{
			SYSLOG_ERR( "rdb_get_single('%s') error ", var_name );
			change_gps_valid_status( AGPS, PKT_INVALID );
			return FALSE;
		}
		SYSLOG_DEBUG( "'%s' = %s", var_name, buf );
	}

	/* save MS assisted gps data to common field and set gps source to MS assisted */
	for (i = 0; i < MAX_COMMON_FIELD_NUMBER; i++)
	{
		buf = (char *)&ms_assisted_data[i][0];
		sprintf(var_name, "%s.common.%s", gps_prefix, common_field_names[i]);
		if( rdb_set_single( var_name, buf ) != 0 )
		{
			SYSLOG_ERR( "failed to set '%s'", var_name );
			return FALSE;
		}
	}
	sprintf(var_name, "%s.source", gps_prefix);
	if( rdb_set_single( var_name, "agps" ) != 0 )
	{
		SYSLOG_ERR( "failed to set '%s'", var_name );
		return FALSE;
	}
	update_agps_last_valid_time();
	SYSLOG_DEBUG( "updated agps data variable" );
	change_gps_valid_status( AGPS, PKT_VALID );
	return TRUE;
}

BOOL cmd_sent = FALSE;
int cmd_retry_cnt = 0;
BOOL change_to_agps_mode = FALSE;
time_t gps_cmd_sent_time = (time_t)0;
time_t gps_cmd_elapsed_time = (time_t)0;
char *gps_cmd_name[4] = { "disable", "enable", "agps", NULL };

static void reset_gps_cmd_variable(gps_mode_cmd_type cmd)
{
	cmd_sent = FALSE;
	cmd_retry_cnt = 0;
	gps_cmd_sent_time = (time_t)0;
	gps_cmd_elapsed_time = (time_t)0;
	if ( cmd == CMD_AGPS_MODE )
		change_to_agps_mode = FALSE;
}

static void reset_timeout_variables(void)
{
	gps_data_timeout_count = 0;
	gps_rmc_data_timeout_count = 0;
	gps_data_invalid_count = 0;
}

static int send_gps_mode_cmd(gps_mode_cmd_type cmd)
{
	if( rdb_set_single( rdb_name( "cmd", "status" ), "" ) != 0 )
	{
		SYSLOG_ERR( "failed to set %s (%s)", rdb_name( "cmd", "status" ), strerror( errno ) );
		return -1;
	}
	if( rdb_set_single( rdb_name( "cmd", "command" ), gps_cmd_name[cmd] ) != 0 )
	{
		SYSLOG_ERR( "failed to set %s to %s (%s)",
				rdb_name( "cmd", "command" ), gps_cmd_name[cmd], strerror( errno ) );
		return -1;
	}
	return 0;
}

static int wait_gps_mode_cmd_response(gps_mode_cmd_type cmd)
{
	char buf[16];
	(void) memset(buf, 16, 0x00);
	if ( rdb_get_single( rdb_name( "cmd", "status" ), buf, 16 ) < 0 )
	{
		SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "cmd", "status" ) );
		return -1;
	}
	//SYSLOG_DEBUG( "gps command status %s", buf );
	if ( strncmp(buf, "[done]", 6) == 0 )
	{
		SYSLOG_DEBUG( "succeeded gps mode cmd : %s", gps_cmd_name[cmd] );
		if ( cmd == CMD_AGPS_MODE && !read_and_check_agps_data())
		{
			/* if AGPS data is invalid, retry at next update time */
			SYSLOG_ERR( "failed to read/check AGPS data");
		}
		reset_gps_cmd_variable(cmd);
		return 0;
	}
	else if ( strncmp(buf, "[error]", 7) == 0 )
	{
		reset_gps_cmd_variable(cmd);
		if ( rdb_get_single( rdb_name( "cmd", "errcode" ), buf, 16 ) < 0 )
		{
			SYSLOG_ERR( "rdb_get_single('%s') error ", rdb_name( "cmd", "errcode" ) );
		}
		SYSLOG_ERR( "failed gps mode cmd : %s, err : %s", gps_cmd_name[cmd], buf );
		if ( cmd == CMD_AGPS_MODE ) {
			//change_gps_valid_status( AGPS, PKT_INVALID );
			return 1;	/* mark retry for initial AGPS command */
		}
		return 0;
	}
	else if ( strcmp(buf, "") == 0 )
	{
		return 255;
	}
	return -1;
}

BOOL send_gps_control_cmd( gps_mode_cmd_type cmd )
{
	BOOL cmd_sent = FALSE;

	while( running )
	{
		fd_set fderr;
		int selected, result;
		int nfds;
		int rdb_fd = rdb_get_fd();
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

		FD_ZERO( &fderr );
		FD_SET( gps_fd, &fderr );
		FD_SET( rdb_fd, &fderr );
		nfds = 1 + (gps_fd > rdb_fd ? gps_fd : rdb_fd);
		selected = select( nfds, NULL, NULL, &fderr, &timeout );
		if( !running ) { return TRUE; }
		if( selected < 0 ) { SYSLOG_ERR( "select() failed with error %d (%s), ignore it", selected, strerror( errno ) );}
		else if( selected > 0 )
		{
			if( FD_ISSET( gps_fd, &fderr ) )
			{
				SYSLOG_ERR( "GPSD/port exception error (%s)", strerror( errno ) );
				return FALSE;
			}
			if( FD_ISSET( rdb_fd, &fderr ) )
			{
				SYSLOG_ERR( "RDB exception error (%s)", strerror( errno ) );
				return FALSE;
			}
			SYSLOG_DEBUG( "selected > 0 (%d)", selected );
		}
		if( !cmd_sent )
		{
			SYSLOG_INFO( "send gps %s cmd", gps_cmd_name[cmd] );
			gps_cmd_sent_time = time(NULL);
			if (cmd_retry_cnt++ > 5)
			{
				SYSLOG_ERR( "failed to send gps %s cmd", gps_cmd_name[cmd] );
				return FALSE;
			}
			if ( send_gps_mode_cmd(cmd) < 0 )
				continue;
			cmd_sent = TRUE;
		}
		else
		{
			SYSLOG_DEBUG( "gps cmd sent at %d, elapsed %d",
							(int)gps_cmd_sent_time, (int)gps_cmd_elapsed_time );
			gps_cmd_elapsed_time = time(NULL) - gps_cmd_sent_time;
			if ( gps_cmd_elapsed_time > 5)	/* cmd timeout 5 secs */
			{
				SYSLOG_ERR( "gps mode cmd : %s :  timeout", gps_cmd_name[cmd] );
				return FALSE;
			}
			result = wait_gps_mode_cmd_response(cmd);
			if ( result < 0 )
				return FALSE;
			else if ( result == 0 )
				return TRUE;
		}
	}
	return FALSE;
}

int main_loop( void )
{
	BOOL first = TRUE;
	int result, agps_retry_cnt = 0;
	char buf[128];
	while( running )
	{
		fd_set fdr, fderr;
		int selected;
		int nfds;
		int rdb_fd = rdb_get_fd();
		struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

		FD_ZERO( &fdr );
		FD_ZERO( &fderr );
		FD_SET( gps_fd, &fdr );
		FD_SET( rdb_fd, &fdr );
		FD_SET( gps_fd, &fderr );
		FD_SET( rdb_fd, &fderr );
		nfds = 1 + (gps_fd > rdb_fd ? gps_fd : rdb_fd);
		selected = select( nfds, &fdr, NULL, &fderr, &timeout );
		if( !running ) { return 0; }
		if( selected < 0 ) { SYSLOG_ERR( "select() failed with error %d (%s), ignore it", selected, strerror( errno ) );}
		else if( selected > 0 )
		{
			if( FD_ISSET( rdb_fd, &fdr ) )
			{
				handle_rdb_event();
			}
			if( FD_ISSET( gps_fd, &fdr ) )
			{
				if ( handle_gps_event() < 0 )
				{
					return -1;
				}
			}
			if( FD_ISSET( rdb_fd, &fderr ) )
			{
				SYSLOG_ERR( "RDB exception error (%s), exit gpsd client", strerror( errno ) );
				return -1;
			}
			if( FD_ISSET( gps_fd, &fderr ) )
			{
				SYSLOG_ERR( "GPSD exception error (%s), exit gpsd client", strerror( errno ) );
				return -1;
			}
		}
		else
		{
			if ( gps_data_timeout_count++ % 60 == 0 )
				SYSLOG_DEBUG( "select timeout in standalone mode" );
			if ( gps_gsv_data_invalid_count++ > 60 )
			{
				SYSLOG_DEBUG( "gps_gsv_data_invalid_count is over 60, clear data base" );
				gsv_data_base_reset();
				gps_gsv_data_invalid_count = 0;
			}
		}
		if( change_to_agps_mode )
		{
			reset_timeout_variables();
			if ( !cmd_sent )
			{
				SYSLOG_DEBUG( "send agps mode cmd" );
				if (cmd_retry_cnt++ > 5)
				{
					SYSLOG_ERR( "failed to send agps mode cmd" );
					reset_gps_cmd_variable(CMD_AGPS_MODE);
					first = FALSE;
					continue;
				}
				if ( send_gps_mode_cmd(CMD_AGPS_MODE) < 0 )
					continue;
				cmd_sent = TRUE;
				cmd_retry_cnt = 0;
			}
			else
			{
				SYSLOG_DEBUG( "gps cmd sent at %d, elapsed %d",
							(int)gps_cmd_sent_time, (int)gps_cmd_elapsed_time );
				gps_cmd_elapsed_time = time(NULL) - gps_cmd_sent_time;
				if ( gps_cmd_elapsed_time > TIMEOUT_FOR_AGPS)
				{
					SYSLOG_ERR( "gps mode cmd : %s :  timeout, change to invalid state", gps_cmd_name[CMD_AGPS_MODE] );
					reset_gps_cmd_variable(CMD_AGPS_MODE);
					change_gps_valid_status( AGPS, PKT_INVALID );
					first = FALSE;
				} else {
					result = wait_gps_mode_cmd_response(CMD_AGPS_MODE);
					if (result < 0 ) {
						continue;
					} else if (result == 1 && agps_retry_cnt++ > 2) {
						change_gps_valid_status( AGPS, PKT_INVALID );
						first = FALSE;
					} else {
						first = FALSE;
					}
				}
			}
		}
		agps_elapsed_time = time(NULL) - agps_last_updated_time;
		if (agps_elapsed_time < 0) {
			agps_last_updated_time = time(NULL);
			agps_elapsed_time = agps_update_interval+1;
		}
		
		//SYSLOG_DEBUG( "selected == %d", selected );
		SYSLOG_DEBUG( "invalid cnt %d, timeout cnt %d, rmc cnt %d, agps elapt %d, upd int %d",
				gps_data_invalid_count, gps_data_timeout_count, gps_rmc_data_timeout_count,
				(int) agps_elapsed_time, (int) agps_update_interval );
		
		/* if AGPS (MS assised) is not supported, do not check AGPS timer */
		if (!support_agps_mode) {
			continue;
		}

		/* Do not send AGPS command during PLMN manual search to prevent error */
		if( rdb_get_single("wwan.0.PLMN_command_state", buf, sizeof(buf)) == 0 &&
			strcmp(buf, "1") == 0 ) {
			//SYSLOG_ERR( "skip AGPS command during PLMN manual searching" );
			continue;
		}
		
		/* if SGPS is disabled, should reply on AGPS always */
		if (support_sgps_mode == 0)
			gps_data_timeout_count = MAX_TIMEOUT_COUNT+1;

		if ( ((( gps_data_invalid_count > MAX_INVALID_COUNT ||
			gps_rmc_data_timeout_count  > MAX_RMC_INVALID_COUNT ||
			gps_data_timeout_count > MAX_TIMEOUT_COUNT ) &&
			agps_elapsed_time > agps_update_interval) || first )&&
			change_to_agps_mode == 0 )
		{
			SYSLOG_INFO( "agps elapsed time %d > update interval %d, change to AGPS mode",
					(int) agps_elapsed_time, (int) agps_update_interval );
			reset_timeout_variables();
			agps_last_updated_time = time(NULL);
			gps_cmd_sent_time = time(NULL);
			change_to_agps_mode = TRUE;
			//first = FALSE;
		}
	}
	return 0;
}

/*
* vim:ts=4:sw=4:
*/

