/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#include "cybtldr_command.h"
#include "cybtldr_api.h"

/* The highest number of memory arrays for any device. This includes flash and EEPROM arrays */
#define MAX_DEV_ARRAYS    0x80
/* The default value if a flash array has not yet received data */
#define NO_FLASH_ARRAY_DATA 0
/* The maximum number of flash arrays */
#define MAX_FLASH_ARRAYS 0x40
/* The minimum array id for EEPROM arrays. */
#define MIN_EEPROM_ARRAY 0x40

unsigned long g_validRows[MAX_FLASH_ARRAYS];
static CyBtldr_CommunicationsData* g_comm;

int BootLoaderCommunicator::transferData() {
	int err = g_comm->WriteData(cmdBuf, cmdSize);
	if (responseSize && (CYRET_SUCCESS == err))
		err = g_comm->ReadData(responseBuf, responseSize);
	if (CYRET_SUCCESS != err)
		err |= CYRET_ERR_COMM_MASK;
	return err;
}

int BootLoaderCommunicator::communicate() {
//	printf("Communicator %s \n", getDesc());
	int err = transferData();
	if (CYRET_SUCCESS == err)
		err = parseResult();
	if (CYRET_SUCCESS != status)
		err = status | CYRET_ERR_BTLDR_MASK;
	return err;
}

int CyBtldr_ValidateRow(unsigned char arrayId, unsigned short rowNum) {
	int err = CYRET_SUCCESS;
	if (arrayId < MAX_FLASH_ARRAYS) {
		if (NO_FLASH_ARRAY_DATA == g_validRows[arrayId]) {
			GetFlashSizeCommunicator communicator(arrayId);
			err = communicator.communicate();
			if (CYRET_SUCCESS == err)
				g_validRows[arrayId] = (communicator.minRow << 16)
						+ communicator.maxRow;
		}
		if (CYRET_SUCCESS == err) {
			unsigned short minRow =
					(unsigned short) (g_validRows[arrayId] >> 16);
			unsigned short maxRow = (unsigned short) g_validRows[arrayId];
			if (rowNum < minRow || rowNum > maxRow)
				err = CYRET_ERR_ROW;
		}
	} else
		err = CYRET_ERR_ARRAY;
	return err;
}

int CyBtldr_StartBootloadOperation(CyBtldr_CommunicationsData* comm, unsigned long * siliconId,
            unsigned char * siliconRev, unsigned long* blVer, const unsigned char* securityKeyBuf)
{
    const unsigned long SUPPORTED_BOOTLOADER = 0x010000;
    const unsigned long BOOTLOADER_VERSION_MASK = 0xFF0000;
    *siliconRev = 0;
    *siliconId = 0;

    g_comm = comm;
    for (int i = 0; i < MAX_FLASH_ARRAYS; i++)
        g_validRows[i] = NO_FLASH_ARRAY_DATA;

    int err = g_comm->OpenConnection();
    if (CYRET_SUCCESS != err)
        return err | CYRET_ERR_COMM_MASK;

    EnterBootLoaderCommunicator communicator(siliconId, siliconRev, blVer, securityKeyBuf);
    err = communicator.communicate();
    if (CYRET_SUCCESS == err)
    {
        if ((*blVer & BOOTLOADER_VERSION_MASK) != SUPPORTED_BOOTLOADER)
            err = CYRET_ERR_VERSION;
    }
    return err;
}

int CyBtldr_EndBootloadOperation(void)
{
    ExitBootLoaderCommunicator communicator;
    int err = communicator.communicate();
    if (CYRET_SUCCESS == err)
    {
        err = g_comm->CloseConnection();
        g_comm = NULL;
        if (CYRET_SUCCESS != err)
            err |= CYRET_ERR_COMM_MASK;
    }
    return err;
}

int CyBtldr_ProgramRow(unsigned char arrayID, unsigned short rowNum, unsigned char* buf, unsigned short size)
{
    const int TRANSFER_HEADER_SIZE = 11;
    unsigned offset = 0;
    unsigned short subBufSize;
    int err = CYRET_SUCCESS;

    if (arrayID < MAX_FLASH_ARRAYS)
        err = CyBtldr_ValidateRow(arrayID, rowNum);

    //Break row into pieces to ensure we don't send too much for the transfer protocol
    while ((CYRET_SUCCESS == err) && ((size - offset + TRANSFER_HEADER_SIZE) > g_comm->MaxTransferSize))
    {
        subBufSize = (unsigned short)(g_comm->MaxTransferSize - TRANSFER_HEADER_SIZE);
        SendDataCommunicator communicator(&buf[offset], subBufSize);
        err = communicator.communicate();
        offset += subBufSize;
    }

    if (CYRET_SUCCESS == err)
    {
        subBufSize = (unsigned short)(size - offset);
        ProgramRowCommunicator communicator(arrayID, rowNum, &buf[offset], subBufSize);
        err = communicator.communicate();
    }
    return err;
}

int CyBtldr_VerifyApplication() {
	VerifyChecksumCommunicator communicator;
	int err = communicator.communicate();
	if ((CYRET_SUCCESS == err) && (!communicator.checksumValid))
		err = CYRET_ERR_CHECKSUM;
	return err;
}
