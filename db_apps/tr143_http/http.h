#ifndef __PING_H
#define __PING_H

#include <time.h>
#include "comms.h"
#include "parameters.h"


typedef struct  TDownloadSession
{
	TParameters*	m_parameters;
	TConnectSession*m_connection;

	
	unsigned int 	m_TotalBytesSent; //	sent bytes.

	unsigned int 	m_TestBytesReceived; //	Size of the file received.
	unsigned int 	m_TotalBytesReceived; //Bytes received over interface during download.

}TDownloadSession;

typedef struct  TUploadSession
{
	TParameters*	m_parameters;
	TConnectSession*m_connection;

	unsigned int 	m_TestFileLength; //Size of test file to generate in bytes.

	unsigned int 	m_TotalBytesSent; //Bytes sent into interface during upload.
	unsigned int 	m_BytesSent; //Bytes sent into interface during upload.


}TUploadSession;







int http_download(TConnectSession*pConnection, TDownloadSession*pSession);

int http_upload(TConnectSession*pConnection, TUploadSession*pSession);


#define PROTOCOL_NAME_DOWNLOAD 	"httpd"

#define PROTOCOL_NAME_UPLOAD	"httpu"


#endif
