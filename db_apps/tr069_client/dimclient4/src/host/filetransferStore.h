/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef FILETRANSFERSTORE_H
#define FILETRANSFERSTORE_H

#include "utils.h" 
#include "storage.h"

typedef int (newFtInfo) (char *, char *);

/** Read all informations of pending file transfers from the storage
 *  for every file info the callback function is called.
 */
int readFtInfos( newFtInfo *callbackF );

/** Write the informations for one file transfer into the storage
 *  
 * \param name 	unique name of the information
 * \param data	transfer informations
 */
int storeFtInfo( const char *name, const char *data );


/** Delete the information of a specific file transfer
 * 
 * \param name	unique name of the information
 */
int deleteFtInfo( const char *name );


/** Delete all informations about pending file transfers
 */
int clearAllFtInfo( void );

/** Returns a default filename as destination for a download.
 * This is used if the ACS does not deliver a destination filename in the download request
 */
char* getDefaultDownloadFilename( void );

#endif /*FILETRANSFERSTORE_H*/
