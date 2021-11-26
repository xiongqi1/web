/*!
 * Copyright Notice:
 * Copyright (C) 2002-2009 Call Direct Cellular Solutions Pty. Ltd.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include "ModemEmulator.h"
#include "ModemEmulator_Utils.h"

char rdb_buf[MAX_RDB_VAR_SIZE];

int setUpPort(int argc, char* argv[],int host_port_only);

extern int errno;
extern int alreadyAutoSentCHV1;
extern int user_entered_code;
extern volatile int pastThreadPaused;
extern WWANPARAMS* currentProfile;
extern CONFV250 confv250;
extern FILE* comm_host;
extern FILE* comm_phat;

pthread_t ME_V24_info;
pthread_t ME_Rx_info;
u_long call_terminate_time;
int no_phone = 0;
int currentProfileIx;
pthread_attr_t attr;
extern int keep_alive_cnt;

#ifdef RESTART_PROCESS_VIA_SIGHUP
/* Saved arguments to main(). */
int saved_argc;
char **saved_argv;
#endif

void ModemEmulatorLoopInit(FILE* stream)
{
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 128*1024);
	currentProfileIx = getSingleVal("link.profile.profilenum");
	currentProfile = (WWANPARAMS*)malloc(sizeof(WWANPARAMS));
	CDCS_getWWANProfile(currentProfile, currentProfileIx);
	pthread_create(&ME_Rx_info, &attr, ME_Rx, stream);
	pthread_create(&ME_V24_info, &attr, ME_V24, stream);
}

char* listOKResponses[] =
{
	"OK", "MODE CHANGED", "SPC UNLOCKED", "DIRECTORY NUMBER CHANGED", "MOBILE ID CHANGED", "SPC CHANGED", "CONNECT", NULL
};

char* listVerboseResponses[] =
{
	"0", "1", NULL
};

/*static void sig_handler(int signum)
{
char pos;
	//syslog(LOG_INFO "sig: %u\n", signum );
	pos = getSingleVal("link.profile.modememulator_update");
	switch (signum)
	{
		case SIGHUP:
			switch (pos)
			{
			case 1: //wwan.html
				currentProfileIx = getSingleVal("link.profile.profilenum");
				CDCS_getWWANProfile(currentProfile, currentProfileIx);
				syslog(LOG_INFO, "Set Profile: %u\n", currentProfileIx );
			break;
			case 2:	//v250.html
				CDCS_V250Initialise(comm_host);
			break;
			}
			//CDCS_V250Hangup(comm_host);
		break;
		case SIGINT:
		case SIGTERM:
			setUpPort(0,0,1);
			break;
		case SIGQUIT:
		case SIGKILL:
			fclose(comm_phat);
			usleep(100000);
			fclose(comm_host);
			close(rdbfd);
			exit(0);
		break;
	}
}*/

int checkOKResponse(char* str)
{
	char **p_listOKResponses;
	int i;

	if (confv250.opt_1 &QUIET_ON)
	{
		return 1;
	}
	else if (confv250.opt_1 &VERBOSE_RSLT)
	{
		p_listOKResponses = listOKResponses;
	}
	else
	{
		p_listOKResponses = listVerboseResponses;
	}
	for (i = 0; p_listOKResponses[i] != NULL;i++)
	{
		if (strstr(str, p_listOKResponses[i]))
			return i + 1;
	}
	return 0;
}

int waitSilence(int fd, char* inbuf, unsigned int maxlen, int to)
{
	unsigned cpos = 0;
	struct timeval tv;
	fd_set readset;
	int got, ret, to_cnt = to;;

	while (cpos < maxlen)
	{
		FD_ZERO(&readset);
		FD_SET(fd, &readset);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(fd + 1, &readset, (fd_set*)0, (fd_set*)0, &tv);
		if (ret < 0)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return cpos;
		}
		if (ret == 0) {
			to_cnt--;
			if (to_cnt == 0)
				return cpos;
			else
				continue;
		}
		got = read(fd, &inbuf[cpos], maxlen - cpos);

		if (got < 0)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;
			return cpos;
		}
		if (got == 0)
			return cpos;
		cpos += got;
	}
	return cpos;
}

