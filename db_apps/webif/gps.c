/*!
 * THIS IS ONLY USED FOR V1 WEBUI. FOR V2+ THIS FUNCTIONALITY IS NOW INCLUDED IN THE HTML PAGES SERVERSIDE SCRIPTING
 * Copyright Notice:
 * Copyright (C) 2002-2008 Call Direct Cellular Solutions
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of CDCS
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CDCS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CDCS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/msg.h>
#include <ctype.h>
#include "string.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#include "rdb_util.h"

#if (defined MAX_VALUE_LENGTH) && (MAX_VALUE_LENGTH < 512)
#undef MAX_VALUE_LENGTH
#define MAX_VALUE_LENGTH 512
#endif
void print_historical_source( char *tmp )
{
	if (strncmp(tmp, "historical-standalone", 21) == 0) {
		printf("datasource=\"Previously Stored GPS Data (Standalone)\";");
	} else if (strncmp(tmp, "historical-agps", 15) == 0) {
		printf("datasource=\"Previously Stored GPS Data (Mobile Assisted)\";");
	} else {
		printf("datasource=\"Previously Stored GPS Data (N/A)\";");
	}
}

#define MAX_SATELLITE     12
#define FIELD_LEN		  4	
char rdb_fix[MAX_SATELLITE][FIELD_LEN];
char rdb_prn[MAX_SATELLITE][FIELD_LEN];
char rdb_snr[MAX_SATELLITE][FIELD_LEN];
char rdb_elev[MAX_SATELLITE][FIELD_LEN];
char rdb_azim[MAX_SATELLITE][FIELD_LEN];
void read_n_parse_satellite_info( void )
{
	int i;
	char *del1 = ";", *del2 = ",";
	char *str1, *str2, *token, *token2, *savestr1, *savestr2;
	str1 = get_single("sensors.gps.0.standalone.satellitedata");
	for (i = 0; i < MAX_SATELLITE; i++, str1 = savestr1)
	{
		token = strtok_r(str1, del1, &savestr1);
		str2 = token;
        token2 = strtok_r(str2, del2, &savestr2);
        if (token2 == NULL)	strcpy(rdb_fix[i], "0");
		else strcpy(rdb_fix[i], token2);
		str2 = savestr2;

        token2 = strtok_r(str2, del2, &savestr2);
        if (token2 == NULL)	strcpy(rdb_prn[i], "N/A");
		else strcpy(rdb_prn[i], token2);
		str2 = savestr2;

        token2 = strtok_r(str2, del2, &savestr2);
        if (token2 == NULL)	strcpy(rdb_snr[i], "N/A");
		else strcpy(rdb_snr[i], token2);
		str2 = savestr2;

        token2 = strtok_r(str2, del2, &savestr2);
        if (token2 == NULL)	strcpy(rdb_elev[i], "N/A");
		else strcpy(rdb_elev[i], token2);
		str2 = savestr2;

        token2 = strtok_r(str2, del2, &savestr2);
        if (token2 == NULL)	strcpy(rdb_azim[i], "N/A");
		else strcpy(rdb_azim[i], token2);
	}
}

int gps_status_refresh(void)
{
	char *tmp;
	int i, temp, hour, min, sec, yy, mm, dd, sgps_valid=0;

	/* The order of printed variable should match the order of html document reading order */

	/* define variables to prevent page error in IE */
	printf("var datasource, sgps_status, agps_status, date, time;");
	printf("var latitude_direction, latitude, longitude_direction, longitude;");
	printf("var altitude, height_of_geoid, ground_speed_kph, ground_speed_knots;");
	printf("var pdop, hdop, vdop, number_of_satellites;");
	printf("var satellite_prn_for_fix = new Array();");
	printf("var satellite_prn = new Array();");
	printf("var snr = new Array();");
	printf("var elevation = new Array();");
	printf("var azimuth = new Array();");

	/* read gps source & validity*/
	tmp = get_single("sensors.gps.0.enable");
	if (strncmp(tmp, "1", 1) == 0) {
		tmp = get_single("sensors.gps.0.source");		/* ex) standalone/agps/historical */
		if (strncmp(tmp, "agps", 4) == 0) {
			printf("datasource=\"MS Assisted GPS\";");
		}
		else if (strncmp(tmp, "standalone", 10) == 0) {
			printf("datasource=\"Stand-alone GPS\";");
		} else {
			print_historical_source(tmp);
		}
	} else {
		tmp = get_single("sensors.gps.0.source");		/* ex) standalone/agps/historical */
		if (strncmp(tmp, "agps", 4) == 0) {
			printf("datasource=\"Previously Stored GPS Data (Mobile Assisted)\";");
		}
		else if (strncmp(tmp, "standalone", 10) == 0) {
			printf("datasource=\"Previously Stored GPS Data (Standalone)\";");
		} else {

			print_historical_source(tmp);
		}
	}

	tmp = get_single("sensors.gps.0.standalone.valid");
	if (strncmp(tmp, "N/A", 3) == 0) {
		printf("sgps_status=\"%s\";", tmp);
	} else if (strncmp(tmp, "valid", 5) == 0) {
		printf("sgps_status=\"Normal\";");
		sgps_valid = 1;
	} else {
		printf("sgps_status=\"Invalid\";");
	}
	tmp = get_single("sensors.gps.0.assisted.valid");
	if (strncmp(tmp, "N/A", 3) == 0) {
		printf("agps_status=\"%s\";", tmp);
	} else if (strncmp(tmp, "valid", 5) == 0) {
		if(sgps_valid == 1)
			printf("agps_status=\"Normal (Not In Use)\";");
		else
			printf("agps_status=\"Normal\";");
	} else {
		printf("agps_status=\"Invalid\";");
	}

	/* read date & time */
	tmp = get_single("sensors.gps.0.common.date");		/* ex) 230394 */
	if (strncmp(tmp, "N/A", 3)) {
		temp = atoi(tmp);
		dd = temp / 10000;
		mm = (temp % 10000) / 100;
		yy = temp % 100;
		if (yy >= 80) {
			printf("date=\"%02d.%02d.19%02d\";\n", dd, mm, yy);
		} else {
			printf("date=\"%02d.%02d.20%02d\";\n", dd, mm, yy);
		}
	} else {
		printf("date=\"%s\";", tmp);
	}

	tmp = get_single("sensors.gps.0.common.time");		/* ex) 123519 */
	if (strncmp(tmp, "N/A", 3)) {
		temp = atoi(tmp);
		hour = temp / 10000;
		min = (temp % 10000) / 100;
		sec = temp % 100;
		printf("time=\"%02d:%02d:%02d UTC\";", hour, min, sec);
	} else {
		printf("time=\"%s\";", tmp);
	}

	/* read latitude & longitude... */
	printf("latitude_direction=\"%s\";", get_single("sensors.gps.0.common.latitude_direction"));
	printf("latitude=\"%s\";", get_single("sensors.gps.0.common.latitude"));
	printf("longitude_direction=\"%s\";", get_single("sensors.gps.0.common.longitude_direction"));
	printf("longitude=\"%s\";", get_single("sensors.gps.0.common.longitude"));
	printf("altitude=\"%s   m\";", get_single("sensors.gps.0.standalone.altitude"));
	printf("height_of_geoid=\"%s   m\";", get_single("sensors.gps.0.common.height_of_geoid"));
	printf("ground_speed_kph=\"%s   km/h\";", get_single("sensors.gps.0.standalone.ground_speed_kph"));
	printf("ground_speed_knots=\"%s   knots\";", get_single("sensors.gps.0.standalone.ground_speed_knots"));
	printf("pdop=\"%s\";", get_single("sensors.gps.0.standalone.pdop"));
	printf("hdop=\"%s\";", get_single("sensors.gps.0.standalone.hdop"));
	printf("vdop=\"%s\";", get_single("sensors.gps.0.standalone.vdop"));
	printf("number_of_satellites=\"%s\";", get_single("sensors.gps.0.standalone.number_of_satellites"));
