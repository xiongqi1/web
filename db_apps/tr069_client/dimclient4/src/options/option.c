/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** 
 This module is in duty for handling options defined in vouchers.
 The Voucher is sent from the ACS and stored in the file system.
 The options are stored in memory as a linked list.
 In the file system they are stored as a XML file.
 
 A voucher option is identified by an unique VSerialNum.
 The server is using the Option name to identify an Option.
*/

#include "globals.h"

#ifdef HAVE_VOUCHERS_OPTIONS

#include <stdio.h>
#include <dirent.h>
#include <string.h>

#include "utils.h"
#include "parameter.h"
#include "debug.h"
#include "option.h"
#include "paramconvenient.h"
#include "optionStore.h"

#define DO_UPDATE	1
#define NO_UPDATE	0
#define MAX_OPTION_DATA_SIZE  	256

typedef struct OptionEntry
{
	char 		  		optionName[65];
	char				optionDesc[257];
	char			 	voucherSN[65];
	xsd__unsignedInt 	state;
	xsd__int			mode;
	xsd__dateTime		startDate;
	xsd__dateTime		expirationDate;
	bool				isTransferable;
	struct OptionEntry	*next;
} OptionEntry;

static OptionEntry *firstEntry = NULL;

static int writeOption( OptionEntry *, void * );
static int addOption( struct OptionStruct * );
static int voucher2signature( struct Signature *, const char * );
static xsd__dateTime calcEndDate( int, const char * );
static int convMode( const char * );
static int insertOption( OptionEntry *, bool  );
static int checkDeviceIdData( struct DeviceId *, struct DeviceId *);
static int countOptions( void );
static int removeOption( OptionEntry * );
static int deleteOptionFile( char *  );
static int newOption1( char *name, char *data );
static OptionEntry * findBySerialNum( char * );
static void printOption( OptionEntry * );

/** 
	Returns all or one specific option(s) to the caller in the optionArray structure.
	The memory it allocated temporary with emallocTemp(),

	@param optionName	if NULL or empty all options are returned
	@param optionArray	Structure to return the option data.

	@returns int		error code
*/
int getOptions( char *optionName, struct ArrayOfOptions *optionArray )
{
	register int optionCount, idx;
	register cwmp__Option *soapOption;
	OptionEntry *tmp = firstEntry;
	
	optionCount = countOptions();
	// nothing to do?
	if( optionCount == 0 ) {
		optionArray->__size = optionCount;
		return OK;
	}
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_OPTIONS, "OptionName: %s\n", optionName );
	)
	// optionName could be NULL or a specific option name
	if ( optionName != NULL && strlen( optionName ) > 0 ) {
		optionArray->__ptrOptionStruct = (cwmp__Option**)emallocTemp( sizeof( cwmp__Option *));
	} else {
		optionArray->__ptrOptionStruct = (cwmp__Option**)emallocTemp( sizeof( cwmp__Option *) * optionCount );
	}
	if ( optionArray->__ptrOptionStruct == NULL )
		return ERR_RESOURCE_EXCEED;
	idx = 0;
	while ( tmp != NULL ) {
		if ( optionName == NULL || strCmp( optionName, tmp->optionName ) ) {
			soapOption = (cwmp__Option*)emallocTemp( sizeof( cwmp__Option ));
			if ( soapOption == NULL )
				return ERR_RESOURCE_EXCEED;
			soapOption->OptionName = strnDupTemp( soapOption->OptionName, tmp->optionName, strlen( tmp->optionName ));
			soapOption->VoucherSN = strnDupTemp( soapOption->OptionName, tmp->voucherSN, strlen( tmp->voucherSN ) );
			soapOption->StartDate = tmp->startDate;
			soapOption->ExpirationDate = tmp->expirationDate;
			soapOption->Mode = tmp->mode;
			soapOption->State = tmp->state;
			soapOption->IsTransferable = tmp->isTransferable;
		
			optionArray->__ptrOptionStruct[idx++] = soapOption;
		}
		tmp = tmp->next;
	}		
	optionArray->__size = idx;
	return OK;
}

/** 
	Extract the options from a given voucher file.
	The filename is the pathname

	\param filename 	path to the voucher file
	\returns int		error code
*/
int readVoucherFile( const char *filename )
{
	int ret = OK;
	int i;
	struct Signature signature;
	
	ret = voucher2signature( &signature, filename );
	if ( ret != OK )
		return ret;
	
	for ( i = 0; i != signature.__size; i++ ) {
		ret = addOption( signature.__ptrObject[i].Option );
		if ( ret != OK )
			return ret;
	}
	return ret;
}

