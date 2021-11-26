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
 */

#include "iio.h"
#include "io_mgr.h"
#include "utils.h"

#include "cdcs_shellfs.h"
#include <vector>
#include <algorithm>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define IIO_MAX_SYSFS_CONTENT_LEN 64 /* maximum content length of each sysfs entry */

static const int numberOfSamples = 4*1000; /* kernel device buffer length - 4 second buffer */

static vector<void*> iioDevs;	// Our container of Io devices - vector<void*> used to avoid bloat

// This determine the channels capability from its name ( input channels only )
static int chanNameToCapability( const char *el )
{
	if ( strstr(el, "current_") ) {
		return IO_INFO_CAP_MASK_CIN;
	}
	if ( strstr(el, "NAMUR") ) {
		return IO_INFO_CAP_MASK_NAMUR;
	}
	if ( strstr(el, "ct_") ) {
		return IO_INFO_CAP_MASK_CT;
	}
	if ( strstr(el, "temp_") ) {
		return IO_INFO_CAP_MASK_TEMP;
	}
	if ( strstr(el, "_CCI") ) {
		return IO_INFO_CAP_MASK_CC;
	}
//	if ( strstr(el, "voltage_ADC") || strstr(el, "voltage_INP") )
		return IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN;
//	return 0;
}

typedef  void *PVOID;

// This is called by the sorter to compare two channels. We sort on their index
static bool sortCompare( const PVOID &c1, const PVOID &c2 )
{
	IIoChannel *pCh1 = (IIoChannel *)c1;
	IIoChannel *pCh2 = (IIoChannel *)c2;
	return pCh1->index < pCh2->index;
}

IIoChannel::IIoChannel(const char *_name, int _capabilities, IIoDevice *_pIIoDevice):
	name(_name),
	capabilities(_capabilities),
	pIo(0),
	pIIoDevice(_pIIoDevice),
	index(999),	// Something much higher than a real index ( for sort) so they go at end of list
	mode(0)
{
	enableFileName = string("in_")+name+"_en";
	if (capabilities & IO_INFO_CAP_MASK_BUFFERED) {	// Lets check it does support buffering and clear that capability if not
		if ( !enableBuffering(true) ) {
			DBG(LOG_DEBUG,"IIoChannel - %s doesn't support buffering",+name.c_str());
			capabilities &= ~IO_INFO_CAP_MASK_BUFFERED;
		}
		enableBuffering(false);
		string indexFileName = "in_"+name+"_index";		// The index file gives the order of channels when reading from the device
		getIntFromFile(pIIoDevice->pathToScanElements, indexFileName, &index );
	}

	outputFileName = "out_"+name+"_raw";
}

// This writes 1 or 0 to the enable file in /sys
bool IIoChannel::enableBuffering(bool enable)
{
	if ( isOutputMode(mode) ) {
		return false;
	}
	if (capabilities & IO_INFO_CAP_MASK_BUFFERED) {
		return shellfs_echo_with_path( pIIoDevice->pathToScanElements.c_str(),  enableFileName.c_str(), enable ? "1": "0" ) >= 0;
	}
	return false;
}

/*
 * _pIo is the this pointer of the IoMgr class attaching to the channel
 * _mode is the Io mode that is going to be used
 */
bool IIoChannel::attach(Io *_pIo, uint _mode)
{
	DBG(LOG_DEBUG,"IIoChannel::attach %s - new mode %s cur mode %s cap - %s", name.c_str(),
		capabilitiesToString(_mode).c_str(),
		capabilitiesToString(mode).c_str(),
		capabilitiesToString(capabilities).c_str());
	if (pIo && (pIo != _pIo )) {
		DBG(LOG_DEBUG,"IIoChannel::attach %s - already attached", name.c_str() );
		return false;
	}

	if ( mode && (isInputMode(_mode) != isInputMode(mode) ) ){
		DBG(LOG_DEBUG,"IIoChannel::attach %s - can't switch input <--> output", name.c_str() );
		return false;
	}
	if ( (_mode & capabilities) == 0 ) {
		DBG(LOG_DEBUG,"IIoChannel::attach %s - invalid mode %s (%s)", name.c_str(), capabilitiesToString(_mode).c_str(), capabilitiesToString(capabilities).c_str());
		return false;
	}
	DBG(LOG_DEBUG,"IIoChannel::attach - %s",name.c_str() );
	pIo = _pIo;
	mode = _mode;
	return true;
}

