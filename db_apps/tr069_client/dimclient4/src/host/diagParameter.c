/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <arpa/ftp.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <math.h>
#include <stdarg.h>


#include "diagParameter.h"
#include "parameterStore.h"
#include "callback.h"
#include "parameter.h"
#include "globals.h"
#include "utils.h"
#include "filetransfer.h"
#include "parameter.h"
#include "utils.h"
#include "httpda.h"
#include "list.h"
#include "serverdata.h"
#include "ftp_ft.h"
#include "ftp_var.h"
#include "eventcode.h"

#ifdef HAVE_DIAGNOSTICS

#if defined(TR_111_DEVICE)
#define		DownloadDiagnostics_DiagnosticsState			"Device.DownloadDiagnostics.DiagnosticsState"
#define		DownloadDiagnostics_Interface					"Device.DownloadDiagnostics.Interface"
#define		DownloadDiagnostics_DownloadURL					"Device.DownloadDiagnostics.DownloadURL"
#define		DownloadDiagnostics_DSCP						"Device.DownloadDiagnostics.DSCP"
#define		DownloadDiagnostics_EthernetPriority			"Device.DownloadDiagnostics.EthernetPriority"
#define		DownloadDiagnostics_ROMTime						"Device.DownloadDiagnostics.ROMTime"
#define		DownloadDiagnostics_BOMTime						"Device.DownloadDiagnostics.BOMTime"
#define		DownloadDiagnostics_EOMTime						"Device.DownloadDiagnostics.EOMTime"
#define		DownloadDiagnostics_TestBytesReceived			"Device.DownloadDiagnostics.TestBytesReceived"
#define		DownloadDiagnostics_TotalBytesReceived			"Device.DownloadDiagnostics.TotalBytesReceived"
#define		DownloadDiagnostics_TCPOpenRequestTime			"Device.DownloadDiagnostics.TCPOpenRequestTime"
#define		DownloadDiagnostics_TCPOpenResponseTime			"Device.DownloadDiagnostics.TCPOpenResponseTime"

#define		UploadDiagnostics_DiagnosticsState				"Device.UploadDiagnostics.DiagnosticsState"
#define		UploadDiagnostics_Interface						"Device.UploadDiagnostics.Interface"
#define		UploadDiagnostics_UploadURL						"Device.UploadDiagnostics.UploadURL"
#define		UploadDiagnostics_DSCP							"Device.UploadDiagnostics.DSCP"
#define		UploadDiagnostics_EthernetPriority				"Device.UploadDiagnostics.EthernetPriority"
#define		UploadDiagnostics_TestFileLength				"Device.UploadDiagnostics.TestFileLength"
#define		UploadDiagnostics_ROMTime						"Device.UploadDiagnostics.ROMTime"
#define		UploadDiagnostics_BOMTime						"Device.UploadDiagnostics.BOMTime"
#define		UploadDiagnostics_EOMTime						"Device.UploadDiagnostics.EOMTime"
#define		UploadDiagnostics_TotalBytesSent				"Device.UploadDiagnostics.TotalBytesSent"
#define		UploadDiagnostics_TCPOpenRequestTime			"Device.UploadDiagnostics.TCPOpenRequestTime"
#define		UploadDiagnostics_TCPOpenResponseTime			"Device.UploadDiagnostics.TCPOpenResponseTime"

#define		UDPEchoConfig_Enable							"Device.UDPEchoConfig.Enable"
#define		UDPEchoConfig_Interface							"Device.UDPEchoConfig.Interface"
#define		UDPEchoConfig_SourceIPAddress					"Device.UDPEchoConfig.SourceIPAddress"
#define		UDPEchoConfig_UDPPort							"Device.UDPEchoConfig.UDPPort"
#define		UDPEchoConfig_EchoPlusEnabled					"Device.UDPEchoConfig.EchoPlusEnabled"
#define		UDPEchoConfig_EchoPlusSupported					"Device.UDPEchoConfig.EchoPlusSupported"
#define		UDPEchoConfig_PacketsReceived					"Device.UDPEchoConfig.PacketsReceived"
#define		UDPEchoConfig_PacketsResponded					"Device.UDPEchoConfig.PacketsResponded"
#define		UDPEchoConfig_BytesReceived						"Device.UDPEchoConfig.BytesReceived"
#define		UDPEchoConfig_BytesResponded					"Device.UDPEchoConfig.BytesResponded"
#define		UDPEchoConfig_TimeFirstPacketReceived			"Device.UDPEchoConfig.TimeFirstPacketReceived"
#define		UDPEchoConfig_TimeLastPacketReceived			"Device.UDPEchoConfig.TimeLastPacketReceived"

#define		IPPingDiagnostics_DiagnosticsState				"Device.LAN.IPPingDiagnostics.DiagnosticsState"
#define		IPPingDiagnostics_Interface						"Device.LAN.IPPingDiagnostics.Interface"
#define		IPPingDiagnostics_Host							"Device.LAN.IPPingDiagnostics.Host"
#define		IPPingDiagnostics_NumberOfRepetitions			"Device.LAN.IPPingDiagnostics.NumberOfRepetitions"
#define		IPPingDiagnostics_Timeout						"Device.LAN.IPPingDiagnostics.Timeout"
#define		IPPingDiagnostics_DataBlockSize					"Device.LAN.IPPingDiagnostics.DataBlockSize"
#define		IPPingDiagnostics_DSCP							"Device.LAN.IPPingDiagnostics.DSCP"
#define		IPPingDiagnostics_SuccessCount					"Device.LAN.IPPingDiagnostics.SuccessCount"
#define		IPPingDiagnostics_FailureCount					"Device.LAN.IPPingDiagnostics.FailureCount"
#define		IPPingDiagnostics_AverageResponseTime			"Device.LAN.IPPingDiagnostics.AverageResponseTime"
#define		IPPingDiagnostics_MinimumResponseTime			"Device.LAN.IPPingDiagnostics.MinimumResponseTime"
#define		IPPingDiagnostics_MaximumResponseTime			"Device.LAN.IPPingDiagnostics.MaximumResponseTime"

#define		TraceRouteDiagnostics_DiagnosticsState			"Device.TraceRouteDiagnostics.DiagnosticsState"
#define		TraceRouteDiagnostics_Interface					"Device.TraceRouteDiagnostics.Interface"
#define		TraceRouteDiagnostics_Host						"Device.TraceRouteDiagnostics.Host"
#define		TraceRouteDiagnostics_NumberOfTries				"Device.TraceRouteDiagnostics.NumberOfTries"
#define		TraceRouteDiagnostics_Timeout					"Device.TraceRouteDiagnostics.Timeout"
#define		TraceRouteDiagnostics_DataBlockSize				"Device.TraceRouteDiagnostics.DataBlockSize"
#define		TraceRouteDiagnostics_DSCP						"Device.TraceRouteDiagnostics.DSCP"
#define		TraceRouteDiagnostics_MaxHopCount				"Device.TraceRouteDiagnostics.MaxHopCount"
#define		TraceRouteDiagnostics_ResponseTime				"Device.TraceRouteDiagnostics.ResponseTime"
#define		TraceRouteDiagnostics_RouteHopsNumberOfEntries	"Device.TraceRouteDiagnostics.RouteHopsNumberOfEntries"

