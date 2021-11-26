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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#define _XOPEN_SOURCE
#define __USE_XOPEN
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

#include "rdb_ops.h"
#include "./util/daemon.h"
#include "./util/gps.h"
#include "./util/rdb_util.h"

#include "gps_common.h"

/* Version Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1

#define APPLICATION_NAME "gps_port_manager"

/* The user under which to run */
#define RUN_AS_USER "daemon"

const char* dev_nmea = "/dev/nmea";
volatile int running = 1;
int gps_fd = -1;
static int pty_fd = -1;
static char *gps_port_name = NULL;
int gps_data_invalid_count = 0;
int gps_data_timeout_count = 0;
int gps_rmc_data_timeout_count = 0;
int gps_gsv_data_invalid_count = 0;
int support_agps_mode = 1;
int support_sgps_mode = 1;

static int readline_( int fd, char* buf, int size, int timeout_sec )
{
	char* p = buf;
	int len = 0;
	int selected;
	fd_set fdr;
	int t = timeout_sec > 0 ? 1 : 0;
	while( TRUE )
	{
		struct timeval timeout = { .tv_sec = t, .tv_usec = 100000 };
		FD_ZERO( &fdr );
		FD_SET( fd, &fdr );
		selected = select( fd + 1, &fdr, NULL, NULL, &timeout );
		if( selected < 0 ) { SYSLOG_ERR( "select() failed with error %d (%s)", selected, strerror( errno ) ); return -1; }
		if( selected > 0 )
		{
			if( read( fd, p, 1 ) <= 0 ) { SYSLOG_ERR( "failed to read from %d (%s)", fd, strerror( errno ) ); return -1; }
			//fprintf( stderr, "%02X ", *p ); // TODO: remove
			++len;
			if( *p == '\n' ) { buf[len] = 0; /* SYSLOG_DEBUG( "[%s]", buf ); fprintf( stderr, "\n" ) */; return len; }
			if( len == size - 1 ) { buf[len] = 0; SYSLOG_ERR( "expected the line shorter than %d, got: [%s]", size, buf ); return len; }
			++p;
		}
		else
		{
			if( timeout_sec-- > 0 ) { continue; }
			SYSLOG_ERR( "timed out" );
			buf[len] = 0;
			return len;
		}
	}
}

static int writeline_( int fd, char* buf, int size )
{
	char* p = buf;
	int selected;
	fd_set fdw;
	while( TRUE )
	{
		struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };
		FD_ZERO( &fdw );
		FD_SET( fd, &fdw );
		selected = select( fd + 1, NULL, &fdw, NULL, &timeout );
		//SYSLOG_ERR( "selected = %d", select);
		if( selected < 0 ) { SYSLOG_ERR( "select() failed with error %d (%s)", selected, strerror( errno ) ); return -1; }
		else if( selected > 0 )
		{
			return (write( fd, p, size ));
		}
		else
		{
			return -1;
		}
	}
}