void IIoChannel::detach()
{
	if (!pIo) return; // Nothing attached, nothing to do
	DBG(LOG_DEBUG,"IIoChannel::detach - %s",name.c_str() );
	pIo = 0;
	if ( pIIoDevice->isBuffering() ) {
		pIIoDevice->stopBuffering();
		enableBuffering(false);
		pIIoDevice->startBuffering();
	}
}

bool IIoChannel::outputValue(int val)
{
	DBG(LOG_DEBUG,"outputValue %s", name.c_str() );
	if ( !isOutputMode(mode) ) {
	DBG(LOG_DEBUG,"outputValue failed - not output mode %s", capabilitiesToString(mode).c_str() );
		return false;
	}
	return pIIoDevice->writeIntToFile( outputFileName.c_str(), val );
}

// Find channel by name - this is only called for static IO like "vin"
IIoChannel * IIoDevice::findChannel(const char *chanName )
{
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel *pCh = (IIoChannel *)*it;
		if ( 0 == strcmp( chanName, pCh->name.c_str() ) )
			return pCh;
	}
	return NULL;
}

// Find channel by capability - this is called for Modcomms
IIoChannel * IIoDevice::findChannel(uint mode)
{
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel *pCh = (IIoChannel *)*it;
		if ( pCh->capabilities & mode )
			return pCh;
	}
	return NULL;
}

IIoDevice::IIoDevice(const char *name, const char *fullPath):
	deviceName(name),
	pathToIIoDevice(fullPath),
	isModcommsDevice(false),
	capabilities(0),
	hdev(-1),
	pFS(0)
{
	pathToScanElements = pathToIIoDevice+"/scan_elements";
	char tmpNameFromNameFile[IIO_MAX_SYSFS_CONTENT_LEN];
	if( shellfs_cat_with_path( fullPath, "name", tmpNameFromNameFile, sizeof(tmpNameFromNameFile) ) ) {
		trimEnd(tmpNameFromNameFile);
		nameFromNameFile=strdup(tmpNameFromNameFile);
		isModcommsDevice = is_modcomms_device(nameFromNameFile.c_str());
	}
}

IIoDevice::~IIoDevice()
{
	DBG(LOG_DEBUG,"~IIoDevice");
	stopBuffering();
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel *pCh = (IIoChannel *)*it;
		delete pCh;
	}
	iioChannels.clear();
}

bool IIoDevice::writeIntToFile(const char *name, int value)
{
	char str[IIO_MAX_SYSFS_CONTENT_LEN];
	snprintf(str,sizeof(str),"%d",value);
	return shellfs_echo_with_path(pathToIIoDevice.c_str(),name,str) >= 0;
}

bool IIoDevice::enableBuffering(bool enable)
{
	return writeIntToFile("buffer/enable", enable ? 1: 0);
}

void onIIoDeviceSelected(void *param, void *pFm)
{
	IIoDevice * pIioDev = (IIoDevice *)param;
	pIioDev->onSelected();
}

bool IIoDevice::startBuffering()
{
	DBG(LOG_DEBUG,"IIoDevice::startBuffering() - %s",deviceName.c_str() );
	if( (hdev >= 0) || (!pFS) ) {
		return false;
	}

	std::sort( iioChannels.begin(), iioChannels.end(), sortCompare );

	// Lets count the number of attached channels in buffered input mode and sum their datasize;
	int numChannels = 0;
	sizeofAllChannels = 0;
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel *pCh = (IIoChannel *)*it;
		if ( pCh->pIo && pCh->enableBuffering(true) ) {
			numChannels++;
			sizeofAllChannels += pCh->dataSize();
		}
	}
	if ( numChannels == 0 ) {
		DBG(LOG_DEBUG,"No attached channels");
		return false;
	}

	DBG(LOG_DEBUG,"IIoDevice::startBuffering %s - #%d col %d", deviceName.c_str(), numChannels, sizeofAllChannels );

	/* set kernel driver buffer length */
	writeIntToFile("buffer/length", numberOfSamples* sizeofAllChannels);

#ifdef V_PROCESSOR_am335	/* set continuous mode */
	shellfs_echo_with_path(pathToIIoDevice.c_str(),"mode","continuous");