#else
#define		DownloadDiagnostics_DiagnosticsState			"InternetGatewayDevice.DownloadDiagnostics.DiagnosticsState"
#define		DownloadDiagnostics_Interface					"InternetGatewayDevice.DownloadDiagnostics.Interface"
#define		DownloadDiagnostics_DownloadURL					"InternetGatewayDevice.DownloadDiagnostics.DownloadURL"
#define		DownloadDiagnostics_DSCP						"InternetGatewayDevice.DownloadDiagnostics.DSCP"
#define		DownloadDiagnostics_EthernetPriority			"InternetGatewayDevice.DownloadDiagnostics.EthernetPriority"
#define		DownloadDiagnostics_ROMTime						"InternetGatewayDevice.DownloadDiagnostics.ROMTime"
#define		DownloadDiagnostics_BOMTime						"InternetGatewayDevice.DownloadDiagnostics.BOMTime"
#define		DownloadDiagnostics_EOMTime						"InternetGatewayDevice.DownloadDiagnostics.EOMTime"
#define		DownloadDiagnostics_TestBytesReceived			"InternetGatewayDevice.DownloadDiagnostics.TestBytesReceived"
#define		DownloadDiagnostics_TotalBytesReceived			"InternetGatewayDevice.DownloadDiagnostics.TotalBytesReceived"
#define		DownloadDiagnostics_TCPOpenRequestTime			"InternetGatewayDevice.DownloadDiagnostics.TCPOpenRequestTime"
#define		DownloadDiagnostics_TCPOpenResponseTime			"InternetGatewayDevice.DownloadDiagnostics.TCPOpenResponseTime"

#define		UploadDiagnostics_DiagnosticsState				"InternetGatewayDevice.UploadDiagnostics.DiagnosticsState"
#define		UploadDiagnostics_Interface						"InternetGatewayDevice.UploadDiagnostics.Interface"
#define		UploadDiagnostics_UploadURL						"InternetGatewayDevice.UploadDiagnostics.UploadURL"
#define		UploadDiagnostics_DSCP							"InternetGatewayDevice.UploadDiagnostics.DSCP"
#define		UploadDiagnostics_EthernetPriority				"InternetGatewayDevice.UploadDiagnostics.EthernetPriority"
#define		UploadDiagnostics_TestFileLength				"InternetGatewayDevice.UploadDiagnostics.TestFileLength"
#define		UploadDiagnostics_ROMTime						"InternetGatewayDevice.UploadDiagnostics.ROMTime"
#define		UploadDiagnostics_BOMTime						"InternetGatewayDevice.UploadDiagnostics.BOMTime"
#define		UploadDiagnostics_EOMTime						"InternetGatewayDevice.UploadDiagnostics.EOMTime"
#define		UploadDiagnostics_TotalBytesSent				"InternetGatewayDevice.UploadDiagnostics.TotalBytesSent"
#define		UploadDiagnostics_TCPOpenRequestTime			"InternetGatewayDevice.UploadDiagnostics.TCPOpenRequestTime"
#define		UploadDiagnostics_TCPOpenResponseTime			"InternetGatewayDevice.UploadDiagnostics.TCPOpenResponseTime"

#define		UDPEchoConfig_Enable							"InternetGatewayDevice.UDPEchoConfig.Enable"
#define		UDPEchoConfig_Interface							"InternetGatewayDevice.UDPEchoConfig.Interface"
#define		UDPEchoConfig_SourceIPAddress					"InternetGatewayDevice.UDPEchoConfig.SourceIPAddress"
#define		UDPEchoConfig_UDPPort							"InternetGatewayDevice.UDPEchoConfig.UDPPort"
#define		UDPEchoConfig_EchoPlusEnabled					"InternetGatewayDevice.UDPEchoConfig.EchoPlusEnabled"
#define		UDPEchoConfig_EchoPlusSupported					"InternetGatewayDevice.UDPEchoConfig.EchoPlusSupported"
#define		UDPEchoConfig_PacketsReceived					"InternetGatewayDevice.UDPEchoConfig.PacketsReceived"
#define		UDPEchoConfig_PacketsResponded					"InternetGatewayDevice.UDPEchoConfig.PacketsResponded"
#define		UDPEchoConfig_BytesReceived						"InternetGatewayDevice.UDPEchoConfig.BytesReceived"
#define		UDPEchoConfig_BytesResponded					"InternetGatewayDevice.UDPEchoConfig.BytesResponded"
#define		UDPEchoConfig_TimeFirstPacketReceived			"InternetGatewayDevice.UDPEchoConfig.TimeFirstPacketReceived"
#define		UDPEchoConfig_TimeLastPacketReceived			"InternetGatewayDevice.UDPEchoConfig.TimeLastPacketReceived"

#define		IPPingDiagnostics_DiagnosticsState				"InternetGatewayDevice.IPPingDiagnostics.DiagnosticsState"
#define		IPPingDiagnostics_Interface						"InternetGatewayDevice.IPPingDiagnostics.Interface"
#define		IPPingDiagnostics_Host							"InternetGatewayDevice.IPPingDiagnostics.Host"
#define		IPPingDiagnostics_NumberOfRepetitions			"InternetGatewayDevice.IPPingDiagnostics.NumberOfRepetitions"
#define		IPPingDiagnostics_Timeout						"InternetGatewayDevice.IPPingDiagnostics.Timeout"
#define		IPPingDiagnostics_DataBlockSize					"InternetGatewayDevice.IPPingDiagnostics.DataBlockSize"
#define		IPPingDiagnostics_DSCP							"InternetGatewayDevice.IPPingDiagnostics.DSCP"
#define		IPPingDiagnostics_SuccessCount					"InternetGatewayDevice.IPPingDiagnostics.SuccessCount"
#define		IPPingDiagnostics_FailureCount					"InternetGatewayDevice.IPPingDiagnostics.FailureCount"
#define		IPPingDiagnostics_AverageResponseTime			"InternetGatewayDevice.IPPingDiagnostics.AverageResponseTime"
#define		IPPingDiagnostics_MinimumResponseTime			"InternetGatewayDevice.IPPingDiagnostics.MinimumResponseTime"
#define		IPPingDiagnostics_MaximumResponseTime			"InternetGatewayDevice.IPPingDiagnostics.MaximumResponseTime"

#define		TraceRouteDiagnostics_DiagnosticsState			"InternetGatewayDevice.TraceRouteDiagnostics.DiagnosticsState"
#define		TraceRouteDiagnostics_Interface					"InternetGatewayDevice.TraceRouteDiagnostics.Interface"
#define		TraceRouteDiagnostics_Host						"InternetGatewayDevice.TraceRouteDiagnostics.Host"
#define		TraceRouteDiagnostics_NumberOfTries				"InternetGatewayDevice.TraceRouteDiagnostics.NumberOfTries"
#define		TraceRouteDiagnostics_Timeout					"InternetGatewayDevice.TraceRouteDiagnostics.Timeout"
#define		TraceRouteDiagnostics_DataBlockSize				"InternetGatewayDevice.TraceRouteDiagnostics.DataBlockSize"
#define		TraceRouteDiagnostics_DSCP						"InternetGatewayDevice.TraceRouteDiagnostics.DSCP"
#define		TraceRouteDiagnostics_MaxHopCount				"InternetGatewayDevice.TraceRouteDiagnostics.MaxHopCount"
#define		TraceRouteDiagnostics_ResponseTime				"InternetGatewayDevice.TraceRouteDiagnostics.ResponseTime"
#define		TraceRouteDiagnostics_RouteHopsNumberOfEntries	"InternetGatewayDevice.TraceRouteDiagnostics.RouteHopsNumberOfEntries"

#endif

#define			DiagnosticsState_None								"None"
#define			DiagnosticsState_Requested							"Requested"
#define			DiagnosticsState_Completed							"Completed"
#define			DiagnosticsState_Error_InitConnectionFailed			"Error_InitConnectionFailed"
#define			DiagnosticsState_Error_NoResponse					"Error_NoResponse"
#define			DiagnosticsState_Error_TransferFailed				"Error_TransferFailed"
#define			DiagnosticsState_Error_PasswordRequestFailed		"Error_PasswordRequestFailed"
#define			DiagnosticsState_Error_LoginFailed					"Error_LoginFailed"
#define			DiagnosticsState_Error_NoTransferMode				"Error_NoTransferMode"
#define			DiagnosticsState_Error_NoPASV						"Error_NoPASV"
#define			DiagnosticsState_Error_IncorrectSize				"Error_IncorrectSize"
#define			DiagnosticsState_Error_Timeout						"Error_Timeout"
#define			DiagnosticsState_Error_NoCWD						"Error_NoCWD"
#define			DiagnosticsState_Error_NoSTOR						"Error_NoSTOR"


#ifdef HAVE_FILE