int SendATCommand(FILE* pPort, char* pReq, char* pResp, int resp_size, int timeout_sec)
{
	char inbuf[PASSTHRUBUFFSIZE], tmpbuf[PASSTHRUBUFFSIZE];
	int got, fd, ret, i;
	int resp_index = 0, first_resp = 1, cmd_len = strlen(pReq);

	// we do not use AT port if there is no phone
	if(no_phone) {
		return OK;
	}

	if (pPort == NULL)
	{
		return NORESP;
	}
    keep_alive_cnt = 0;
	pastThreadPaused++;

	// Set the phone to no echo or echo depending on simple_at_manager running.
	// NOTE - each individual AT command sends a no echo as part of the command since the application
	// does the echoing so we don't want the echo from the phone too. However, we have to set no echo
	// here otherwise the first AT command (i.e ATE0+CSQ?) will be echoed! We don't want this to be echoed.
	//syslog(LOG_DEBUG,"SendATCommand() : --> %s", ECHO_CMD);
	//fputs(ECHO_CMD"\r", pPort);
	//fflush(pPort);
	//CDCS_Sleep(500);
	// drop response on the floor
	// NOTE - we have to do the read to flush out what is in comm_phat otherwise
	// upon doing the next read we will read this response plus the response to the next read.
	fd = fileno(pPort);
	//got = waitSilence(fd, inbuf, sizeof(inbuf), 500);
	if (pResp)
		*pResp = 0;
	*inbuf = 0;  //the inbuf may got the data from ATE0 command, we need to clean the inbuf
	syslog(LOG_DEBUG,"SendATCommand() : --> %s", pReq);
	fputs(pReq, pPort);
	fputs("\r", pPort);
	fflush(pPort);
	while (1)
	{
		got = waitSilence(fd, inbuf, sizeof(inbuf), timeout_sec);
		syslog(LOG_DEBUG,"SendATCommand() : <-- %s, got=%d", inbuf, got);
		if (got == 0)
		{
ret_resp:
			pastThreadPaused--;
			if ((confv250.opt_1 &QUIET_ON) || (strcmp(pReq, "ATQ1") == 0))
				return OK;
			else
			{
				return NORESP;
			}
		}
		if (got < 0)
		{
			pastThreadPaused--;
			return NORESP;
		}

		/* wipe out response string from phone module */
		if (first_resp && strncmp(inbuf, pReq, strlen(pReq)) == 0) {
			syslog(LOG_DEBUG,"SendATCommand() : === found response, wipe out");
			/* skip /r */
			for (i = cmd_len; i < strlen(inbuf) && inbuf[i] == 0x0d; i++);
			cmd_len = i;
			if (got == cmd_len) goto ret_resp;
			(void) memcpy(tmpbuf, inbuf, PASSTHRUBUFFSIZE);
			(void) memset(inbuf, 0x00, PASSTHRUBUFFSIZE);
			(void) memcpy(inbuf, (char *)&tmpbuf[cmd_len], got - cmd_len);
			got -= cmd_len;
		}
		first_resp = 0;

		if (pResp)
		{
			if (resp_index < resp_size && got > 0)
			{
				ret = resp_size - resp_index;
				if (ret > got)
					ret = got;
				memcpy(pResp + resp_index, inbuf, ret);
				resp_index += ret;
			}
		}
		if (checkOKResponse(inbuf))
		{
			pastThreadPaused--;
			return OK;
		}
		pastThreadPaused--;
		return NORESP;
	}
	pastThreadPaused--;
	return NORESP;
}

int SendATCommandRetry(FILE* pPort, char* pReq, char* pResp, int timeout_sec, int retries)
{
	pastThreadPaused++;
	while (retries--)
	{
		if (SendATCommand(pPort, pReq, pResp, AT_RETRY_BUFFSIZE, timeout_sec) == OK)
		{
			pastThreadPaused--;
			return OK;
		}
	}
	pastThreadPaused--;
	return NORESP;
}

