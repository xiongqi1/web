/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_FILE

#include <signal.h>
#include <arpa/ftp.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "filetransfer.h"
#include "parameter.h"
#include "utils.h"
#include "httpda.h"
#include "list.h"
#include "serverdata.h"
#include "host/filetransferStore.h"
#include "host/diagParameter.h"
#include "ftp_ft.h"
#include "ftp_var.h"
#include "eventcode.h"
#include "paramconvenient.h"

#define FILE_TYPE_STR_LEN		64
#define URL_STR_LEN				256
#define USER_STR_LEN			256
#define PASS_STR_LEN			256
#define DEST_FILE_PATH_LEN		256
#define FAULT_STR_LEN			256
#define MAX_NAME_SIZE			32

/** The following structures defines the path and names
 * of the defined upload files for
 * 
 * 1 Vendor Configuration File
 * 2 Vendor Log File
 * 
 * The mapping from the file type sent by the ACS to the real filename in the file system
 * is made in type2file(). If a new file type has to be supported, add an entry in the
 * UploadFiles array and implement the mapping in type2file function.
 */
#if defined(PLATFORM_PLATYPUS)
extern int li_transfer_get_configfilename(char **value, size_t *len);
char configPath_Name[DEST_FILE_PATH_LEN];
char configFile_Name[DEST_FILE_PATH_LEN];
static UploadFile UploadFiles[] = {
	{ "/tmp/vt_3g36wv_Settings_Export.dat",
	  "vt_3g36wv_Settings_Export.dat",
	  "text/plain" 
	},
	{ "logfile.txt",
	  "logfile.txt",
	  "text/plain" 
	}
};
#else
static UploadFile UploadFiles[] = {
	{ "config.txt",
	  "config.txt",
	  "text/plain" 
	},
	{ "logfile.txt",
	  "logfile.txt",
	  "text/plain" 
	}
};
#endif

// Type of file transfers
enum Type { UPLOAD, DOWNLOAD };

/** A TransferEntry holds all the data we need to up- or download
 * The data is supported by the ACS 
 */
typedef struct TransferEntry
{
	// Type of transfer
	enum Type type;
	// All Data we got from the ACS
	char commandKey[CMD_KEY_STR_LEN+1];
	char fileType[FILE_TYPE_STR_LEN+1];
	char URL[URL_STR_LEN+1];
	char username[USER_STR_LEN+1];
	char password[PASS_STR_LEN+1];
	unsigned int fileSize;
	char targetFileName[DEST_FILE_PATH_LEN+1];
	char successURL[URL_STR_LEN+1];
	char failureURL[URL_STR_LEN+1];
	// time this object was created, also used for persistence
	time_t createTime;
	// StartTime is calculated from now() + delayedSeconds
	time_t startTime;
	// CompleteTime
	time_t completeTime;
	// Transfer State
	TransferState status;
	// Fault Handling
	unsigned int faultCode;
	char faultString[FAULT_STR_LEN+1];
	char	announceURL[URL_STR_LEN+1];
	char	transferURL[URL_STR_LEN+1];
	enum	Transfers_type	initiator;
} TransferEntry;

extern DownloadCB downloadCBB;
extern UploadCB uploadCBB;
extern DownloadCB downloadCBA;
extern UploadCB uploadCBA;

List transferList;
bool	isAutonomousTransferComplete = true;

#ifdef HAVE_FILE_DOWNLOAD
static int doDownload (char *, char *, char *, int, char *, const char *);
static int doFtpDownload (char *, char *, char *, int, char *);
static int doHttpDownload (char *, char *, char *, int, char *);
#endif

#ifdef HAVE_FILE_UPLOAD
static int doUpload (char *, char *, char *, char *);
static int doFtpUpload (char *, char *, char *, char *);
static int doHttpUpload (char *, char *, char *, char *);
static int uploadFile( struct soap *, const UploadFile *, long, bool );
static int sendFile( struct soap *, const UploadFile * );
#endif

static void addTransfer (TransferEntry *);
static int deleteTransferEntry (TransferEntry *);
static int writeTransferEntry (TransferEntry *);
static int readTransferList (void);
static int readTransferListEntry (char *, char *);
static void printTransferList (void);

/** Initialize the transfer list and load the transfer list from file
 if there is one available
*/
int
initFiletransfer (DownloadCB dCBB, UploadCB uCBB, DownloadCB dCBA, UploadCB uCBA)
{
	downloadCBB = dCBB;
	uploadCBB = uCBB;
	downloadCBA = dCBA;
	uploadCBA = uCBA;
	initList (&transferList);

	return readTransferList ();
}