#if defined(PLATFORM_PLATYPUS)
#define		HTTP_DOWNLOAD_DIAGNOSTICS_FILE	"/var/tr069/HTTP_DOWNLOAD_DIAGNOSTICS_FILE"
#define		FTP_DOWNLOAD_DIAGNOSTICS_FILE	"/var/tr069/FTP_DOWNLOAD_DIAGNOSTICS_FILE"
#else
#define		HTTP_DOWNLOAD_DIAGNOSTICS_FILE	"HTTP_DOWNLOAD_DIAGNOSTICS_FILE"
#define		FTP_DOWNLOAD_DIAGNOSTICS_FILE	"FTP_DOWNLOAD_DIAGNOSTICS_FILE"
#endif

static	struct	types {
	const char *t_name;
	const char *t_mode;
	int t_type;
	const char *t_arg;
} types[] = {
	{ "ascii",	"A",	TYPE_A,	NULL },
	{ "binary",	"I",	TYPE_I,	NULL },
	{ "image",	"I",	TYPE_I,	NULL },
	{ "ebcdic",	"E",	TYPE_E,	NULL },
	{ "tenex",	"L",	TYPE_L,	bytename },
	{ NULL, NULL, 0, NULL }
};

static char strTestBytesReceived[25];
static char strTotalBytesReceived[25];
static char strTotalBytesSent[25];


static int SetTimeParam( const char * );
static	void _do_settype( const char * );
static	int _doLogin( const char *, const char *, const char * );
static	int _ftp_login( char *, const char *, const char * );

int SetSmartParam(const char *parameterPath, void *value)
{
	bool	notifyTmp;

	return	setParameter2Host( parameterPath, &notifyTmp, value );
}

static int SetTimeParam( const char *parameterPath )
{
#if defined(PLATFORM_PLATYPUS)
	char new_value[32] = {'\0'};
	bool	notifyTmp=false;
	struct timeval tv;
	struct timezone tz;
	struct tm *tm;
	gettimeofday(&tv, &tz);
	tm=gmtime(&tv.tv_sec);

	sprintf(new_value, "%4d-%02d-%02dT%02d:%02d:%02d.%06d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);

	return setParameter2Host( parameterPath, &notifyTmp, new_value );
#else
	bool	notifyTmp;
	time_t	t = time(NULL);

	return	setParameter2Host( parameterPath, &notifyTmp, (char *)&t );
#endif
}

/*
 * Set transfer type.
 */
static	void
_do_settype(const char *thetype)
{
	struct types *p;
	int comret;

	for (p = types; p->t_name; p++)
		if (strcmp(thetype, p->t_name) == 0)
			break;
	if (p->t_name == 0) {
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "_do_settype: %s - unknown mode\n", thetype);
		)

		code = -1;
		return;
	}
	if ((p->t_arg != NULL) && (*(p->t_arg) != '\0'))
		comret = command("TYPE %s %s", p->t_mode, p->t_arg);
	else
		comret = command("TYPE %s", p->t_mode);
	if (comret == COMPLETE) {
		(void) strcpy(typename, p->t_name);
		curtype = type = p->t_type;
	}
}

/** Connect to a host with username and password
*/
static	int
_doLogin( const char *host, const char *user, const char *pass )
{
	int n;
	char *nHost;
	unsigned short port;

	port = ftp_port;

	nHost = hookup(host, port);
	if ( nHost ) {
		// set username
		if ( !*user )
			user = "anonymous";
		n = command("USER %s", user);
		if (n != CONTINUE) {
			SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_PasswordRequestFailed );
			return (0);
		}
		if (n == CONTINUE) {
			// set password
			if ( !*pass )
				pass = "guest";
			n = command("PASS %s", pass);
		}
		if (n != COMPLETE) {
			SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_LoginFailed );
			return (0);
		}
		_do_settype( "binary" );
		return (1);
	} else {
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_InitConnectionFailed );
		return (0);
	}
}

static	int
_ftp_login(char *hostname, const char *username, const char *password )
{
	ftp_port = htons(21);
	passivemode = 0;

	cpend = 0;	//* no pending replies
	proxy = 0;	//* proxy not active
	sendport = -1;	//* not using ports
	/*
	 * Connect to the remote server
	 */
	connected = _doLogin( hostname, username, password);
	return ( connected == 1 ? OK : ERR_DOWNLOAD_FAILURE );
}

#endif /* HAVE_FILE */

#endif /* HAVE_DIAGNOSTICS */

#ifdef HAVE_DOWNLOAD_DIAGNOSTICS

static int DiagDoHttpDownload (char *, char *, char *, int, char *);
static	int _dimget( char *, char *, long * );
static	int _ftp_get( char *, char *, long * );
static int DiagDoFtpDownload (char *, char *, char *, int, char *);

static int
DiagDoHttpDownload (char *URL, char *username, char *password, int bufferSize, char *targetFileName)
{
	int ret = OK;
	int readSize = 0;

	char *inpPtr;
	int cnt = 0;
	int fd = 0;
	bool isError = false;

	// get a soap structure and use it's buf for input
	struct soap soap;
	struct soap *tmpSoap;

	soap_init (&soap);
	tmpSoap = &soap;

	soap_begin (tmpSoap);
	tmpSoap->recv_timeout = 30;	// Timeout after 30 seconds stall on recv
	tmpSoap->send_timeout = 60;	// Timeout after 1 minute stall on send
	tmpSoap->keep_alive = 1;
	tmpSoap->status = SOAP_GET;
	tmpSoap->authrealm = "";

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "DiagHttpDownloadGet: %s Host: %s Port: %d\n", URL, tmpSoap->host, tmpSoap->port);
	)

	SetTimeParam( DownloadDiagnostics_TCPOpenRequestTime );
	if (make_connect(tmpSoap, URL)
	    || tmpSoap->fpost (tmpSoap, URL, "", 0, tmpSoap->path, "", 0))
	{
		ret = tmpSoap->error;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Request send %d %d\n", ret, tmpSoap->errmode);
		)
		soap_end (tmpSoap);
		soap_done (tmpSoap);
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_InitConnectionFailed );

		return DIAG_ERROR;
	}

	SetTimeParam( DownloadDiagnostics_TCPOpenResponseTime );
	SetTimeParam( DownloadDiagnostics_ROMTime );

	// Parse the HTML response header and remove it from the work data
	ret = tmpSoap->fparse (tmpSoap);

	if ( ret != OK )
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Request %d\n", ret);
		)
		soap_closesock (tmpSoap);
		soap_destroy (tmpSoap);
		soap_end (tmpSoap);
		soap_done (tmpSoap);
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_NoResponse );
		return DIAG_ERROR;
	}

	SetTimeParam( DownloadDiagnostics_BOMTime );
	unsigned int uintTotalBytesReceived = tmpSoap->buflen;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Request 2 %d endpoint: %s\n", ret, tmpSoap->endpoint);
	)

	inpPtr = tmpSoap->buf;

	// open the output file, must be given with the complete path
	if ((fd = open (targetFileName, O_RDWR | O_CREAT, 0777)) < 0)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Can't open File: %s", targetFileName);
		)
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_TransferFailed );
		return DIAG_ERROR;
	}

	// the first part of the data is already in the buffer,
	// Therefore calculate the offset and write it to the output file
	// bufidx is the index to the first data after the HTML header
	cnt = soap.buflen - soap.bufidx;
	inpPtr = soap.buf;
	inpPtr += soap.bufidx;
	if (write (fd, inpPtr, cnt) < 0)
		isError = true;

	// Get the rest of the data
	while (isError == false
	       && (readSize =
		   tmpSoap->frecv (tmpSoap, inpPtr, SOAP_BUFLEN)) > 0)
	{
		if (write (fd, inpPtr, readSize) < 0)
		{
			isError = true;
			break;
		}
		cnt += readSize;
	}

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "BuffSize: %d  read: %d  soap->err: %d\n", bufferSize, cnt, tmpSoap->errnum);
	)
	if (close (fd) < 0)
	{
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_TransferFailed );
		return DIAG_ERROR;
	}

	SetTimeParam( DownloadDiagnostics_EOMTime );

	sprintf( strTestBytesReceived, "%d", cnt);
	sprintf( strTotalBytesReceived, "%d", cnt + uintTotalBytesReceived);

	SetSmartParam( DownloadDiagnostics_TestBytesReceived, strTestBytesReceived );
	SetSmartParam( DownloadDiagnostics_TotalBytesReceived, strTotalBytesReceived );

	soap_closesock (tmpSoap);
	soap_destroy (tmpSoap);
	soap_end (tmpSoap);
	soap_done (tmpSoap);

	// Check if we got all the data we expected
	if (cnt == 0)
	{
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_TransferFailed );
		return DIAG_ERROR;
	}
	else
	{
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Completed );
		return OK;
	}
}