#endif
	if ( !enableBuffering(true) ) {
		DBG(LOG_DEBUG,"iio device %s - doesn't support buffering", deviceName.c_str() );
		return false;
	};
	/* open iio device node */
	string dev_name = string("/dev/")+deviceName;
	hdev=open(dev_name.c_str(),O_RDONLY|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);

	DBG(LOG_DEBUG,"open %s  %d %s", dev_name.c_str(), hdev, strerror(errno) );
	if(hdev<0) {
		DBG(LOG_ERR, "failed to open iio device %s %s", dev_name.c_str(), strerror(errno) );
		return false;
	};

	// Register our handle and function  with the selector
	pFS->addMonitor(hdev, onIIoDeviceSelected, this);
	return true;
}

void IIoDevice::stopBuffering()
{
	DBG(LOG_DEBUG,"IIoDevice::stopBuffering() - %s", deviceName.c_str());
	if( hdev <0 )
		return;
	pFS->removeMonitor(hdev);
	close(hdev);
	hdev=-1;
	enableBuffering(false);
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel *pCh = (IIoChannel *)*it;
		if ( pCh->pIo ) {
			pCh->enableBuffering(false);
		}
	}
}

void IIoDevice::addChannel(const char *name, int cap )
{
	DBG(LOG_DEBUG,"addChannel %s %s", name, capabilitiesToString(cap).c_str());
	IIoChannel * pCh = findChannel(name);
	if ( pCh ) {
		DBG(LOG_DEBUG,"existing Channel");
		pCh->capabilities |= cap;
	}
	else {
		DBG(LOG_DEBUG,"new Channel");
		pCh = new IIoChannel(name,cap, this);
		iioChannels.push_back(pCh);
	}
	capabilities |= cap;
}

// When the device has data to read this function is called
void IIoDevice::onSelected()
{
//	DBG(LOG_DEBUG,"IIoDevice::onSelected - %s", deviceName.c_str() );

	char read_buf[512];
	/* read */
	int read_cnt=read(hdev,read_buf,sizeof(read_buf));
	if(read_cnt<=0) {
		DBG(LOG_DEBUG,"failed to read (iio_dev=%s) - %s", deviceName.c_str(), strerror(errno));
		return;
	}
//	DBG(LOG_DEBUG,"read_cnt %d %d %s", hdev, read_cnt, strerror(errno) );

#if 0
	char buf[128];
	char * p = buf;
	int cnt = read_cnt;
	if ( cnt > 20 ) cnt = 20;
	for (int i = 0; i<cnt;i++)
		p += sprintf( p, "  %2x",read_buf[i]  );
	DBG(LOG_ERR,"read from %s - <%s>", deviceName.c_str(), buf );
#endif
	if(read_cnt%sizeofAllChannels) {
		DBG(LOG_DEBUG,"incorrect read count for %s(%s) readcnt=%d,colbcnt=%d", deviceName.c_str(), nameFromNameFile.c_str(), read_cnt, sizeofAllChannels);
		return;
	}

	// The channels are interleaved in the buffer, this pulls out the channel's data
	int channelOffset = 0;
	for (std::vector<void*>::const_iterator it = iioChannels.begin(); it != iioChannels.end(); ++it) {
		IIoChannel * pCh = (IIoChannel *)*it;
		if ( !pCh->isBuffering() ) {
			continue;
		}
		int dataSize = pCh->dataSize();
		for ( int sampleOffset = 0; ; sampleOffset += sizeofAllChannels ) {
			int offset = channelOffset + sampleOffset;
			if ( (offset+dataSize) > read_cnt) break;
			pCh->push((uint*)&read_buf[offset]);
		}
		pCh->flush();
		channelOffset += dataSize;
	}
}

bool IIoDevice::registerWith(FileSelector *pfs)
{
	// The actual registering is done later, just save the pointer for now
//	DBG(LOG_DEBUG,"registerWith %s  %d", deviceName.c_str(), hdev );
	pFS = pfs;
	return true;
}

// Class to traverse output channels and add to IIO device
class IIoOutputs_Traverser : public DirectoryTraverser {
public:
	IIoOutputs_Traverser(const char *base, IIoDevice *iio_dev ):
		DirectoryTraverser(base, "out_.*_raw" ),
		pIioDev(iio_dev){};
private:
	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {

		DBG(LOG_DEBUG,"IIoOutputs_Traverser %s",de->name);
		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+4 );
		char *endName = strstr(elemName,"_raw");
		if (!endName)
			return true;
		*endName=0;

