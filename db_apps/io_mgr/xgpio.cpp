/*
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
 *
 * This file contains the bulk of the Modcomms enhancements
 * For initialization is scans both the GPIO and IIO in /sys
 * GPIO IO goes through the existing IO_MGR paths
 * IIO IO is done here through /sys
 */

extern "C" {
#include "commonIncludes.h"
#include "xgpio.h"
#include "iio.h"
#include "rdb.h"
#include "tick64.h"
} // extern "C"
#include "cdcs_shellfs.h"


/* xgpio sysfs directory information */
#define XGPIO_DIR_DEVICE_DIR	"/sys/class/gpio"
#define XGPIO_DIR_DEVICE_FILES	"gpiochip[0-9]*"

int xaux_cnt = 1;	// This gives the next xaux number

// Set RDB variable to describe the XAUX ( which modcomms board )
static void setRdbLabel(const char * xaux, const char * label ) {
	char rdbVar[64]; // xaux points only to "xauxXXX" so 64 is enough
	sprintf( rdbVar, "sys.sensors.io.%s.label", xaux );
	rdb_setVal( rdbVar,label, 0); // Not persistent
}

/* Scans a positive integer from the string */
int xgpio_scan_uint( const char * ichars) {
	int i;
	if ( 1 == sscanf(ichars,"%d", &i ) )
		return i;
	return -1;
}

// Trim trailing space and return pointer to end of string
char * trimEnd(char * str ) {
	int len = strlen(str);
	if ( len < 1 )
		return str;
	char * end = str + len - 1;
	while(end > str && isspace(*end))
		end--;

	// Write new null terminator
	*++end = 0;
	return end;
}

int xgpio_set_mode(const struct io_info_t * io ,int val) {
	DBG(LOG_ERR,"NOT IMPLEMENTED set IIO mode %d",val);
	return 0;
}

void xgpio_set_pullup(const struct io_info_t * io ,const char * val) {
	DBG(LOG_ERR,"NOT IMPLEMENTED set IIO pullup %s",val);
}

/*
 * Take a new IO definition and add them to global array
 */
int add_io( struct io_info_t * pNewIo, const char * label )
{
	// modcommsExcludeList is a comma separated list of names that the IoMgr will not work with
	if (modcommsExcludeList) {
//		DBG(LOG_ERR,"check exclude list for %s", label );
		if ( strstr(modcommsExcludeList, label)) {
//			DBG(LOG_ERR,"%s found in exclude list", label );
			return -1;
		}
	}

	// This Rdb variable says how many Ios are managed. ( Used by WebUI Io page)
	char val[8];
	snprintf(val, sizeof(val), "%d", xaux_cnt);
	rdb_setVal("sys.sensors.info.lastio", val, 0);

	char io_name[8]; // We going to create xauxXX
	sprintf(io_name,"xaux%d", xaux_cnt++ );
//	DBG(LOG_DEBUG,"add device %s", io_name );
	pNewIo->io_name = strdup(io_name);

	// Resize the current array
	int numNewIo = 1;
	size_t newSize = (io_cnt+numNewIo) * sizeof(struct io_info_t);
	struct io_info_t * newIoArray= (struct io_info_t *)realloc(io_info, newSize);
	if (newIoArray) {
		// copy the new II definitions
		memcpy( &newIoArray[io_cnt] , pNewIo, numNewIo*sizeof(struct io_info_t) );
		// Reflect the new array and size
		io_info = newIoArray;
		io_cnt += numNewIo;

		setRdbLabel(pNewIo->io_name, label);
		return 0;
	}
	return -1;
}

int is_modcomms_device(const char * devname )
{
	return strstr(devname,"-mice") != 0;
}

static int xgpio_gpio_device_init(const char* bname)
{
	DBG(LOG_DEBUG,"gpio base = %s", bname);
	char name[PATH_MAX];
	if ( shellfs_cat_with_path(bname, "base", name, sizeof(name) ) < 0 )
		return -1;
	int baseGpio = xgpio_scan_uint(name);
	if ( baseGpio < 0 )
		return -1;
	if ( shellfs_cat_with_path(bname, "ngpio", name, sizeof(name) ) < 0 )
		return -1;
	int numIo = xgpio_scan_uint(name);
	if ( shellfs_cat_with_path(bname, "label", name, sizeof(name) ) < 0 )
		return -1;
	char * endofName = trimEnd(name);

//	DBG(LOG_DEBUG,"gpio name = %s sysfs = %s",name, bname);
	if ( is_modcomms_device(name) ) {
		if ( ( numIo > 0 ) && ( numIo < 100 ) ) {
			for ( int i = 0; i< numIo; i++ ) {
				struct io_info_t newIo;
				memset(&newIo,0,sizeof(newIo));
				struct io_info_t * pIo = &newIo;

				pIo->power_type = IO_MGR_POWERSOURCE_NONE;
				pIo->cap = IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT;
				pIo->scale=IO_INFO_SCALE(1,1);
				pIo->correction=1;
				pIo->d_in_threshold=0;
					pIo->hardware_gain=1;
					pIo->init_mode=IO_INFO_CAP_MASK_DIN;
				pIo->gpio_output_pin=baseGpio;
				pIo->gpio_input_pin=baseGpio;
					pIo->gpio_pullup_pin=GPIO_UNUSED;
				pIo->dynamic = 1;

				sprintf( endofName, "-DIO%d", i+1 );
				add_io(&newIo,name);
				baseGpio++;
			}
		}
	}
	return 0;
}

// Class to scan and add GPIO devices
class GPIO_DirectoryTraverser : public DirectoryTraverser {
public:
	GPIO_DirectoryTraverser():
		DirectoryTraverser(XGPIO_DIR_DEVICE_DIR, XGPIO_DIR_DEVICE_FILES){};

	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {
		/* create xgpio device */
		if( xgpio_gpio_device_init( getFullName(de->name) ) < 0) {
			DBG(LOG_ERR,"failed to create xgpio device for %s",de->name);
		}
		return true;
	}
};

/*
 * This routine is called early in the IO_MGR initialization
 * to add all the dynamic IO ports we find by scanning /sys
 */
void xgpio_add_gpio_from_sysfs() {
	// first scan the static IO xauxN definitions to see where the extra ones will start ( N+1)
	for ( int i = 0; i < io_cnt; i++) {
		struct io_info_t * io = &io_info[i];
		if ( 0 == strncmp( io->io_name, "xaux", 4) )
			xaux_cnt++;
	}
	DBG(LOG_INFO,"first dynamic xaux will start at %d", xaux_cnt);

	// Scan and add the GPIO from /sys
	GPIO_DirectoryTraverser gpiotraverser;
	gpiotraverser.traverse();

	return;
}

