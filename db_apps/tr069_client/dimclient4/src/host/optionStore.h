/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef OPTIONSTORE_H
#define OPTIONSTORE_H

#include "utils.h" 
#include "globals.h"

typedef int (newOption) (char *, char *);

#ifdef HAVE_VOUCHERS_OPTIONS

/** Get a filename to store a Voucher on the files ystem.
 * The file don't has to in the persistent part of the file system.
 * The parameter is the actual index of the voucher array starting at 0,
 * this can be used to create a unique filename.
 * This filename is used by gSOAP to read the information of the voucher and get the 
 * option informations.
 */
const char *getVoucherFilename( int );

/** Deletes all options from the persistent storage
 */
int deleteAllOptions( void );

/** Delete a named option from the persistent storage
 * 
 */
int deleteOption( const char * );

/** Stores a new option in the persistent storage
 */
int storeOption( const char *, const char * );
 
/** Load all Options from the persistent storage.
 * For every file the challback function is called.
 */
int reloadOptions( newOption * );

#endif /* HAVE_VOUCHERS_OPTIONS */
#endif /*OPTIONSTORE_H*/
