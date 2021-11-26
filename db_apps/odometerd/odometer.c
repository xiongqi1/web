/*!
* Copyright Notice:
* Copyright (C) 2013 NetComm Wireless Limited
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
#include <unistd.h>

#include <sys/select.h>
#include <sys/types.h>

#include <math.h>
#include <float.h>
#include <fcntl.h>

#include "cdcs_types.h"
#include "cdcs_utils.h"
#include "cdcs_syslog.h"

#include "rdb_ops.h"
#include "logger.h"
#include "daemon.h"

#define MAX_RDB_VAR_SIZE 512
#define APPLICATION_NAME "odometerd"

//TODO: make the IGNITION_PORT_RDB flexible
#define IGNITION_PORT_RDB "sys.sensors.io.ign.d_in"

enum { PAUSE=-1, STOP, START }; //for glb.run

/* Globals */
struct _glb {
	int		run;
	int		verbosity;
	unsigned int min_threshold;
	unsigned int interval;
	int ingnition;
	double prev_latitude;
	double prev_longitude;
	double odometer;
} glb;

char rdb_buf[MAX_RDB_VAR_SIZE];


#define PI							(3.1415926535897932) /* 3.1415926535897932384L */
#define DEGREES_PER_RADIAN			(180.0 / PI)
#define RADIANS_PER_DEGREE			(PI / 180.0)
#define METERS_PER_NAUTICAL_MILE	(1853.32055)
#define LAT_METERS_PER_DEGREE		(METERS_PER_NAUTICAL_MILE * 60.0)

double gprmc_MyRadLatLon(char* var)
{
	char* pos;
	double Rad;

	pos=strchr(var, '.');
	if(!pos) return 0;

	Rad = ((double)atof(pos-2))/60.0;
	*(pos-2)=0;
	Rad += (double)atof(var);
	return Rad*RADIANS_PER_DEGREE;
}


double gprmc_MeterDistRadianLatLon(double lat1, double lat2, double long1, double long2)
{
	double d1, d2, dist;

	/* parameters should be in radians */
	/*  assert(lat1 <= PI/2);
	assert(lat1 >= -PI/2);
	assert(long1 <= PI*2);
	assert(long1 >= -PI*2);

	assert(lat2 <= PI/2);
	assert(lat2 >= -PI/2);
	assert(long2 <= PI*2);
	assert(long2 >= -PI*2);*/

	d1 = lat1-lat2;
	d2 = (long1-long2) * cos(lat1);

	dist = sqrt(d1*d1+d2*d2)*DEGREES_PER_RADIAN*LAT_METERS_PER_DEGREE;
	//log_INFO("lat1=%f, lat2=%f, long1=%f, long2=%f, dist=%f\n",lat1,lat2,long1,long2, dist);
	return dist;
}

int updata_odometer( void )
{
	double latitude, longitude;
	double dist;

	if(glb.run==1)
	{
		if(rdb_get_single("sensors.gps.0.common.latitude", rdb_buf, sizeof(rdb_buf))<0) {
			return 0;
		}

		latitude=gprmc_MyRadLatLon(rdb_buf);
		if(rdb_get_single("sensors.gps.0.common.longitude", rdb_buf, sizeof(rdb_buf))<0) {
			return 0;
		}

		longitude=gprmc_MyRadLatLon(rdb_buf);

		if(glb.prev_latitude==0 || glb.prev_longitude==0 ) //possible on powerup
		{
			glb.prev_latitude = latitude;
			glb.prev_longitude = longitude;
		}

		dist=gprmc_MeterDistRadianLatLon(latitude, glb.prev_latitude, longitude, glb.prev_longitude);

		if( dist > glb.min_threshold )
		{
			glb.odometer += dist;
			glb.prev_latitude = latitude;
			glb.prev_longitude = longitude;
		}
	}
	return 1;
}