#ifdef HAVE_GET_QUEUED_TRANSFERS
int
execGetQueuedTransfers (struct ArrayOfQueuedTransfers *transferArray)
{
	int ret = OK;
	ListEntry *entry = NULL;
	struct	QueuedTransferStruct	*soapQueuedTransfer;
	TransferEntry *te = NULL;

	transferArray->__ptrQueuedTransferStruct = (struct QueuedTransferStruct **)emallocTemp( sizeof( struct QueuedTransferStruct *));
	if ( transferArray->__ptrQueuedTransferStruct == NULL )
		return ERR_RESOURCE_EXCEED;
	transferArray->__size = 0;

	while ((entry = iterateList (&transferList, entry)))
	{
		soapQueuedTransfer = (struct QueuedTransferStruct *)emallocTemp( sizeof( struct QueuedTransferStruct ));
		if ( soapQueuedTransfer == NULL )
			return ERR_RESOURCE_EXCEED;
		te = (TransferEntry *) entry->data;
		if(!te->initiator) {
			soapQueuedTransfer->CommandKey = strnDupTemp( soapQueuedTransfer->CommandKey, te->commandKey, strlen( te->commandKey ));
			soapQueuedTransfer->State = te->status;
			transferArray->__ptrQueuedTransferStruct[transferArray->__size++] = soapQueuedTransfer;
		}
	}

	return ret;
}
#endif

#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
int
execGetAllQueuedTransfers (struct ArrayOfAllQueuedTransfers *transferArray)
{
	int ret = OK;

	ListEntry *entry = NULL;
	struct	AllQueuedTransferStruct	*soapAllQueuedTransfer;
	TransferEntry *te = NULL;

	transferArray->__ptrAllQueuedTransferStruct = (struct AllQueuedTransferStruct **)emallocTemp( sizeof( struct AllQueuedTransferStruct *));
	if ( transferArray->__ptrAllQueuedTransferStruct == NULL )
		return ERR_RESOURCE_EXCEED;
	transferArray->__size = 0;

	while ((entry = iterateList (&transferList, entry)))
	{
		soapAllQueuedTransfer = (struct AllQueuedTransferStruct *)emallocTemp( sizeof( struct AllQueuedTransferStruct ));
		if ( soapAllQueuedTransfer == NULL )
			return ERR_RESOURCE_EXCEED;
		te = (TransferEntry *) entry->data;
		soapAllQueuedTransfer->CommandKey = strnDupTemp( soapAllQueuedTransfer->CommandKey, te->commandKey, strlen( te->commandKey ));
		soapAllQueuedTransfer->State = te->status;
		soapAllQueuedTransfer->IsDownload = te->type;
		soapAllQueuedTransfer->FileType = strnDupTemp( soapAllQueuedTransfer->FileType, te->fileType, strlen( te->fileType ));
		soapAllQueuedTransfer->FileSize = te->fileSize;
		soapAllQueuedTransfer->TargetFileName = strnDupTemp( soapAllQueuedTransfer->TargetFileName, te->targetFileName, strlen( te->targetFileName ));
		transferArray->__ptrAllQueuedTransferStruct[transferArray->__size++] = soapAllQueuedTransfer;
	}

	return ret;
}
#endif

/** resets all file transfers and deletes the bootstrap file
*/
int
resetAllFiletransfers (void)
{
	return clearAllFtInfo();
}

#ifdef HAVE_FILE_DOWNLOAD
int
execDownload (struct soap *soap,
	      char 		*CommandKey,
	      char 		*FileType,
	      char 		*URL,
	      char 		*Username,
	      char 		*Password,
	      unsigned int 	FileSize,
	      char 		*TargetFileName,
	      unsigned int 	DelaySeconds,
	      char 		*SuccessURL,
	      char 		*FailureURL,
	      enum	Transfers_type	initiator,
	      char	*announceURL,
		  char	*transferURL,
		  cwmp__DownloadResponse *response)
{
	int returnCode = OK;
	TransferEntry *te;
	char *targetFileName;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "CommandKey  : %s\n", CommandKey);
			dbglog (SVR_INFO, DBG_TRANSFER, "FileType    : %s\n", FileType);
			dbglog (SVR_INFO, DBG_TRANSFER, "URL         : %s\n", URL);
			dbglog (SVR_INFO, DBG_TRANSFER, "Target      : %s\n", TargetFileName);
			dbglog (SVR_INFO, DBG_TRANSFER, "FileSize    : %d\n", FileSize);
			dbglog (SVR_INFO, DBG_TRANSFER, "Username    : %s\n", Username);
			dbglog (SVR_INFO, DBG_TRANSFER, "Password    : %s\n", Password);
			dbglog (SVR_INFO, DBG_TRANSFER, "Delay       : %d\n", DelaySeconds);
			dbglog (SVR_INFO, DBG_TRANSFER, "SuccessURL  : %s\n", SuccessURL);
			dbglog (SVR_INFO, DBG_TRANSFER, "FailureURL  : %s\n", FailureURL);
			dbglog (SVR_INFO, DBG_TRANSFER, "Initiator   : %d\n", initiator);
			dbglog (SVR_INFO, DBG_TRANSFER, "AnnounceURL : %s\n", announceURL);
			dbglog (SVR_INFO, DBG_TRANSFER, "TransferURL : %s\n", transferURL);
	)

	// If ACS delivers no TargetFileName therefore we use a default one
	if (TargetFileName && strlen(TargetFileName))
	{
		targetFileName = TargetFileName;
	}
	else
	{
#if defined(PLATFORM_PLATYPUS)
		if (!strcmp(FileType, "1 Firmware Upgrade Image"))
			targetFileName = DEFAULT_DOWNLOAD_FIRMWARE;
		else if (!strcmp(FileType, "3 Vendor Configuration File"))
			targetFileName = DEFAULT_DOWNLOAD_CONFIGFILE;
		else
			targetFileName = getDefaultDownloadFilename();
#else
		targetFileName = getDefaultDownloadFilename();
#endif
	}

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Delayed Download\n");
	)
	response->Status = Transfer_NotStarted;
	response->StartTime = UNKNOWN_TIME;
	response->CompleteTime = UNKNOWN_TIME;
	// Can't use emallocTemp() because of unfinished transfers with status = 0
	te = (TransferEntry *) emalloc (sizeof (TransferEntry));
	if (te == NULL)
		return ERR_RESOURCE_EXCEED;

	te->type = DOWNLOAD;
	strnCopy (te->commandKey, CommandKey, (sizeof (te->commandKey) - 1));
	strnCopy (te->fileType, FileType, (sizeof (te->fileType) - 1));
	strnCopy (te->URL, URL, (sizeof (te->URL) - 1));
	strnCopy (te->username, Username, (sizeof (te->username) - 1));
	strnCopy (te->password, Password, (sizeof (te->password) - 1));
	strnCopy (te->targetFileName, targetFileName, (sizeof (te->targetFileName) - 1));
	strnCopy (te->successURL, SuccessURL, (sizeof (te->successURL) - 1));
	strnCopy (te->failureURL, FailureURL, (sizeof (te->failureURL) - 1));
	te->fileSize = FileSize;
	te->createTime = time (NULL);
	te->startTime = te->createTime + DelaySeconds;
	te->completeTime = 0;
	te->status = Transfer_NotStarted;
	te->faultCode = NO_ERROR;
	te->initiator = initiator;
	strnCopy (te->announceURL, announceURL, (sizeof (te->announceURL) - 1));
	strnCopy (te->transferURL, transferURL, (sizeof (te->transferURL) - 1));
	addTransfer (te);

	return returnCode;
}