static	int
_dimget( char *remoteFile, char *localFile, long *size )
{
	return recvrequest("RETR", localFile, remoteFile, "w", size );
}

static	int
_ftp_get( char *remoteFile, char *localFile, long *size )
{
	*size = 0;
	return _dimget( remoteFile, localFile, size );
}

static int
DiagDoFtpDownload (char *URL, char *username, char *password, int bufferSize, char *targetFileName)
{
	// extract remote host and remote filename from URL
	// Only ftp://hostname[:port]/dir/dir/remotename  is allowed
	// the remote name must be relative to the ftp directory
	//
	char *tmp;
	char *host;
	char *remoteFileName;
	unsigned int size;
	int	ret = 0;

	// skip scheme "ftp://"
	host = URL + 6; // strstr( URL, "://" );
	tmp = host;
	while ( *tmp && *tmp != ':' && *tmp != '/' ) {
		tmp++;
	}
	*tmp = '\0';
	tmp++;
	remoteFileName = tmp;

	if ( _ftp_login( host, username, password ) != OK )
		return DIAG_ERROR;

	SetTimeParam( DownloadDiagnostics_TCPOpenRequestTime );
	SetTimeParam( DownloadDiagnostics_TCPOpenResponseTime );
	SetTimeParam( DownloadDiagnostics_ROMTime );
	SetTimeParam( DownloadDiagnostics_BOMTime );

	ret = _ftp_get( remoteFileName, targetFileName, (long *)&size );
	if ( ret != OK )
	{
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_NoTransferMode );
		return DIAG_ERROR;
	}
	else
	{
		SetTimeParam( DownloadDiagnostics_EOMTime );
		sprintf( strTestBytesReceived, "%d", size );
		sprintf( strTotalBytesReceived, "%d", size );
		SetSmartParam( DownloadDiagnostics_TestBytesReceived, strTestBytesReceived );
		SetSmartParam( DownloadDiagnostics_TotalBytesReceived, strTotalBytesReceived );
		SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Completed );
	}
	if ( size < 0 || ( bufferSize != 0 && size != bufferSize))
		return DIAG_ERROR;

	ftp_disconnect();

	return OK;
}

#endif /* HAVE_DOWNLOAD_DIAGNOSTICS */

#ifdef HAVE_UPLOAD_DIAGNOSTICS

static	int data = -1;
static sigjmp_buf sendabort;
static int abrtflag = 0;

extern FILE *dataconn( const char * );

static int DiagSendFile( struct soap *, const UploadFile *, long );
static int DiagUploadFile( struct soap *, const UploadFile *, long );
static int DiagDoHttpUpload ( char *, char *, char *, long );
static void abortsend( int );
static int _sendrequest( const char *, char *, char *, long * );
static	int _dimput( char *, char *, long * );
static	int _ftp_put( char *, char *, long * );
static int DiagDoFtpUpload (char *, char *, char *, long );

static int DiagSendFile( struct soap *tmpSoap, const UploadFile *srcFile, long fileSize )
{
	int	i = 0;

	while(i < fileSize)
		tmpSoap->tmpbuf[i++] = 'a';
	tmpSoap->tmpbuf[i++] = '\n';
	tmpSoap->tmpbuf[i] = '\0';
	return	soap_send_raw( tmpSoap, tmpSoap->tmpbuf, i );
}

static int DiagUploadFile( struct soap *tmpSoap, const UploadFile *srcFile, long fileSize )
{
	long contentLength;
	char fileSizeStr[20];
	char boundary[20];

	SetTimeParam( UploadDiagnostics_ROMTime );

	sprintf (boundary, "%ld", getTime ());
	sprintf (tmpSoap->tmpbuf, "POST %s HTTP/1.1\r\n", tmpSoap->path);
	soap_send( tmpSoap, tmpSoap->tmpbuf );
	sprintf (tmpSoap->tmpbuf, "%s:%d", tmpSoap->host, tmpSoap->port);
	tmpSoap->fposthdr (tmpSoap, "Host", tmpSoap->tmpbuf);

	tmpSoap->fposthdr (tmpSoap, "User-Agent", "DimarkCPE");
	tmpSoap->fposthdr (tmpSoap, "Connection",
			   tmpSoap->keep_alive ? "keep_alive" : "close");
	sprintf (tmpSoap->tmpbuf, "multipart/form-data; boundary=%s", boundary);
	tmpSoap->fposthdr (tmpSoap, "Content-Type", tmpSoap->tmpbuf);
	sprintf (tmpSoap->tmpbuf,
				 "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"
				 "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"
				 "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
				 boundary, "MAX_FILE_SIZE", "5000000",
				 boundary, "upload", "1",
				 boundary, "userfile", srcFile->filepath, srcFile->filetype );

	// Calc contentlength
	// contentlength = filesize + 2 * boundary + CType + CDisposition + "--\r\n"
	contentLength = strlen(tmpSoap->tmpbuf) + fileSize + strlen(boundary) + 4;
	sprintf (fileSizeStr, "%ld", contentLength);
	tmpSoap->fposthdr (tmpSoap, "Content-Length", fileSizeStr);
	soap_send( tmpSoap, "\r\n" );
	soap_send( tmpSoap, tmpSoap->tmpbuf );

	SetTimeParam( UploadDiagnostics_BOMTime );

	if ( DiagSendFile( tmpSoap, srcFile, fileSize ) != OK ) {
		soap_end( tmpSoap );
		soap_done( tmpSoap );
		SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Error_NoResponse );
		return DIAG_ERROR;
	}

	soap_send( tmpSoap, "--" );
	soap_send( tmpSoap, boundary );
	soap_send( tmpSoap, "--\r\n" );

	return OK;
}

static int
DiagDoHttpUpload ( char *URL, char *username, char *password, long fileSize )
{
	int ret = 0;
	const UploadFile *srcFile;
	unsigned int size;
	// get a soap structure and use it's buf for input
	struct soap soap;
	struct soap *tmpSoap;

	srcFile = type2file ("2 Vendor Log File", &size);

	soap_init (&soap);
	tmpSoap = &soap;

	tmpSoap->keep_alive = 1;
	tmpSoap->recv_timeout = 30;	// Timeout after 30 seconds stall on recv
	tmpSoap->send_timeout = 60;	// Timeout after 1 minute stall on send

	soap_set_endpoint(tmpSoap, URL);

	SetTimeParam( UploadDiagnostics_TCPOpenRequestTime );

	tmpSoap->socket = tmpSoap->fopen(tmpSoap, URL, tmpSoap->host, tmpSoap->port);
    if (tmpSoap->error)
    {
		SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Error_InitConnectionFailed );
		return DIAG_ERROR;
    }

	SetTimeParam( UploadDiagnostics_TCPOpenResponseTime );

	ret = DiagUploadFile( tmpSoap, srcFile, fileSize );

	if( ret != OK )
		return DIAG_ERROR;

	// Parse the HTML response header and remove it from the work data
    ret = tmpSoap->fparse (tmpSoap);
    if (ret)
    {
		SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Error_NoResponse );
		return DIAG_ERROR;
    }

   	SetTimeParam( UploadDiagnostics_EOMTime );

   	sprintf( strTotalBytesSent, "%d", (unsigned int)tmpSoap->buflen );
   	SetSmartParam( UploadDiagnostics_TotalBytesSent, strTotalBytesSent );

	SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Completed );
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Request 2 %d endpoint: %s\n", ret, tmpSoap->endpoint);
	)

	return OK;
}

