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

#ifndef GPS_PORT_MANAGER_GPS_H_
#define GPS_PORT_MANAGER_GPS_H_

#include "cdcs_types.h"

typedef enum { gps_gga_invalid = '0', gps_gga_gps_fix = '1' /* etc, add as needed */ } gps_gga_fix_quality_enum;

typedef enum
{
	gga_type
	, gga_time
	, gga_latitude
	, gga_latitude_direction
	, gga_longitude
	, gga_longitude_direction
	, gga_fix_quality
	, gga_number_of_satellites
	, gga_hdop
	, gga_altitude
	, gga_height_of_geoid
	, gga_elapsed_since_last_dgps_update
	, gga_dgps_station_id
} gps_gga_enum;


typedef enum { gps_rmc_active, gps_rmc_void } gps_rmc_status_enum;

/* NMEA 2.3
* RMC and VTG command have a new field named 'mode' at the end of the packet just before the checksum.
*/
typedef enum { gps_autonomous_mode = 'A', gps_differencial_mode = 'D', gps_estimated_mode = 'E', gps_notvalid_mode = 'N', gps_simulated_mode = 'S' } gps_rmc_vtg_mode_enum;

typedef enum
{
	rmc_type
	, rmc_time
	, rmc_status
	, rmc_latitude
	, rmc_latitude_direction
	, rmc_longitude
	, rmc_longitude_direction
	, rmc_speed_knots
	, rmc_track_angle
	, rmc_date
	, rmc_magnetic_variation
	, rmc_magnetic_variation_direction
	, rmc_mode
} gps_rmc_enum;


typedef enum
{
	vtg_type
	, vtg_true_track_made_good
	, vtg_true_track_made_good_label
	, vtg_magnetic_track_made_good
	, vtg_magnetic_track_made_good_label
	, vtg_ground_speed_knots
	, vtg_ground_speed_knots_label
	, vtg_ground_speed_kph
	, vtg_ground_speed_kph_label
	, vtg_mode
} gps_vtg_enum;

typedef enum { gps_gsa_auto_selection = 'A', gps_gsa_manual_selection = 'M' } gps_gsa_auto_selection_enum;
typedef enum { gps_gsa_no_fix = '1', gps_gsa_2d_fix = '2', gps_gsa_3d_fix = '3' } gps_gsa_3d_fix_enum;

typedef enum
{
	gsa_type
	, gsa_auto_selection
	, gsa_3d_fix
	, gsa_satellite_prn1
	, gsa_satellite_prn2
	, gsa_satellite_prn3
	, gsa_satellite_prn4
	, gsa_satellite_prn5
	, gsa_satellite_prn6
	, gsa_satellite_prn7
	, gsa_satellite_prn8
	, gsa_satellite_prn9
	, gsa_satellite_prn10
	, gsa_satellite_prn11
	, gsa_satellite_prn12
	, gsa_pdop
	, gsa_hdop
	, gsa_vdop
} gps_gsa_enum;

typedef enum
{
	gsv_type = 0
	, gsv_number_of_sentences
	, gsv_sentence_number
	, gsv_number_of_satellites
	, gsv_satellite_prn1
	, gsv_elevation1
	, gsv_azimuth1
	, gsv_snr1
	, gsv_satellite_prn2
	, gsv_elevation2
	, gsv_azimuth2
	, gsv_snr2
	, gsv_satellite_prn3
	, gsv_elevation3
	, gsv_azimuth3
	, gsv_snr3
	, gsv_satellite_prn4
	, gsv_elevation4
	, gsv_azimuth4
	, gsv_snr4
} gps_gsv_enum;

BOOL gps_checksum_valid( const char* buf );

const char* gps_field( const char* line, unsigned int field );

const char* gps_field_copy( char* d, const char* s );

unsigned int gps_field_count( const char* line );

#endif /* GPS_PORT_MANAGER_GPS_H_ */
