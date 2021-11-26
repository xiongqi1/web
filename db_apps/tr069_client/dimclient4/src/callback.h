/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef callback_H
#define callback_H

#include "utils.h" 
#include "list.h"

#define CALLBACK_REPEAT		1
#define CALLBACK_STOP		0

extern List *initParametersDoneCbList;
extern List *preSessionCbList;
extern List *postSessionCbList;
extern List *cleanUpCbList;

typedef int (*Callback) (void);

void initCallbackList (void);
void addCallback( Callback, List * );

/** Call all callback functions in the list
 *  the callback is removed after the call
 */
int executeCallback( List * );

#endif /* callback_H */