int printOptionList( void )
{
	OptionEntry *tmp = firstEntry;
	
	while( tmp != NULL ) {
		printOption( tmp );
		tmp = tmp->next;
	}
	return OK;
}

/** load all options from the file system database
	inform the host system about the options
*/
int loadOptions( void )
{
	reloadOptions( (newOption *)&newOption1 );

	return OK;
}

/** load all options from the file system database
	inform the host system about the options
*/
int resetAllOptions( void )
{
	return deleteAllOptions();
}

/** Handle host command on options
 	the only command supported by now is deleteOption.
	
	\param voucherSN	unique number which identifies an option
	\param cmd			only delete supported
	\return int			error code
*/
int handleHostOption( char *voucherSN, char *cmd )
{
	OptionEntry *opt;
	
	opt = findBySerialNum( voucherSN );
	if (opt == NULL)
		return ERR_INVALID_OPTION_NAME;
	if ( deleteOptionFile( opt->voucherSN ) == OK ) {
		removeOption( opt );
		efree( opt );
		return OK;
	} else {
		return ERR_CANT_DELETE_OPTION;
	}
}

static int voucher2signature( struct Signature *signature, const char *filename )
{
	static struct Signature sigtab = {0, NULL};
	struct soap soap;
	soap_init( &soap );
	//soap_set
	soap.recvfd = open( filename, O_RDONLY );
	if ( soap.recvfd < 0 ) {
		soap_done( &soap );
		return ERR_READ_OPTION;
	}
	if ( !soap_begin_recv( &soap ) ) {
		if ( !soap_get_Signature( &soap, &sigtab, "Signature", NULL )) {
			close( soap.recvfd );
   			soap_done( &soap );
   			return ERR_READ_OPTION;
  		} 
 	} 
 	soap_end_recv( &soap );
 	close( soap.recvfd );
 	soap_done( &soap );
 	*signature = sigtab; 
 	return OK;
}


/** Writes a single Parameter into a file using the pathname of the parameter as its filename
* their must a special directory exist
*/
static int writeOption( OptionEntry *entry, void *arg )
{
	int ret = OK;
	char buf[MAX_OPTION_DATA_SIZE];
 
	ret = sprintf( buf, "%s;%s;%s;%u;%u;%ld;%ld;%d;",
       		entry->optionName, entry->optionDesc, entry->voucherSN, entry->state, entry->mode, entry->startDate,
       		entry->expirationDate, entry->isTransferable );
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_OPTIONS, "WriteOption: %s ret: %d\n", buf, ret );
	)
	ret = storeOption( entry->voucherSN, buf );
	return ret;
}

/** newOption1() is used as callback function for loading existing options at boot time
 * from the persistent storage.
 */
static int newOption1( char *name, char *data )
{
	char *bufPtr;
	OptionEntry *oe = NULL; 
	
	oe = (OptionEntry*)emalloc( sizeof( OptionEntry ));
	if ( oe == NULL )
		return ERR_RESOURCE_EXCEED;
	
	bufPtr = data;
	strcpy( oe->optionName, strsep ( &bufPtr, ";" ));
	strcpy( oe->optionDesc, strsep( &bufPtr, ";" ));
	strcpy( oe->voucherSN, strsep( &bufPtr, ";" ));
	oe->state = a2i( strsep( &bufPtr, ";" ));
	oe->mode = a2i( strsep( &bufPtr, ";" ));
	oe->startDate = a2l( strsep( &bufPtr, ";" ));
	oe->expirationDate = a2l( strsep( &bufPtr, ";" ));
	oe->isTransferable = a2i( strsep( &bufPtr, ";" ));
	insertOption( oe, NO_UPDATE );
	return OK;
}

static int deleteOptionFile( char *optionname )
{
	return deleteOption( optionname );
}