static void
abortsend(int ignore)
{
	(void)ignore;

	mflag = 0;
	abrtflag = 0;
	// printf("\nsend aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
	siglongjmp(sendabort, 1);
}

/** Send a file from local filename to remote
 * no restart support
 *
 * @return errorcode 	0 = OK , > 1 = Error
 */
static int
_sendrequest(const char *cmd, char *local, char *remote, long *size )
{
    register int d;
    FILE *volatile dout = 0;
    int (*volatile closefunc)(FILE *);
    void (*volatile oldintr)(int);
    void (*volatile oldintp)(int);
    volatile long bytes = 0;
    char buf[BUFSIZ];
    const char *volatile lmode;

	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	lmode = "w";
	if (sigsetjmp(sendabort, 1)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT,oldintr);
		if (oldintp)
			(void) signal(SIGPIPE,oldintp);
		code = -1;
		return 1;
	}
	oldintr = signal(SIGINT, abortsend);
	closefunc = fclose;

	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;

		return 1;
	}
	if (sigsetjmp(sendabort, 1))
		goto abort;

	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);

			return 1;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);

			return 1;
		}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;
	oldintp = signal(SIGPIPE, SIG_IGN);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		long	i = *size;
		buf[0] = 'a';

		while( i-- )
			if ((d = write(fileno(dout), buf, 1)) <= 0)
				break;

		if (d < 0) {
			if (errno != EPIPE)
				;
			bytes = -1;
		}
		break;
	}

	(void) fclose(dout);
	/* closes data as well, so discard it */
	data = -1;
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	return 0;
abort:
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return 1;
	}
	if (dout) {
		(void) fclose(dout);
	}
	if (data >= 0) {
		/* if it just got closed with dout, again won't hurt */
		(void) close(data);
		data = -1;
	}
	(void) getreply(0);HAVE_DIAGNOSTICS
	code = -1;

	return 1;
}

/** Put a file to the remote host.
 *
 * \param remoteFile	remotePathname relative to ftp home or absolute
 * \param localFile		localPathname relative or better absolute
 * \return errorcode
 */
static	int
_dimput( char *remoteFile, char *localFile, long *size )
{
	return _sendrequest("STOR", localFile, remoteFile, size );
}

static	int
_ftp_put( char *remoteFile, char *localFile, long *size )
{
	return _dimput( remoteFile, localFile, size );
}

static int
DiagDoFtpUpload (char *URL, char *username, char *password, long fileSize )
{
	// extract remote host and remote filename from URL
	// Only ftp://hostname[:port]/dir/dir/remotename  is allowed
	// the remote name must be relative to the ftp directory
	// If no remote name is found in the URL the srcFilename is used.
	// Attention: the complete srcFilename including path is used.

	int ret = OK;
	char *tmp;
	char *host;
	char *remoteFileName;
	unsigned int size;
	const UploadFile *srcFile;

	srcFile = type2file ("2 Vendor Log File", &size);

	// skip scheme "ftp://"
	host = URL + 6; // strstr( URL, "://" );
	tmp = host;
	while ( *tmp && *tmp != ':' && *tmp != '/' ) {
		tmp++;
	}
	*tmp = '\0';
	tmp++;
	if ( strlen( tmp ) == 0 )
		remoteFileName = srcFile->filename;
	else
		remoteFileName = tmp;
	if ( _ftp_login( host, username, password ) != OK )
		return DIAG_ERROR;

	SetTimeParam( UploadDiagnostics_TCPOpenRequestTime );
	SetTimeParam( UploadDiagnostics_TCPOpenResponseTime );
	SetTimeParam( UploadDiagnostics_ROMTime );
	SetTimeParam( UploadDiagnostics_BOMTime );

	ret = _ftp_put( remoteFileName, srcFile->filepath, &fileSize );
	if ( ret != OK ) {
		ftp_disconnect();
		SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Error_NoTransferMode );
		return DIAG_ERROR;
	}

	SetTimeParam( UploadDiagnostics_EOMTime );
	sprintf( strTotalBytesSent, "%ld", fileSize );
	SetSmartParam( UploadDiagnostics_TotalBytesSent, strTotalBytesSent );
	SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Completed );
	ftp_disconnect();

	return OK;
}

#endif /* HAVE_UPLOAD_DIAGNOSTICS */

#ifdef HAVE_UDP_ECHO

extern	int procId;
extern	Func getArray[];
extern	Func setArray[];

static	bool first = true;
static	unsigned int	*UDPEchoConfig_UDPPort_uint = NULL;
static	unsigned int	*UDPEchoConfig_PacketsReceived_uint = NULL;
static	unsigned int	*UDPEchoConfig_PacketsResponded_uint = NULL;
static	unsigned int	*UDPEchoConfig_BytesReceived_uint = NULL;
static	unsigned int	*UDPEchoConfig_BytesResponded_uint = NULL;
static	bool 			*UDPEchoConfig_Enable_bool = NULL;
static	char			strUDPEchoConfig_PacketsReceived[25];
static	char			strUDPEchoConfig_BytesReceived[25];
static	char			strUDPEchoConfig_PacketsResponded[25];
static	char			strUDPEchoConfig_BytesResponded[25];
static	char			*strUDPEchoConfig_SourceIPAddress = NULL;

static	int startServer( int );
static	void waitForConnections( int );

// 	returns the server socket, or -1 if we couldn't bind the socket
static	int
startServer(int port)
{
	int sockfd;
	struct sockaddr_in address;

	// open the server socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return -1;
	}

	// setup bind stuffs
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((unsigned short)port);

	// bind the socket
	if (bind(sockfd, (struct sockaddr *)&address, sizeof(struct sockaddr)) == -1) {
		return -1;
	}

	// return the socket
	return sockfd;
}

// 	waits in an infinite loop for sockect connections, then passes them to handleConnection
static	void
waitForConnections(int sockfd)
{
	char buffer[1024];
	int size = 0;
	struct sockaddr_in remoteAddress;
	int remoteAddressSize = sizeof(struct sockaddr_in);
	typedef struct {
		unsigned int DestinationPort;
		unsigned int SourcePort;
		unsigned int Length;
		unsigned int Checksum;
		unsigned int TestGenSN;
		unsigned int TestRespSN;
		unsigned int TestRespRecvTimeStamp;
		unsigned int TestRespReplyTimeStamp;
		unsigned int TestRespReplyFailureCount;
		unsigned int Data;
	} Pack;

	Pack	*pack;

	pack = (Pack*) buffer;

	for (;;)
	{
		// read the client message
		do {
			memset(buffer,0,1024);
			size = recvfrom(sockfd, buffer, 1023, 0, (struct sockaddr *)&remoteAddress, (socklen_t *)&remoteAddressSize);
			getParameter( UDPEchoConfig_Enable, &UDPEchoConfig_Enable_bool );
			getParameter( UDPEchoConfig_SourceIPAddress, &strUDPEchoConfig_SourceIPAddress );
		} while (!*UDPEchoConfig_Enable_bool || inet_addr (strUDPEchoConfig_SourceIPAddress) != remoteAddress.sin_addr.s_addr);

		getParameter( UDPEchoConfig_PacketsReceived, &UDPEchoConfig_PacketsReceived_uint );
		++*UDPEchoConfig_PacketsReceived_uint;
		sprintf( strUDPEchoConfig_PacketsReceived, "%d", *UDPEchoConfig_PacketsReceived_uint);
		SetSmartParam( UDPEchoConfig_PacketsReceived, strUDPEchoConfig_PacketsReceived );

		getParameter( UDPEchoConfig_BytesReceived, &UDPEchoConfig_BytesReceived_uint );
		*UDPEchoConfig_BytesReceived_uint += size;
		sprintf( strUDPEchoConfig_BytesReceived, "%d", *UDPEchoConfig_BytesReceived_uint);
		SetSmartParam( UDPEchoConfig_BytesReceived, strUDPEchoConfig_BytesReceived );

		if (first)
		{
			first = false;
			SetTimeParam( UDPEchoConfig_TimeFirstPacketReceived );
		}

		// print the client message and what we're doing
		DEBUG_OUTPUT (
				dbglog (SVR_DEBUG, DBG_DIAGNOSTIC, "Client said \"%s\". Echoing...\n", buffer);
		)

		pack->TestGenSN++;
		pack->TestRespSN++;
		pack->TestRespRecvTimeStamp = time(NULL);
		pack->TestRespReplyTimeStamp = time(NULL);

		// echo the client message
		sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&remoteAddress, (socklen_t)remoteAddressSize);

		getParameter( UDPEchoConfig_PacketsResponded, &UDPEchoConfig_PacketsResponded_uint );
		++*UDPEchoConfig_PacketsResponded_uint;
		sprintf( strUDPEchoConfig_PacketsResponded, "%d", *UDPEchoConfig_PacketsResponded_uint);
		SetSmartParam( UDPEchoConfig_PacketsResponded, strUDPEchoConfig_PacketsResponded );

		getParameter( UDPEchoConfig_BytesResponded, &UDPEchoConfig_BytesResponded_uint );
		*UDPEchoConfig_BytesResponded_uint += size;
		sprintf( strUDPEchoConfig_BytesResponded, "%d", *UDPEchoConfig_BytesResponded_uint);
		SetSmartParam( UDPEchoConfig_BytesResponded, strUDPEchoConfig_BytesResponded );

		SetTimeParam( UDPEchoConfig_TimeLastPacketReceived );
	}
}

