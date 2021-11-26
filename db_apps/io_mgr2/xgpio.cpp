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
 * This file contains the Gpio class. A thin OO wrapper around the libgio functionality
 * For Modcomms there is some discovery and initialization code
 */

#include "xgpio.h"
#include "io_mgr.h"
#include "utils.h"

#include <cdcs_shellfs.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <libgpio.h>
}

static bool isLibGpioInitialized = false;

/*
 *  This is the Gpio constructor
 * Parameters are
 *  _gpio - the number of the gpio pin
 *  mode - input or output
 *  val  - the value to output if an output
 * This constructor reserves the gpio pin and sets the appropriate mode and output value
 * This constructor also initialises the libgpio if not done already
 * Failure of this constructor is indicated by gpio nuumber being set to GPIO_UNUSED
 */
Gpio::Gpio(int _gpio) :
	gpio(_gpio),
	mode(0)
{
	DBG(LOG_DEBUG,"Gpio(%d)",gpio);

	if (!isLibGpioInitialized) {			// Initialise the library if required
		DBG(LOG_DEBUG, "libpio init");
		if(gpio_init("/dev/gpio") < 0) {
			DBG(LOG_ERR, "failed to open gpio driver");
			gpio = GPIO_UNUSED;
			return;
		}
		isLibGpioInitialized = true;
	}

	if(gpio_request_pin(gpio) < 0) {
		DBG(LOG_ERR,"failed in gpio_request_pin %d - %s", gpio, strerror(errno));
		gpio = GPIO_UNUSED;
		return;
	}

	/* setup mux */
	if(gpio_gpio(gpio) < 0) {
		DBG(LOG_ERR,"failed in gpio_gpio %d - %s", gpio, strerror(errno));
		gpio = GPIO_UNUSED;
		return;
	}
}

bool Gpio::setMode(uint _mode)
{
	if (_mode == IO_INFO_CAP_MASK_DIN ) {
		/* set input mode */
		if(gpio_set_input(gpio) < 0) {
			DBG(LOG_ERR,"failed to set gpio input mode (gpio=%d) - %s", gpio, strerror(errno));
			return false;
		}
	}
	mode = _mode;
	return true;
}

// Destructor just has to release the pin
Gpio::~Gpio()
{
	DBG(LOG_DEBUG,"~Gpio %d", gpio);
	if ( gpio != GPIO_UNUSED ) {
		gpio_free_pin(gpio);
	}
}

uint Gpio::readValue()
{
	if (gpio == GPIO_UNUSED){
		DBG(LOG_ERR,"read from invalid gpio");
		return 0;
	}
	if (mode != IO_INFO_CAP_MASK_DIN) {
		DBG(LOG_ERR,"can't read from output gpio (%d)", gpio);
		return 0;
	}
	uint val = gpio_read(gpio);
//	DBG(LOG_DEBUG,"readValue gpio (%d) - %d",gpio, val);
	return val;
}

void Gpio::writeValue(int val)
{
	if (gpio == GPIO_UNUSED){
		DBG(LOG_ERR,"write to invalid gpio");
		return;
	}
	DBG(LOG_DEBUG,"output gpio (%d) - %d", gpio, val);
	if (mode != IO_INFO_CAP_MASK_DOUT ) {
		DBG(LOG_ERR,"can't output to input gpio (%d)", gpio);
		return;
	}
	/* digital output */
	if(gpio_set_output(gpio, val) <0 ) {
		DBG(LOG_ERR,"failed to output gpio (%d) - %s",gpio,strerror(errno));
	}
}

// This is called by the main program as part of the termination clean up
void Gpio::close()
{
	DBG(LOG_DEBUG,"Gpio close");
	if (isLibGpioInitialized) {
		DBG(LOG_DEBUG,"Gpio exit");
		gpio_exit();
		isLibGpioInitialized = false;
	}
}

#ifdef V_MODCOMMS

static int xgpio_gpio_device_init(const char* bname)
{
	DBG(LOG_DEBUG,"gpio base = %s", bname);

	int baseGpio = -1;
	if ( !getIntFromFile(bname, "base", &baseGpio ) )
		return -1;
	int numIo = -1;
	if ( !getIntFromFile(bname, "ngpio", &numIo ) )
		return -1;

	char name[PATH_MAX];
	if ( shellfs_cat_with_path(bname, "label", name, sizeof(name) ) < 0 )
		return -1;
	trimEnd(name);

	if ( is_modcomms_device(name) ) {
		gpioCreate(name, baseGpio, numIo );
	}
	return 0;
}


// Class to scan and add GPIO devices
class GPIO_DirectoryTraverser : public DirectoryTraverser {
public:
	GPIO_DirectoryTraverser():
		DirectoryTraverser("/sys/class/gpio", "gpiochip[0-9]*"){};

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
 * to add all the dynamic GPIO ports we find by scanning /sys
 */
void gpio_addModcomms() {
	GPIO_DirectoryTraverser gpiotraverser;
	gpiotraverser.traverse();
}
#endif
