/***************************************************************************
 *    Copyright (C) 2004-2010 Dimark Software Inc.                         *
 *    support@dimark.com                                                   *
 ***************************************************************************/

#ifndef PARAMETERSTORE_H
#define PARAMETERSTORE_H

#include "utils.h" 
#include "parameter.h"
#include "paramaccess.h"

#define NO_PARAMETER_FILE_EXISTS 	0
#define PARAMETER_DATA_FILE_EXISTS 	1
#define PARAMETER_META_FILE_EXISTS 	2

typedef int (newParam) (char *, char *);

/** Read the initial parameter file line by line and use the callback function
 * to store the data in dimclient. After that all additional parameters
 * created by storeParameter() and the updateParameters has to be read.
 * 
 */ 
int loadInitialParameterFile (newParam *);

/** Store one parameter in the persistent memory.
 * The data contains all parameter data.
 * This function is used to create a new parameter which is not in the
 * initial parameter file.
 * 
 */
int storeParameter( const char *, const char * );

/** Update one parameter, which is in the initial parameter file
 * but has be updated by the ACS or the hostsystem.
 * Only data which can be changed is stored.
 */
int updateParameter( const char *, const char * );

/** Removes a parameter from the storage
 */
int removeParameter( const char * );

int removeAllParameters( void );

/** Init the value of a parameter given as name in the first argument
 */
int initParamValue( const char *, ParameterType, ParameterValue * );

/** Store the value of a parameter given as name in the first argument
 */
int storeParamValue( const char *, ParameterType, ParameterValue * );

/** Retrieve the value of a parameter, which name is in first argument
 */
int retrieveParamValue( const char *, ParameterType, ParameterValue * );

/** Check the existence of a parameter data and/or metadata file
 * Return 
 * 	0 = no file exists
 *  1 = data file exists
 *  2 = meta file exists
 *  3 = both files exists
 */
unsigned int checkParameter( const char * );

#endif /*PARAMETERSTORE_H*/
