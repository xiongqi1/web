/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

/** Example implementation for parameter storage.
 * 
 * All parameter data is stored in the file system.
 * 
 */

#include <dirent.h>

#include "parameterStore.h"
#include "globals.h"
#include "debug.h"
#include "unistd.h"
#include "storage.h"

#include "luaCore.h"
#include "luaParameter.h"

#define BUFLEN 1024

#define		MAX_PATH_NAME_SIZE		400
#define 	MAX_PARAM_PATH_LENGTH 	257


static int loadParamFile (char*, char *, newParam *);
static int reloadParameters( newParam *);


/** Load the initial parameter file ( normally tmp.param )
 * and call callbackF for every line read.
 */
int loadInitialParameterFile( newParam *callbackF )
{
	int ret = OK;
	
	ret = loadParamFile(DEFAULT_PARAMETER_FILE, NULL, callbackF);
//	ret += reloadParameters(callbackF);

	return ret;
}

/** Store the metadata of parameter in name 
 * The metadata is given in a character string
 */
int storeParameter( const char *name, const char *data)
{
	if(li_param_meta_store(name, data)) return ERR_RESOURCE_EXCEED;

	return OK;
}

/** Update one parameter, which is in the initial parameter file
 * but has be updated by the ACS or the hostsystem.
 * Only data which can be changed is stored.
 */ 
int updateParameter( const char *name, const char *data)
{
	// AY: this function is never used it seems!
	
	if(li_param_meta_store(name, data)) return ERR_RESOURCE_EXCEED;

	return OK;
}

/** Check the existence of a parameter data and/or metadata file
 * Return 
 * 	0 = no file exists
 *  1 = data file exists
 *  2 = meta file exists
 *  3 = both files exists
 */
unsigned int checkParameter( const char *name )
{
	char *buf = NULL;
	size_t len = 0;
	int ret = NO_PARAMETER_FILE_EXISTS;

	if(li_param_meta_retrieve(name, &buf, &len)) ret |= PARAMETER_META_FILE_EXISTS;
	if(li_param_value_retrieve(name, &buf, &len)) ret |= PARAMETER_DATA_FILE_EXISTS;

	return ret;
}

/** remove a named parameter from the persistent storage
 * remove the data and the metadata file
 * 
 */
int removeParameter( const char *name )
{
	if(li_param_meta_delete(name) || li_param_value_delete(name)) return ERR_INTERNAL_ERROR;

	return OK;
}

/** Remove all data and metadata files.
 * Used by factory reset.
 */
int removeAllParameters( void )
{
	if(li_param_delete()) return ERR_INTERNAL_ERROR;

	return OK;
}

/** Initilise the data value of a parameter
 */
int initParamValue( const char *name, ParameterType type , ParameterValue *value )
{
	int ret = OK;
	char buf[BUFLEN];

	// Ignore calls without data
	if ( value == NULL )
		return ret;

	switch (type)
	{
		case DefIntegerType:
		case IntegerType:
		case DefBooleanType:
		case BooleanType:
			snprintf(buf, BUFLEN, "%d", value->in_int);
			ret = li_param_value_init(name, buf);
			break;
		case DefUnsignedIntType:
		case UnsignedIntType:
			snprintf(buf, BUFLEN, "%u", value->in_uint);
			ret = li_param_value_init(name, buf);
			break;
		case DefStringType:
		case StringType:
		case DefBase64Type:
		case Base64Type:
			ret = li_param_value_init(name, value->in_cval);
			break;
		case DefDateTimeType:
		case DateTimeType:
			snprintf(buf, BUFLEN, "%lu", value->in_timet);
			ret = li_param_value_init(name, buf);
			break;
		default:
			DEBUG_OUTPUT(
				dbglog(SVR_ERROR, DBG_PARAMETER, "initParamValue: Unknown ParameterType: %d\n", type);
			)
			abort();
			break;
	}		

	return ret;
}


/** Store the data value of a parameter
 */
int storeParamValue( const char *name, ParameterType type , ParameterValue *value )
{
	int ret = OK;
	char buf[BUFLEN];

	// Ignore calls without data
	if ( value == NULL )
		return ret;

	switch (type)
	{
		case DefIntegerType:
		case IntegerType:
		case DefBooleanType:
		case BooleanType:
			snprintf(buf, BUFLEN, "%d", value->in_int);
			ret = li_param_value_store(name, buf);
			break;
		case DefUnsignedIntType:
		case UnsignedIntType:
			snprintf(buf, BUFLEN, "%u", value->in_uint);
			ret = li_param_value_store(name, buf);
			break;
		case DefStringType:
		case StringType:
		case DefBase64Type:
		case Base64Type:
			ret = li_param_value_store(name, value->in_cval);
			break;
		case DefDateTimeType:
		case DateTimeType:
			snprintf(buf, BUFLEN, "%lu", value->in_timet);
			ret = li_param_value_store(name, buf);
			break;
		default:
			DEBUG_OUTPUT(
				dbglog(SVR_ERROR, DBG_PARAMETER, "storeParamValue: Unknown ParameterType: %d\n", type);
			)
			abort();
			break;
	}		

	return ret;
}
 	
