/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#include "globals.h"

#ifdef HAVE_FILE

/** This file is an example for a file transfer callback function.
 * the fileDownloadCallback() is opening the transfered file and reads
 * byte by byte the data and calculates the checksum
 */
#include "ftcallback.h"
#include "globals.h"

#include "luaTransfer.h"

/** Callback function is called before a file is downloaded
*/
int fileDownloadCallbackBefore (const char *filename, const char *filetype)
{
	return li_transfer_download_before(filename, filetype);
/*
	int ret = OK;

	DEBUG_OUTPUT (
			dbglog( SVR_DEBUG, DBG_PARAMETER, "Download Callback Before: %s  Filetype: %s  Checksum: %d\n", filename, filetype, 0 );
	)

	return ret;
*/
}

/** Callback function is called after a file is downloaded
*/
int fileDownloadCallbackAfter (const char *filename, const char *filetype)
{
	return li_transfer_download_after(filename, filetype);
/*
	int ret = OK;
//	int data, chksum;
//	FILE *fp;

//	fp = fopen( filename , "r" );
//	if ( fp == NULL )
//		return ERR_DOWNLOAD_FAILURE;
	
//	chksum = 0;
//	while ( ( data = fgetc( fp ) ) != EOF ) {
//		chksum += data;
//	}
//	fclose( fp );

	DEBUG_OUTPUT (
			dbglog( SVR_DEBUG, DBG_PARAMETER, "Download Callback After: %s  Filetype: %s  Checksum: %d\n", filename, filetype, 0 );
	)

	return ret;
*/
}

/** Callback function is called before a file is uploaded
*/
int fileUploadCallbackBefore (const char* targetFileName, const char* targetType )
{
	return li_transfer_upload_before(targetFileName, targetType);
/*
	int ret = OK;

	DEBUG_OUTPUT (
			dbglog( SVR_DEBUG, DBG_PARAMETER, "Upload Callback Before: %s  Checksum: %d\n", filename, 0 );
	)

	return ret;
*/
}

/** Callback function is called after a file is uploaded
*/
int fileUploadCallbackAfter (const char* targetFileName, const char* targetType )
{
	return li_transfer_upload_after(targetFileName, targetType);
/*
	int ret = OK;

	DEBUG_OUTPUT (
			dbglog( SVR_DEBUG, DBG_PARAMETER, "Upload Callback After: %s  Checksum: %d\n", filename, 0 );
	)

	return ret;
*/
}

#endif /* HAVE_FILE */