static int 
doDownload(char *URL, char *username, char *password, int bufferSize, char *targetFileName, const char *fileType)
{
	if ( strnStartsWith( URL, "http", 4 ) || strnStartsWith( URL, "https", 5) )
		return doHttpDownload( URL, username, password, bufferSize, targetFileName );

	if ( strnStartsWith( URL, "ftp", 3 ) )
		return doFtpDownload( URL, username, password, bufferSize, targetFileName );

	return ERR_NO_TRANS_PROTOCOL;	
}

static int
doFtpDownload (char *URL, char *username, char *password, int bufferSize, char *targetFileName)
{
	// extract remote host and remote filename from URL
	// Only ftp://hostname[:port]/dir/dir/remotename  is allowed
	// the remote name must be relative to the ftp directory

	char *tmp;
	char *host;
	char *remoteFileName;
	int  size;
	
	// skip scheme "ftp://"
	host = URL + 6; // strstr( URL, "://" );
	tmp = host;

	while ( *tmp && *tmp != ':' && *tmp != '/' ) {
		tmp++;
	}

	*tmp = '\0';
	tmp++;
	remoteFileName = tmp;

	/* Dimark [20080604 0000015] */
	if ( ftp_login( host, username, password ) != OK )
		return ERR_TRANS_AUTH_FAILURE;

	ftp_get( remoteFileName, targetFileName, &size );

	if ( size < 0 || ( bufferSize != 0 && size != bufferSize))
		return ERR_DOWNLOAD_FAILURE;

	ftp_disconnect();

	return OK;
}