/** Get the data value of the parameter given in name
 */
int retrieveParamValue( const char *name, ParameterType type, ParameterValue *value )
{
	int ret = OK;
	size_t len = 0;
	char *buf = NULL;

	switch (type) {
		case DefIntegerType:
		case IntegerType:
		case DefBooleanType:
		case BooleanType:
			ret = li_param_value_retrieve(name, &buf, &len);
			if(ret) return ret;
			ret = sscanf(buf, "%d", &value->out_int);
			free(buf);
			break;
		case DefUnsignedIntType:
		case UnsignedIntType:
			ret = li_param_value_retrieve(name, &buf, &len);
			if(ret) return ret;
			ret = sscanf(buf, "%u", &value->out_uint);
			free(buf);
			break;
		case DefStringType:
		case StringType:
		case DefBase64Type:
		case Base64Type:
			ret = li_param_value_retrieve(name, &buf, &len);
			if(ret) return ret;
			value->out_cval = emallocTemp(len + 1);
			memcpy(value->out_cval, buf, len);
			value->out_cval[len] = 0;
			free(buf);
			ret = 1;
			break;
		case DefDateTimeType:
		case DateTimeType:
			ret = li_param_value_retrieve(name, &buf, &len);
			if(ret) return ret;
			ret = sscanf(buf, "%lu", &value->out_timet);
			free(buf);
			break;
		default:
			DEBUG_OUTPUT(
				dbglog(SVR_ERROR, DBG_PARAMETER, "retrieveParamValue: Unknown ParameterType: %d\n", type);
			)
			abort();
			break;
	}

	if(ret != 1) return ERR_INVALID_PARAMETER_VALUE;

	return OK;
}

static 
int loadParamFile (char *filename, char *name, newParam* callbackF)
{
	int ret = OK;
	char buf[MAX_PATH_NAME_SIZE + 1];
	FILE *file;

	file = fopen (filename, "r");
	if (file == NULL)
	{
		DEBUG_OUTPUT (
				dbglog (SVR_ERROR, DBG_PARAMETER, "loadInitialParameters: file not found %s\n", filename);
		)

		return ERR_INTERNAL_ERROR;
	}

	while (fgets (buf, MAX_PATH_NAME_SIZE, file) != NULL)
	{
		if ( buf[0] == '#' || buf[0] == ' ' || buf[0] == '\n' || buf[0] == '\0' )
			continue;
		buf[strlen (buf) - 1] = '\0';	/* remove trailing EOL  */
		ret = callbackF (name, buf);
	}
	fclose (file);
	return ret;
} 

static 
int reloadParameters( newParam *callbackF )
{
	char buf[MAX_PATH_NAME_SIZE];
	struct dirent *entry;
	int ret = OK;
	DIR *dir;
	int nfiles;
	bool readLoop = true;

	nfiles = 0;
	while( readLoop ) {
		readLoop = false;
		dir = opendir (PERSISTENT_PARAMETER_DIR);
		if (dir == NULL)
		{
			DEBUG_OUTPUT (
					dbglog (SVR_ERROR, DBG_PARAMETER, "reloadParameters: directory not found %s\n", PERSISTENT_PARAMETER_DIR);
			)

			return ERR_INTERNAL_ERROR;
		}
		while ((entry = readdir (dir)) != NULL)
		{
			if (strcmp (entry->d_name, ".") == 0)
				continue;
			if (strcmp (entry->d_name, "..") == 0)
				continue;
			strcpy (buf, PERSISTENT_PARAMETER_DIR);
			strcat (buf, entry->d_name);

			DEBUG_OUTPUT (
					dbglog (SVR_INFO, DBG_PARAMETER, "reloadParameters: %s\n", buf);
			)

			ret = loadParamFile (buf, entry->d_name, callbackF);
			// If one parent is not found we don't leave the readLoop
			if ( ret == INFO_PARENT_NOT_FOUND )
				readLoop = true;
		}
		closedir (dir);
	}
	return OK;
}



