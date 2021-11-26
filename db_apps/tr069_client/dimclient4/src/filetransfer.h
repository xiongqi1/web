/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef filetransfer_H
#define filetransfer_H

#include "utils.h"

/* Callback for download */
typedef int (*DownloadCB)(const char* targetFileName, const char* targetType );

/* Callback for upload, called before the upload starts,
  the callback has to prepare the files for upload, depending on the fileType */
typedef int (*UploadCB)(const char* targetFileName, const char* targetType );
typedef enum transferState { Transfer_NotStarted = 1, Transfer_InProgress = 2, Transfer_Completed = 3, Apply_After_Boot = 4 } TransferState;
typedef struct UploadFile {
	char *filepath;
	char *filename;
	char *filetype;
} UploadFile;

int initFiletransfer (DownloadCB, UploadCB, DownloadCB, UploadCB);

#ifdef HAVE_GET_QUEUED_TRANSFERS
int	execGetQueuedTransfers (struct ArrayOfQueuedTransfers *);
#endif

#ifdef HAVE_GET_ALL_QUEUED_TRANSFERS
int	execGetAllQueuedTransfers (struct ArrayOfAllQueuedTransfers *);
#endif

#ifdef	 HAVE_AUTONOMOUS_TRANSFER_COMPLETE
extern bool isAutonomousTransferComplete;
#endif /* HAVE_AUTONOMOUS_TRANSFER_COMPLETE */

int resetAllFiletransfers( void );

#ifdef HAVE_FILE_DOWNLOAD
int execDownload( struct soap *, char *, char *, char *, char *, char *, unsigned int, char *, unsigned int,
					char *, char *, enum Transfers_type, char	*, char	*, cwmp__DownloadResponse * );
#endif

#ifdef HAVE_FILE_UPLOAD
int execUpload( struct soap *, char *, char *, char *, char *, char *, unsigned int, enum Transfers_type,
					char *, char *, cwmp__UploadResponse * );
#endif

bool isDelayedFiletransfer (void);
int handleDelayedFiletransfers( struct soap * );
int handleDelayedFiletransfersEvents (struct soap *);
int clearDelayedFiletransfers( struct soap * );
const UploadFile *type2file (const char *, unsigned int *);
int make_connect (struct soap *, const char *);

#endif /* filetransfer_H */
