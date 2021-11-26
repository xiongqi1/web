/*
 * The PSoC bootloader maanges the applications on the Modcomms boards.
 * It is started at init or by the udev daemon when a PSoC module appears on the USB bus
 * Udev doesn't pick up devices already present from boot ( all of them ). It only picks up PSoCs which have rebooted by themselves
 * It first scans the usb bus for a PSoC bootloader by VID/PID and determines what type of board it is from the device's bootloader
 * If there exists a firmware file for the board this is uploaded to the board if it is a different version ( upgrade or downgrade )
 * Finally the PSoC application is triggered if the application is valid
 * TODOs
 * - Use PSoC EEPROM data to supplement metadata
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// Set this to produce a special version for our production team only
// This must be built manually by developer by uncommenting the following line
#define DEVELOPER_OPTIONS

#include "cybtldr_utils.h"
#include "cybtldr_command.h"
#include "cybtldr_api.h"
#include "cybtldr_api2.h"

#include "cdcs_shellfs.h"
#include "NetcommUSB.h"
#include "rdb_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>
#include <errno.h>
#include <syslog.h>
#include <netinet/in.h>

char * firmwareFilename;	// Firmware file specified on the command line
char * metadataString;		// Points to a string to be programmed in PSoC memory
short eepromRow;			// This specifies the eeprom row number to store the data
char * eepromFilename;		// Firmware file specified on the command line
int eepromRowCnt = 1;		// This specifies the eeprom row number to store the data

static char * usbBusNo;     // USB bus number specified by udev in the environment. String of digits like "001"
static struct usb_device * currentUsbDevice;

// RDB session handle
static struct rdb_session *g_rdb_session;

/*
modcomms.slot.<N>.type
modcomms.slot.<N>.bootloaderVersion
modcomms.slot.<N>.applicationVersion
modcomms.slot.<N>.status
*/
static char rdb_base_name[64];
static char * rdb_name_ptr;

static void setRdbBaseName(int num )
{
	int len;
	len=sprintf( rdb_base_name,"modcomms.slot.%d.",num);
	rdb_name_ptr = rdb_base_name + len;
}

static int rdb_set_value(const char* variable,const char* value)
{
	int stat;
	printf("Set rdb string %s to %s\n",variable,value);
	if( (stat=rdb_set_string(g_rdb_session, variable, value))== -ENOENT)
		stat=rdb_create_string(g_rdb_session, variable, value, CREATE, ALL_PERM);
	if(stat<0)
		printf("failed to rdb_create_string %s\n",variable);

	return stat;
}

static void setRdbString(const char* variable, const char* value) {
	if ( g_rdb_session && rdb_name_ptr ) {
		strcpy(rdb_name_ptr,variable);
		rdb_set_value(rdb_base_name,value);
	}
}

void setRdbStatus(const char* value) {
	setRdbString( "status",value);
}

void setRdbSn(const char* value) {
	setRdbString( "sn",value);
}

void setRdbBootloaderVersion(unsigned short ver) {
	char tmpbuf[16];
	sprintf(tmpbuf,"0x%04x",ver);
	setRdbString( "bootloaderVersion",tmpbuf);
}

void setRdbAppVersion(unsigned short ver) {
	char tmpbuf[16];
	sprintf(tmpbuf,"0x%04x",ver);
	setRdbString( "applicationVersion",tmpbuf);
}

void setRdbUnitType( const char * utype) {
	setRdbString( "type",utype);
}

static usb_dev_handle * device_handle;

// Next function based on from http://stackoverflow.com/questions/194465/how-to-parse-a-string-to-an-int-in-c
int str2int (int * i, char const *s)
{
	char *end;
	long  l;
	errno = 0;
	l = strtol(s, &end, 0);
	if ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX) {
		return 1;// OVERFLOW;
	}
	if ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) {
		return 1;// UNDERFLOW;
	}
	if (*s == '\0' || *end != '\0') {
		return 1;//INCONVERTIBLE;
	}
	*i = (int)l;
	return 0;//SUCCESS;
}