void *
udpHandler(void *param)
{
	int	ret = OK;
	int sockfd = 0;

	ret = getParameter( UDPEchoConfig_UDPPort, &UDPEchoConfig_UDPPort_uint );

	if (ret != OK || !*UDPEchoConfig_UDPPort_uint)
	{
		if ((sockfd = startServer(UDP_ECHO_DEFAULT_PORT + (procId))) != -1) {
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "UDP Echo Port: %d\n", UDP_ECHO_DEFAULT_PORT + (procId));
			)

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Port bind success\n");
			)

			// wait for incoming connections
			waitForConnections(sockfd);
		}
	}
	else
	{
		if ((sockfd = startServer(*UDPEchoConfig_UDPPort_uint)) != -1) {
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "UDP Echo Port: %d\n", *UDPEchoConfig_UDPPort_uint);
			)

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Port bind success\n");
			)

			// wait for incoming connections
			waitForConnections(sockfd);
		}
	}

	return NULL;
}

#endif /* HAVE_UDP_ECHO */

#ifdef HAVE_DOWNLOAD_DIAGNOSTICS

static int cbDownloadDiagnostics (void);

/** Callback function to handle a download test
*/
static int cbDownloadDiagnostics (void)
{
	char	*DownloadDiagnostics_DownloadURL_str = NULL;
	unsigned int	*DownloadDiagnostics_DSCP_uint = NULL;
	unsigned int	*DownloadDiagnostics_EthernetPriority_uint = NULL;

	getParameter( DownloadDiagnostics_DownloadURL, &DownloadDiagnostics_DownloadURL_str );
	getParameter( DownloadDiagnostics_DSCP, &DownloadDiagnostics_DSCP_uint );
	getParameter( DownloadDiagnostics_EthernetPriority, &DownloadDiagnostics_EthernetPriority_uint );

	if ( strnStartsWith( DownloadDiagnostics_DownloadURL_str, "http", 4 ) )
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Begin of HTTP Download Diagnostic\n");
		)

		DiagDoHttpDownload( DownloadDiagnostics_DownloadURL_str, "", "", 0, HTTP_DOWNLOAD_DIAGNOSTICS_FILE );
		remove( HTTP_DOWNLOAD_DIAGNOSTICS_FILE );

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "End of HTTP Download Diagnostic\n");
		)
	}
	else
		if ( strnStartsWith( DownloadDiagnostics_DownloadURL_str, "ftp", 3 ) )
		{
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Begin of FTP Download Diagnostic\n");
			)

			DiagDoFtpDownload( DownloadDiagnostics_DownloadURL_str, "", "", 0, FTP_DOWNLOAD_DIAGNOSTICS_FILE );
			remove( FTP_DOWNLOAD_DIAGNOSTICS_FILE );

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "End of FTP Download Diagnostic\n");
			)
		}
		else
			SetSmartParam( DownloadDiagnostics_DiagnosticsState, DiagnosticsState_Error_InitConnectionFailed );

	addEventCodeSingle ( EV_DIAG_COMPLETE );
	setAsyncInform(true);

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback DownloadDiagnostics: %s\n", DownloadDiagnostics_DownloadURL_str);
	)

	return CALLBACK_STOP;
}
#endif

/** This function is called by setParameters()
	if data == Requested then install the callback function
	the callback is called after all requests are done.
	There is no way to cancel the callback function.
*/
int setDownloadDiagnostics( const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_DOWNLOAD_DIAGNOSTICS
	// First store the value in the permanent storage
	int ret = storeParamValue( name, type, data);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "setDownloadDiagnostics()->storeParamValue() returns error = %i\n",ret);
		)
		return ret;
	}
	if ( strncmp( *(char **)data, DiagnosticsState_Requested, strlen( *(char**)data ) ) == 0 ) {
		addCallback( cbDownloadDiagnostics,  postSessionCbList );
	}
#endif
	return OK;
}

#ifdef HAVE_UPLOAD_DIAGNOSTICS
static int cbUploadDiagnostics (void);

/** Callback function to handle a upload test
*/
static int cbUploadDiagnostics (void)
{
	char			*UploadDiagnostics_UploadURL_str = NULL;
	unsigned int	*UploadDiagnostics_DSCP_uint = NULL;
	unsigned int	*UploadDiagnostics_EthernetPriority_uint = NULL;
	unsigned int	*UploadDiagnostics_TestFileLength_uint = NULL;

	getParameter( UploadDiagnostics_UploadURL, &UploadDiagnostics_UploadURL_str );
	getParameter( UploadDiagnostics_DSCP, &UploadDiagnostics_DSCP_uint );
	getParameter( UploadDiagnostics_EthernetPriority, &UploadDiagnostics_EthernetPriority_uint );
	getParameter( UploadDiagnostics_TestFileLength, &UploadDiagnostics_TestFileLength_uint );

	if ( strnStartsWith( UploadDiagnostics_UploadURL_str, "http", 4 ) )
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Begin of HTTP Upload Diagnostic\n");
		)

		DiagDoHttpUpload( UploadDiagnostics_UploadURL_str, "", "", *UploadDiagnostics_TestFileLength_uint );

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_DIAGNOSTIC, "End of HTTP Upload Diagnostic\n");
		)
	}
	else
		if ( strnStartsWith( UploadDiagnostics_UploadURL_str, "ftp", 3 ) )
		{
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Begin of FTP Upload Diagnostic\n");
			)

			DiagDoFtpUpload( UploadDiagnostics_UploadURL_str, "", "", *UploadDiagnostics_TestFileLength_uint );

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_DIAGNOSTIC, "End of FTP Upload Diagnostic\n");
			)
		}
		else
			SetSmartParam( UploadDiagnostics_DiagnosticsState, DiagnosticsState_Error_InitConnectionFailed );

	addEventCodeSingle ( EV_DIAG_COMPLETE );
	setAsyncInform(true);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback UploadDiagnostics: %s, %d\n", UploadDiagnostics_UploadURL_str, *UploadDiagnostics_TestFileLength_uint);
	)
	return CALLBACK_STOP;
}
#endif

/** This function is called by setParameters()
	if data == Requested then install the callback function
	the callback is called after all requests are done.
	There is no way to cancel the callback function.
*/
int setUploadDiagnostics( const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_UPLOAD_DIAGNOSTICS
	// First store the value in the permanent storage
	int ret = storeParamValue( name, type, data);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "setUploadDiagnostics()->storeParamValue() returns error = %i\n",ret);
		)
		return ret;
	}
	if ( strncmp( *(char **)data, DiagnosticsState_Requested, strlen( *(char**)data ) ) == 0 ) {
		addCallback( cbUploadDiagnostics,  postSessionCbList );
	}