int
doHttpDownload (char *URL, char *username, char *password, int bufferSize, char *targetFileName)
{
	int ret = 0;
	int readSize = 0;
	char *inpPtr;
	int cnt = 0;
	int fd = 0;
	bool isError = false;
	bool digest_init_done = 0;
	
	// get a soap structure and use it's buf for input
	struct soap soap;
	struct soap *tmpSoap;

	// Data for digest authorization
	struct http_da_info da_info;

	soap_init (&soap);
	tmpSoap = &soap;

	// Register Digest Authentication Plugin
	soap_register_plugin (tmpSoap, http_da);

	if (username != NULL && strlen (username) > 0)
	{
		tmpSoap->userid = username;
		tmpSoap->passwd = password;
	}
	if (password != NULL && strlen (password) > 0)
	{
		tmpSoap->passwd = password;
	}
	soap_begin (tmpSoap);
	tmpSoap->keep_alive = 1;
	tmpSoap->status = SOAP_GET;
	tmpSoap->authrealm = "";

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "HttpDownloadGet: %s Host: %s Port: %d\n", URL, tmpSoap->host, tmpSoap->port);
	)

	if (make_connect(tmpSoap, URL)
	    || tmpSoap->fpost (tmpSoap, URL, "", 0, tmpSoap->path, "", 0))
	{
		ret = tmpSoap->error;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "Request send %d %d\n", ret, tmpSoap->errmode);
		)
		soap_end (tmpSoap);
		soap_done (tmpSoap);

		return ERR_DOWNLOAD_FAILURE;
	}

	// Parse the HTML response header and remove it from the work data
	ret = tmpSoap->fparse (tmpSoap);

	switch (ret)
	{
	case SOAP_OK:
		break;
	case 401:		/* Authorization failure */
		http_da_save (tmpSoap, 
				&da_info, 
				(tmpSoap->authrealm != NULL ? tmpSoap->authrealm : CONNECTION_REALM),
				 username, password);
		digest_init_done = 1;
		make_connect (tmpSoap, URL);
		tmpSoap->fprepareinit (tmpSoap);

		tmpSoap->fpost (tmpSoap, URL, "", 0, tmpSoap->path, "", 0);
		ret = tmpSoap->fparse (tmpSoap);
		if (ret == SOAP_OK)
			break;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "401 Request %d\n", ret);
		)
		soap_closesock (tmpSoap);
		soap_destroy (tmpSoap);
		soap_end (tmpSoap);
		http_da_release(tmpSoap, &da_info);
		soap_done (tmpSoap);

		return ERR_TRANS_AUTH_FAILURE;
	default:
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "Request %d\n", ret);
		)
		soap_closesock (tmpSoap);
		soap_destroy (tmpSoap);
		soap_end (tmpSoap);
		soap_done (tmpSoap);

		return ERR_DOWNLOAD_FAILURE;
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Request 2 %d endpoint: %s\n", ret, tmpSoap->endpoint);
	)
	     
	inpPtr = tmpSoap->buf;

	tmpSoap->recv_timeout = 30;	// Timeout after 30 seconds stall on recv
	tmpSoap->send_timeout = 60;	// Timeout after 1 minute stall on send

	// open the output file, must be given with the complete path
	if ((fd = open (targetFileName, O_RDWR | O_CREAT, 0777)) < 0)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "Can't open File: %s", targetFileName);
		)
		return ERR_DOWNLOAD_FAILURE;
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
			dbglog (SVR_INFO, DBG_TRANSFER, "BuffSize: %d  read: %d  soap->err: %d\n", bufferSize, cnt, tmpSoap->errnum);
	)

	if (close (fd) < 0)
		return ERR_DOWNLOAD_FAILURE;
//		return soap_receiver_fault (tmpSoap, "9010", "Can't close file");

	soap_closesock (tmpSoap);
	soap_destroy (tmpSoap);
	soap_end (tmpSoap);

	if ( digest_init_done ) {
		http_da_release(tmpSoap, &da_info);
	}

	soap_done (tmpSoap);

	// Check if we got all the data we expected
	if (cnt == 0 || ( bufferSize != 0 && cnt != bufferSize))
		return ERR_DOWNLOAD_FAILURE;
	else
		return NO_ERROR;
}
#endif

#ifdef HAVE_FILE_UPLOAD
#if defined(PLATFORM_PLATYPUS)
int getConfigfileName(char * filename) {
	size_t len = 0;
	char *buf = NULL;
	int ret;
	
	ret = li_transfer_get_configfilename(&buf, &len);

	if(ret) return 1;

	strcpy(filename, buf);
	free(buf);

	return OK;
}
#endif

int
execUpload (struct soap *soap,
	    char *cwmp__CommandKey,
	    char *cwmp__FileType,
	    char *cwmp__URL,
	    char *cwmp__Username,
	    char *cwmp__Password,
	    unsigned int cwmp__DelaySeconds,
	    enum	Transfers_type	initiator,
	    char	*announceURL,
		char	*transferURL,
	    cwmp__UploadResponse * response)
{
	int returnCode = OK;
	const UploadFile *srcFile;
	TransferEntry *te;

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "CommandKey  : %s\n", cwmp__CommandKey);
			dbglog (SVR_INFO, DBG_TRANSFER, "FileType    : %s\n", cwmp__FileType);
			dbglog (SVR_INFO, DBG_TRANSFER, "URL         : %s\n", cwmp__URL);
			dbglog (SVR_INFO, DBG_TRANSFER, "Name        : %s\n", cwmp__Username);
			dbglog (SVR_INFO, DBG_TRANSFER, "Password    : %s\n", cwmp__Password);
			dbglog (SVR_INFO, DBG_TRANSFER, "Delay       : %d\n", cwmp__DelaySeconds);
			dbglog (SVR_INFO, DBG_TRANSFER, "Initiator   : %d\n", initiator);
			dbglog (SVR_INFO, DBG_TRANSFER, "AnnounceURL : %s\n", announceURL);
			dbglog (SVR_INFO, DBG_TRANSFER, "TransferURL : %s\n", transferURL);
	)

	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Delayed Upload\n");
	)

	response->Status = Transfer_NotStarted;
	response->StartTime = UNKNOWN_TIME;
	response->CompleteTime = UNKNOWN_TIME;

	// Can't use emallocTemp() because of unfinished transfers with status = 0
	te = (TransferEntry *) emalloc (sizeof (TransferEntry));
	if (te == NULL)
		return ERR_RESOURCE_EXCEED;

#if defined(PLATFORM_PLATYPUS)
	getConfigfileName(configFile_Name);
	sprintf(configPath_Name, "tmp/%s", configFile_Name);
	UploadFiles[0].filepath = configPath_Name;
	UploadFiles[0].filename = configFile_Name;