#ifdef V_ODOMETER_y
	printf("odometer=\"%s\";", get_single("sensors.gps.0.odometer"));
	printf("odometer_starttime=\"%s\";", get_single("sensors.gps.0.odometer.starttime"));
#endif
	read_n_parse_satellite_info();

//#define GPS_DEBUG

	for (i=0; i< MAX_SATELLITE; i++)
	{
#ifdef GPS_DEBUG
		printf("satellite_prn_for_fix[%d]=\"1\";", i);
#else
		printf("satellite_prn_for_fix[%d]=\"%s\";", i, rdb_fix[i]);
#endif
	}
	for (i=0; i< MAX_SATELLITE; i++)
	{
#ifdef GPS_DEBUG
		printf("satellite_prn[%d]=\"%d\";", i, i);
		printf("snr[%d]=\"%d\";", i, i*2);
		printf("elevation[%d]=\"%d\";", i, i*3);
		printf("azimuth[%d]=\"%d\";", i, i*4);
#else
		printf("satellite_prn[%d]=\"%s\";", i, rdb_prn[i]);
		printf("snr[%d]=\"%s\";", i, rdb_snr[i]);
		printf("elevation[%d]=\"%s\";", i, rdb_elev[i]);
		printf("azimuth[%d]=\"%s\";", i, rdb_azim[i]);
#endif
	}

	return (0);
}

int main(int argc, char* argv[], char* envp[])
{
char *p_str, *p_str2;
	printf("Content-Type: text/html \r\n\r\n");
	p_str=getenv("SESSION_ID");
	p_str2=getenv("sessionid");
	if( p_str==0 || p_str2==0 || strcmp(p_str, p_str2) ) {
		exit(0);
	}

	if(rdb_start())
	{
		printf( "can't open cdcs_DD %i ( %s )\n", -errno, strerror(errno));
	}
	else
	{
		gps_status_refresh();
	}
	rdb_end();
	exit(0);
}