#endif
	return OK;
}

#ifdef HAVE_IP_PING_DIAGNOSTICS

#define		Diag_Requested	"Requested"
#define		Diag_Complete	"Complete"
#define		Diag_Error		"Error_CannotResolveHostName"

static int cbIPPing (void);

/** Callback function to handle a ping test
*/
static int cbIPPing (void)
{
	char *hostName;
	getParameter( IPPingDiagnostics_Host, &hostName );
	setParameter( IPPingDiagnostics_DiagnosticsState, "Complete" );
	addEventCodeSingle ( EV_DIAG_COMPLETE );
	setAsyncInform(true);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback IPPing: %s\n", hostName);
	)
	return CALLBACK_STOP;
}
#endif

/** This function is called by setParameters()
	if data == Requested then install the callback function
	the callback is called after all requests are done.
	There is no way to cancel the callback function.
*/
int setIPPingDiagnostics( const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_IP_PING_DIAGNOSTICS
	int ret;
	// First store the value in the permanent storage
	//storeParamValue( name, type, data); //san comented

	if ( strncmp( *(char **)data, Diag_Requested, strlen( *(char**)data ) ) == 0 ) {
		ret = storeParamValue( name, type, data);
		if (ret != OK)
		{
			DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_IPPingDiagnostics_DiagnosticsState: storeParamValue() error\n");
			)
			return ret;
		}
		addCallback( cbIPPing,  postSessionCbList );
	}
#endif
	return OK;
}



#ifdef HAVE_TRACE_ROUTE_DIAGNOSTICS

//States for TraceRoute Diagnostic
#define		DiagnosticsState_Requested							"Requested"
#define		DiagnosticsState_Complete							"Complete"
#define		DiagnosticsState_Error_CannotResolveHostName		"Error_CannotResolveHostName"
#define		DiagnosticsState_Error_MaxHopCountExceeded			"Error_MaxHopCountExceeded"

static int cbTraceRoute (void);

/** Callback function to handle a traceroute test test
*/
static int cbTraceRoute  (void)
{
	int i;
	int ret;
	int statusDeleteObject;
	int newInstance;
	unsigned int numberOfTries, timeout, dataBlockSize, _DSCP, maxHopCount, responseTime, routeHopsNumberOfEntries, newInstanceIndex;
	static float fl_responseTime;
	char command[128] = {'\0'};
	int isUnknownHost = 0;

	// default values
	numberOfTries = 3;
	dataBlockSize = 38;
	timeout = 5000;
	_DSCP = 0;
	maxHopCount = 30;

	char * tmp_str = NULL;
	unsigned int * tmp_uint = NULL;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "cbTraceRoute(): Start TraceRoute Diagnostic\n");
	)

	//Retrieving values:
	ret = getParameter( TraceRouteDiagnostics_Interface, &tmp_str);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_Interface);
		)
		return ret;
	}
	char interfacePath[strlen(tmp_str)+1];
	strcpy(interfacePath, tmp_str);

	if (NULL != strstr(interfacePath,".WANDevice."))
	{
		strcat(interfacePath,".Name");

	}
	else if (NULL != strstr(interfacePath,".LANDevice."))
	{
		strcat(interfacePath,".IPInterfaceIPAddress");
	}

	ret = getParameter(interfacePath , &tmp_str);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_Interface);
		)
		return ret;
	}
	char interfaceName[strlen(tmp_str)+1];
	strcpy(interfaceName, tmp_str);

	ret = getParameter( TraceRouteDiagnostics_Host, &tmp_str );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_Host);
		)
		return ret;
	}
	char hostName[strlen(tmp_str)+1];
	strcpy(hostName, tmp_str);

	ret = getParameter( TraceRouteDiagnostics_NumberOfTries, &tmp_uint );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_NumberOfTries);
		)
		return ret;
	}
	else
	{
	numberOfTries = *tmp_uint;
	*tmp_uint = 0;
	}

	ret = getParameter( TraceRouteDiagnostics_Timeout, &tmp_uint );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_Timeout);
		)
		return ret;
	}
	else
	{
	timeout = *tmp_uint;
	*tmp_uint = 0;
	}

	ret = getParameter( TraceRouteDiagnostics_DataBlockSize, &tmp_uint );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_DataBlockSize);
		)
		return ret;
	}
	else
	{
	dataBlockSize = *tmp_uint;
	*tmp_uint = 0;
	}

	ret = getParameter( TraceRouteDiagnostics_DSCP, &tmp_uint );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_DSCP);
		)
		return ret;
	}
	else
	{
	_DSCP = *tmp_uint;
	*tmp_uint = 0;
	}

	ret = getParameter( TraceRouteDiagnostics_MaxHopCount, &tmp_uint );
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "cbTraceRoute(): getParameter(%s) error\n",TraceRouteDiagnostics_MaxHopCount);
		)
		return ret;
	}
	else
	{
	maxHopCount = *tmp_uint;
	*tmp_uint = 0;
	}

	//Restrictions serve until diagnostics performance isn't carried out in separate flows.
	if (timeout > 100000)   // 100 sec
	{
		timeout = 100000;
	}

	if (strlen(interfaceName) == 0)
	{
		sprintf(command, "traceroute  -m %u -q %u -t %u -w %u  %s %u", maxHopCount, numberOfTries, _DSCP, timeout/1000,  hostName, dataBlockSize);
	}
	else
	{
		sprintf(command, "traceroute  -i %s -m %u -q %u -t %u -w %u  %s %u", interfaceName, maxHopCount, numberOfTries, _DSCP, timeout/1000,  hostName, dataBlockSize);
	}
	DEBUG_OUTPUT (
		dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Executing traceroute command: %s\n", command);
	)

	responseTime = 0;
	struct timeval tv1,tv2,dtv;
	struct timezone tz;
	gettimeofday(&tv1, &tz);

	FILE* fp = popen(command,"r");

	gettimeofday(&tv2, &tz);
	dtv.tv_sec= tv2.tv_sec -tv1.tv_sec;
	dtv.tv_usec=tv2.tv_usec-tv1.tv_usec;
	if(dtv.tv_usec < 0)
	{
		dtv.tv_sec--;
		dtv.tv_usec+=1000000;
	}

	fl_responseTime = lroundf(dtv.tv_sec*1000+dtv.tv_usec/1000);


	if(!fp)
	{
		ParameterValue temp_value;
		temp_value.in_cval = "Error_Internal";
		ret = storeParamValue(TraceRouteDiagnostics_DiagnosticsState, StringType, &temp_value);
		if (ret != OK)
		{
			DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "cbTraceRoute: storeParamValue() error1\n");
			)
		}
		return CALLBACK_STOP;
	}
	routeHopsNumberOfEntries = 0;
	int count = 0;
	char p[1024] = {'\0'};

	//Delete objects if exist
	int countInstancesValue = 0;
	countInstances("InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.", &countInstancesValue);

	for (i = 1; i <= countInstancesValue; i++)
	{
		sprintf (p, "InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.%d.", i);
		deleteObject(p, &statusDeleteObject);
	}

	fgets(line, sizeof(line), fp); //skip header line


	newInstanceIndex = countInstancesValue;
	while( fgets(line, sizeof(line), fp))
	{
		char hopHost[256] = {'\0'};
		char hopHostAddress[256] = {'\0'};
		unsigned int hopErrorCode;
		char hopRTTimes[16] = {'\0'};
		char templateHopHost[256] = {'\0'};

		char p[1024] = {'\0'};
		routeHopsNumberOfEntries++;
		newInstanceIndex++;
		if (NULL != strstr(line,"N!") || NULL != strstr(line,"!N") || NULL != strstr(line,"H!") || NULL != strstr(line,"!H") ||  NULL != strstr(line,"P!") || NULL != strstr(line,"!P"))
		{
			isUnknownHost = 1;
		}


		struct cwmp__AddObjectResponse responseAddObject;
		// Add new object
		addObject("InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.", &responseAddObject);

		if (responseAddObject.Status != OK)
		{
			DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "cbTraceRoute: addObjectIntern() error5\n");
			)
			continue;
		}

		sprintf(templateHopHost, " %d", routeHopsNumberOfEntries);
		strcat(templateHopHost, "  %s (");
		sscanf(line, templateHopHost, &hopHost);


		sprintf (p, "InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.%d.HopHost", newInstanceIndex);


	    ParameterValue temp_value1;
		temp_value1.in_cval = hopHost;
		ret = storeParamValue(p, StringType, &temp_value1);


		int ress = sscanf(line, "%*s %*s (%s)", &hopHostAddress);
		hopHostAddress[strlen(hopHostAddress) - 1] = '\0';


		sprintf (p, "InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.%d.HopHostAddress", newInstanceIndex);
		ParameterValue temp_value2;
		temp_value2.in_cval = hopHostAddress;
		ret = storeParamValue(p, StringType, &temp_value2);

		//getting a hop error code from the ICMP error message
		if (NULL != strstr(line,"N!") || NULL != strstr(line,"!N"))
		{
			hopErrorCode = 0;
		}
		else if (NULL != strstr(line,"H!") || NULL != strstr(line,"!H"))
		{
			hopErrorCode = 1;
		}
		else if (NULL != strstr(line,"P!") || NULL != strstr(line,"!P"))
		{
			hopErrorCode = 2;
		}
		else if (NULL != strstr(line,"S!") || NULL != strstr(line,"S!"))
		{
			hopErrorCode = 5;
		}
		else if (NULL != strstr(line,"F!") || NULL != strstr(line,"!F"))
		{
			hopErrorCode = 4;
		}
		else if (NULL != strstr(line,"X!") || NULL != strstr(line,"X!"))
		{
			hopErrorCode = 13;
		}

		if (NULL != &hopErrorCode)
		{
			sprintf (p, "InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.%d.HopErrorCode", newInstanceIndex);
			char strHopErrorCode[25];
			sprintf(strHopErrorCode, "%d", hopErrorCode);
			ParameterValue temp_value3;
			temp_value3.in_uint = hopErrorCode;
			ret = storeParamValue(p, IntegerType, &temp_value3);
		}



		char templateTimes[256];
		for (i = 0; i < numberOfTries; i++)
		{
			strcat(templateTimes, "%f ms");
		}

		float arrayHopTimes[10] = {'\0'};
		sscanf(line, templateTimes, &arrayHopTimes[0], &arrayHopTimes[1], &arrayHopTimes[2], &arrayHopTimes[3], &arrayHopTimes[4], &arrayHopTimes[5], &arrayHopTimes[6], &arrayHopTimes[7], &arrayHopTimes[8], &arrayHopTimes[9]);

		char tempHopTimes[256] = {'\0'};
		for (i = 0; i < numberOfTries; i++)
		{
			sprintf(tempHopTimes, "%d", lroundf(arrayHopTimes[i]));
			strcat(hopRTTimes, tempHopTimes);
			strcat(hopRTTimes, ",");
		}
		hopRTTimes[strlen(hopRTTimes) - 1] = '\0';

		sprintf (p, "InternetGatewayDevice.TraceRouteDiagnostics.RouteHops.%d.HopRTTimes", newInstanceIndex);

		ParameterValue temp_value4;
		temp_value4.in_cval = hopRTTimes;
		ret = storeParamValue(p, StringType, &temp_value4);

	}
	pclose(fp);



	ParameterValue temp_value7;
	temp_value7.in_uint = routeHopsNumberOfEntries;
	ret = storeParamValue(TraceRouteDiagnostics_RouteHopsNumberOfEntries, UnsignedIntType, &temp_value7);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "cbTraceRoute: storeParamValue() error3\n");
		)
		return CALLBACK_STOP;
	}

	ParameterValue temp_value5;
	temp_value5.in_uint = responseTime;
	ret = storeParamValue(TraceRouteDiagnostics_ResponseTime, UnsignedIntType, &temp_value5);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "cbTraceRoute: storeParamValue() error4\n");
		)
		return CALLBACK_STOP;
	}


	ParameterValue temp_value6;
	if (isUnknownHost)
	{
		temp_value6.in_cval = DiagnosticsState_Error_CannotResolveHostName;
	}
	else
	{
		temp_value6.in_cval = DiagnosticsState_Complete;
	}
	ret = storeParamValue(TraceRouteDiagnostics_DiagnosticsState, StringType, &temp_value6);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
			dbglog (SVR_ERROR, DBG_ACCESS, "cbTraceRoute: storeParamValue() error2\n");
		)
		return CALLBACK_STOP;
	}

	addEventCodeSingle ( EV_DIAG_COMPLETE );
	setAsyncInform(true);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback TraceRoute: %s\n", hostName);
	)
	return CALLBACK_STOP;
}
#endif

