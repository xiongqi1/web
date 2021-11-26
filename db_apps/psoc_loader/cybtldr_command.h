/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#ifndef __CYBTLDR_COMMAND_H__
#define __CYBTLDR_COMMAND_H__

#include "cybtldr_utils.h"

/* Maximum number of bytes to allocate for a single command.  */
#define MAX_COMMAND_SIZE 512

//STANDARD PACKET FORMAT:
// Multi byte entries are encoded in LittleEndian.
/*******************************************************************************
* [1-byte] [1-byte ] [2-byte] [n-byte] [ 2-byte ] [1-byte]
* [ SOP  ] [Command] [ Size ] [ Data ] [Checksum] [ EOP  ]
*******************************************************************************/

/* The first byte of any boot loader command. */
#define CMD_START               0x01
/* The last byte of any boot loader command. */
#define CMD_STOP                0x17
/* The minimum number of bytes in a bootloader command. */
#define BASE_CMD_SIZE           0x07

/* Command identifier for verifying the checksum value of the bootloadable project. */
#define CMD_VERIFY_CHECKSUM     0x31
/* Command identifier for getting the number of flash rows in the target device. */
#define CMD_GET_FLASH_SIZE      0x32
/* Command identifier for getting info about the app status. This is only supported on multi app bootloader. */
#define CMD_GET_APP_STATUS      0x33
/* Command identifier for reasing a row of flash data from the target device. */
#define CMD_ERASE_ROW           0x34
/* Command identifier for making sure the bootloader host and bootloader are in sync. */
#define CMD_SYNC                0x35
/* Command identifier for setting the active application. This is only supported on multi app bootloader. */
#define CMD_SET_ACTIVE_APP      0x36
/* Command identifier for sending a block of data to the bootloader without doing anything with it yet. */
#define CMD_SEND_DATA           0x37
/* Command identifier for starting the boot loader.  All other commands ignored until this is sent. */
#define CMD_ENTER_BOOTLOADER    0x38
/* Command identifier for programming a single row of flash. */
#define CMD_PROGRAM_ROW         0x39
/* Command identifier for verifying the contents of a single row of flash. */
#define CMD_VERIFY_ROW          0x3A
/* Command identifier for exiting the bootloader and restarting the target program. */
#define CMD_EXIT_BOOTLOADER     0x3B
/* Command identifier for retrieving metadata. */
#define CMD_GET_METADATA     0x3C
/* Command identifier for getting a row from eeprom. */
#define CMD_GET_EEPROM_ROW     0x3D

/*
 * This enum defines the different types of checksums that can be
 * used by the bootloader for ensuring data integrety.
 */
typedef enum
{
    /* Checksum type is a basic inverted summation of all bytes */
    SUM_CHECKSUM = 0x00,
    /* 16-bit CRC checksum using the CCITT implementation */
    CRC_CHECKSUM = 0x01,
} CyBtldr_ChecksumType;

/*******************************************************************************
* Function Name: CyBtldr_ComputeChecksum
********************************************************************************
* Summary:
*   Computes the 2byte checksum for the provided command data.  The checksum is
*   the 2's complement of the 1-byte sum of all bytes.
*
* Parameters:
*   buf  - The data to compute the checksum on
*   size - The number of bytes contained in buf.
*
* Returns:
*   The checksum for the provided data.
*
*******************************************************************************/
unsigned short CyBtldr_ComputeChecksum(unsigned char* buf, unsigned long size);

/*******************************************************************************
* Function Name: CyBtldr_SetCheckSumType
********************************************************************************
* Summary:
*   Updates what checksum algorithm is used when generating packets
*
* Parameters:
*   chksumType - The type of checksum to use when creating packets
*
* Returns:
*   NA
*
*******************************************************************************/
void CyBtldr_SetCheckSumType(CyBtldr_ChecksumType chksumType);

/*
 * This is the base ( abstract ) class for communicating with the boot loader on the PSoC
 * This class provide routines to send and receive data to the PSoC as well
 * as some helper routines to format the command and decode the response
 * Derived classes must provide routines to format the specific command and decode the specific response
 * as well as setting responseSize
 */
class BootLoaderCommunicator {
public:
	/*
	 * This is the main function called by the application.
	 * This calls the relevant createCmd(), does the IO and calls the parseResult()
	 */
	int communicate();

	unsigned char responseBuf[MAX_COMMAND_SIZE];
	unsigned char cmdBuf[MAX_COMMAND_SIZE];
	unsigned char status;
	unsigned cmdSize;
	unsigned responseSize;

	unsigned char * responseData() {return &responseBuf[4];}
	int responseDataSize() {return responseSize - BASE_CMD_SIZE;}
	/*
	 * This routine formats the buffer with the command and data
	 * Parameters
	 * cmd	- the opcode the the command
	 * data	- pointer to the buffer to be parsed
	 * size - amount of data pointed to by data
	 * offset - This gives the amount of additional data that is set up manually by the derived class's createCmd()
	 */
	void createCmdCommon(unsigned char cmd, const unsigned char* data,
			unsigned size, unsigned offset = 0);
	/*
	 * This routine does some basic checks of the response.
	 */
	int parseResultcommon();
private:
	virtual int parseResult() { return parseResultcommon();};
	virtual const char * getDesc() { return "Unk"; };
	/*
	 * This routine performs the read and write of the data
	 */
	int transferData();
};

/*
 * Communicator class to set the Metadata on the PSoC
 * The constructor is given a pointer to, and size of, the Metadata
 */