#endif

	srcFile = type2file (cwmp__FileType, &te->fileSize);
	te->type = UPLOAD;
	strnCopy (te->commandKey, cwmp__CommandKey, (sizeof (te->commandKey) - 1));
	strnCopy (te->fileType, cwmp__FileType, (sizeof (te->fileType) - 1));
	strnCopy (te->URL, cwmp__URL, (sizeof (te->URL) - 1));
	strnCopy (te->username, cwmp__Username, (sizeof (te->username) - 1));
	strnCopy (te->password, cwmp__Password, (sizeof (te->password) - 1));

	if ( srcFile )
		strnCopy( te->targetFileName, srcFile->filepath, (sizeof (te->targetFileName) - 1));

	te->createTime = time (NULL);
	te->startTime = te->createTime + cwmp__DelaySeconds;
	te->completeTime = 0;
	te->status = Transfer_NotStarted;
	te->faultCode = NO_ERROR;
	te->initiator = initiator;
	strnCopy (te->announceURL, announceURL, (sizeof (te->announceURL) - 1));
	strnCopy (te->transferURL, transferURL, (sizeof (te->transferURL) - 1));
	addTransfer (te);

	return returnCode;
}

static int
doUpload(char *URL, char *username, char *password, char *fileType)
{
	if ( strnStartsWith( URL, "http", 4 ) || strnStartsWith( URL, "https", 5) )
		return doHttpUpload( URL, username, password, fileType );

	if ( strnStartsWith( URL, "ftp", 3 ) )
		return doFtpUpload( URL, username, password, fileType );

	return ERR_NO_TRANS_PROTOCOL;	
}

static int
doFtpUpload (char *URL, char *username, char *password, char *fileType)
{
	// extract remote host and remote filename from URL
	// Only ftp://hostname[:port]/dir/dir/remotename  is allowed
	// the remote name must be relative to the ftp directory
	// If no remote name is found in the URL the srcFilename is used.
	// !!!Attention!!! the complete srcFilename including path is used.
	
	char *tmp;
	char *host;
	char *remoteFileName;
	unsigned int size;
	const UploadFile *srcFile;
	
	srcFile = type2file (fileType, &size);

	if (srcFile == NULL || size == 0)
		return ERR_UPLOAD_FAILURE;

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

	if ( ftp_login( host, username, password ) != OK )
		return ERR_TRANS_AUTH_FAILURE;

	ftp_put( remoteFileName, srcFile->filepath, (long *)&size );

	if ( size < 0 ) {
		ftp_disconnect();
		return ERR_UPLOAD_FAILURE;
	}

	ftp_disconnect();

	return OK;
}

static int
doHttpUpload ( char *URL, char *username, char *password, char *fileType)
{
	int ret = 0;
	const UploadFile *srcFile;
	unsigned int size;
	// Data for digest authorization
	struct http_da_info da_info;
	// get a soap structure and use it's buf for input
	struct soap soap;
	struct soap *tmpSoap;
	
	srcFile = type2file (fileType, &size);

	if (srcFile == NULL || size == 0)
		return ERR_UPLOAD_FAILURE;

	soap_init (&soap);
	tmpSoap = &soap;
	
	// Register Digest Authentication Plugin
	soap_register_plugin (tmpSoap, http_da);
	
	tmpSoap->keep_alive = 1;
	soap_set_endpoint(tmpSoap, URL);
	tmpSoap->socket = tmpSoap->fopen(tmpSoap, URL, tmpSoap->host, tmpSoap->port);

	if (tmpSoap->error)
        return ERR_UPLOAD_FAILURE;

	ret = uploadFile( tmpSoap, srcFile, size, false );

	if( ret != OK )
		return ret;

	// Parse the HTML response header and remove it from the work data
    ret = tmpSoap->fparse (tmpSoap);

	switch( ret ) {
		case SOAP_OK:
			break;
		case 401:		/* Authorization failure */
			soap_begin(tmpSoap);

			if (username != NULL && strlen (username) > 0)
			{
				tmpSoap->userid = username;
				tmpSoap->passwd = password;
			}

			http_da_save (tmpSoap,
				&da_info, 
				(tmpSoap->authrealm != NULL ? tmpSoap->authrealm : CONNECTION_REALM),
		 		username, password);

			tmpSoap->fprepareinit (tmpSoap);

			soap_begin(tmpSoap);
			ret = uploadFile( tmpSoap, srcFile, size, true );

			if ( ret != OK )
				return ret;

			ret = tmpSoap->fparse (tmpSoap);

			if (ret == SOAP_OK)
				break;

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_TRANSFER, "401 Request %d\n", ret);
			)

			soap_end (tmpSoap);
			http_da_release(tmpSoap, &da_info);
			soap_done (tmpSoap);

			return ERR_TRANS_AUTH_FAILURE;

		default:			
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_TRANSFER, "Request %d\n", ret);
			)
			soap_end (tmpSoap);
			soap_done (tmpSoap);

			return ERR_UPLOAD_FAILURE;
	}
	
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Request 2 %d endpoint: %s\n", ret, tmpSoap->endpoint);
	)

	return OK;
}
#endif

/** Converts the file type from an upload request into a physical filename
 * it returns the physical filename and the file size
 */