// Since phone modules generally default to 115200, but we want them to be 230400 then
// this routine detects that and sets the module and uart to 230400.
// NOTE - baudToSet is the baud rate we want to set the port and phone to.
void SetPhoneBaud(FILE* pPort, speed_t baudToSet)
{
	char i;
	int ret;
	char buf[PASSTHRUBUFFSIZE];
	char pResp[PASSTHRUBUFFSIZE];

	// Try talking to the phone at the current baud rate
	if ((ret = SendATCommandRetry(pPort, "AT", pResp, 2, 3)) != OK)
	{
		fputs("\r\nCannot talk to phone, re-initialising", comm_host);
		fflush(comm_host);

		// Try connecting to the phone at each baud rate.
		// The phone supports the following rates:-
		// 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400
		// 11 In total hence we go through the loop below up to 11 times.
		for (i = 0;i < 11;i++)
		{
			// No we can't lets change the baud rate and try again
			switch (get_serial_port_baud(pPort))
			{
				case B230400:
					cfg_serial_port(pPort, B115200);
					break;
				case B115200:
					cfg_serial_port(pPort, B57600);
					break;
				case B57600:
					cfg_serial_port(pPort, B38400);
					break;
				case B38400:
					cfg_serial_port(pPort, B19200);
					break;
				case B19200:
					cfg_serial_port(pPort, B9600);
					break;
				case B9600:
					cfg_serial_port(pPort, B4800);
					break;
				case B4800:
					cfg_serial_port(pPort, B2400);
					break;
				case B2400:
					cfg_serial_port(pPort, B1200);
					break;
				case B1200:
					cfg_serial_port(pPort, B600);
					break;
				case B600:
					cfg_serial_port(pPort, B300);
					break;
				case B300:
					cfg_serial_port(pPort, B230400);
					break;
			}
			fputs(".", pPort);
			fflush(pPort);

			// Try the current baud rate and see if we can talk to the phone
			if ((ret = SendATCommandRetry(comm_phat, "AT", pResp, 2, 3)) == OK)
			{
				break;
			}
		}
	}
	if (ret != OK)
	{
		// this is not applicable to 821 and 882
		return ;
	}
	// Are we already at the requested rate?
	if (get_serial_port_baud(comm_phat) == baudToSet)
		return ;

	sprintf(buf, "AT+IPR=%d", baud_to_int(baudToSet));
	if (SendATCommand(comm_phat, buf, NULL, 0, 2) == OK)
	{
		cfg_serial_port(comm_phat, baudToSet);
	}
	else
	{
		// this is not applicable to 821 and 882
	}
}

// displays the welcome banner on start up has when requested by the ati command
void displayBanner(void)
{
//#define METERING_PACKET_TEST
#ifdef METERING_PACKET_TEST
	// send test packet and receive from metering device
	unsigned char test_packet[] = { 0xAC, 0x00, 0x00, 0x74, 0xAA };
	int i;
	fd_set readset;
	struct timeval tv;
	int fd;
	char buf[256] = {0,};
	u_long timeout = 5000;
	int retry = 1;
retry_send:
	syslog(LOG_INFO,"send test packet to serial port, retry = %d", retry);
	//for (i = 0; i < 5; i++)	fputc(test_packet[i], comm_host);
	fwrite(test_packet, 1, 5, comm_host);
	fflush(comm_host);
	syslog(LOG_INFO,"wait for response packet from serial port");

	fd = fileno(comm_host);
	while (1)
	{
		FD_ZERO(&readset);
		FD_SET(fd, &readset);
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		// do we have something to read.....
		switch (select(fd + 1, &readset, 0, 0, &tv))
		{
			case - 1:
			case 0:
				if (retry++ >= 5) return;
				else goto retry_send;
				break;
			case 1:
				if ((i = fread(buf, 1, 256, comm_host)) > 0) {
					syslog(LOG_DEBUG,"read %d bytes :", i);
					print_pkt(&buf[0], i);
				}
				break;
		}
	}
#else
	syslog(LOG_DEBUG,"send banner");
	fputs("\r\n", comm_host);
	fprintf(comm_host, getSingle("system.product.title"));
	fputs("\r\n", comm_host);
	fflush(comm_host);
#endif
}

int wait_for_port(const char* port, int timeout_sec)
{
	struct stat port_stat;
	syslog(LOG_DEBUG,"waiting for port '%s' for %d seconds...", port, timeout_sec);
	while (stat(port, &port_stat) < 0)
	{
		if (timeout_sec-- <= 0)
		{
			syslog(LOG_INFO,"waiting for port %s timed out (%s)", port, strerror(errno));
			return -1;
		}
		sleep(1);
	}
	syslog(LOG_DEBUG,"waiting for port '%s' done", port);
	return 0;
}

int is_cinterion_module = 0;

