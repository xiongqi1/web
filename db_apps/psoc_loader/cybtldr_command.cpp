/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#include "cybtldr_command.h"
#include <string.h>

void parse_appData(struct psoc_appdata * pAppdata, unsigned char * ptr);
const char * dumpMetadata(struct psoc_appdata * pAppdata);
void dumpBuffer(unsigned char* data, int size);
void setRdbSn(const char* value);


/* Variable used to store the currently selected packet checksum type */
CyBtldr_ChecksumType CyBtldr_Checksum = SUM_CHECKSUM;

unsigned short CyBtldr_ComputeChecksum(unsigned char* buf, unsigned long size)
{
    if (CyBtldr_Checksum == CRC_CHECKSUM)
    {
	    unsigned short crc = 0xffff;
	    unsigned short tmp;
	    int i;

	    if (size == 0)
		    return (~crc);

	    do
	    {
		    for (i = 0, tmp = 0x00ff & *buf++; i < 8; i++, tmp >>= 1)
		    {
			    if ((crc & 0x0001) ^ (tmp & 0x0001))
				    crc = (crc >> 1) ^ 0x8408;
			    else
			        crc >>= 1;
		    }
	    }
	    while (--size);

	    crc = ~crc;
        tmp = crc;
	    crc = (crc << 8) | (tmp >> 8 & 0xFF);

	    return crc;
    }
    else /* SUM_CHECKSUM */
    {
        unsigned short sum = 0;
	    while (size-- > 0)
		    sum += *buf++;

	    return (1 + ~sum);
    }
}

void CyBtldr_SetCheckSumType(CyBtldr_ChecksumType chksumType)
{
    CyBtldr_Checksum = chksumType;
}


/*
 * This is the common command to format up a packet buffer for transmission
 * The caller provides the command and a data buffer.
 * Offset gives the number of bytes that the caller has set up in addition to the data buffer
 */
void BootLoaderCommunicator::createCmdCommon(unsigned char cmd,
		const unsigned char* data, unsigned size, unsigned offset) {
	cmdBuf[0] = CMD_START;
	cmdBuf[1] = cmd;
	cmdBuf[2] = (unsigned char) (size + offset);
	cmdBuf[3] = (unsigned char) ((size + offset) >> 8);
	for (unsigned i = 0; i < size; i++)
		cmdBuf[i + 4 + offset] = data[i];
	cmdSize = size + offset + BASE_CMD_SIZE;
	unsigned short checksum = CyBtldr_ComputeChecksum(cmdBuf, cmdSize-3 );
	cmdBuf[cmdSize - 3] = (unsigned char) checksum;
	cmdBuf[cmdSize - 2] = (unsigned char) (checksum >> 8);
	cmdBuf[cmdSize - 1] = CMD_STOP;
}

/*
 * This routine provides a basic sanity check of the response
 */
int BootLoaderCommunicator::parseResultcommon() {
	int size = responseSize - BASE_CMD_SIZE;
	if (responseBuf[0] != CMD_START) {
		return CYRET_ERR_DATA;
	}
	if (responseBuf[responseSize - 1] != CMD_STOP) {
		return CYRET_ERR_DATA;
	}
	if (responseBuf[2] != size) {
		return CYRET_ERR_DATA;
	}
	if (responseBuf[3] != (size >> 8)) {
		return CYRET_ERR_DATA;
	}
	status = responseBuf[1];
	if (CYRET_SUCCESS != status)
		return status | CYRET_ERR_BTLDR_MASK;
	return CYRET_SUCCESS;
}

/*
 * Perform specific actions on the response
 */
int EnterBootLoaderCommunicator::parseResult() {
	int err = parseResultcommon();
	if (CYRET_SUCCESS == err) {
		*siliconId = (responseBuf[7] << 24) | (responseBuf[6] << 16)
				| (responseBuf[5] << 8) | responseBuf[4];
		*siliconRev = responseBuf[8];
		*blVer = (responseBuf[11] << 16) | (responseBuf[10] << 8)
				| responseBuf[9];
	}
	return err;
}

/*
 * Perform specific actions on the response
 */
int VerifyChecksumCommunicator::parseResult() {
	int err = parseResultcommon();
	if (CYRET_SUCCESS == err) {
		checksumValid = responseBuf[4];
	}
	return err;
}

/*
 * Perform specific actions on the response
 */
int GetMetaDataCommunicator::parseResult() {
	int err = parseResultcommon();
	if (CYRET_SUCCESS == err) {
		unsigned char * ptr = responseData();
		int size = responseDataSize();
		parse_appData(pAppdata, ptr);
		printf("App data from device %s\n", dumpMetadata(pAppdata));
#define SN_START 8
#define SN_SIZE 15
		dumpBuffer(&ptr[SN_START], size-SN_START);

		// The boards serial number is 15 ascii digits
		// May or may not be null terminated so be sure
		char sn[SN_SIZE+1];
		memcpy(sn, &ptr[SN_START], SN_SIZE);
		sn[SN_SIZE] = 0;
		setRdbSn(sn);
	}
	return err;
}

/*
 *  Read an EEPROM row
 */
int GetEEPromRowCommunicator::parseResult() {
	int err = parseResultcommon();
	if (CYRET_SUCCESS == err) {
		unsigned char * ptr = responseData();
		int size = responseDataSize();
		printf("EEPROM data from device - ");
		dumpBuffer(&ptr[0], size);
		printf("\n");
	}
	return err;
}

/*
 * Perform specific actions on the response
 */
int GetFlashSizeCommunicator::parseResult()
{
    int err = parseResultcommon();
    if (CYRET_SUCCESS == err) {
        minRow = (responseBuf[5] << 8) | responseBuf[4];
        maxRow = (responseBuf[7] << 8) | responseBuf[6];
    }
    return err;
}

