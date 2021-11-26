/*******************************************************************************
* Copyright 2011-2012, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
********************************************************************************/

#include <string.h>
#include <stdlib.h>

#include "cybtldr_command.h"
#include "cybtldr_parse.h"
#include "cybtldr_api.h"
#include "cybtldr_api2.h"

extern char * firmwareFilename;
extern char * metadataString;
extern short eepromRow;
extern char * eepromFilename;
extern int eepromRowCnt;

void    setRdbBootloaderVersion(unsigned short Ver);
void    setRdbAppVersion(unsigned short Ver);
void    setRdbUnitType( const char * utype);
void    setRdbStatus(const char* value);

unsigned char g_abort;
void dumpBuffer(unsigned char* data, int size);
void dumpBufferToFile(FILE * f, unsigned char* data, int size);

static int getEndian(unsigned long siliconId)
{
    if ( siliconId == 0x1e080069L)
        return BIG_ENDIAN;
    return LITTLE_ENDIAN;
}
static unsigned long longAt(unsigned char ptr[], int endian )
{
    if ( endian == BIG_ENDIAN )
        return (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
    return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
}

static unsigned short shortAt(unsigned char ptr[], int endian )
{
    if ( endian == BIG_ENDIAN )
        return (ptr[0] << 8) | ptr[1];
    return (ptr[1] << 8) | ptr[0];
}
/*
static unsigned char byteAt(unsigned char ptr[] )
{
    return ptr[0];
}
*/

const char * dumpMetadata(struct psoc_appdata * pAppdata)
{
    static char tmpbuf[128];
    sprintf(tmpbuf,"Appdata - siliconId 0x%lx, siliconRev 0x%x appId 0x%x, appVers 0x%x, appCustomId 0x%lx", pAppdata->siliconId, pAppdata->siliconRev, pAppdata->appId, pAppdata->appVers,pAppdata->appCustomId );
    return tmpbuf;
}

void parse_appData(struct psoc_appdata * pAppdata, unsigned char * ptr )
{
    int endian = getEndian(pAppdata->siliconId);
    pAppdata->appId = shortAt(ptr, endian);
	pAppdata->appVers = shortAt(ptr+2, endian);
	pAppdata->appCustomId = longAt(ptr+4, endian);
}

/*
 * This function opens,reads, and closes the  PSoC firmware file to read in the app data metadata
 */
static int getMetadataFromFile(const char* file, struct psoc_appdata * pAppdata )
{
    char line[MAX_BUFFER_SIZE];
    unsigned int lineLen;
    int err;

    printf("Parse file %s\n", file);

    err = CyBtldr_OpenDataFile(file);
    if (CYRET_SUCCESS == err)
    {
        err = CyBtldr_ReadLine(&lineLen, line);
        if (CYRET_SUCCESS == err)
            err = CyBtldr_ParseHeader(lineLen, line, &pAppdata->siliconId, &pAppdata->siliconRev, &pAppdata->chksumtype);

        if (CYRET_SUCCESS == err)
        {
            // These 3 constants have been determined emperically by viewing the *.cyacd file
            // and looking for the relevant data setup in the Cypress IDE
            // The data is in the last row of flash so should be constant for this device type.

            int offsetofInfo= 0xc0+20;
            int rowOfInfo=0xFF;
            int arrayOfInfo=getEndian(pAppdata->siliconId) == LITTLE_ENDIAN ? 3 : 0;
            while (CYRET_SUCCESS == err)
            {
                err = CyBtldr_ReadLine(&lineLen, line);
                if (CYRET_SUCCESS == err) {
                    unsigned short rowNum = 0;
                    unsigned short bufSize = 0;
                    unsigned char checksum = 0;
                    unsigned char arrayId = 0;
                    unsigned char buffer[MAX_BUFFER_SIZE];
                    err = CyBtldr_ParseRowData(lineLen, line, &arrayId, &rowNum, buffer, &bufSize, &checksum);
//                    printf("File row  contains %d arrayId 0x%x, rowNum 0x%x\n", bufSize, arrayId, rowNum );
                    if ( (arrayId==arrayOfInfo)&&(rowNum==rowOfInfo)&&(bufSize>offsetofInfo)) {
//                        dumpBuffer(buffer+offsetofInfo,bufSize-offsetofInfo);
                        unsigned char * ptr = buffer+offsetofInfo;
                        parse_appData( pAppdata, ptr );
                        printf("from file %s\n", dumpMetadata(pAppdata) );
                        CyBtldr_CloseDataFile();
                        return err;
                    }
                }
            }
        }
        CyBtldr_CloseDataFile();
    }
    return err;
}

/*
 * Associate an App Id to a file name
 */
static const char * getFileNameFromMetadata( struct psoc_appdata * pAppdata )
{
    switch ( pAppdata->appId ) {
    case PSOC_APPID_IO_MICE:
        return "IO-Mice.cyacd";
    case PSOC_APPID_RF_MICE:
        return "RF-Mice.cyacd";
    case PSOC_APPID_GPS_MICE:
        return "GPS-Mice.cyacd";
    case PSOC_APPID_CHUBB_MICE:
        return "Chubb-Mice.cyacd";
    case PSOC_APPID_AERIS_MICE:
        return "Aeris-Mice.cyacd";
    case PSOC_APPID_IND_IO_MICE:
        return "IND-IO-Mice.cyacd";
    case PSOC_APPID_GPS_CAN_MICE:
        return "GPS-Mice_CAN.cyacd";
    default:
        break;
    }
    return 0;
}

/*
 * get unit description from App Id
 */
static const char * getUnitTypeFromMetadata( struct psoc_appdata * pAppdata )
{
    switch ( pAppdata->appId ) {
    case PSOC_APPID_IO_MICE:
        return "IO-Mice";
    case PSOC_APPID_RF_MICE:
        return "RF-Mice";
    case PSOC_APPID_GPS_MICE:
        return "GPS-Mice";
    case PSOC_APPID_CHUBB_MICE:
        return "Chubb-Mice";
    case PSOC_APPID_AERIS_MICE:
        return "Aeris-Mice";
    case PSOC_APPID_IND_IO_MICE:
        return "IND-IO-Mice";
    case PSOC_APPID_GPS_CAN_MICE:
        return "GPS-CAN-Mice";
    default:
        break;
    }
    return "Unknown unit";
}

int CyBtldr_doProgram(struct psoc_appdata * pAppdata)
{
    unsigned short rowNum = 0;
    unsigned short bufSize = 0;
    unsigned char arrayId = 0;
    unsigned char buffer[MAX_BUFFER_SIZE];
    char line[MAX_BUFFER_SIZE];
    unsigned int lineLen;
    const char * file;
    struct psoc_appdata fileAppdata;
    if ( firmwareFilename )
        file = firmwareFilename;
    else {
        file =  getFileNameFromMetadata(pAppdata);
        if ( !file) {
            printf("Could not determine file from metadata\n");
            return CYRET_ERR_DEVICE;
        }
    }
    // If file doesn't exist or not specified return the App status
    // If it was valid this will reboot the psoc into the application firmware
    if ( !file) return CYRET_ERR_FILE;

    int file_err = getMetadataFromFile(file, &fileAppdata);
    if (CYRET_SUCCESS != file_err){
        printf("Could get metadata from firmware file\n");
        return file_err;
    }
    // Don't do sanity checks if firmware specified at command line
    if ( !firmwareFilename ) {
        if ( pAppdata->appId != fileAppdata.appId ){
            printf("Firmware App Id does not match the PSoC App Id\n");
            return CYRET_ERR_FILE;
        }
        if ( pAppdata->appVers == fileAppdata.appVers ){
            printf("Firmware App version is the same as the PSoC App version - not reflashing\n");
            return CYRET_SUCCESS;
        }
        printf("Firmware App version 0x%x differs from the PSoC App version 0x%x - %s\n",
                fileAppdata.appVers, pAppdata->appVers,
                fileAppdata.appVers > pAppdata->appVers ? "upgrading" : "downgrading" );
    }
    int err = CyBtldr_OpenDataFile(file);
    if (CYRET_SUCCESS == err)
    {
        setRdbStatus("Loading firmware");
        err = CyBtldr_ReadLine(&lineLen, line);
        if (CYRET_SUCCESS == err)
        {
            while (CYRET_SUCCESS == err)
            {
                if (g_abort)
                {
                    err = CYRET_ABORT;
                    break;
                }

                err = CyBtldr_ReadLine(&lineLen, line);
                if (CYRET_SUCCESS == err) {
                    unsigned char checksum = 0;
                    err = CyBtldr_ParseRowData(lineLen, line, &arrayId, &rowNum, buffer, &bufSize, &checksum);
                }
                if (CYRET_SUCCESS == err)
                {
                    err = CyBtldr_ProgramRow(arrayId, rowNum, buffer, bufSize);
                }
                else if (CYRET_ERR_EOF == err)
                {
                    err = CYRET_SUCCESS;
                    break;
                }
            }

            if (CYRET_SUCCESS == err)
            {
                    err = CyBtldr_VerifyApplication();
                    setRdbAppVersion(fileAppdata.appVers);
                    setRdbUnitType( getUnitTypeFromMetadata(&fileAppdata) );
                    setRdbStatus("Firmware loaded");
            }
        }
        CyBtldr_CloseDataFile();
    }

    return err;
}

int convertHexString( char * pch, unsigned char * buf, int size ) {
	int charCnt = 0;
	int idx = 0;
	char dig = 0;
	while (true) {
		char ch = *pch++;
		charCnt++;
		dig <<= 4;
		if ((ch >= '0') && (ch <= '9'))
			dig |= ch - '0';
		else if ((ch >= 'a') && (ch <= 'f'))
			dig |= ch - 'a' + 10;
		else if ((ch >= 'A') && (ch <= 'F'))
			dig |= ch - 'A' + 10;
		else if ( ch == 0 )
			break;
		else
			return CYRET_ERR_DATA;
		if ((charCnt & 1) == 0) {
			buf[idx++] = dig;
			if (idx >= size)
				break;
		}
	}
	return CYRET_SUCCESS;
}

int CyBtldr_Program(const unsigned char* securityKey,
		CyBtldr_CommunicationsData* comm) {
	unsigned long blVer = 0;
	int err;
	struct psoc_appdata appdata;
	struct psoc_appdata * pAppdata = &appdata;
	memset(pAppdata, 0, sizeof(*pAppdata));
	pAppdata->chksumtype = SUM_CHECKSUM;

	g_abort = 0;
	setRdbStatus("Running bootloader");
	CyBtldr_SetCheckSumType(pAppdata->chksumtype);
	err = CyBtldr_StartBootloadOperation(comm, &pAppdata->siliconId,
			&pAppdata->siliconRev, &blVer, securityKey);
	if (CYRET_SUCCESS != err) {
		return err;
	}
	printf(
			"Started boot loader operation siliconId 0x%lx, siliconRev 0x%x, blVer 0x%lx\n",
			pAppdata->siliconId, pAppdata->siliconRev, blVer);
	setRdbBootloaderVersion((unsigned short) blVer);

	if ( eepromFilename ) {
		FILE * f = fopen( eepromFilename, "wt");
		if (!f) {
			printf("Could not open file %s",eepromFilename);
			return 1;
		}
		while ( eepromRowCnt-- ) {
			GetEEPromRowCommunicator communicator(eepromRow++);
			err = communicator.communicate();
			if (CYRET_SUCCESS != err) {
				printf("Could not read EEProm row %d\n", eepromRow);
				fclose(f);
				return 1;
			}
			dumpBufferToFile(f,communicator.responseData(), communicator.responseDataSize() );
		}
		fclose(f);
	}
	else if (metadataString) {
		unsigned char eeprombuf[16]; // Each eeprom row is 16 bytes
		memset(eeprombuf, 0, sizeof(eeprombuf));

		bool hex = false;
		int len = strlen(metadataString);
		if (len > 4) {
			char ch1 = metadataString[0], ch2 = metadataString[1];
			if ((ch1 == '0') && (ch2 == 'x'))
				hex = true;
		}
		if (hex) {
			err = convertHexString(metadataString + 2, eeprombuf, sizeof(eeprombuf) );
		} else {
			if (len > sizeof(eeprombuf))
				len = sizeof(eeprombuf);
			memcpy(eeprombuf, metadataString, len);
		}
		if (CYRET_SUCCESS == err)
			err = CyBtldr_ProgramRow(0x40, eepromRow, eeprombuf, sizeof(eeprombuf)); // 0x40 is the arrayId for eeprom
	} else {
		err = CyBtldr_VerifyApplication();
		printf("Application firmware is %s, %d\n",
				CYRET_SUCCESS == err ? "valid" : "invalid", err);
		if (CYRET_SUCCESS == err) {
			GetMetaDataCommunicator communicator(pAppdata);
			err = communicator.communicate();
			if (CYRET_SUCCESS == err) {
				setRdbAppVersion(pAppdata->appVers);
			}
			printf("Metadata from device is %s\n",
					CYRET_SUCCESS == err ? "valid" : "invalid");
		}
		setRdbUnitType(getUnitTypeFromMetadata(pAppdata));
		setRdbAppVersion(pAppdata->appVers);

		err = CyBtldr_doProgram(pAppdata);

		// Don't end the bootloader if there was an error otherwise we just cause device to reboot into the bootloader
		// causing an infinite loop
		if (CYRET_SUCCESS == err) {
			CyBtldr_EndBootloadOperation();
			setRdbStatus("Running application firmware");
		} else
			printf("Failed to install valid app - leaving device as is\n");
	}
	return err;
}

