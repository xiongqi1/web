/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/**
 * All file transfer information is stored in the file system.
 */

#include <dirent.h>

#include "filetransferStore.h"
#include "globals.h"
#include "debug.h"
#include "unistd.h"
#include "storage.h"

#include "luaTransfer.h"

static int loadFtFile (char *, char *, newFtInfo * );

/** Read all informations of pending file transfers from the storage
 *  for every file info the callback function is called.
 */
int readFtInfos( newFtInfo *func )
{
	return li_transfer_getAll(func);
/*
	char buf[MAX_PATH_NAME_SIZE];
	char *bufPtr;
	struct dirent *entry;
	int ret = OK;
	DIR *dir;
	int nfiles;

	nfiles = 0;
	dir = opendir (PERSISTENT_TRANSFERLIST_DIR);
	if (dir == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_TRANSFER, "reloadTransferlist: directory not found %s\n", PERSISTENT_TRANSFERLIST_DIR);
		)

		return ERR_DIM_TRANSFERLIST_READ;
	}
	
	strcpy (buf, PERSISTENT_TRANSFERLIST_DIR);
	bufPtr = (buf + strlen(PERSISTENT_TRANSFERLIST_DIR));
	
	while ((entry = readdir (dir)) != NULL)
	{
		// skip . and .. and all files starting with .
		if (entry->d_name[0] == '.') 
			continue;
		*bufPtr = '\0';
		strcat (bufPtr, entry->d_name);
		ret = loadFtFile (buf, entry->d_name, func);
	}
	return ret;
*/
}

/** Write the informations for one file transfer into the storage
 *  
 * \param name 	unique name of the information
 * \param data	transfer informations
 */
int storeFtInfo( const char *name, const char *data )
{
	return li_transfer_add(name, data);
/*
	int ret = OK;
	char path[MAX_PATH_NAME_SIZE];
	int fd;
	
	// build pathname for storage file
	strcpy( path, PERSISTENT_TRANSFERLIST_DIR );
	strcat( path, name);
	
	fd = open( path, O_RDWR|O_CREAT, 0777 );
	if ( fd <= 0 )
		return ERR_DIM_TRANSFERLIST_WRITE;
	ret = write( fd, data, strlen(data));
	if ( ret != OK )
		ret = ERR_DIM_TRANSFERLIST_WRITE;
	ret = close( fd );
	if ( ret != OK )
		ret = ERR_DIM_TRANSFERLIST_WRITE;
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_TRANSFER, "Save FTInfo: %s : %d\n", data, ret );
	)
	return ret;	
*/
}

/** Delete the information of a specific file transfer
 * 
 * \param name	unique name of the information
 */
int deleteFtInfo( const char *name )
{
	return li_transfer_delete(name);

/*
	int ret = OK;
	char path[MAX_PATH_NAME_SIZE];
	
	// build pathname for storage file
	strcpy( path, PERSISTENT_TRANSFERLIST_DIR );
	strcat( path, name);
	ret = remove(path);
	if ( ret != OK )
		ret = ERR_DIM_TRANSFERLIST_WRITE;	
	return ret;
*/
}


/** Delete all informations about pending file transfers
 */
int clearAllFtInfo( void )
{
	return li_transfer_deleteAll();
/*
	int ret = OK;
	struct dirent *entry;
	DIR *dir;
	int nfiles;
	char buf[MAX_PATH_NAME_SIZE];

	nfiles = 0;
	dir = opendir (PERSISTENT_TRANSFERLIST_DIR);
	if (dir == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_TRANSFER, "deleteTransferlist: directory not found %s\n", PERSISTENT_TRANSFERLIST_DIR);
		)
		return ERR_DIM_TRANSFERLIST_WRITE;
	}
	while ((entry = readdir (dir)) != NULL)
	{
		if (strcmp (entry->d_name, ".") == 0)
			continue;
		if (strcmp (entry->d_name, "..") == 0)
			continue;
		sprintf (buf, "%s%s", PERSISTENT_TRANSFERLIST_DIR, entry->d_name);
		ret = remove (buf);
		if ( ret < 0 ) {
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_TRANSFER, "deleteTransferlist: file not found %s\n", buf);
			)
			return ERR_DIM_TRANSFERLIST_WRITE;
		}
	}
	return ret;	
*/
}

/** Returns a default filename as destination for a download.
 * This is used if the ACS does not deliver a destination filename in the download request
 */
char* getDefaultDownloadFilename( void )
{
	return DEFAULT_DOWNLOAD_FILE;
}

/*
static int
loadFtFile(char *filename, char *name, newFtInfo* callbackF )
{
	int ret = OK;
	char buf[FILETRANSFER_MAX_INFO_SIZE + 1];
	FILE *file;

	file = fopen (filename, "r");
	if (file == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_TRANSFER, "loadInitialParameters: file not found %s\n", filename);
		)

		return ERR_INTERNAL_ERROR;
	}

	while (fgets (buf, FILETRANSFER_MAX_INFO_SIZE, file) != NULL)
	{
		if ( buf[0] == '#' || buf[0] == ' ' || buf[0] == '\n' || buf[0] == '\0' )
			continue;
		buf[strlen (buf) - 1] = '\0';	// remove trailing EOL 
		ret += callbackF(name, buf);
	}

	return ret;
} 
*/