/* Function used to open the communications connection */
static int OpenConnection(void)
{
	printf("Open USB Device\n"); //there was an error

	if ( !currentUsbDevice ) {
		printf("USB Device not found\n"); //there was an error
		return CYRET_ERR_DEVICE;
	}
	device_handle= usb_open(currentUsbDevice);
	if (!device_handle) {
		printf("Open Device Error\n"); //there was an error
		return CYRET_ERR_DEVICE;
	}
	printf("USB Device opened\n"); //there was an error

	char name[128];
	if(usb_get_driver_np(device_handle, 0, name, sizeof(name)) == 0) { //find out if kernel driver is attached
		//        printf("Kernel Driver Active - %s\n", name );
		if(usb_detach_kernel_driver_np(device_handle, 0) == 0) //detach it
			;//            printf("Kernel Driver Detached!\n");
	}

	int r = usb_claim_interface(device_handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
	if(r < 0) {
		printf("Cannot Claim Interface\n");
		return CYRET_ERR_DEVICE;
	}
	return CYRET_SUCCESS;
};

void dumpBufferToFile(FILE * f, unsigned char* data, int size)
{
	int i;
	fprintf(f, "0x");
	for( i = 0; i < size; i++)
		fprintf(f, "%.2x", data[i]);
	fprintf(f,"\n" );
}

void dumpBuffer(unsigned char* data, int size)
{
	dumpBufferToFile(stdout, data, size);
}

/* Function used to close the communications connection */
static int CloseConnection(void)
{
	if ( device_handle ) {
		usb_close(device_handle);
		device_handle = 0;
	}
	return CYRET_SUCCESS;
}
/* Function used to read data over the communications connection */
static int ReadData(unsigned char* data, int size)
{
	// printf("ReadData %d\n", size );
	char tmpdata[64]; // We must read 64 bytes
	int rc = usb_interrupt_read(device_handle, (2 | USB_ENDPOINT_IN), tmpdata, sizeof(tmpdata), 1000);
	//    printf("%d\n ", rc);
	if ( (rc < 0 ) || ( rc < size ) )
		return CYRET_ERR_DEVICE;
	memcpy(data,tmpdata,size);
	// dumpBuffer(data,size);
	return CYRET_SUCCESS;
}

/* Function used to write data over the communications connection */
static int WriteData(unsigned char*data, int size)
{
	// printf("WriteData %d\n", size );
	// dumpBuffer(data,size);
	int rc = usb_interrupt_write(device_handle, (1 | USB_ENDPOINT_OUT), (char*)data, size, 0);
	//    printf("%d\n ", rc);
	if ( (rc < 0 ) || ( rc != size ) )
		return CYRET_ERR_DEVICE;
	return CYRET_SUCCESS;
}

static CyBtldr_CommunicationsData comms = {
	OpenConnection,
	CloseConnection,
	ReadData,
	WriteData,
	64      // Device can handle upto 64 bytes at a time
};

// Read in a file and return the value of the integer therein
int getUintFromFile(const char * filename, int radix) {
	char tmpbuf[32]; // 32 is enough as we only expect to read an integer
	char * pEnd;
	if ( 0 == shellfs_cat(filename, tmpbuf, sizeof(tmpbuf)) )
		return -1;
	// The string will be terminated for us
	long rc = strtol(tmpbuf, &pEnd, radix);
	return (int)rc;
}

/*
 * This class scans a /sys/bus/usb device directory.
 * It looks at some files to determine if this directory corresponds to the device we are working on
 * If it matches we count the dots in the file name as this corresponds to the slot position in the Modcomms Stack
 */
class Sys_DirectoryTraverser : public DirectoryTraverser {
public:
	const char * foundAt;   // This points to the /sys path when found
	int slotNo;             // This is the modcomms slot number calculated from the path

	// Scan directory dir for device dev
	Sys_DirectoryTraverser( const char * dir, struct usb_device * dev):
	DirectoryTraverser(dir, "1-*"),
	foundAt(NULL),
	slotNo(0),
	devTofind(dev)
	{};
private:
	Sys_DirectoryTraverser(); // Don't use this one
	struct usb_device * devTofind;
	// This is called for every  entry in the directory
	virtual bool process( shellfs_dirent_t *de ) {

		if ( strchr(de->name,':' ) ) // A : indicates an interface which doesn't interest us
			return true;

		const char * full_name = getFullName(de->name);
		//        printf("Sys_DirectoryTraverser %s\n",full_name);

		// Look at a few files in this directory to see if a match
		int dirNameLen = strlen(full_name);
		char filename[dirNameLen+16];  // Allow a little extra space for concat of filename. The number must be big enough for the follwing strcpys
		strcpy(filename, full_name );
		char * copyHere = filename+dirNameLen;
		*copyHere++ = '/';

		strcpy(copyHere, "idVendor");
		uint16_t idVendor = getUintFromFile(filename,16);
		//        printf("idVendor %x\n",idVendor);
		if ( idVendor!=devTofind->descriptor.idVendor)
			return true;

		strcpy(copyHere, "idProduct");
		uint16_t idProduct = getUintFromFile(filename,16);
		//        printf("idProduct %x\n",idProduct);
		if ( idProduct!=devTofind->descriptor.idProduct)
			return true;

		strcpy(copyHere, "devnum");
		uint8_t devNum = getUintFromFile(filename,10);
		//        printf("devnum %x\n",devNum );
		if (devNum==devTofind->devnum)
			foundAt = full_name;
		//        printf("found is %s\n",found ? "true" :"false");
		if ( foundAt ) {
			//                printf("found at %s\n",foundAt);
			// The number of dots in the name gives us the slot number
			int dotCnt=0;
			const char * p = de->name;
			while ( (p = strchr(p,'.') ) ) {
				p++;
				dotCnt++;
			}
			//                printf("slot is %d?\n",dotCnt);
			slotNo = dotCnt;
			return false;
		}
		return true;
	}
};

// Scan the upper level USB directory and for each directory call another scanner for that
class USB_DirectoryTraverser : public DirectoryTraverser {
public:
	char * foundAt;
	int slotNo;

	USB_DirectoryTraverser(struct usb_device *dev):
	DirectoryTraverser("/sys/bus/usb/devices", "1-*"),
	foundAt(NULL),
	slotNo(0),
	devTofind(dev)
	{};
	virtual ~USB_DirectoryTraverser() {
		//        printf("~USB_DirectoryTraverser\n");
		if ( foundAt)
			free(foundAt);
	}
private:
	USB_DirectoryTraverser(); // Don't use this
	struct usb_device * devTofind;

	virtual bool process( shellfs_dirent_t *de ) {
		if ( strchr(de->name,':' ) )
			return true;
		const char * full_name = getFullName(de->name);
		//        printf("USB_DirectoryTraverser %s\n",full_name);

		// Scan the lower level
		Sys_DirectoryTraverser sysScanner(full_name,devTofind);
		sysScanner.traverse();
		if ( sysScanner.foundAt ) {
			foundAt =  strdup(sysScanner.foundAt); // save a copy  for after this object is destroyed for storing in RDB
			slotNo = sysScanner.slotNo;
			return false;
		}
		return true;
	}
};

// Return true if we did launch a PSoC application
static bool program_PSoCs() {
	struct usb_bus *bus;
	struct usb_device *dev;
	bool psocLaunched = false;

	for (bus = usb_busses; bus; bus = bus->next) {

		if ( usbBusNo ) { // If called from udev match the bus
			if ( strcmp(usbBusNo, bus->dirname) ) {
//				printf("No match for bus %s\n", usbBusNo );
				continue;
			}
		}

		for (dev = bus->devices; dev; dev = dev->next) {
			printf("Dev %s: ID %04x:%04x DevNum %02x\n",
			       dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct,
				dev->devnum
			);

			if (( NETCOMM_VID == dev->descriptor.idVendor) && ( MODCOMMS_BOOTLOADER_PID == dev->descriptor.idProduct)) {
				// Look through /sys to find this device and determine the slot number
				USB_DirectoryTraverser usbScanner(dev);
				usbScanner.traverse();
				if ( !usbScanner.foundAt ) {
					printf("Failed to find sysfs entry for device\n");
					continue;
				}
				setRdbBaseName(usbScanner.slotNo);
				setRdbStatus("Discovered by usb scan");
				setRdbString("sysfs", usbScanner.foundAt );

				currentUsbDevice = dev;
				int rc = CyBtldr_Program( 0, &comms);
				printf("CyBtldr_Program returns %d\n",rc);
				psocLaunched = true;
			}
		}
	}
	return psocLaunched;
}

void usage(char **argv)
{
#ifdef DEVELOPER_OPTIONS
	fprintf(stderr, "\nUsage: %s [-f file] [-s string -r rrr] [-d file -r rrr [-c ccc] ]\n", argv[0]);
#else
	fprintf(stderr, "\nUsage: %s [-f file]\n", argv[0]);
#endif
	fprintf(stderr, "\n Options:\n");
	fprintf(stderr, "\t-f Force firmware image file\n");
#ifdef DEVELOPER_OPTIONS
	fprintf(stderr, "\t-s Store the string in the PSoC EEPROM row rrr ( Each row is 16 bytes )\n");
	fprintf(stderr, "\t-d Dump ccc rows of EEPROM data to file starting with row rrr\n");
#endif
	fprintf(stderr, "\n");
}

int main(int argc, char * argv[] )
{
	int rc;
	printf("PSoC bootloader\n");

	// Parse Options
#ifdef DEVELOPER_OPTIONS
	while ((rc = getopt(argc, argv,  "hf:s:r:d:c:")) != EOF)
#else
	while ((rc = getopt(argc, argv,  "hf:")) != EOF)
#endif
	{
		switch (rc)
		{
			case 'f':
				firmwareFilename=optarg;
				break;
			case 'd':
				eepromFilename=optarg;
				break;
			case 's':
				metadataString=optarg;
				break;
			case 'r':
				eepromRow=atoi(optarg);
				break;
			case 'c':
				eepromRowCnt=atoi(optarg);
				break;
			case 'h':
			default:
				usage(argv);
				return 0;
		}
	}

	if (( metadataString && !eepromRow )||( eepromFilename && !eepromRow )) {
		printf("The eeprom row must be specified\n");
		return -1;
	}
	eepromRow--; // make it zero based

	usbBusNo = getenv("BUSNUM"); // This variable exists if we are started by udev
	if ( usbBusNo )
		printf("Called from udev for bus %s\n", usbBusNo);
	else
		printf("No bus specified\n");

	usb_init(); //initialize a library session; //for return values
	usb_find_busses();
	usb_find_devices();

	if (rdb_open(NULL, &g_rdb_session) < 0)
		printf("can't open RDB");

	bool psocLaunched = program_PSoCs();

	CloseConnection(); // Make sure this is not left open
	if (g_rdb_session) rdb_close(&g_rdb_session);

	if (psocLaunched && usbBusNo) {
		// This PSOC was caught by a udev event so
		// after launching the PSOC app there will be a string of more udev
		// events
		// We need IoMgr to rescan the system after it has settled
		printf("Called from udev, tell IoMgr to rescan\n");
		system("killall -USR2 io_mgr");
	}

	return rc;
}