#define BUF_SIZE 1024
static gps_event_result_type handle_gps_port_event( void )
{
	char *buf = malloc(BUF_SIZE);
	int len = readline_( gps_fd, buf, BUF_SIZE, 3 );
	int written_bytes = 0;
	static int count = 0;
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
	if (buf[0] != '$')
	{
		SYSLOG_DEBUG( "No GPS data string, increase timeout count" );
		gps_data_timeout_count++;
		return PKT_INVALID;
	}
	else
	{
		gps_data_timeout_count = 0;
	}

	/* MC8704 sends nonsense $GPGGA and $GPRMS packets soon after changing to
	AGPS mode that should be ignored. */
	if (change_to_agps_mode)
	{
		SYSLOG_DEBUG( "ignore standalone GPS packet in AGPS mode" );
		free(buf);
		return GEN_ERROR;
	}
	
	if (support_sgps_mode == 0)
	{
		SYSLOG_DEBUG( "support_sgps_mode == 0, ignore standalone GPS packet" );
		free(buf);
		return GEN_ERROR;
	}
	
	if( len > 1 )
	{
		/* For less burden, flush input port periodically.
		* If everything is happy with gps deamon, it can be removed.
		*/
		if (count++ > 4)
		{
			SYSLOG_DEBUG( "flush %s port", gps_port_name );
			count = 0;
			if( tcflush( gps_fd, TCIFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of %s failed (%s)", gps_port_name, strerror( errno ) ); }
		}
		if( !gps_checksum_valid( buf ) )
		{
			SYSLOG_ERR( "invalid checksum in '%s' of %d bytes (starting with 0x%02X)", buf, strlen( buf ), *buf );
			free(buf);
			return PKT_INVALID;
		}
		if( memcmp( buf, "$GPGGA", STRLEN( "$GPGGA" ) ) == 0 )
		{
			ret = update_nmea_data( buf, gga_field_names, gga_field_names_size / sizeof( const char* ), gga_valid );
		}
		else if( memcmp( buf, "$GPRMC", STRLEN( "$GPRMC" ) ) == 0 )
		{
			ret = update_nmea_data( buf, rmc_field_names, rmc_field_names_size / sizeof( const char* ), rmc_valid );
		}
		else if( memcmp( buf, "$GPVTG", STRLEN( "$GPVTG" ) ) == 0 )
		{
			ret = update_nmea_data( buf, vtg_field_names, vtg_field_names_size / sizeof( const char* ), vtg_valid );
		}
		else if( memcmp( buf, "$GPGSA", STRLEN( "$GPGSA" ) ) == 0 )
		{
			ret = update_nmea_data( buf, gsa_field_names, gsa_field_names_size / sizeof( const char* ), gsa_valid );
		}
		else if( memcmp( buf, "$GPGSV", STRLEN( "$GPGSV" ) ) == 0 )
		{
			ret = handle_gpgsv( buf, gsv_field_names, gsv_valid );
			if ( ret == PKT_VALID )
				gps_gsv_data_invalid_count = 0;
			else
				gps_gsv_data_invalid_count++;
		}
	}
	//written_bytes = write( pty_fd, buf, len );
	written_bytes = writeline_( pty_fd, buf, len );
	if( written_bytes >= len ) { /*SYSLOG_DEBUG( "wrote %d bytes to %d (%s)", len, pty_fd, dev_nmea );*/ free(buf); return ret; }
	// TODO: maybe we don't need to flush at all
	SYSLOG_DEBUG( "failed to write to fd %d (%s), %d / %d bytes discarded: err '%s'; trying to flush pty slave", pty_fd, dev_nmea, written_bytes, len, strerror( errno ) );
	if( tcflush( pty_fd, TCOFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of %s failed (%s)", dev_nmea, strerror( errno ) ); }
	free(buf);
	return GEN_ERROR;
}

static int open_pty( void )
{
	const char* pty_slave_name;
	struct termios ios;
	SYSLOG_DEBUG( "opening pty..." );
	if( ( pty_fd = open( "/dev/ptmx", O_RDWR | O_NOCTTY | O_NONBLOCK ) ) < 0 ) { SYSLOG_ERR( "failed to open /dev/ptmx (%s)", strerror( errno ) ); return -1; }
	if( tcgetattr( pty_fd, &ios ) != 0 ) { SYSLOG_ERR( "tcgetattr() failed: %s", strerror( errno ) ); return -1; }
	ios.c_oflag |= ONLCR;
	if( tcsetattr( pty_fd, TCSANOW, &ios ) != 0 ) { SYSLOG_ERR( "tcsetattr() failed: %s", strerror( errno ) ); return -1; }
	pty_slave_name = ptsname( pty_fd );
	if( !pty_slave_name ) { SYSLOG_ERR( "ptsname(%d) failed (%s)", pty_fd, strerror( errno ) ); return -1; }
	if( grantpt( pty_fd ) != 0 ) { SYSLOG_ERR( "grantpt(%d) failed (%s)", pty_fd, strerror( errno ) ); return -1; }
	if( unlockpt( pty_fd ) != 0 ) { SYSLOG_ERR( "unlockpt(%d) failed (%s)", pty_fd, strerror( errno ) ); return -1; }
	if( symlink( pty_slave_name, dev_nmea ) < 0 ) { SYSLOG_ERR( "symlink( %s, %s ) failed (%s)", pty_slave_name, dev_nmea, strerror( errno ) ); return -1; }
	SYSLOG_INFO( "pty slave %s symlinked to %s", pty_slave_name, dev_nmea );
	if( tcflush( pty_fd, TCOFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of %s failed (%s)", dev_nmea, strerror( errno ) ); }
	return 0;
}

static void close_pty( void )
{
	unlink( dev_nmea );
	SYSLOG_INFO( "unlink( \"%s\" ): %s", dev_nmea, strerror( errno ) );
	close( pty_fd );
}

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
		SYSLOG_INFO( "stopping gps data..." );
		if ( !send_gps_control_cmd( CMD_DISABLE ) )
		{
			SYSLOG_ERR( "failed to disable gps mode");
		}
		SYSLOG_DEBUG( "flushing gps port..." );
		if( tcflush( gps_fd, TCIFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of gps port failed (%s)", strerror( errno ) ); }
		SYSLOG_INFO( "closing gps port..." );
		close( gps_fd );
		gps_fd = -1;
		SYSLOG_INFO( "gps port closed" );
	}
	close_rdb();
	close_pty();
	release_singleton();
}

static void shutdown_gps_port_manager( void )
{
	SYSLOG_INFO( "shutting down..." );
	release_resources();
}

/* Newer versions of the gpsd library require this. */
void gpsd_report(int errlevel, const char *fmt, ...) 
{ 
} 

static int open_gps_port( const char* gps_port )
{
	struct termios ios;
	struct stat port_stat;
	unsigned int timeout = 60;
	SYSLOG_INFO( "waiting for port '%s' for %d seconds...", gps_port, timeout );
	while( stat( gps_port, &port_stat ) < 0 )
	{
		if( timeout-- == 0 ) { SYSLOG_ERR( "waiting for port %s timed out (%s)", gps_port, strerror( errno ) ); return -1; }
		sleep( 1 );
	}
	SYSLOG_INFO( "opening GPS port '%s'...", gps_port );
	if( ( gps_fd = open( gps_port, O_RDWR | O_NOCTTY | O_NONBLOCK ) ) <= 0 ) { SYSLOG_ERR( "failed to open '%s' (%s)", gps_port, strerror( errno ) ); return -1; }
	if( tcgetattr( gps_fd, &ios ) != 0 ) { SYSLOG_ERR( "tcgetattr() failed: %s", strerror( errno ) ); return -1; }
	ios.c_iflag |= IGNCR;
	if( tcsetattr( gps_fd, TCSANOW, &ios ) != 0 ) { SYSLOG_ERR( "tcsetattr() failed: %s", strerror( errno ) ); return -1; }
	if( tcflush( gps_fd, TCIFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of %s failed (%s)", gps_port_name, strerror( errno ) ); }
	return 0;
}

static int init_gps_port_manager( const char* gps_port, BOOL be_daemon )
{
	int retry_cnt = MAX_GPS_RETRY_CNT;
	SYSLOG_INFO( "starting with%s SGPS mode and with%s AGPS mode...",
		support_sgps_mode? "":"out", support_agps_mode? "":"out" );
	ensure_singleton();
	if( be_daemon )	{
		SYSLOG_INFO( "daemonize %s", APPLICATION_NAME );
		daemonize( "/var/lock/subsys/" APPLICATION_NAME, "" );
	}
	SYSLOG_DEBUG( "setting signal handlers..." );
	signal( SIGHUP, sig_handler );
	signal( SIGINT, sig_handler );
	signal( SIGTERM, sig_handler );
	signal( SIGQUIT, sig_handler );
	signal( SIGUSR1, sig_handler );
	signal( SIGUSR2, sig_handler );
	if( init_rdb() < 0 ) { goto cleanup; }
	if( open_gps_port( gps_port ) != 0 ) { goto cleanup; }
	if( open_pty() < 0 ) { goto cleanup; }
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

cleanup:
	release_resources();
	return -1;
}

#define MAX_GPS_PORT_ERROR_COUNT	20
int handle_gps_event( void )
{
	static int gps_port_error_cnt = 0;
	gps_event_result_type result;

	//SYSLOG_DEBUG( "got event on GPS port" );
	gps_data_timeout_count = 0;
	result = handle_gps_port_event();
	if( result == PKT_INVALID )
	{
		SYSLOG_DEBUG( "invalid gps packet" );
		gps_data_invalid_count++;
		change_gps_valid_status( STANDALONE, PKT_INVALID );
	}
	else if( result == GEN_ERROR )
	{
		SYSLOG_DEBUG( "general gps packet handling error, count %d", ++gps_port_error_cnt);
		if ( gps_port_error_cnt >  MAX_GPS_PORT_ERROR_COUNT )
		{
			SYSLOG_DEBUG( "gps port error count exceed max limit (%d), close and reopen ports", MAX_GPS_PORT_ERROR_COUNT );
			if( tcflush( gps_fd, TCIFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of gps port failed (%s)", strerror( errno ) ); }
			close( gps_fd );
			gps_fd = -1;
			if( tcflush( pty_fd, TCOFLUSH ) != 0 ) { SYSLOG_ERR( "tcflush() of %s failed (%s)", dev_nmea, strerror( errno ) ); }
			close_pty();
			pty_fd = -1;
			if( open_gps_port( gps_port_name ) != 0 ) { return -1; }
			if( open_pty() < 0 ) { return -1; }
		}
	}
	else if( result == PKT_VALID )
	{
		gps_data_invalid_count = 0;
	}
	return 0;
}

static void usage( void )
{
	fprintf(stderr, "\nUsage: gps_port_manager [options]\n");
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-d don't detach from controlling terminal (don't daemonise)\n");
	fprintf(stderr, "\t-i instance\n");
	fprintf(stderr, "\t-p GPS port\n");
	fprintf(stderr, "\t-v increase verbosity\n");
	fprintf(stderr, "\t-V version information\n");
	fprintf(stderr, "\n");
	exit( -1 );
}

int main(int argc, char **argv)
{
	int	ret = 0;
	int	verbosity = 0;
	BOOL be_daemon = TRUE;
	const char* instance = "0";

	while( ( ret = getopt( argc, argv, "dvVhi:p:?s:a:" ) ) != EOF )
	{
		switch( ret )
		{
			case 'd': be_daemon = FALSE; break;
			case 'i': instance = optarg; break;
			case 'p': gps_port_name = optarg; break;
			case 'v': ++verbosity ; break;
			case 'V': fprintf( stderr, "%s Version %d.%d.%d\n", argv[0], VER_MJ, VER_MN, VER_BLD ); break;
			case 'a': support_agps_mode = atoi(optarg) ; break;
			case 's': support_sgps_mode = atoi(optarg) ; break;
			case 'h':
			case '?': usage();
			default: break;
		}
	}

	if( !gps_port_name ) { usage(); }
	sprintf( gps_prefix, "sensors.gps.%s", instance );
	openlog(APPLICATION_NAME, LOG_PID | (be_daemon ? 0 : LOG_PERROR), LOG_LOCAL5);
	setlogmask(LOG_UPTO(verbosity + LOG_ERR));
	if( ( ret = init_gps_port_manager( gps_port_name, be_daemon ) ) != 0 ) { SYSLOG_ERR( "fatal: failed to initialize" ); exit( ret ); }
	ret = main_loop();
	shutdown_gps_port_manager();
	SYSLOG_INFO( "exit (%s)", ret == 0 ? "normal" : "failure" );
	SYSLOG_CLOSE;
	exit( ret );
}

/*
* vim:ts=4:sw=4:
*/

