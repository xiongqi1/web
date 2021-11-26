/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#ifndef __CYBTLDR_API2_H__
#define __CYBTLDR_API2_H__

#include "cybtldr_utils.h"

int CyBtldr_doProgram( struct psoc_appdata * pAppdata);

/*******************************************************************************
* Function Name: CyBtldr_Program
********************************************************************************
* Summary:
*   This function reprograms the bootloadable portion of the PSoC�s flash with
*   the contents of the provided *.cyacd file.
*
* Parameters:
*   securityKey - The 6 byte or null security key used to authenticate with bootloader component
*   comm        - Communication struct used for communicating with the target device
*
* Returns:
*   CYRET_SUCCESS	    - The device was programmed successfully
*   CYRET_ERR_DEVICE	- The detected device does not match the desired device
*   CYRET_ERR_VERSION	- The detected bootloader version is not compatible
*   CYRET_ERR_LENGTH	- The result packet does not have enough data
*   CYRET_ERR_DATA	    - The result packet does not contain valid data
*   CYRET_ERR_ARRAY	    - The array is not valid for programming
*   CYRET_ERR_ROW	    - The array/row number is not valid for programming
*   CYRET_ERR_BTLDR	    - The bootloader experienced an error
*   CYRET_ERR_COMM	    - There was a communication error talking to the device
*   CYRET_ABORT		    - The operation was aborted
*
*******************************************************************************/
EXTERN int CyBtldr_Program(const unsigned char* securityKey, CyBtldr_CommunicationsData* comm);

#endif