static void sig_handler(int signum)
{
	//log_INFO("Caught Sig %d\n", signum);
	switch(signum)
	{
	case SIGHUP:
		if(rdb_get_single("sensors.gps.0.odometer_enable", rdb_buf, sizeof(rdb_buf))<0) {
			log_ERR( "failed to read %s - %s", "sensors.gps.0.odometer_enable", strerror(errno));
			return;
		}
		if( strcmp(rdb_buf, "stop" )==0 ) {
			glb.run = PAUSE;
		}
		else if( strcmp(rdb_buf, "continue")==0 ) {
			glb.run = START;
		}

		if(rdb_get_single("sensors.gps.0.odometer_reset", rdb_buf, sizeof(rdb_buf))<0) {
			log_ERR( "failed to read %s - %s", "sensors.gps.0.odometer_reset", strerror(errno));
			return;
		}
		if( strcmp(rdb_buf, "reset")==0 ) {
			glb.odometer = 0;
		}

		if(rdb_get_single("sys.sensors.io.ign.d_in", rdb_buf, sizeof(rdb_buf))<0) {
			log_ERR( "failed to read %s - %s", "sys.sensors.io.ign.d_in", strerror(errno));
			return;
		}
		if(glb.ingnition>=0) {
			if(glb.ingnition==atoi(rdb_buf))
			{
				glb.run = START;
			}
			else
			{
				glb.run = PAUSE;
			}
		}
		break;
	case SIGPOLL:
		updata_odometer();
		break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		glb.run = 0;
		break;
	}
}

static void ensure_singleton( void )
{
	const char* lockfile = "/var/lock/subsys/"APPLICATION_NAME;
	if( open( lockfile, O_RDWR | O_CREAT | O_EXCL, 0640 ) >= 0 )
	{
		log_INFO( "got lock on %s", lockfile );
		return;
	}
	log_ERR( "another instance of %s already running (because creating "
		"lock file %s failed: %s)", APPLICATION_NAME, lockfile, strerror( errno ) );
	exit( EXIT_FAILURE );
}

static void release_singleton( void )
{
	unlink( "/var/lock/subsys/"APPLICATION_NAME );
	log_ERR( "singleton released" );
}

int init_rdb_variables( void )
{
	if(rdb_get_single("sensors.gps.0.odometer", rdb_buf, sizeof(rdb_buf))<0) {
		strcpy(rdb_buf, "0");
	}
	glb.odometer=atof(rdb_buf);
	if( rdb_update_single( "sensors.gps.0.odometer", rdb_buf, CREATE|PERSIST, ALL_PERM, 0, 0 ) != 0 )
	{
		log_ERR( "failed to set %s (%s)", "sensors.gps.0.odometer", strerror( errno ) );
		return -1;
	}

	*rdb_buf=0;
	rdb_get_single("sensors.gps.0.odometer.starttime", rdb_buf, sizeof(rdb_buf));
	if(!(*rdb_buf)) {
		system("rdb_set -p sensors.gps.0.odometer.starttime \"`date`\"");
	}

	if (rdb_subscribe_variable_signal( "sensors.gps.0.odometer_enable", SIGHUP ) != 0)
	{
		log_ERR("failed to subscribe to '%s', fail : %s", "sensors.gps.0.odometer_enable", strerror(errno));
		return -1;
	}

	if (rdb_subscribe_variable_signal( "sensors.gps.0.odometer_reset", SIGHUP ) != 0)
	{
		log_ERR("failed to subscribe to '%s', fail : %s", "sensors.gps.0.odometer_reset", strerror(errno));
		return -1;
	}

	if (rdb_subscribe_variable_signal( "sys.sensors.io.ign.d_in", SIGHUP ) != 0)
	{
		log_ERR("failed to subscribe to '%s', fail : %s", "sys.sensors.io.ign.d_in", strerror(errno));
		return -1;
	}
	if (rdb_subscribe_variable_signal( "sensors.gps.0.common.longitude", SIGPOLL ) != 0)
	{
		log_ERR("failed to subscribe to '%s', fail : %s", "sensors.gps.0.common.longitude", strerror(errno));
		return -1;
	}
	return 1;
}

