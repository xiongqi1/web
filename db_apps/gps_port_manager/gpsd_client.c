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
#include <stdarg.h>

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

/* Version Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD		1

#define APPLICATION_NAME "gpsd_client"

/* The user under which to run */
#define RUN_AS_USER "daemon"

volatile int running = 1;
int gps_fd = -1;
#define DEFAULT_GPSD_SERVER	"localhost"
#define NULL_GPSD_SERVER	"0.0.0.0"
static char* gpsd_server = DEFAULT_GPSD_SERVER;
static char* gpsd_port = DEFAULT_GPSD_PORT;
#if GPSD_API_MAJOR_VERSION > 4
static struct gps_data_t c_gpsdata;
static struct gps_data_t *gpsdata = &c_gpsdata;
#else
static struct gps_data_t *gpsdata;
#endif
int gps_data_invalid_count = 0;
int gps_data_timeout_count = 0;
int gps_rmc_data_timeout_count = 0;
int gps_gsv_data_invalid_count = 0;
int support_agps_mode = 1;
int support_sgps_mode = 1;

#define BUF_SIZE 1024
//#define SIMULATE_GPSD_DATA
#ifdef SIMULATE_GPSD_DATA
const char fake_gga[] = "$GPGGA,123519,3353.623475,S,15111.8049179999,E,1,08,0.9,545.4,M,46.9,M,,*55";
const char fake_rmc[]= "$GPRMC,023044,A,3353.623475,S,15111.8049179999,E,0.0,156.1,131102,15.3,E,A*30";
const char fake_vtg[]= "$GPVTG,096.5,T,083.5,M,0.0,N,0.0,K,D*22";
const char fake_gsa[] = "$GPGSA,A,10,27,08,28,13,31,10,03,29,23,01,,,2.5,1.3,2.1*39";
const char fake_gsv[] = "$GPGSV,3,1,10,27,68,048,42,08,63,326,53,28,48,239,75,13,39,154,39*79";
#endif
static gps_event_result_type handle_gps_port_event( const char *r_data, size_t len )
{
	char *buf = malloc(BUF_SIZE);
	int ret = NORMAL;

	if (!buf)
	{
		SYSLOG_ERR( "malloc error");
		return GEN_ERROR;
	}
	if( len <= 0 )
	{
		SYSLOG_ERR( "wrong len = %d", len);
		free(buf);
		return GEN_ERROR;
	}

	(void) memset(buf, 0x00, BUF_SIZE);
	(void) memcpy(buf, r_data, len);
	/* Data format from gpsd is "$..." or "{...".
	* Ignore control message data.
	*/
	if( buf[0] == '$' )
	{
		if( !gps_checksum_valid( buf ) )
		{
			SYSLOG_ERR( "invalid checksum in '%s' of %d bytes (starting with 0x%02X)", buf, strlen( buf ), *buf );
			free(buf);
			return PKT_INVALID;
		}
		if( memcmp( buf, "$GPGGA", STRLEN( "$GPGGA" ) ) == 0 )
		{
#ifdef SIMULATE_GPSD_DATA
			(void) memset(buf, 0x00, BUF_SIZE);
			(void) memcpy(buf, fake_gga, sizeof(fake_gga));
#endif
			ret = update_nmea_data( buf, gga_field_names, gga_field_names_size / sizeof( const char* ), gga_valid );
		}
		else if( memcmp( buf, "$GPRMC", STRLEN( "$GPRMC" ) ) == 0 )
		{
#ifdef SIMULATE_GPSD_DATA
			(void) memset(buf, 0x00, BUF_SIZE);
			(void) memcpy(buf, fake_rmc, sizeof(fake_rmc));
#endif
			ret = update_nmea_data( buf, rmc_field_names, rmc_field_names_size / sizeof( const char* ), rmc_valid );
		}
		else if( memcmp( buf, "$GPVTG", STRLEN( "$GPVTG" ) ) == 0 )
		{
#ifdef SIMULATE_GPSD_DATA
			(void) memset(buf, 0x00, BUF_SIZE);
			(void) memcpy(buf, fake_vtg, sizeof(fake_vtg));
#endif
			ret = update_nmea_data( buf, vtg_field_names, vtg_field_names_size / sizeof( const char* ), vtg_valid );
		}
		else if( memcmp( buf, "$GPGSA", STRLEN( "$GPGSA" ) ) == 0 )
		{
#ifdef SIMULATE_GPSD_DATA
			(void) memset(buf, 0x00, BUF_SIZE);
			(void) memcpy(buf, fake_gsa, sizeof(fake_gsa));
#endif
			ret = update_nmea_data( buf, gsa_field_names, gsa_field_names_size / sizeof( const char* ), gsa_valid );
		}
		else if( memcmp( buf, "$GPGSV", STRLEN( "$GPGSV" ) ) == 0 )
		{
#ifdef SIMULATE_GPSD_DATA
			(void) memset(buf, 0x00, BUF_SIZE);
			(void) memcpy(buf, fake_gsv, sizeof(fake_gsv));
#endif
			ret = handle_gpgsv( buf, gsv_field_names, gsv_valid );
			if ( ret == PKT_VALID )
				gps_gsv_data_invalid_count = 0;
			else
				gps_gsv_data_invalid_count++;
		}
	}
	free(buf);
	return ret;
}