class GetEEPromRowCommunicator: public BootLoaderCommunicator {
public:
	GetEEPromRowCommunicator(unsigned short rowNum) {
		/*
		 * Format the command and set the expected response size
		 */
		unsigned char buf[3];
		buf[0] = 0;		//arrayId;
		buf[1] = (unsigned char)rowNum;
		buf[2] = (unsigned char)(rowNum >> 8);
		createCmdCommon(CMD_GET_EEPROM_ROW, buf, sizeof(buf) );
		responseSize = BASE_CMD_SIZE + 16;
	}
private:
	int parseResult();
	virtual const char * getDesc() { return "SetMetaData"; };
};

/*
 * Communicator class to fetch the Metadata from the PSoC
 * The constructor is given the pointer to assemble the received data into
 */
class GetMetaDataCommunicator: public BootLoaderCommunicator {
public:
	GetMetaDataCommunicator(struct psoc_appdata * _pAppdata) :
			pAppdata(_pAppdata) {
		/*
		 * Format the command and set the expected response size
		 */
	    createCmdCommon(CMD_GET_METADATA, NULL, 0);
	    responseSize = BASE_CMD_SIZE + 56;
	}
private:
	struct psoc_appdata * pAppdata;
	int parseResult();
	virtual const char * getDesc() { return "GetMetaData"; };
};

/*
 * Communicator class end the boot loader transactions and start the PSoC application

 */
class ExitBootLoaderCommunicator: public BootLoaderCommunicator {
public:
	ExitBootLoaderCommunicator()
	{
		/*
		 * Format the command and set the expected response size
		 */
	    createCmdCommon(CMD_EXIT_BOOTLOADER, NULL, 0);
	    responseSize = 0;
	}
private:
	int parseResult(){ return CYRET_SUCCESS;};
	virtual const char * getDesc() { return "ExitBootLoader"; };
};

/*
 * Communicator class to begin boot loader transactions and fetch some information from the PSoC
 * The constructor is given pointers for the data to be retrieved
 */
class EnterBootLoaderCommunicator: public BootLoaderCommunicator {
public:
	EnterBootLoaderCommunicator(unsigned long * _siliconId,
			unsigned char * _siliconRev, unsigned long* _blVer,
			const unsigned char* securityKeyBuf) :
			siliconId(_siliconId), siliconRev(_siliconRev), blVer(_blVer) {
		/*
		 * Format the command and set the expected response size
		 */
		const unsigned RESULT_DATA_SIZE = 8;
		const unsigned BOOTLOADER_SECURITY_KEY_SIZE = 6;
		createCmdCommon(CMD_ENTER_BOOTLOADER, securityKeyBuf,
				securityKeyBuf != NULL ? BOOTLOADER_SECURITY_KEY_SIZE : 0);
		responseSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
	}
private:
	unsigned long * siliconId;
	unsigned char * siliconRev;
	unsigned long* blVer;
	int parseResult();
	virtual const char * getDesc() { return "EnterBootLoader"; };
};

/*
 * Communicator class to retrieve the size of the flash array
 * The constructor is given the Id of the array and the class
 * fills members minRow, maxRow
 */
class GetFlashSizeCommunicator: public BootLoaderCommunicator {
public:
	unsigned short minRow;
	unsigned short maxRow;
	GetFlashSizeCommunicator(unsigned char arrayId) {
		/*
		 * Format the command and set the expected response size
		 */
		const unsigned RESULT_DATA_SIZE = 4;
		const unsigned COMMAND_DATA_SIZE = 1;
		createCmdCommon(CMD_GET_FLASH_SIZE, &arrayId, COMMAND_DATA_SIZE );
		responseSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
	};
private:
	int parseResult();
	virtual const char * getDesc() { return "GetFlashSize"; };
};

/*
 * Communicator class verify the PSoC application's validity
 * The member
 */
class VerifyChecksumCommunicator: public BootLoaderCommunicator {
public:
	unsigned char checksumValid;
	VerifyChecksumCommunicator() {
		/*
		 * Format the command and set the expected response size
		 */
		const unsigned RESULT_DATA_SIZE = 1;
		createCmdCommon(CMD_VERIFY_CHECKSUM, NULL, 0);
		responseSize = BASE_CMD_SIZE + RESULT_DATA_SIZE;
	}

private:
	int parseResult();
	virtual const char * getDesc() { return "VerifyChecksum"; };
};

/*
 * Communicator class to send data to the PSoC
 * The constructor is given a pointer to, and size of, the data
 */
class SendDataCommunicator: public BootLoaderCommunicator {
public:
	SendDataCommunicator(const unsigned char* data, unsigned size){
		/*
		 * Format the command and set the expected response size
		 */
	    createCmdCommon(CMD_SEND_DATA, data, size );
	    responseSize = BASE_CMD_SIZE;
	};
private:
	virtual const char * getDesc() { return "SendData"; };
};

/*
 * Communicator class to send data to, and program the PSoC
 * The constructor is given a pointer to, and size of, the data and also
 * the array and row to program
 */
class ProgramRowCommunicator: public BootLoaderCommunicator {
public:
	ProgramRowCommunicator(unsigned char arrayId, unsigned short rowNum,
			const unsigned char* data, unsigned size) {
		/*
		 * Format the command and set the expected response size
		 */
		const unsigned long COMMAND_DATA_SIZE = 3;
		cmdBuf[4] = arrayId;
		cmdBuf[5] = (unsigned char)rowNum;
		cmdBuf[6] = (unsigned char)(rowNum >> 8);
		createCmdCommon(CMD_PROGRAM_ROW, data, size, COMMAND_DATA_SIZE );
		responseSize = BASE_CMD_SIZE;
	};
private:
	virtual const char * getDesc() { return "SendData"; };
};

#endif
