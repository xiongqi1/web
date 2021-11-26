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

#include "unit_test.h"
#include "../util/gps.h"
#include "../util/rdb_util.h"

/* Newer versions of the gpsd library require this. */
void gpsd_report(int errlevel, const char *fmt, ...) 
{ 
} 

/*
#define SATELLITE_INFO_SIZE		4
#define	MAX_SATELLITE_INFO		4
static const char* gsv_names[] =
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
void TEST_AUTO( test_gps_gvs_field_copy )
{
	int cnt;
	const char gsv_buffer1[] = "$GPGSV,3,1,10,27,68,048,42,08,63,326,43,28,48,239,40,13,39,154,39*7E";
	const char gsv_buffer2[] = "$GPGSV,3,2,10,31,38,069,34,10,23,282,,03,12,041,,29,09,319,*7C";
	const char gsv_buffer3[] = "$GPGSV,3,3,10,23,07,325,,01,05,145,*7E";
	char *data = &gsv_buffer1;
	char **names = &gsv_names;
	static unsigned int gsv_sentence = 0;
	char prefix[64], value[64], type[6];
	unsigned int sentence_number, offset;
	gps_gsv_enum i;

	for (cnt = 0; cnt < 3; cnt++) {
		gsv_sentence %= 3;
		switch ( gsv_sentence ) { case 0: data = &gsv_buffer1; break; case 1: data = &gsv_buffer2; break;	case 2: data = &gsv_buffer3; break; }
		gsv_sentence++;
		memcpy( type, data + 1, 5 );
		type[5] = 0;

		sentence_number = offset = 0;
		fprintf(stderr, "==================================================================\n");
		for( i = gsv_type; i <= gsv_snr4; ++i )
		{
			const char* field;

			field = gps_field( data, i );
			if ( i < gsv_satellite_prn1 ) {
				if( !names[i] ) { continue; }
				if( !field ) { fprintf(stderr, "failed to parse '%s'\n", names[i] ); break; }
			}
			else {
				if( !names[i + offset] ) { continue; }
				if( !field ) { fprintf(stderr, "failed to parse '%s'\n", names[i + offset] ); break; }
			}
			switch ( i )
			{
			case gsv_sentence_number:
				if ( ( gps_field_copy( value, field ) ) ) {
					sentence_number	= value[0] - '0';
					offset = SATELLITE_INFO_SIZE * MAX_SATELLITE_INFO * ( sentence_number - 1 );
					fprintf(stderr, "sentence no %d, offset %d\n", sentence_number, offset);
					fprintf(stderr, "field update[set] '%s.%s' (%s)\n", prefix, names[i], value );
				}
				else {
					fprintf(stderr, "field copy failed '%s.%s' (%s)\n", prefix, names[i], value );
				}
				break;

			case gsv_type:
			case gsv_number_of_sentences:
			case gsv_number_of_satellites:
				if ( ( gps_field_copy( value, field ) ) ) {
					fprintf(stderr, "field update[set] '%s.%s' (%s)\n", prefix, names[i], value );
				}
				else {
					fprintf(stderr, "field copy failed '%s.%s' (%s)\n", prefix, names[i], value );
				}
				break;

			default:
				if ( ( gps_field_copy( value, field ) ) ) {
					fprintf(stderr, "field update[set] '%s.%s' (%s)\n", prefix, names[i + offset], value );
				}
				else {
					fprintf(stderr, "field copy failed '%s.%s' (%s)\n", prefix, names[i + offset], value );
				}
				break;
			}
		}
	}
}
*/

/*
void TEST_AUTO( test_gps_rmc_22_field_copy )
{
	char field[32];
	const char buffer[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";	// NMEA 2.2 version
	const char *buf = &buffer;
	int cnt;

	fprintf( stderr, "GPRMC V2.2 packet = '%s'\n", buffer );
	for ( cnt = 0; cnt < 12; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_rmc_23_field_copy )
{
	char field[32];
	const char buffer[] = "$GPRMC,023044,A,3907.3840,N,12102.4692,W,0.0,156.1,131102,15.3,E,A*37";	// NMEA 2.3 version
	const char *buf = &buffer;
	int cnt;

	fprintf( stderr, "GPRMC V2.3 packet = '%s'\n", buffer );
	for ( cnt = 0; cnt < 13; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_vtg_22_field_copy )
{
	char field[32];
	const char buffer[] = "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48";								// NMEA 2.2 version
	const char *buf = &buffer;
	int cnt;

	fprintf( stderr, "GPVTG V2.2 packet = '%s'\n", buffer );
	for ( cnt = 0; cnt < 9; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_vtg_23_field_copy )
{
	char field[32];
	const char buffer[] = "$GPVTG,096.5,T,083.5,M,0.0,N,0.0,K,D*22";								// NMEA 2.3 version
	const char *buf = &buffer;
	int cnt;

	fprintf( stderr, "GPVTG V2.3 packet = '%s'\n", buffer );
	for ( cnt = 0; cnt < 10; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_gsa_field_copy )
{
	char field[32];
	const char buffer[] = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39";
	const char *buf = &buffer;
	int cnt;

	for ( cnt = 0; cnt < 18; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_gga_field_copy )
{
	char field[32];
	const char buffer[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
	const char *buf = &buffer;
	int cnt;

	for ( cnt = 0; cnt < 15; cnt++) {
		TEST_CHECK( gps_field_copy( field, gps_field( buf, cnt ) ) );
		fprintf( stderr, "field[%d] = '%s'\n", cnt, gps_field_copy( field, gps_field( buf, cnt ) ) );
	}
}

void TEST_AUTO( test_gps_checksum_negative )
{
	TEST_CHECK( !gps_checksum_valid( "" ) );
	TEST_CHECK( !gps_checksum_valid( "blah" ) );
	TEST_CHECK( !gps_checksum_valid( "$*55" ) );
	TEST_CHECK( !gps_checksum_valid( "$blah" ) );
}
*/