void check_cinterion_module( void )
{
	char pResp[AT_RETRY_BUFFSIZE];

	if ( (SendATCommand(comm_phat, "AT+CGMI", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ) &&
		 (SendATCommand(comm_phat, "AT$CGMI?", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ) &&
		 (SendATCommand(comm_phat, "AT+GMI", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ))
	{
		syslog(LOG_INFO,"CGMI failed\n");
		is_cinterion_module = 0;
		return;
	}
	if (strstr(pResp, "Cinterion"))
		is_cinterion_module = 1;
	else
		is_cinterion_module = 0;
}

//
// Disclaimer : supposed to be the very last fix to the existing modem emulator (honestly!)
// which later on will be replaced by an alternative ME architecture allowing PPP daemon
// running on the device, rather than using module's PPP. It will be integrated with
// the data stream manager.
//
// The intention of this fix is that at least nwl12/6200 with Cinterion modules will work no worse than other
// devices (such as 6908 with Sierra modules).
//
// On 6200, and probably on any platforms using QMI manager, it is not possible to
// use the modem emulator in conjunction with a PPP dial-up client.
// E.g. dial up client thinks that it is connected to PSTN dial-up modem which we
// are emulating.
// This is not often used scenario, but is required by particular customer.
//
// The same thing works more or less with 6908/Sierra combination
//
// Here is why the problem occurs (in addition to other issues that are solved separately and
// deal with incorrect handling of CD line, as well as incorrect RTS/CTS flow control handling):
// on Cinterion modules, we can only write PDP contexts using QMI manager.
// If the PDP context is set up via AT commands, and then we activate that PDP context, the
// thing fails. This is as per technical advice from Cinterion people apparently.
//
// So, to avoid QMI/AT conflicts as above, our existing software works as follows: the Simple AT manager on
// startup deletes all PDP profiles from the module and lets QMI manager to recreate only those profiles
// that are enabled.
// As a result, even if there is a disabled profile (e.g. one of our 6 profiles), it is not
// written to the Cinterion module.
// On the other hand, on Sierra and other non-QMI modules,
// the PDP contexts are NOT deleted from the module, even if the profile is disabled
// So a call can be made by external dial up client using *99***1# of *99#, without
// needing to issue AT+cgdcont= or AT^PASS commands from the connecting client (which is good
// because the client should not know anything about these commands!)
//
// So this function (which only works if it detects Cinterion module)
//  1) Checks existing pdp contexts in the module, and if there are none, then
//      2) Checks that any link.profile.X has at least one non-empty APN, and if there is at least one
//          3) Uses the very first non-empty link.profile.X and copies what is needed to
//              wwan.0.profile.cmd.command structure and the issues "write" command
//              (BUT NOT the "activate" command!!!!!!).
//              The "write" command triggers QMI manager to add a PDP context to the module.
//
// Return values
// 1 - PDP context written successfully to module
// 0 - PDP context is not written, and there is no need
// -1 - any other fail

static int doAdditionalCinterionInitialization(void)
{
    #define _PROFILE_CMD_RDB_ROOT "wwan.0.profile.cmd"
    #define _MAX_PROFILES 6
    char pResp[AT_RETRY_BUFFSIZE];
    char *pos;
    int profile_index;
    char link_profile_rdb[256];

    // if anything smells wrong, exit
    if (!is_cinterion_module) {
        return 0;
    }

    // find out what PDP contexts are already there
    if (SendATCommand(comm_phat, "AT+CGDCONT?", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ) {
        return 0;
    }

    // check if first 2 profiles are empty. This may not be
    // true for all modules/devices, but 6200 with Cinterion
    // is Ok
    if (!strstr(pResp, "+CGDCONT: 1,\"IP\",\"\",\"0.0.0.0\",0,0") ||
        !strstr(pResp, "+CGDCONT: 2,\"IP\",\"\",\"0.0.0.0\",0,0")) {
        return 0;
    }

    // note getSingle NEVER returns a NULL pointer. It always
    // return a pointer to a buffer which may have nothing in it
    // if the RDB variable is either not there, or contains an empty
    // string. In both cases return of getSingle points to a buffer
    // which has 0 in the index 0 (e.g. is null terminated).

    // find the very first non-empty APN
    for (profile_index = 1 ; profile_index <= _MAX_PROFILES ; profile_index++)
    {
        sprintf(link_profile_rdb, "link.profile.%d.apn", profile_index);
        pos = getSingle(link_profile_rdb);
        // check that apn RDB variable is non-empty
        if (*pos) {
            break;
        }
    }

    // nothing in profiles, nothing to do then
    if (profile_index == _MAX_PROFILES) {
        return 0;
    }

    // copy from link.profile.X to command structure
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.apn", pos, 0, 0, 0, 0);

    sprintf(link_profile_rdb, "link.profile.%d.auth_type", profile_index);
    pos = getSingle(link_profile_rdb);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.auth_type", pos, 0, 0, 0, 0);

    sprintf(link_profile_rdb, "link.profile.%d.pass", profile_index);
    pos = getSingle(link_profile_rdb);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.password", pos, 0, 0, 0, 0);

    sprintf(link_profile_rdb, "link.profile.%d.user", profile_index);
    pos = getSingle(link_profile_rdb);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.user",pos, 0, 0, 0, 0);

    // write a few other things, possibly needed
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.data_compression", "", 0, 0, 0, 0);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.header_compression", "", 0, 0, 0, 0);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.ipv4_addr_pref", "0.0.0.0", 0, 0, 0, 0);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.profile_id", "0", 0, 0, 0, 0);
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".param.pdp_type", "ipv4", 0, 0, 0, 0);

    // clear command status
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".status", "", 0, 0, 0, 0);

    // write command request
    rdb_update_single( _PROFILE_CMD_RDB_ROOT".command", "write", 0, 0, 0, 0);

    // wait for command status to be ready
    sleep(2);

    pos = getSingle("wwan.0.profile.cmd.status");
    return strstr(pos, "[done]") ? 1 : -1;
}