/** Insert the options from the signature struct into the option list.
    Existing options are overwritten.
	Before inserting, the deviceId information is checked against the CPE DeviceId data.
*/
static int addOption( struct OptionStruct *obj )
{	
	int ret = OK;
	OptionEntry *oe = NULL;
	
	ret = checkDeviceIdData( obj->DeviceId, getDeviceId() );
	if ( ret != OK )
		return ret;
	
	oe = (OptionEntry*)emalloc( sizeof( OptionEntry ));
	if ( oe == NULL )
		return ERR_RESOURCE_EXCEED;
	else {
		strncpy( oe->optionName, obj->OptionIdent, sizeof( oe->optionName ));
		strncpy( oe->optionDesc, obj->OptionDesc, sizeof( oe->optionDesc ));
		strncpy( oe->voucherSN, obj->VSerialNum, sizeof( oe->voucherSN ));
		oe->state = 0;
		oe->startDate = obj->StartDate;
		// Handle no startDate available, means valid w/o expiration
		if ( oe->startDate == 0 ) {
			oe->startDate = getTime();
		}
		
		oe->mode = convMode(obj->Mode);
		if ( oe->mode == OPTION_MODE_ENABLE_WO_EXP ) {
			oe->expirationDate = oe->startDate;
		} else {
			oe->expirationDate = oe->startDate + calcEndDate( obj->Duration, obj->DurationUnits );
		}
		oe->isTransferable = obj->Transferable;
		oe->next = NULL;
	}
	return insertOption( oe, DO_UPDATE );
}

/* Remove the optionEntry from the list of options
 * the corresponding option file is not deleted.
 */
static int removeOption( OptionEntry *oe )
{
	OptionEntry *prev = NULL, *act = firstEntry;

	// Remove the option from our option list and remove
	// the corresponding storage file
	while( act != NULL ) {
		if ( strCmp( act->voucherSN , oe->voucherSN ) ) {
			if ( prev == NULL ) {
				firstEntry = act->next;
				prev = act->next;
			} else {
				prev->next = act->next;
			}
			act = NULL;
			break;
		} else {
			prev = act;
			act = act->next;
		}
	}

	return ERR_INVALID_OPTION_NAME;
}

static int insertOption( OptionEntry *oe, bool update )
{
	OptionEntry *prev = NULL, *act = firstEntry;

	if ( act == NULL ) {
		firstEntry = oe;
		if ( update )
			return writeOption( oe, NULL );
		else
			return OK;
	} 
	while( act != NULL ) {
		// replace the option entry if the voucher serial number are equal
		if ( strCmp( act->voucherSN , oe->voucherSN ) ) {
			oe->next = act->next;
			if ( prev != NULL )
				prev->next = oe;
			else
				firstEntry = oe;
			efree( act );
			if ( update )
				return writeOption( oe, NULL );
			else
				return OK;
		}
		prev = act;
		act = prev->next;
	}
	if ( prev != NULL )
		prev->next = oe;
	if ( update )
		return writeOption( oe, NULL );
	else
		return OK;
}

static OptionEntry * findBySerialNum( char *srch )
{
	OptionEntry *tmp = firstEntry;

	while ( tmp != NULL ) {
		if ( strCmp( tmp->voucherSN, srch ) )
			return tmp;
		else
			tmp = tmp->next;
	}			
	return tmp;
}

static xsd__dateTime calcEndDate( int duration, const char *unit )
{
	xsd__dateTime result = 0l;

	if ( strCmp( unit, OPTION_DAYS_UNIT ) == 0 )
		result = duration * SECS_PER_DAY;
	else if ( strCmp( unit, OPTION_MONTHS_UNIT ) == 0 )
		result = duration * SECS_PER_MONTH;
	return result;	
}

static int convMode( const char *mode )
{
	if ( strCmp( mode, OPTION_MODE_DISABLE_STRING ) )
		return 0;
	if ( strCmp( mode, OPTION_MODE_ENABLE_W_EXP_STRING ) ) 
		return 1;
	if ( strCmp( mode, OPTION_MODE_ENABLE_WO_EXP_STRING ) )
		return 2;
	return 0;
}

static void printOption ( OptionEntry *oe )
{
	DEBUG_OUTPUT (
			dbglog (SVR_INFO, DBG_OPTIONS, "Option: %ld %s\n", oe->voucherSN, oe->optionName );
	)
}

static int checkDeviceIdData( struct DeviceId *dev1, struct DeviceId *dev2 )
{
	if ( !strCmp( dev1->Manufacturer, dev2->Manufacturer ) )
		return ERR_REQUEST_DENIED;
	if ( !strCmp( dev1->OUI, dev2->OUI ) != 0 )
		return ERR_REQUEST_DENIED;
	if ( !strCmp( dev1->ProductClass, dev2->ProductClass ) )
		return ERR_REQUEST_DENIED;
	if ( !strCmp( dev1->SerialNumber, dev2->SerialNumber ) )
		return ERR_REQUEST_DENIED;
	return OK;	
}

static int countOptions( void )
{
	register OptionEntry *tmp = firstEntry;
	register int cnt = 0;
	
	while ( tmp != NULL ) {
		cnt ++;
		tmp = tmp->next;
	}
	return cnt;
}

#endif /* HAVE_VOUCERS_OPTIONS */
