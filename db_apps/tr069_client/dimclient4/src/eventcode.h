/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef eventcode_H
#define eventcode_H

#include "utils.h" 

typedef int *(newEvent) (char *);

int loadEventCode( void );
int getEventCodeList(struct ArrayOfEventStruct *);
void freeEventList(void );
int addEventCodeMultiple( const char *, const char * );
int addEventCodeSingle( const char * );
int resetEventCodes( void );

#if defined(PLATFORM_PLATYPUS)
int sizeOfevtList( void );
#endif

#endif /* eventcode_H */
