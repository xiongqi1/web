#ifndef __IIO_H__28112015
#define __IIO_H__28112015

/*
 * Industrial IO support
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 * This is the header for all the IIO ( Industrial IIO functionality )
 *
 * IIo consists of IIo devices each of which consists of IIo channels
 * An example of an IIO device is the on chip ADC. This would typically have multiple analog inputs.
 * Each analog input appears as channel under the IIO device and names would be like voltage0 voltage1 etc
 */

#include "io_mgr.h"
#include <cdcs_fileselector.h>
#include <unistd.h>
#include <string>
#include <vector>

using namespace std;

class IIoDevice;

/*
 * This class represents an IIo channel
 * It is created as part of the scan of /sys
 * The IoMgr then attaches to this channel
 * Data read from the IIo Device is pushed back to the Io that attached
 */
class IIoChannel {
public:
	string name;                    // channel name like "voltage7"
	int capabilities;               // What modes this channel supports
	Io *pIo;                        // Pointer to the IoMgr class that attaches to this channel
	IIoDevice *pIIoDevice;          // Pointer to the IIO device that owns this channel
	int index;			// index configuration information - driver-specified physical channel index (read from /sys)

	IIoChannel(const char *name, int capabilities, IIoDevice *pIIoDevice);
	bool attach(Io *_pIo, uint ioMode);
	void detach();

	bool enableBuffering(bool enable);
	void push(uint *pUint)           // Store the sample in our buffer
        {
		samples.push_back(*pUint);
	}
	void flush()                    // Send all our data to the IoMgr
	{
		int cnt = samples.size();
		if ( cnt ) {
			ioPushSamples(pIo, &samples[0], cnt );
			samples.clear();
		}
	}
	bool isBuffering() { return pIo && (capabilities&IO_INFO_CAP_MASK_BUFFERED); }
	size_t dataSize() { return sizeof(uint); }; // possible TODO - handle other types
        bool outputValue(int val);
private:
	string enableFileName;      // name if the channel enable file, like in_chanName_en
        string outputFileName;      // name to the output file, like out_chanName_raw
	vector<uint> samples;       // This array store the values read from the device
        uint mode;                  // The IO mode
};

/*
 * This class represents an IIo Device
 * It is created as part of the scan of /sys
 * An IIo device has multiple IIo Channels
 * This class is responsible for opening the device and then reading the data
 * then pushing the data to the channels
 */

class IIoDevice {
public:
	string deviceName;          // like iio:device0
	string nameFromNameFile;    // like gps-mice.1-1
	string pathToIIoDevice;     // like /sys/bus/iio/devices/iio:device0
	string pathToScanElements;  // like /sys/bus/iio/devices/iio:device0/scan_elements
	vector<void*> iioChannels;  // Array of channels for this device - void * to avoid bloat
	bool isModcommsDevice;
        uint capabilities;          // A sum of capabilities of all channels

	IIoDevice(const char *name, const char *fullPath);
	~IIoDevice();
	bool scanChannels();                                // This initialisation function scans /sys for channel names and calls
	void addChannel(const char *name, int capabilities); // addChannel when it finds them

	IIoChannel * findChannel(const char *chanName );    // This is called by IoMgr at start for the hardwared Io - like "vin" on channel "voltage1"
	IIoChannel * findChannel(uint ioMode);              // This is called by IoMgr for Modcomms Io
	bool registerWith(FileSelector *fs);                // This allows is to do a select of the devices file handle
	void onSelected();                                  // and this is called when data is available

	bool writeIntToFile(const char *name,int value);

	bool enableBuffering(bool enable);
	bool startBuffering();
	void stopBuffering();
	bool isBuffering() { return hdev >= 0; }
private:
	int sizeofAllChannels;                              // This is the size in bytes of a set of data from all enabled channels
	int hdev;                                           // device file handle
	FileSelector *pFS;                                  // A copy of the file selector for deregistering
};

int iio_create();       // Scan /sys and create our IIO objects
void iio_destroy();     // and then clean up

void iio_addModcomms(); // Scan our objects for Modcomms devices
IIoChannel * iio_findChannel(const string &channelName);    // This is called by IoMgr at start for the hardwared Io - like "vin" on channel "voltage1"
void iio_changeSamplingFreq(int freq);
#endif