static void handle_hooked_gps_raw_data(struct gps_data_t *g_data, char *r_data, size_t len)
{
	gps_event_result_type result;
	SYSLOG_DEBUG( "%d bytes, data : %s", len, r_data );
	if (r_data[0] != '$')
	{
		SYSLOG_DEBUG( "No GPS data string, increase timeout count" );
		gps_data_timeout_count++;
		return;
	}
	else
	{
		gps_data_timeout_count = 0;
	}
	result = handle_gps_port_event( r_data, len );
	if( result == PKT_INVALID )
	{
		SYSLOG_DEBUG( "invalid gps packet" );
		gps_data_invalid_count++;
		change_gps_valid_status( STANDALONE, PKT_INVALID );
	}
	else if( result == GEN_ERROR )
	{
		SYSLOG_DEBUG( "general gps packet handling error" );
	}
	else if( result == PKT_VALID )
	{
		gps_data_invalid_count = 0;
	}
}

#if GPSD_API_MAJOR_VERSION > 4
/* Newer versions of the gpsd library require this. */
void gpsd_report(int errlevel, const char *fmt, ...) 
{ 
} 
#endif

static void ensure_singleton( void )
{
	const char* lockfile = "/var/lock/subsys/"APPLICATION_NAME;
	if( open( lockfile, O_RDWR | O_CREAT | O_EXCL, 0640 ) >= 0 )
	{
		SYSLOG_INFO( "got lock on %s", lockfile );
		return;
	}
	SYSLOG_ERR( "another instance of %s already running (because creating "
			"lock file %s failed: %s)", APPLICATION_NAME, lockfile, strerror( errno ) );
	exit( EXIT_FAILURE );
}

static void release_singleton( void )
{
	unlink( "/var/lock/subsys/"APPLICATION_NAME );
	SYSLOG_INFO( "singleton released" );
}

static void release_resources( void )
{
	if( gps_fd > 0 )
	{
#ifdef V_MODCOMMS
		/* The GPS chip on the GPS mice is already started so we're done here */
#else
		SYSLOG_INFO( "stopping gps data..." );
		if ( !send_gps_control_cmd( CMD_DISABLE ) )
		{
			SYSLOG_INFO( "failed to disable gps mode");
		}
#endif
		SYSLOG_INFO( "closing gps session..." );
		gps_close( gpsdata );
		gps_fd = -1;
		SYSLOG_INFO( "gps port closed" );
	}
	close_rdb();
	release_singleton();
}

static void shutdown_gpsd_client( void )
{
	SYSLOG_INFO( "shutting down..." );
	release_resources();
}