/** This function is called by setParameters()
	if data == Requested then install the callback function
	the callback is called after all requests are done.
	There is no way to cancel the callback function.
*/
int setTraceRouteDiagnostics( const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_TRACE_ROUTE_DIAGNOSTICS
	int ret;
	// First store the value in the permanent storage
	//storeParamValue( name, type, data); //san comented

	if ( strncmp( *(char **)data, DiagnosticsState_Requested, strlen( *(char**)data ) ) == 0 ) {
		ret = storeParamValue( name, type, data);
		if (ret != OK)
		{
			DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_ACCESS, "set_InternetGatewayDevice_TraceRouteDiagnostics_DiagnosticsState: storeParamValue() error\n");
			)
			return ret;
		}
		addCallback( cbTraceRoute,  postSessionCbList );
	}
#endif
	return OK;
}

#ifdef HAVE_WANDSL_DIAGNOSTICS

static int cbWANDSL( void );

static int cbWANDSL( void )
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback cbWANDSL\n" );
	)
	return CALLBACK_STOP;
}

#endif /* HAVE_WANDSL_DIAGNOSTICS */

int setWANDSLDiagnostics(const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_WANDSL_DIAGNOSTICS
	addCallback( cbWANDSL,  postSessionCbList );
#endif /* HAVE_WANDSL_DIAGNOSTICS */

	return OK;
}

#ifdef HAVE_ATMF5_DIAGNOSTICS

int cbATMF5( void );

int cbATMF5( void )
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_DIAGNOSTIC, "Callback cbATMF5\n");
	)
	return CALLBACK_STOP;
}

#endif /* HAVE_ATMF5_DIAGNOSTICS */

int setATMF5Diagnostics( const char *name, ParameterType type, ParameterValue *data )
{
#ifdef HAVE_ATMF5_DIAGNOSTICS
	addCallback( cbATMF5,  postSessionCbList );
#endif /* HAVE_ATMF5_DIAGNOSTICS */

	return OK;
}

int	setUDPEchoConfig( const char *name, ParameterType type , ParameterValue *value_in )
{
#ifdef HAVE_UDP_ECHO
	ParameterValue value_out;
	time_t	t = 0;

	int ret = retrieveParamValue (name, type, &value_out);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "setUDPEchoConfig()->retrieveParamValue() returns error = %i\n",ret);
		)
		return ret;
	}
	if (!value_out.out_int && value_in->in_int)
	{
		first = true;
		SetSmartParam( UDPEchoConfig_PacketsReceived, "0" );
		SetSmartParam( UDPEchoConfig_PacketsResponded, "0" );
		SetSmartParam( UDPEchoConfig_BytesReceived, "0" );
		SetSmartParam( UDPEchoConfig_BytesResponded, "0" );

		SetSmartParam( UDPEchoConfig_TimeFirstPacketReceived, &t );
		SetSmartParam( UDPEchoConfig_TimeLastPacketReceived, &t );
	}
	ret = storeParamValue (name, type, value_in);
	if (ret != OK)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_DIAGNOSTIC, "setUDPEchoConfig()->storeParamValue() returns error = %i\n",ret);
		)
		return ret;
	}
#endif /* HAVE_UDP_ECHO */
	return	OK;
}
