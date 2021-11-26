/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef option_H
#define option_H

#define OPTION_MODE_DISABLE_STRING			"Disable"
#define OPTION_MODE_ENABLE_W_EXP_STRING 	"EnableWithExpiration"
#define OPTION_MODE_ENABLE_WO_EXP_STRING 	"EnableWithoutExpiration"

#define OPTION_MODE_DISABLE					0
#define OPTION_MODE_ENABLE_W_EXP			1
#define OPTION_MODE_ENABLE_WO_EXP		 	2

#define OPTION_DAYS_UNIT	"Days"
#define OPTION_MONTHS_UNIT	"Months"

#define SECS_PER_DAY	(24*3600)
#define SECS_PER_MONTH	(SECS_PER_DAY*30)

int readVoucherFile( const char * );
int loadOptions( void );
int printOptionList( void );
int getOptions( char *, struct ArrayOfOptions * );
int handleHostOption( char *, char * );	
int resetAllOptions( void );

#endif /* option_H */