static int open_gpsd_port( void )
{
	int retry_cnt = 0;
#if GPSD_API_MAJOR_VERSION > 4
	int result = 0;
#endif
#if GPSD_API_MAJOR_VERSION > 4
	SYSLOG_INFO( "gps_open( %s, %s) with gpsd 3.4", gpsd_server, gpsd_port );
#else
	SYSLOG_INFO( "gps_open( %s, %s) with gpsd 2.94", gpsd_server, gpsd_port );
#endif

	while (running)
	{
		if ( !running )
			return -1;

		if ( retry_cnt %2 == 0)
		{
			gpsd_server = DEFAULT_GPSD_SERVER;
#if GPSD_API_MAJOR_VERSION > 4
			result = gps_open(gpsd_server, gpsd_port, gpsdata);
#else            
			gpsdata = gps_open(gpsd_server, gpsd_port);
#endif            
		}
		else
		{
			gpsd_server = NULL_GPSD_SERVER;
#if GPSD_API_MAJOR_VERSION > 4
			result = gps_open(gpsd_server, gpsd_port, gpsdata);
#else
			gpsdata = gps_open(gpsd_server, gpsd_port);
#endif
		}
#if GPSD_API_MAJOR_VERSION > 4
		if ( result < 0 )
		{
			SYSLOG_ERR( "gps_open( %s, %s) = %d, error : '%s', retry cnt = %d", 
					gpsd_server, gpsd_port, result, gps_errstr( errno ), retry_cnt++ );
#else
		if ( !gpsdata )
		{
			SYSLOG_ERR( "gps_open( %s, %s) error : '%s', retry cnt = %d",
					gpsd_server, gpsd_port, gps_errstr( errno ), retry_cnt++ );
#endif        
			if ( retry_cnt > 5 )
			{
				SYSLOG_ERR( "give-up gps_open( %s, %s)", gpsd_server, gpsd_port );
				return -1;
			}
			sleep(1);
		}
		else
		{
			SYSLOG_INFO( "gps_open( %s, %s) succeeded",
					gpsd_server, gpsd_port );
			gps_fd = gpsdata->gps_fd;
			break;
		}
	}

	/* assign special function to process hooked gps raw data */
#if GPSD_API_MAJOR_VERSION <= 4
	gps_set_raw_hook(gpsdata, handle_hooked_gps_raw_data);
#endif    

	/* enable gpsd data stream */
	if ( gps_stream(gpsdata, WATCH_ENABLE | WATCH_NMEA, NULL) < 0 )
	{
		SYSLOG_ERR( "failed start gps_stream");
		return -1;
	}
	else
	{
		SYSLOG_INFO( "gps_stream started");
	}
	return 0;
}

static int init_gpsd_client( BOOL be_daemon )
{
	int retry_cnt = MAX_GPS_RETRY_CNT;
	SYSLOG_INFO( "starting with%s SGPS mode and with%s AGPS mode...",
		support_sgps_mode? "":"out", support_agps_mode? "":"out" );
	ensure_singleton();
	if( be_daemon )
	{
		SYSLOG_INFO( "daemonize %s", APPLICATION_NAME );
		daemonize( "/var/lock/subsys/" APPLICATION_NAME, "" );
	}
	SYSLOG_INFO( "setting signal handlers..." );
	signal( SIGHUP, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGTERM, sig_handler );
	signal( SIGQUIT, sig_handler );
	signal( SIGUSR1, sig_handler );
	signal( SIGUSR2, sig_handler );

	if( init_rdb() < 0 )
	{
		goto cleanup;
	}

	if( open_gpsd_port() != 0 )
	{
		goto cleanup;

	}
#ifdef V_MODCOMMS
	/* The GPS chip on the GPS mice is already started so we're done here */
	return 0;
#else
	/* Some module (MC8790V) does not start standalone GPS with AGPS command
	* so we need to enable GPS tracking first and send AGPS command.
	* This command is so important that it may worth to retry several times before giving up. */
	while (retry_cnt-- > 0) {
		if ( send_gps_control_cmd( CMD_ENABLE ) )
		{
			SYSLOG_INFO( "GPSD client initialized" );
			return 0;
		}
		SYSLOG_ERR( "failed to enable gps mode, remaining retry_cnt %d", retry_cnt);
	}
#endif

cleanup:
	release_resources();
	return -1;
}

#if GPSD_API_MAJOR_VERSION <= 4
#define USE_GPS_WAITING_FUNCTION
#endif
int handle_gps_event( void )
{
#ifdef USE_GPS_WAITING_FUNCTION
	int gpsd_mask = 0;
	if( gps_waiting( gpsdata ) )
	{
		//SYSLOG_INFO( "gpsd has data to process" );
#if GPSD_API_MAJOR_VERSION > 4
		gpsd_mask = gps_read( gpsdata);
		if (gpsd_mask != -1)
		{
			/* MC8704 sends nonsense $GPGGA and $GPRMS packets soon after changing to
			AGPS mode that should be ignored. */
			if (change_to_agps_mode)
			{
				SYSLOG_DEBUG( "ignore standalone GPS packet in AGPS mode" );
			} 
			else if (support_sgps_mode == 0)
			{
				SYSLOG_DEBUG( "support_sgps_mode == 0, ignore standalone GPS packet" );
			}
			else if (process_gpsd_gps_data( gpsdata ) == -1)
			{
				SYSLOG_DEBUG( "process_gpsd_gps_data failed" );
			}
		}
#else
		gpsd_mask = gps_poll( gpsdata);
#endif
		//SYSLOG_DEBUG( "gpsd_mask = 0x%x", gpsd_mask );
		if (gpsd_mask == -1)
		{
			SYSLOG_DEBUG( "gpsd_mask = 0x%x, maybe gpsd is down, exit gpsd client appl", gpsd_mask );
			return -1;
		}
	}
#else   /* USE_GPS_WAITING_FUNCTION */
	int i = 0, j = 0, r = 0, newline = 1;
	char buf[4096], serbuf[256];
	r = (int)read(gpsdata->gps_fd, buf, sizeof(buf));
	if (r > 0) 
	{
		SYSLOG_DEBUG( "NMEA %06d bytes :%s", r, buf );
		/* MC8704 sends nonsense $GPGGA and $GPRMS packets soon after changing to
		AGPS mode that should be ignored. */
		if (change_to_agps_mode)
		{
			SYSLOG_DEBUG( "ignore standalone GPS packet in AGPS mode" );
			return 0;
		}
		
		if (support_sgps_mode == 0)
		{
			SYSLOG_DEBUG( "support_sgps_mode == 0, ignore standalone GPS packet" );
			return 0;
		}
		
		for (i = 0; i < r; i++) 
		{
			char c = buf[i];
			if (newline) 
			{
				if (c != '$')
				{
					continue;
				}
				else
				{
					newline = 0;
				}
			}
			if (j < (int)(sizeof(serbuf) - 1))
			{
				serbuf[j++] = buf[i];
			}

			if (c == '\n') 
			{
				handle_hooked_gps_raw_data(NULL, (char *)&serbuf, j-1);
				j = 0;
				newline = 1;
				(void) memset(serbuf, 0x00, 256);
			}
		}
	} 
	else if (r == -1 && errno != EAGAIN)
	{
		SYSLOG_ERR( "read error %s(%d)", strerror(errno), errno );
	}
#endif  /* USE_GPS_WAITING_FUNCTION */
	return 0;
}

static void usage( void )
{
	fprintf(stderr, "\nUsage: gpsd_client [options]\n");
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-i instance\n");
	fprintf(stderr, "\t-v increase verbosity\n");
	fprintf(stderr, "\t-V version information\n");
	fprintf(stderr, "\t-a 0|1 turn support for assisted GPS on or off\n");
	fprintf(stderr, "\t-s 0|1 turn support for standalone GPS on or off\n");
	fprintf(stderr, "\n");
	exit( -1 );
}

int main(int argc, char **argv)
{
	int	ret = 0;
	int	verbosity = 0;
	BOOL be_daemon = TRUE;
	const char* instance = "0";

	while( ( ret = getopt( argc, argv, "dvVhi:?s:a:" ) ) != EOF )
	{
		switch( ret )
		{
			case 'd':
				be_daemon = FALSE;
				break;
			case 'i':
				instance = optarg;
				break;
			case 'v':
				++verbosity;
				break;
			case 'V':
				fprintf( stderr, "%s Version %d.%d.%d\n",
						argv[0], VER_MJ, VER_MN, VER_BLD );
				break;
			case 'a': support_agps_mode = atoi(optarg) ; break;
			case 's': support_sgps_mode = atoi(optarg) ; break;
			case 'h':
			case '?':
				usage();
			default:
				break;
		}
	}

	sprintf( gps_prefix, "sensors.gps.%s", instance );
	openlog(APPLICATION_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity + LOG_ERR));
	
	if( ( ret = init_gpsd_client( be_daemon ) ) != 0 )
	{
		SYSLOG_ERR( "fatal: failed to initialize" );
		exit( ret );
	}
	ret = main_loop();
	shutdown_gpsd_client();
	SYSLOG_INFO( "exit (%s)", ret == 0 ? "normal" : "failure" );
	SYSLOG_CLOSE;
	exit( ret );
}

/*
* vim:ts=4:sw=4:
*/

