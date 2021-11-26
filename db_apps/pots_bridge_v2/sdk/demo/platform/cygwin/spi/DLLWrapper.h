/*
** Copyright 2006-2016, Silicon Labs
** $Id: DLLWrapper.h 5421 2016-01-13 00:55:14Z nizajerk $
**
** DLLWrapper.h
** VMB driver header file
**
** Author(s):
** laj
**
** Distributed by:
** Silicon Laboratories, Inc
**
** File Description:
** This is the header file for VMB driver
**
** Dependancies:
** quad_io.lib, quad_io.dll
**
*/

/*
** EEPROM Functions
**
*/
unsigned char readEEPROMSize();

unsigned char readEEPROMContents(unsigned short *buff);

/*
** Function: openVoiceIntf
**
** Description:
** Initializes motherboard communication
**
** Input Parameters:
** addr: parallel port address
** USB: 1 = USB motherboard, 0 = parallel port motherboard
** path: pointer to firmware file for voice motherboard
**
** Return:
** sucess
*/
int openVoiceIntf (unsigned short addr,int usb,char *path);

/*
** Function: QuadReadRegExp
**
** Description:
** Reads a single ProSLIC register
**
** Input Parameters:
** address: Address of register to read
**
** Return:
** data to read from register
*/
unsigned char QuadReadRegExp (unsigned char addr);

/*
** Function: QuadWriteRegExp
**
** Description:
** Writes a single ProSLIC register
**
** Input Parameters:
** address: Address of register to write
** data: data to write to register
**
** Return:
** none
*/
void QuadWriteRegExp (unsigned char addr, unsigned char data);


/*
** Function: write2RegExp
**
** Description:
** Writes 2 ProSLIC registers
**
** Input Parameters:
** address0: Address of first register to write
** data0: data to write to first register
** address1: Address of second register to write
** data1: data to write to second register
**
** Return:
** none
*/
void Write2RegExp (unsigned char addr0, unsigned char data0,unsigned char addr1,
                   unsigned char data1);
void Write2RegDelayExp (unsigned char addr0, unsigned char data0,
                        unsigned char addr1, unsigned char data1,unsigned short delay);
/*
** Function: setChannel
**
** Description:
** Sets the current channel
**
** Input Parameters:
** channel: new channel
**
** Return:
** none
*/
void setChannel (unsigned short channel);

/*
** Function: QuadReadRamExp
**
** Description:
** Reads a single ProSLIC RAM location
**
** Input Parameters:
** address: Address of RAM location to read
**
** Return:
** data to read from RAM location
*/
unsigned long QuadReadRamExp(unsigned short addr);

/*
** Function: QuadWriteRamExp
**
** Description:
** Writes a single ProSLIC RAM location
**
** Input Parameters:
** address: Address of RAM location to write
** data: data to write to RAM location
**
** Return:
** none
*/
void QuadWriteRamExp(unsigned short addr,unsigned long data);

/*
** Function: Reset
**
** Description:
** Sets the reset pin of the ProSLIC low
**
** Input Parameters:
** none
**
** Return:
** none
*/
void Reset ();

/*
** Function: UNReset
**
** Description:
** Sets the reset pin of the ProSLIC high
**
** Input Parameters:
** none
**
** Return:
** none
*/
void UNReset();

/*
** Function: setBroadcastmode
**
** Description:
** Turns on or off broadcast bit in SPI frame
**
** Input Parameters:
** nbroadcast: 0 = broadcast off, 1 = broadcast on
**
** Return:
** none
*/
void setBroadcastmode (unsigned short nbroadcast);

/*
** Function: setRevASpi
**
** Description:
** Turns on or off revA SPI fix
**
** Input Parameters:
** newval: 0 = off, 1 = on
**
** Return:
** none
*/
void setRevASpi (unsigned short newval);

/*
** Function: setPcmSourceExp
**
** Description:
** Sets up PCM clocks
**
** Input Parameters:
** internal: internal(1) or external(0) clocks
** freq: select frequency for PCLK
**	freq:
**	128 = 8192kHz
**	64 = 4096kHz
**	32 = 2048kHz
**	16 = 1024kHz
**	8 = 512kHz
**	4 = 256kHz
**	512 = 32768kHz
**	12 = 768kHz
**	24 = 1536kHz
** extFsync: set to 1 for external FSYNC only
**
** Return:
** none
*/
unsigned short setPcmSourceExp(unsigned short internal,int freq,int extFsync);

/*
** Function: setLPT
**
** Description:
** Sets up parallel port address
**
** Input Parameters:
** newLPT: address to set parallel port accesses to
**
** Return:
** none
*/
void setLPT (unsigned short newLPT);

/*
** Function: setBroadcastmode
**
** Description:
** Turns on/off broadcast SPI accesses
**
** Input Parameters:
** newbroadcast: 1:on 0:off
**
** Return:
** none
*/
void setBroadcastmode (unsigned short nbroadcast);

