/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef ftcallback_H
#define ftcallback_H

#include "globals.h"

#ifdef HAVE_FILE

#include "utils.h"
/** Callback functions is called before/after a file is dowload/uploaded
*/
int fileDownloadCallbackBefore (const char *, const char *);
int fileDownloadCallbackAfter (const char *, const char *);
int fileUploadCallbackBefore (const char *, const char *);
int fileUploadCallbackAfter (const char *, const char *);

#endif /* HAVE_FILE */

#endif /* ftcallback_H */
