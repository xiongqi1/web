/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** Example implementation for voucher/option storage.
 * 
 * The information of the option is stored in a single file in the persistent 
 * file system.
 */

#include <dirent.h>

#include "optionStore.h"
#include "globals.h"
#include "debug.h"
#include "unistd.h"
#include "storage.h"
#include "stdio.h"

static char filename[256];

static int loadOptionFile (char *, char *, newOption * );
 
const char *getVoucherFilename(int index)
{
	sprintf( filename, VOUCHER_FILE, index );
	return filename;		
}

int deleteAllOptions( void )
{
	char buf[MAX_PATH_NAME_SIZE];
	char *bufPtr;
	struct dirent *entry;
	DIR *dir;

	dir = opendir (PERSISTENT_OPTION_DIR);
	if (dir == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_OPTIONS, "deleteOptions: directory not found\n");
		)

		return ERR_INTERNAL_ERROR;
	}
	
	strcpy( buf, PERSISTENT_OPTION_DIR );
	bufPtr = (buf + strlen(PERSISTENT_OPTION_DIR));
	
	while ((entry = readdir (dir)) != NULL)
	{
		if (strcmp (entry->d_name, ".") == 0)
			continue;
		if (strcmp (entry->d_name, "..") == 0)
			continue;
		*bufPtr = '\0';
		strcat (buf, entry->d_name);
		remove(buf); 
	}

	return OK;	
}	

int deleteOption( const char *name )
{
	int ret = OK;

	char buf[MAX_PATH_NAME_SIZE];
	// remove the option file
	strcpy (buf, PERSISTENT_OPTION_DIR);
	strcat (buf, name);
	remove (buf);

	return ret;
}	

int storeOption( const char *name, const char *data )
{
	int ret = OK;
	int fd;
	char buf[MAX_PATH_NAME_SIZE];

	strcpy (buf, PERSISTENT_OPTION_DIR);
	strcat (buf, name);
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_OPTIONS, "WriteOption: %s\n", buf);
	)
	if ((fd = open (buf, O_RDWR | O_CREAT, FILE_MASK)) < 0)
		return ERR_RESOURCE_EXCEED;
	ret = write( fd, data, strlen(data));
	if ( ret > 0 )
		ret = OK;
	else
		ret = ERR_RESOURCE_EXCEED;
	close (fd);
	return ret;
}

int reloadOptions( newOption *callbackO ) 
{
	char buf[MAX_PATH_NAME_SIZE];
	struct dirent *entry;
	int ret = OK;
	DIR *dir;
	int nfiles;

	nfiles = 0;
	dir = opendir (PERSISTENT_OPTION_DIR);
	if (dir == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_OPTIONS, "reloadOptions: directory not found %s\n", PERSISTENT_OPTION_DIR);
		)

		return ERR_INTERNAL_ERROR;
	}
	while ((entry = readdir (dir)) != NULL)
	{
		if (strcmp (entry->d_name, ".") == 0)
			continue;
		if (strcmp (entry->d_name, "..") == 0)
			continue;
		strcpy (buf, PERSISTENT_OPTION_DIR);
		strcat (buf, entry->d_name);
		DEBUG_OUTPUT (
				dbglog (SVR_INFO, DBG_OPTIONS, "ReloadOptions: %s\n", buf);
		)
		ret = loadOptionFile (buf, entry->d_name, callbackO);
	}
	return OK;
}

static int
loadOptionFile (char *filename, char *name, newOption* callbackO )
{
	int ret = OK;
	char buf[MAX_PATH_NAME_SIZE + 1];
	FILE *file;

	file = fopen (filename, "r");
	if (file == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_OPTIONS, "loadOption: file not found %s\n", filename);
		)

		return ERR_INTERNAL_ERROR;
	}

	while (fgets (buf, MAX_PATH_NAME_SIZE, file) != NULL)
	{
		if ( buf[0] == '#' || buf[0] == ' ' || buf[0] == '\n' || buf[0] == '\0' )
			continue;
		buf[strlen (buf) - 1] = '\0';	/* remove trailing EOL  */
		ret += callbackO(name, buf);
	}

	return ret;
} 