const UploadFile *
type2file (const char *fileType, unsigned int *fileSize)
{
	struct stat fstat;
	UploadFile *uploadFile;

	if (*fileType == '1')
		uploadFile = &UploadFiles[0];
	else if (*fileType == '2')
		uploadFile = &UploadFiles[1];
	else
		return NULL;

	if (stat (uploadFile->filepath, &fstat) == 0)
		*fileSize = (unsigned int)fstat.st_size;
	else
		*fileSize = 0;
	return uploadFile;
}

/** Build a connection to the endpoint
*/
int
make_connect (struct soap *server, const char *endpoint)
{
	soap_set_endpoint( server, endpoint);
	return soap_connect_command (server, SOAP_GET, endpoint, "");
}


/** checks the existence of a delayed filet ransfer.
 * if there is an entry in the transfer list, then return true
 * else false.
 */
bool
isDelayedFiletransfer (void)
{
	return getFirstEntry (&transferList) != NULL;
}

/** Iterates through the list of delayed transfers.
 * When find one, it executes the up/download and informs the ACS about the result.
 * For informing the ACS, TransferComplete() WebService is called immediately after the transfer.
 *
 * Attention!!! The list of delayed transfers is not cleared
 * call clearDelayedFiletransfers() after this function
 */
/** 
 \param server	Pointer to the soap data
 \return int	Number of downloaded files
 */
int
handleDelayedFiletransfers (struct soap *server)
{
	int cnt = 0;
	int ret = OK;
	ListEntry *entry = NULL;
	TransferEntry *te = NULL;

	printTransferList ();
	while ((entry = iterateList (&transferList, entry)))
	{
		te = (TransferEntry *) entry->data;
		time_t actTime = time (NULL);

#ifdef HAVE_FILE_DOWNLOAD
		if (te->status == Apply_After_Boot) {
			te->completeTime = actTime;
			cnt++;
			te->status = Transfer_Completed;
#ifdef FILETRANSFER_STORE_STATUS
			writeTransferEntry (te);
#endif

		}

		// all conditions full filled ?
		if (te->type == DOWNLOAD && te->status == Transfer_NotStarted
		    && te->startTime <= actTime)
		{
			te->startTime = actTime;
			cnt++;
			te->status = Transfer_InProgress;

			if ( downloadCBB != NULL ) {
				te->faultString[0] = '\0';
				// in targetFileName is the path of the upload file
				ret = downloadCBB( te->targetFileName, te->fileType );
				if (ret != OK)
					te->faultCode = ret;
			}

			ret = doDownload (te->URL, te->username, te->password,
					  te->fileSize, te->targetFileName, te->fileType);
			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_TRANSFER, "handleDelayedDownload URL: %s  ret: %d\n", te->URL, ret);
			)
			te->faultCode = ret;
			if (ret != SOAP_OK)
			{
				strnCopy (te->faultString, te->URL, (sizeof (te->faultString) - 1));
			}
			else
			{
				te->faultString[0] = '\0';
#if defined(PLATFORM_PLATYPUS)
				if (downloadCBA != NULL) {
					if (*(te->fileType) == '1') {
						te->status = Apply_After_Boot;
						te->completeTime = time (NULL);
#ifdef FILETRANSFER_STORE_STATUS
						writeTransferEntry (te);
#endif
					}
					ret = downloadCBA (te->targetFileName, te->fileType);
				}
#else
				if (downloadCBA != NULL)
					ret = downloadCBA (te->targetFileName, te->fileType);
#endif

				if (ret != OK)
					te->faultCode = ret;
			}

#if defined(PLATFORM_PLATYPUS)
			if (((*(te->fileType) == '1')|| (*(te->fileType) == '3') ) && (ret == SOAP_OK))
#else
			if (*(te->fileType) == '1')
#endif
			{
				te->status = Apply_After_Boot;
				setReboot();
			    DEBUG_OUTPUT (
			 		   dbglog (SVR_INFO, DBG_TRANSFER, "handleDelayedFiletransfers() -> setReboot()\n");
			    )
			} else {
				te->status = Transfer_Completed;
			}

			te->completeTime = time (NULL);
#ifdef FILETRANSFER_STORE_STATUS
			writeTransferEntry (te);
#endif

			continue;
		}
#endif

#ifdef HAVE_FILE_UPLOAD
		if (te->type == UPLOAD && te->status == Transfer_NotStarted
		    && te->startTime <= actTime)
		{
			te->startTime = actTime;
			cnt++;
			te->status = Transfer_InProgress;

			if ( uploadCBB != NULL ) {
				te->faultString[0] = '\0';
				// in targetFileName is the path of the upload file
				ret = uploadCBB(te->targetFileName, te->fileType );
				if (ret != OK)
					te->faultCode = ret;
			}

			ret = doUpload (te->URL, te->username, te->password, te->fileType);

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_TRANSFER, "handleDelayedUpload URL: %s  ret: %d \n", te->URL, ret);
			)

			te->faultCode = ret;

			if (ret != SOAP_OK)
			{
				strnCopy (te->faultString, te->URL, (sizeof (te->faultString) - 1));
			}
			else
			{
				te->faultString[0] = '\0';
				if (uploadCBA != NULL)
					ret = uploadCBA (te->targetFileName, te->fileType );
				if (ret != OK)
					te->faultCode = ret;
			}

			te->status = Transfer_Completed;
			te->completeTime = time (NULL);