int setUpPort(int argc, char* argv[],int host_port_only)
{
	char *pos;
	int i;
	speed_t baud;
	char pResp[AT_RETRY_BUFFSIZE];
	const char esc_cmd_buf[2] = {0x1B, 0x00};
	int ret;

	if(!host_port_only) {
		if(comm_phat)
			fclose(comm_phat);

		phone_at_name=0;

		// override V250 port if the port is given or nophone is given
		if (argc == 2) {
			if (!strcmp("nophone", argv[1])) {
				no_phone = 1;
				syslog(LOG_ERR,"nophone mode is specified");
			}
			else {
				phone_at_name=argv[1];
				syslog(LOG_INFO,"use the given port / not using wwan.0.V250_if.1 - port=%s",phone_at_name);
			}
		}
		else {
			pos=getSingle("wwan.0.V250_if.1");
			if(*pos) {
				phone_at_name=strdup(pos);
				syslog(LOG_INFO,"use V250 port from wwan.0.V250_if.1 - port=%s",phone_at_name);
			}
			else {
				syslog(LOG_INFO,"cannot get V250 port from wwan.0.V250_if.1. use default port - port=%s",phone_port1);
				phone_at_name=phone_port1;
			}
		}

		// wait until the port appears
		if(phone_at_name) {
			wait_for_port(phone_at_name,30);
		}

		if(comm_host)
			fclose(comm_host);
	}

	if (strncmp( getSingleNA("confv250.host_port"), "N/A", 3) != 0)
	{
		host_name = malloc( strlen(getSingle("confv250.host_port")) + 1);
		strcpy( host_name, getSingle("confv250.host_port"));
	}
	else if (strncmp( getSingleNA("wwan.0.host_if.1"), "N/A", 3) == 0)
	{
		host_name = malloc(strlen(host_port) + 1);
		strcpy(host_name, host_port);
	}
	else
	{
		host_name = malloc( strlen(getSingle("wwan.0.host_if.1")) + 1);
		strcpy( host_name, getSingle("wwan.0.host_if.1"));
	}
	printf( "Host-Port:%s --- Module-AT-Port:%s\n", host_name, phone_at_name?phone_at_name:"nophone");

	// open external serial port
	if ( (comm_host = fopen(host_name, "r+") ) == 0)
	{
		syslog(LOG_ERR, "Unable to open host port\n");
		printf( "Unable to open host port: %s\n", host_name);
		return -1;
	}

	// register fd
	InitPhysSerialStat(IND_COUNT_PORT, fileno(comm_host));
	syslog(LOG_DEBUG,"comm_host = %d", fileno(comm_host));

	// Load configuration for V250.
	CDCS_V250Initialise(comm_host);
	if (no_phone)
	{
		printf( "no_phone\n");
	}
	baud = int_to_baud( getSingleVal("confv250.Baud_rate") );
	if( baud == -1 )
	{
		baud = B9600;
		setSingleVal("confv250.Baud_rate", baud);
	}
	cfg_port_details(fileno(comm_host), baud, 0, 1, 0);
	setTTYCharacterFormat(comm_host, getSingleVal( "confv250.Format"), getSingleVal( "confv250.Parity"));
	if (!no_phone)
	{
		// open phone at port
		i=0;
		while ((comm_phat = fopen(phone_at_name, "r+")) == 0)
		{
			if( ++i > 10 )
			{
				printf( "Unable to open phone port %s\n", phone_at_name);
				syslog(LOG_INFO,"Unable to open phone port %s\n", phone_at_name);
				return -1;
			}
			CDCS_Sleep(3000);
		}
		cfg_serial_port(comm_phat, B230400);
		// register channels
		InitPhysSerialStat(IND_AT_PORT, fileno(comm_phat));
		syslog(LOG_DEBUG,"comm_phat = %d", fileno(comm_phat));

		/* First AT command may fail when the module stay in message input mode, then
		 * send ESC to quit and retry */
		if (SendATCommand(comm_phat, "AT", pResp, AT_RETRY_BUFFSIZE, 1)!= OK)
		{
			//printf( "Failed initial AT command, send ESC\n" );
			syslog(LOG_INFO,"Failed initial AT command, send ESC\n" );
			(void) SendATCommand(comm_phat, (char *)&esc_cmd_buf, NULL, AT_RETRY_BUFFSIZE, 1);
		}
		if (SendATCommandRetry(comm_phat, "AT", pResp, 2, 2)!= OK)
		{
			printf( "Unable to configure phone\n" );
			syslog(LOG_INFO,"Unable to configure phone\n");
			return -1;
		}
		checkModemQuietMode(1);
		checkModemVerbose(1);
		checkModemAutoAnswer(1);

		/* Cinterion module, AT&D=0 command should be sent to module for auto answer working */
		check_cinterion_module();
		if (is_cinterion_module) {
			if (SendATCommandRetry(comm_phat, "AT&D0", pResp, 2, 2)!= OK)
			{
				syslog(LOG_ERR,"failed AT&D0 command\n");
				return -1;
			}

			// see comments in the function
			ret = doAdditionalCinterionInitialization();
			switch (ret)
			{
				case 0:
				    syslog(LOG_DEBUG,"Additional Cinterion init not needed");
				    break;
				case 1:
				    syslog(LOG_DEBUG,"Additional Cinterion init success - PDP context added");
				    break;
				default:
				    syslog(LOG_DEBUG,"Additional Cinterion init fail");
				    break;
			}
		}
	}
	return 0;
}