		int out_capability = IO_INFO_CAP_MASK_AOUT;
		if ( strstr(elemName,"_DIG" ) )
			out_capability = IO_INFO_CAP_MASK_DOUT;
		pIioDev->addChannel(elemName,out_capability);
		return true;
	}
	IIoDevice *pIioDev;
	IIoOutputs_Traverser();
};

// Class to traverse input channel and add to IIO device
class IIoInputs_Traverser : public DirectoryTraverser {
public:
	IIoInputs_Traverser(const char *base, IIoDevice *iio_dev ):
		DirectoryTraverser(base, "in_.*_raw" ),
		pIioDev(iio_dev){};

private:
	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {

		DBG(LOG_DEBUG,"IIoInputs_Traverser %s",de->name);
		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+3 );
		char *endName = strstr(elemName,"_raw");
		if (!endName)
			return true;
		*endName=0;
		pIioDev->addChannel(elemName,chanNameToCapability(elemName));
		return true;
	}
	IIoDevice *pIioDev;
	IIoInputs_Traverser();
};

// Class to traverse scan elements and disable them
// While we're at it total up the capabilities
class BufferedInput_Traverser : public DirectoryTraverser {
public:
	BufferedInput_Traverser(const char *base, IIoDevice *iio_dev ):
		DirectoryTraverser(base, "in_.*_en" ),
		pIioDev(iio_dev){};
private:
	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de )
	{
		DBG(LOG_DEBUG,"IIOBufferedInput_Traverser %s",de->name);
		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+3 );
		char *endName = strstr(elemName,"_en");
		if (!endName) {
			return true;
		}
		*endName=0;
		pIioDev->addChannel(elemName,chanNameToCapability(elemName) | IO_INFO_CAP_MASK_BUFFERED );
		return true;
	}
	IIoDevice *pIioDev;
	BufferedInput_Traverser();
};

// We scan the /sys directory to discover the IIO device's channels
// Buffered inputs first!
bool IIoDevice::scanChannels()
{
	BufferedInput_Traverser btraverser(pathToScanElements.c_str(), this );
	btraverser.traverse();
	IIoInputs_Traverser itraverser(pathToIIoDevice.c_str(), this );
	itraverser.traverse();
	IIoOutputs_Traverser otraverser(pathToIIoDevice.c_str(), this );
	otraverser.traverse();
	return true;
}

// Class to scan and add IIO devices
class IIO_Traverser : public DirectoryTraverser {
public:
	IIO_Traverser():
		DirectoryTraverser("/sys/bus/iio/devices", "iio:device[0-9]*")
		{};
private:
	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {
		/* create iio device */
		DBG(LOG_DEBUG,"IIO_Traverser %s",de->name);

		IIoDevice *pIioDev = new IIoDevice(de->name, getFullName(de->name));
		if( !pIioDev->scanChannels() ) {
			DBG(LOG_DEBUG,"failed to initialize iio device - %s",de->name);
			delete pIioDev;
			return true;
		}
		iioDevs.push_back(pIioDev);
		return true;
	}
};

int iio_create()
{
	// Scan and add the IIO from /sys
	IIO_Traverser iiotraverser;
	iiotraverser.traverse();
	return 0;
}

void iio_destroy()
{
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice *pIioDev = (IIoDevice *)*it;
		delete pIioDev;
	}
	iioDevs.clear();
}

// Search all the IIO devices for a channel - only used for static IO
IIoChannel * iio_findChannel(const string &channelName)
{
	DBG(LOG_DEBUG,"iio_AttachChannel - %s",channelName.c_str() );
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice *pIioDev = (IIoDevice *)*it;
		IIoChannel *pCh = pIioDev->findChannel(channelName.c_str());
		if (pCh) {
			return pCh;
		}
	}
	return 0;
}

void iio_changeSamplingFreq(int freq)
{
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice *pIioDev = (IIoDevice *)*it;
		pIioDev->writeIntToFile("configs/sampling_freq", freq);
	}
}

#ifdef V_MODCOMMS
void iio_addModcomms()
{
//	DBG(LOG_DEBUG,"iio_addModcomms");
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice *pIIoDev = (IIoDevice *)*it;
		if ( pIIoDev->isModcommsDevice ) {
			iioCreate(pIIoDev);
		}
	}
}
#endif