#ifdef FILETRANSFER_STORE_STATUS
			writeTransferEntry (te);
#endif
		}
#endif
	}
	return cnt;
}

int
handleDelayedFiletransfersEvents (struct soap *server)
{
	int ret = SOAP_OK;
	ListEntry *entry = NULL;
	TransferEntry *te = NULL;

	entry = iterateList (&transferList, entry);
	while (entry)
	{
		te = (TransferEntry *) entry->data;
		if (te->status == Transfer_Completed)
		{
			// It's used in the InformMessage with the TRANSFER COMPLETE event
			if(te->initiator == ACS) {
				addEventCodeMultiple ((te->type == DOWNLOAD) ? EV_M_DOWNLOAD : EV_M_UPLOAD, te->commandKey);
				addEventCodeSingle (EV_TRANSFER_COMPLETE);
			} else {
#ifdef	 HAVE_AUTONOMOUS_TRANSFER_COMPLETE
				if (isAutonomousTransferComplete)	{
					addEventCodeSingle (EV_AUTONOMOUS_TRANSFER_COMPLETE);
				}
#endif
			}
		}
		entry = entry->next;
	}
	return ret;
}

/** For all transfers with status == 1 a TranferComplete Message is sent to the ACS 
 * and the entry is cleared from the file transfer list.
 * In case of an error the status is not changed.
*/
int
clearDelayedFiletransfers (struct soap *server)
{
	int ret = SOAP_OK;
	ListEntry *entry = NULL;
	TransferEntry *te = NULL;

	Fault *fs;

	entry = iterateList (&transferList, entry);
	while (entry)
	{
		te = (TransferEntry *) entry->data;

		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "ClearDelay Type: %d URL: %s  Status: %d\n", te->type, te->URL, te->status);
		)

		if (te->status == Transfer_Completed)
		{
			// Inform Server 
			fs = (Fault *) soap_malloc (server, sizeof (Fault));
			
			fs->FaultCode = te->faultCode;
			fs->FaultString = soap_strdup( server, te->faultString );  
			fs->__sizeParameterValuesFault = 0;
			fs->SetParameterValuesFault = NULL;
			
			if(te->initiator == ACS) {

				ret = soap_call_cwmp__TransferComplete (server, getServerURL(), "",
														te->commandKey,	fs, te->startTime, te->completeTime,
														NULL);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_TRANSFER, "Call TransferComplete ret: %d\n", ret);
				)
			} else {
#ifdef	 HAVE_AUTONOMOUS_TRANSFER_COMPLETE
				if (isAutonomousTransferComplete)	{

					ret = soap_call_cwmp__AutonomousTransferComplete (	server, getServerURL(), "",
																		te->announceURL, te->transferURL, te->type,
																		te->fileType, te->fileSize, te->targetFileName,
																		fs, te->startTime, te->completeTime,
																		NULL);
					DEBUG_OUTPUT (
							dbglog (SVR_INFO, DBG_TRANSFER, "Call AutonomousTransferComplete ret: %d\n", ret);
					)
				}
#endif
			}

			if (ret == SOAP_OK 
				|| ret == 32 ) // remove this when REGMEN bug fixed
			{
				deleteTransferEntry (te);
				DEBUG_OUTPUT (
						dbglog (SVR_INFO, DBG_TRANSFER, "Remove Type: %d URL: %s Status: %d\n", te->type, te->URL, te->status);
				)
				efree (te);
				entry = iterateRemove (&transferList, entry);
			}
		}
		else
		{
			entry = entry->next;
		}
	}
	return ret;
}

/** Helper function to add an TransferEntry to the TransferList
*/
static void
addTransfer (TransferEntry * te)
{
	addEntry (&transferList, te);
	writeTransferEntry (te);
}

static int
deleteTransferEntry (TransferEntry * te)
{
	int ret = OK;
	char buf[MAX_PATH_NAME_SIZE];

	sprintf (buf, "%ld", te->createTime);
	ret = deleteFtInfo(buf);

	return ret;
}

static int
writeTransferEntry (TransferEntry * te)
{
	int ret = OK;
	char name[MAX_NAME_SIZE];
	char data[FILETRANSFER_MAX_INFO_SIZE];

	sprintf (name, "%ld", te->createTime);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Save: %s Status: %d\n", te->URL, te->status);
	)
	ret = sprintf (data,
		       "%d|%s|%s|%s|%s|%s|%d|%s|%s|%s|%ld|%ld|%ld|%d|%d|%s|%s|%s|%d\n",
		       te->type, te->commandKey, te->fileType, te->URL,
		       te->username, te->password, te->fileSize,
		       te->targetFileName, te->successURL, te->failureURL,
		       te->createTime, te->startTime, te->completeTime,
		       te->status, te->faultCode, te->faultString,
		       te->announceURL,
			   te->transferURL,
			   te->initiator
	);

	if (ret < 0)
		return ERR_DIM_TRANSFERLIST_WRITE;

	ret = storeFtInfo( name, data );

	return ret;
}

static int
readTransferList (void)
{
	int ret = OK;
	ret = readFtInfos ((newFtInfo *)&readTransferListEntry);

	return ret;
}