int interrATCmd(FILE* fp, char* cmd, char* resp, unsigned rlen)
{
	char atQhold[512];
	char pResp[AT_RETRY_BUFFSIZE];
	char* cp;

	snprintf(atQhold, sizeof(atQhold), "AT%s?", cmd);
	if (SendATCommand(fp, atQhold, pResp, sizeof(pResp), 2) != OK)
	{
		syslog(LOG_INFO,"interrATCmd: %s does not respond\n", atQhold);
		return 0;
	}
	snprintf(atQhold, sizeof(atQhold), "%s:", cmd);
	cp = strstr(pResp, atQhold);
	if (!cp)
	{
		syslog(LOG_INFO,"interrATCmd: %s does not respond with result\n", cmd);
		return 0;
	}
	if (resp && rlen)
	{
		cp += strlen(atQhold);
		while (*cp && isspace(*cp))
			cp++;
		strncpy(resp, cp, rlen - 1);
		resp[rlen-1] = '\0';
	}
	return 1;
}

void SetATStr(FILE* fp, char* str, int fForce)
{
	char pResp[AT_RETRY_BUFFSIZE];
	int ret;
	if ( getSingleVal("confv250.PassThrough")==0 && !fForce)
		return;

	if ((ret = SendATCommandRetry(fp, str, pResp, 2, 2)) != OK)
	{
		syslog(LOG_INFO,"SetATStr: %s does not respond\n", str);
	}
}

int Hex( char  hex) {
	unsigned char result;
	if(hex>='A' && hex<='F')
		result = hex-('A'-10);
	else if(hex>='a' && hex<='f')
		result = hex-('a'-10);
	else
		result = hex - '0';
	return result;
}

/* latest Sierra modems do not support previous used CNS packet to read model name,
 * read model name with AT command here.
 */