void TEST_AUTO( test_gps_checksum_positive )
{
	//TEST_CHECK( gps_checksum_valid( "$0*30\n\r" ) );
	//TEST_CHECK( gps_checksum_valid( "$1*31\n\r" ) );
	//TEST_CHECK( gps_checksum_valid( "$11*00\n\r" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,1,10,27,68,048,42,08,63,326,53,28,48,239,75,13,39,154,39*79" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,2,10,31,38,069,24,10,23,282,10,03,12,041,,29,09,319,*7C" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,3,10,23,07,325,66,01,05,145,80*76" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,1,12,27,68,048,30,08,63,326,43,28,48,239,65,13,39,154,29*7F" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,2,12,31,38,069,64,10,23,282,70,03,12,041,50,29,09,319,40*7D" ) );
	TEST_CHECK( gps_checksum_valid( "$GPGSV,3,3,12,23,07,325,86,01,05,145,60,14,34,100,35,19,40,200,60*79" ) );
	//const char gsv_buffer1[] = "$GPGSV,3,1,10,27,68,048,42,08,63,326,53,28,48,239,75,13,39,154,39*79";
	//const char gsv_buffer2[] = "$GPGSV,3,2,10,31,38,069,24,10,23,282,10,03,12,041,,29,09,319,*7C";
	//const char gsv_buffer3[] = "$GPGSV,3,3,10,23,07,325,66,01,05,145,80*76";
	//const char gsv_buffer4[] = "$GPGSV,3,1,12,27,68,048,30,08,63,326,43,28,48,239,65,13,39,154,29*7F";
	//const char gsv_buffer5[] = "$GPGSV,3,2,12,31,38,069,64,10,23,282,70,03,12,041,50,29,09,319,40*7D";
	//const char gsv_buffer6[] = "$GPGSV,3,3,12,23,07,325,86,01,05,145,60,14,34,100,35,19,40,200,60*79";
	//TEST_CHECK( gps_checksum_valid( "$GPGGA,123519,3389.394,S,15119.791,E,1,08,0.9,545.4,M,46.9,M,,*55" ) );
	//TEST_CHECK( gps_checksum_valid( "$GPRMC,123519,A,3389.394,S,15119.791,E,022.4,084.4,230394,003.1,W*78" ) );
	//TEST_CHECK( gps_checksum_valid( "$GPRMC,023044,A,3385.9167,S,15120.8822,E,0.0,156.1,131102,15.3,E,A*30" ) );
	//TEST_CHECK( gps_checksum_valid( "$GPRMC,,V,,,,,,,,,,N*53\n\r" ) );
	//TEST_CHECK( gps_checksum_valid( "$GPGSV,1,1,02,01,18,009,,30,18,008,*78" ) );
}

/*
void TEST_AUTO( test_gps_field_copy )
{
	char field[32];
	TEST_CHECK( gps_field_copy( field, "$GPGSA,A,1,,,,,,,,,,,,,,,,*32" ) );
	TEST_CHECK( strcmp( field, "$GPGSA" ) == 0 );
	TEST_CHECK( gps_field_copy( field, gps_field( "$GPGSA,A,1,,,,,,,,,,,,,,,,*32", 0 ) ) );
	TEST_CHECK( strcmp( field, "$GPGSA" ) == 0 );
	TEST_CHECK( gps_field_copy( field, gps_field( "$GPGSA,A,1,,,,,,,,,,,,,,,,*32", 1 ) ) );
	TEST_CHECK( strcmp( field, "A" ) == 0 );
	TEST_CHECK( gps_field_copy( field, gps_field( "$GPGSA,A,1,,,,,,,,,,,,,,,,*32", 2 ) ) );
	TEST_CHECK( strcmp( field, "1" ) == 0 );
	TEST_CHECK( gps_field_copy( field, gps_field( "$GPGSA,A,1,,,,,,,,,,,,,,,,*32", 3 ) ) );
	TEST_CHECK( strcmp( field, "" ) == 0 );
	TEST_CHECK( !gps_field_copy( field, gps_field( "$GPGSA,A,1,,,,,,,,,,,,,,,,*32", 1000 ) ) );
}

void TEST_AUTO( test_rdb )
{
	char prefix[64];
	char type[6];
	const char* which = "last";
	memcpy( type, "GPGGA", 5 );
	type[6] = 0;
	sprintf( prefix, "gps.%s", type );
	strcpy( gps_prefix, "sensors.gps.0" );
	fprintf( stderr, "rdb_name( %s, %s ) = '%s'\n",prefix, which, rdb_name( prefix, which ) );
	TEST_CHECK( strcmp( rdb_name( prefix, which ), "sensors.gps.0.GPGGA.last" ) == 0 );
}
*/

TEST_AUTO_MAIN;