int main_loop( void )
{
struct timeval tv;
int val;
char buf[512];

	while( glb.run )
	{
		tv.tv_sec = glb.interval;
		tv.tv_usec = 0;

		do
		{
			val = select(0, NULL, NULL, NULL, &tv);
		} while (val != 0 && errno == EINTR);

		if(glb.run==1) {
			sprintf(buf, "%.1f", glb.odometer);
			rdb_update_single("sensors.gps.0.odometer", buf, NONBLOCK, DEFAULT_PERM,0,0);
		}
	}
	return 0;
}

static void usage( void )
{
	fprintf(stderr, "\nUsage: odometer [options]\n");
	fprintf(stderr, "\n\tOptions:\n");
	fprintf(stderr, "\t-d don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-x Maximum distance threshold (default = 100 meters)\n");
	fprintf(stderr, "\t-m Minimum distance threshold (default = 5 meters)\n");
	fprintf(stderr, "\t-i Start odometer only the Ignition input match the value (0 or 1, -1(default) = disable)\n");
	fprintf(stderr, "\t-r Reed current odometer\n");
	fprintf(stderr, "\t-z Reset odometer\n");
	fprintf(stderr, "\t-v Update interval (default = 3 second)\n");
	fprintf(stderr, "\n");
	exit( -1 );
}

int main(int argc, char **argv)
{
	glb.verbosity=1;
	int	ret = 0;
	BOOL be_daemon = TRUE;
	glb.run = START;
	glb.min_threshold = 35;
	glb.interval = 3;
	glb.ingnition = -1;			//ingnition control desbaled
	glb.prev_latitude = 0;
	glb.prev_longitude = 0;

	if( rdb_open_db() < 0 )
	{
		log_ERR( "failed to open RDB!" );
		exit (-1);
	}

	while( ( ret = getopt( argc, argv, "dm:i:v:rzh" ) ) != EOF )
	{
		switch( ret )
		{
			case 'd': be_daemon = FALSE; break;
			case 'm': glb.min_threshold = atof(optarg); break;
			case 'i': glb.ingnition = atoi(optarg); break;
			case 'v': glb.interval = atoi(optarg); break;
			case 'r':
				if(rdb_get_single("sensors.gps.0.odometer", rdb_buf, sizeof(rdb_buf))<0) {
					strcpy(rdb_buf, "0");
				}
				glb.odometer=atof(rdb_buf);
				if(rdb_get_single("sensors.gps.0.odometer.starttime", rdb_buf, sizeof(rdb_buf))<0) {
					strcpy(rdb_buf, "");
				}
				fprintf(stderr, "\nCurrent odometer: %.1f   Start time: %s\n",glb.odometer, rdb_buf);
				rdb_close_db();
				exit(0);
				break;
			case 'z':
				system("rdb_set -p sensors.gps.0.odometer 0; rdb_set -p sensors.gps.0.odometer.starttime ");
				rdb_close_db();
				exit(0);
				break;
			case 'h':
			case '?': usage();
			default: break;
		}
	}

	ensure_singleton();
	if (be_daemon)
	{
		daemonize( "/var/lock/subsys/" APPLICATION_NAME, "" );
		log_INFO("daemonized");
	}

	signal( SIGHUP, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGTERM, sig_handler );
	signal( SIGQUIT, sig_handler );
	signal( SIGPOLL, sig_handler );

	log_INFO(APPLICATION_NAME"    x=%d, i=%d, v=%d", glb.min_threshold, glb.ingnition, glb.interval);

	if(init_rdb_variables()>0)
	{
		main_loop();
	}
	rdb_close_db();
	release_singleton();
	return 0;
}

/*
* vim:ts=4:sw=4:
*/