void get_model_name( void )
{
	char pResp[AT_RETRY_BUFFSIZE];
	char myvalue[32];
	int i, j;

	if ( (SendATCommand(comm_phat, "AT+CGMM", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ) &&
		 (SendATCommand(comm_phat, "AT$CGMM?", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ) &&
		 (SendATCommand(comm_phat, "AT+GMM", pResp, AT_RETRY_BUFFSIZE, 1 ) != OK ))
	{
		syslog(LOG_INFO,"CGMM failed\n");
		return;
	}
	(void) memset(&myvalue[0], 0x00, 32);
	/* skip leading CR/LF */
	for (i = 0; i < strlen(pResp) && (pResp[i] == '\n' || pResp[i] == '\r'); i++);
	for (j = 0; i < strlen(pResp) && pResp[i] != '\n' && pResp[i] != '\r'; myvalue[j] = pResp[i], i++, j++);
	rdb_set_single("wwan.0.model", myvalue );
}

#ifdef RESTART_PROCESS_VIA_SIGHUP
static void sighup_handler(int sig)
{
	syslog(LOG_ERR, "Received SIGHUP");
	received_sighup = 1;
	signal(SIGHUP, sighup_handler);

	//This should be in the top of main loop, however usually this process does not hit the top of main loop.
	// Due to this structural fault, place sighup_restart() function here, instead of the top of main loop.
	sighup_restart();
}

void close_all_fd(void)
{
	int fd;

	for (fd = sysconf(_SC_OPEN_MAX) ; fd > 0; --fd)
		close(fd);
}

/*
 * Called from the main loop after receiving SIGHUP.
 * Restarts the process.
 */
void sighup_restart(void)
{
	syslog(LOG_ERR, "Received SIGHUP; restarting %s.", saved_argv[0]);

	close_all_fd();
	signal(SIGHUP, SIG_IGN); /* will be restored after exec */
	execvp(saved_argv[0], saved_argv);
	syslog(LOG_ERR, "RESTART FAILED:(Should use full path) av[0]='%.100s', error: %.100s.", saved_argv[0],
			strerror(errno));
	exit(1);
}
#endif

#if 0
// some test code declared here
extern void LED_Test(void);
#endif

#define GPIO_DEV "/dev/gpio"
int main(int argc, char* argv[])
{
	int i;

	if( rdb_open_db() < 0 )
	{
		syslog(LOG_ERR, "failed to open RDB!" );
		exit (-1);
	}

	if(!getSingleVal("confv250.enable")) {
		syslog(LOG_ERR, "modem emulator deactivated - terminating");
		exit(-1);
	}

	if(gpio_init("/dev/gpio")<0) {
		syslog(LOG_ERR, "gpio_init(/dev/gpio) failed - terminating");
		exit(-1);
	}

	check_logmask_change();

#ifdef RESTART_PROCESS_VIA_SIGHUP
	/* Save argv for HUP signal */
	saved_argc = argc;
	saved_argv = calloc(argc + 1, sizeof(*saved_argv));
	for (i = 0; i < argc; i++)
	{
		saved_argv[i] = strdup(argv[i]);
	}
	saved_argv[i] = NULL;

	signal(SIGHUP, sighup_handler);

	//unblock the SIGHUP signal if it's blocked
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGHUP);
	sigprocmask(SIG_UNBLOCK, &signal_mask, NULL);
#endif

	setUpPort(argc, argv,0);
	// Set initial terminate time
	call_terminate_time = time(0);

	if ((rdb_set_single( "wwan.0.host_if.1", host_name) !=0 ))
	{
		syslog(LOG_INFO,"wwan.0.host_if.1\n");
	}

	get_model_name();

	sleep(2);

	// LED test can be called here if we need to test I/O lines
	//LED_Test();

	ModemEmulatorLoopInit(comm_host);
	displayBanner();
	eatHostPortDataUntilEmpty();
	eatPhonePortDataUntilEmpty();

	// for Cinterion module
	if (is_cinterion_module) {
		syslog(LOG_DEBUG,"send AT to modem");
		send_keep_alive_at();
	}


	ModemEmulatorLoop(comm_host);
	fclose(comm_phat);
	usleep(100000);
	fclose(comm_host);
	rdb_close_db();
	gpio_exit();
	exit(0);
}

/*
 * vim:ts=4:sw=4
         */