static int
readTransferListEntry (char *name, char *data)
{
	int ret = OK;
	TransferEntry *te = NULL;
	char *bufptr;
	// read buffer
	bufptr = data;
	te = (TransferEntry *) emalloc (sizeof (TransferEntry));

	if (te == NULL)
		return ERR_RESOURCE_EXCEED;

	te->type = a2i (strsep (&bufptr, "|"));
	strnCopy (te->commandKey, strsep (&bufptr, "|"), CMD_KEY_STR_LEN);
	strnCopy (te->fileType, strsep (&bufptr, "|"), FILE_TYPE_STR_LEN);
	strnCopy (te->URL, strsep (&bufptr, "|"), URL_STR_LEN);
	strnCopy (te->username, strsep (&bufptr, "|"), USER_STR_LEN);
	strnCopy (te->password, strsep (&bufptr, "|"), PASS_STR_LEN);
	te->fileSize = a2i (strsep (&bufptr, "|"));
	strnCopy (te->targetFileName, strsep (&bufptr, "|"), DEST_FILE_PATH_LEN);
	strnCopy (te->successURL, strsep (&bufptr, "|"), URL_STR_LEN);
	strnCopy (te->failureURL, strsep (&bufptr, "|"), URL_STR_LEN);
	te->createTime = a2l (strsep (&bufptr, "|"));
	te->startTime = a2l (strsep (&bufptr, "|"));
	te->completeTime = a2l (strsep (&bufptr, "|"));
	te->status = a2i (strsep (&bufptr, "|"));
	te->faultCode = a2i (strsep (&bufptr, "|"));
	strnCopy (te->faultString, strsep (&bufptr, "|"), FAULT_STR_LEN);
	strnCopy (te->announceURL, strsep (&bufptr, "|"), URL_STR_LEN);
	strnCopy (te->transferURL, strsep (&bufptr, "|"), URL_STR_LEN);
	te->initiator = a2i (strsep (&bufptr, "|"));
	addTransfer (te);

	return ret;
}

/** Prints the transfer list
 */
static void
printTransferList (void)
{
	ListEntry *entry = NULL;
	TransferEntry *te = NULL;
#ifdef _DEBUG
	time_t now = time (NULL);
#endif /* _DEBUG */

	while ((entry = iterateList (&transferList, entry)))
	{
		te = (TransferEntry *) entry->data;
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_TRANSFER, "URL: %s Status: %d Start in %d sec\n", te->URL, te->status, te->startTime - now);
		)
	}
}

/** Read the file into memory and send it via soap_send
 *  returns OK or ERR_UPLOAD_FAILURE 
 */
#ifdef HAVE_FILE_UPLOAD
static int uploadFile( struct soap *tmpSoap, const UploadFile *srcFile, long fileSize, bool useAuth )
{
	long contentLength;
	char fileSizeStr[20];
	char boundary[20];

	sprintf (boundary, "%ld", getTime ());
	sprintf (tmpSoap->tmpbuf, "POST %s HTTP/1.1\r\n", tmpSoap->path);
	soap_send( tmpSoap, tmpSoap->tmpbuf );
	sprintf (tmpSoap->tmpbuf, "%s:%d", tmpSoap->host, tmpSoap->port);
	tmpSoap->fposthdr (tmpSoap, "Host", tmpSoap->tmpbuf);
	if (tmpSoap->userid && tmpSoap->passwd
		&& strlen(tmpSoap->userid) + strlen(tmpSoap->passwd) < 761)
	  {
	  	sprintf(tmpSoap->tmpbuf+262, "%s:%s", tmpSoap->userid, tmpSoap->passwd);
    	strcpy(tmpSoap->tmpbuf, "Basic ");
    	soap_s2base64(tmpSoap,
    		(const unsigned char*)(tmpSoap->tmpbuf + 262),
    		tmpSoap->tmpbuf + 6,
    		strlen(tmpSoap->tmpbuf+ 262));
    	tmpSoap->fposthdr(tmpSoap, "Authorization", tmpSoap->tmpbuf);
	  }
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
	if ( sendFile( tmpSoap, srcFile ) != OK ) {
		soap_end( tmpSoap );
		soap_done( tmpSoap );
		return ERR_UPLOAD_FAILURE;
	}
	soap_send( tmpSoap, "--" );
	soap_send( tmpSoap, boundary );
	soap_send( tmpSoap, "--\r\n" );

	return OK;
}

static 
int sendFile( struct soap *tmpSoap, const UploadFile *srcFile )
{
	int ret = OK;
	int fd;
	int bufsize = 1023;
	int rSize;
	
	if ((fd = open (srcFile->filepath, O_RDONLY )) > 0) {
		while(( rSize = read( fd, tmpSoap->tmpbuf, bufsize)) > 0 ) {
			tmpSoap->tmpbuf[rSize] = '\0';
			if ( soap_send_raw( tmpSoap, tmpSoap->tmpbuf, rSize ) != SOAP_OK ) {
				ret = ERR_UPLOAD_FAILURE;
				break;
			} 
		}

		close(fd);
		return ret;
	} else {
		return ERR_UPLOAD_FAILURE;
	}	
}
#endif

#endif /* HAVE_FILE */
