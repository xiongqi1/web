/*!
 * Industrial IO support
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
extern "C"
{
#include "iio.h"
#include "xgpio.h"
#include "cdcs_shellfs.h"
#include "commonIncludes.h"
#include "tick64.h"
#include "dq.h"
#include "rdb.h"

} // extern "C"

#include <string>
#include <vector>

#define INTEGER_STRING_SIZE 32 // 32 characters should be more than enough to read an integer

/*
 * An example of an IIO device is the on chip ADC. This would typically have multiple analog inputs.
 * Each analog input appears as channel under the IIO device and names would be like voltage0 voltage1 etc
 */
class IIoChannel {
public:
	char *channelName;	/* channel name like "voltage7" */
	int index;			/* index configuration information - driver-specified physical channel index (read from /sys) */
	int phyidx;			/* physical index - sequence of buffering (capturing) */
	int iomgrIdx;		/* logical index into IoMgrs main table of IOs */
	struct dq_t *q;		/* This is the buffer ( queue ) for the channel data */

	int is_valid() { return q!=NULL; };

	int init(const char *_name, int idx);
	int init_buffering(const char *path);
	void fini();
	int push(const char* buf,int conv_byte_cnt,int conv_column_byte_cnt,int total_byte_cnt);
};

/*
	iio device channel
*/

void IIoChannel::fini() {
	if(q) {
		dq_destroy(q);
		q = NULL;
	}
	safeFree(channelName);
}

int IIoChannel::push(const char* buf,int conv_byte_cnt,int conv_column_byte_cnt,int total_byte_cnt) {
	int total_loop;
	int sp;
	const char *src;
	int i;
	/* check validation - total length vs conversion byte count */
	if(total_byte_cnt%conv_byte_cnt) {
		DBG(LOG_ERR,"total length is not multiplication of conversion byte count (tlen=%d,cbyte=%d,ch=%d)",total_byte_cnt,conv_byte_cnt,iomgrIdx);
		goto err;
	}

	/* check validation - total length vs conversion column byte count */
	if(total_byte_cnt%conv_column_byte_cnt) {
		DBG(LOG_ERR,"total length is not multiplication of column byte (tlen=%d,cbyte=%d,ch=%d)",total_byte_cnt,conv_byte_cnt,iomgrIdx);
		goto err;
	}

	/* check validation - supported byte count */
	if(conv_byte_cnt!=4 && conv_byte_cnt!=2 && conv_byte_cnt!=1) {
		DBG(LOG_ERR,"conversion byte count not supported (bcnt=%d)",conv_byte_cnt);
		goto err;
	}

	/* get total loop count */
	total_loop=total_byte_cnt/conv_column_byte_cnt;

	/* get start point of channel */
	sp=conv_byte_cnt*phyidx;
	src=buf+sp;
	/* extract sampled conversion from chunk */
	i=0;
	while(i++<total_loop) {
		unsigned int v;
		/* get a single conversion based on byte count - TODO: use more platform independent types */
		if(conv_byte_cnt==4) {
			v=*(unsigned int*)src;
		}
		else if (conv_byte_cnt==2) {
			v=*(unsigned short*)src;
		}
		else if (conv_byte_cnt==1) {
			v=*(unsigned char*)src;
		}
		else {
			v=0;
			DBG(LOG_ERR,"interal error - byte count not supported (bcnt=%d)",conv_byte_cnt);
		}

		/* store */
		if(dq_push(q,&v)<0) {
			DBG(LOG_ERR,"failed to push a new sampled value");
			goto err;
		}

		/* increase source */
		src+=conv_column_byte_cnt;
	}
	return 0;
err:
	return -1;
}

/*
 * Parameters to this function are
 *  path	- Path into /sys given channel names, example - /sys/bus/iio/devices/iio:device3/scan_elements
 *  _name	- Channel name, example voltage1
 *  idx		- index into iomgr main array of IOs
 *
 */
int IIoChannel::init(const char *name, int idx) {
	iomgrIdx=idx;
	channelName=strdup(name);
	if(!channelName ) {
		DBG(LOG_ERR,"failed to create IIO Channel - out of memory");
		return -1;
	}
	return 0;
}

int IIoChannel::init_buffering(const char *path ) {
	char fname[PATH_MAX];
	char buf[INTEGER_STRING_SIZE];	// 16 should be more than enough to read an integer
	/* TODO: allow variable byte counts per each channel */
	#define IIO_BUFFERING_BYTE_COUNT	4
	const int queue_count=(IIO_DEV_BUFFER_LEN+IIO_BUFFERING_BYTE_COUNT-1)/IIO_BUFFERING_BYTE_COUNT;
	q = dq_create(queue_count,sizeof(unsigned int));;
	if(!q ) {
		DBG(LOG_ERR,"failed to create IIO Channel - out of memory");
		return -1;
	}
	snprintf(fname,sizeof(fname),"%s/in_%s_index", path, channelName);
	if ( shellfs_cat( fname, buf, sizeof(buf) ) < 0 ) {
		DBG(LOG_ERR,"failed to read channel index (idx=%s)", fname);
		return -1;
	}
	index = xgpio_scan_uint(buf);
	return 0;
}

/*
	iio device
*/
class IIoDevice {
public:
	int valid;
	char *iioDeviceName;
	char *pathToIIoDevice;
	char *nameFromNameFile;
	int isModcommsDevice;
	int hdev; /* device handle */
	int scanCapability; // Sum capability of all the scan elements
	int numChannels; /* channel count */
	IIoChannel iio_ch[IIO_DEVICE_CHANNEL_MAX];
	int col_byte_len; /* length of buffered column byte */
	char* read_buf;
	int read_buf_len;
	int prev_iio_running_stat;
	int initialIoMode;
	int reuseIoIdx; // This is to stop allocating new Ios for mode changes
	/*
	 * Parameters to this call are
	 * full name - /sys/bus/iio/devices/iio:device3
	 * _name	- iio:device3
	 * reuseIoIdx stops allocating a new IO ( for mode change )
	 */
	int init(const char* name, const char* bname, int reuseIoIdx = -1 );
	int get_config_integer(const char *name,int *value);
	int set_config_integer(const char *name,int value);
	int set_ch_config_integer(const char *ch_name, const char *name, int value);
	int init_buffering();
	int start_buffering();
	int proc_buffering();
	void stop_buffering();
	void resume();
	int suspend();

	int is_buffering() {
		return hdev>=0;
	}

	int get_buffer_handle() {
		return hdev;
	}
	void fini_buffering();
	void fini();

	IIoChannel * findChannel(const char * chanName );
	int add_iio_device(int capability, char * chanName );
};

int IIoDevice::add_iio_device(int capability, char * chanName)
{
	struct io_info_t newIo;
	memset(&newIo, 0, sizeof(newIo));

	struct io_info_t * pIo = &newIo;
	pIo->power_type = IO_MGR_POWERSOURCE_NONE;
	pIo->chan_name = strdup(chanName);
	pIo->cap = capability;
	pIo->scale = IO_INFO_SCALE(1,1);
	pIo->correction = 1;
	pIo->d_in_threshold = 0;
	pIo->hardware_gain = 1;
	pIo->init_mode = initialIoMode;
	pIo->gpio_output_pin = GPIO_UNUSED;
	pIo->gpio_input_pin = GPIO_UNUSED;
	pIo->gpio_pullup_pin = GPIO_UNUSED;
	pIo->dynamic = 1;
	// chanName is of form io-mice.1-1
	// but for branding we want io-mice.1-MIO1
	char * pDot = strchr(chanName,'.');
	if (pDot) {
		char * pDash = strchr(pDot,'-');
		if (pDash) {
			size_t pos = pDash - chanName + 1;
			std::string label (chanName);
			label.insert(pos,"MIO");
			return add_io(&newIo, label.c_str() );
		}
	}
	return add_io(&newIo, chanName);
}

int IIoDevice::get_config_integer(const char *name,int *value) {
	char* ep;
	char str[INTEGER_STRING_SIZE];	// Enough storage for an integer string

	if( shellfs_cat_with_path( pathToIIoDevice, name, str, sizeof(str) ) <= 0 ) {
		DBG(LOG_ERR,"failed to get config - name=%s path %s", name, pathToIIoDevice );
		return -1;
	}
	/* reset errno */
	errno=0;

	/* convert */
	*value=strtol(str,&ep,0);

	/* check error */
	if((ep==name) || errno) {
		DBG(LOG_ERR,"failed to convert config (iio_dev=%s,name=%s) - %s",iioDeviceName,name,strerror(errno));
		return -1;
	}
	return 0;
}

int IIoDevice::set_config_integer(const char* name,int value) {
	char str[IIO_MAX_SYSFS_CONTENT_LEN];
	snprintf(str,sizeof(str),"%d",value);
	return shellfs_echo_with_path(pathToIIoDevice,name,str);
}

int IIoDevice::set_ch_config_integer(const char *ch_name, const char *name, int value) {
	char ch_cfg[PATH_MAX];
	snprintf(ch_cfg,sizeof(ch_cfg),"in_%s_%s", ch_name, name);
	return set_config_integer(ch_cfg,value);
}

void IIoDevice::stop_buffering() {
	/* bypass if not valid */
	if(!valid)
		return;

	/* close */
	if(hdev>=0)
		close(hdev);
	hdev=-1;

	/* disable buffer */
	set_config_integer(IIO_DIR_BUFFER_EN,0);
}

int IIoDevice::proc_buffering() {
	int read_cnt;
	IIoChannel* ch;
	unsigned int* samples;
	int sample_count;
	int i;
	unsigned long long now;

	/* read */
	read_cnt=read(hdev,read_buf,read_buf_len);

	if(read_cnt<0) {
		DBG(LOG_ERR,"failed to read (iio_dev=%s) - %s", iioDeviceName, strerror(errno));
		goto err;
	}

	/* get tick */
	now=tick64_get_ms();

	if(read_cnt%col_byte_len) {
		DBG(LOG_ERR,"incorrect read count for %s(%s) readcnt=%d,colbcnt=%d",iioDeviceName, nameFromNameFile, read_cnt, col_byte_len);
	}

	/* extract to each channel */
	for( i=0; i < numChannels; i++) {
		ch=&iio_ch[i];

		/* skip invalid channels */
		if(!ch->is_valid()) {
			continue;
		}

		/* read channel */
		if( ch->push( read_buf,IIO_BUFFERING_BYTE_COUNT, col_byte_len,read_cnt) <0 ) {
			DBG(LOG_ERR,"failed to read a channel for %s(%s) from  buf (ch=%s)",iioDeviceName, nameFromNameFile, ch->channelName);
			continue;
		}
	}

	/* call call-back functions */
	for( i=0; i< numChannels; i++ ) {
		ch=&iio_ch[i];

		/* skip invalid channels */
		if(!ch->is_valid()) {
			continue;
		}

		/* get samples */
		samples=(unsigned int*)dq_peek(ch->q,&sample_count);

		/* call callback functions */
		if(sample_count)
			on_iio_dev(this, ch->iomgrIdx,samples,sample_count,now);

		/* clear q */
		dq_clear(ch->q);
	}
	return 0;

err:
	return -1;
}

void IIoDevice::fini_buffering() {
	/* bypass if not valid */
	if(!valid)
		return;

	stop_buffering();

	/* destroy channels */
	for(int i=0;i<numChannels;i++) {
		iio_ch[i].fini();
	}

	/* destroy buf */
	safeFree(read_buf);
}

IIoChannel * IIoDevice::findChannel(const char * chanName ) {
	for(int i=0; i < numChannels; i++) {
		IIoChannel* ch = &iio_ch[i];
		if ( ch->channelName )
			if ( 0 == strcmp( chanName, ch->channelName) )
				return ch;
	}
	return NULL;
}

static int elementToCapability( const char * el )
{
	if ( strstr(el, "current_") )
		return IO_INFO_CAP_MASK_CIN;
	if ( strstr(el, "NAMUR") )
		return IO_INFO_CAP_MASK_NAMUR;
	if ( strstr(el, "ct_") )
		return IO_INFO_CAP_MASK_CT;
	if ( strstr(el, "temp_") )
		return IO_INFO_CAP_MASK_TEMP;
	if ( strstr(el, "_CCI") )
		return IO_INFO_CAP_MASK_CC;
	if ( strstr(el, "voltage_ADC") || strstr(el, "voltage_INP") )
		return IO_INFO_CAP_MASK_AIN|IO_INFO_CAP_MASK_VDIN;
	return 0;
}

// Class to traverse scan elements and disable them
// While we're at it total up the capabilities
class DisableScanElements_Traverser : public DirectoryTraverser {
public:
	int capabilities;
	DisableScanElements_Traverser(const char * base):
		DirectoryTraverser(base, "in_.*_en" ),
		capabilities(0){};

	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de )
	{
		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+3 );
		char * endName = strstr(elemName,"_en");
		if (!endName)
			return true;
		*endName=0;
		capabilities |= elementToCapability(elemName);
		/* disable scan element */
		shellfs_echo( getFullName(de->name),"0");
		return true;
	}
private:
	DisableScanElements_Traverser();
};

// Class to traverse scan elements and add IIO devices
class ScanElements_Traverser : public DirectoryTraverser {
	const char * path;
	IIoDevice * iioDev;
public:
	ScanElements_Traverser(const char * base, IIoDevice * iio_dev ):
		DirectoryTraverser(base, "in_.*_en" ),
		path(base),
		iioDev(iio_dev){};

	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de )
	{
		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+3 );
		char * endName = strstr(elemName,"_en");
		if (!endName)
			return true;
		*endName=0;
		int capability = elementToCapability(elemName);
		if (iioDev->initialIoMode) {
			if ( 0 == (capability&iioDev->initialIoMode) ) {
//				DBG(LOG_INFO,"mode %s does not match element %s",snprintf_io_info_cap(iioDev->initialIoMode),elemName);
				return true; // Mode has been specified and its not this one
			}
		}
		else
			iioDev->initialIoMode=capability;
		// See if the channel name is one of the system defined channels
		// or for Modcomms use the dynamic channel name
		char * chanName = iioDev->isModcommsDevice ? iioDev->nameFromNameFile:elemName;
		int idx;
		if (iioDev->reuseIoIdx>=0)
			idx = iioDev->reuseIoIdx;
		else
			idx = getIo_Index(chanName);
		if ( (idx <0) && iioDev->isModcommsDevice ) {
			idx = io_cnt;
			if ( iioDev->add_iio_device(iioDev->scanCapability, chanName) < 0 )
				return false;
		}
		if ( idx >=0 ) {
			io_info[idx].cap |= capability;
			IIoChannel *ch = &iioDev->iio_ch[iioDev->numChannels];

			/* init. channel */
			if ( ch->init(elemName,idx) >= 0 ) {
				/* enable/disable scan element */
				if ( shellfs_echo( getFullName(de->name),"1") >= 0 ) {
					if ( ch->init_buffering(path) >= 0 ) {
						/* increase registered channel count */
						iioDev->numChannels++;
					}
				}
				else
					ch->fini();
			}
		}
		return true;
	}
private:
	ScanElements_Traverser();
};


// Class to traverse output channel and add IIO devices
class IIoOutputs_Traverser : public DirectoryTraverser {
public:
	IIoOutputs_Traverser(const char * base, IIoDevice * iio_dev ):
		DirectoryTraverser(base, "out_.*_raw" ),
		iioDev(iio_dev){};

	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {

		// extract channel name from the filename
		char elemName[PATH_MAX];
		strcpy(elemName,de->name+4 );
		char * endName = strstr(elemName,"_raw");
		if (!endName)
			return true;
		*endName=0;

		char * chanName = iioDev->isModcommsDevice ? iioDev->nameFromNameFile:elemName;
		/*
		 * The output channel is either one already seen ( has buffered input ) or a new channel
		 */
		int out_capability = IO_INFO_CAP_MASK_AOUT;
		if ( strstr(elemName,"_DIG" ) )
			out_capability = IO_INFO_CAP_MASK_DOUT;
		if ( iioDev->numChannels ) {
			// This already seen modify the IO with output capability
			IIoChannel * ch = &iioDev->iio_ch[0];
			io_info_t * pIo = &io_info[ch->iomgrIdx];
			pIo->cap |= out_capability;
		}
		else {
			// Create a new IO with output only capability
			int idx = io_cnt;
			if ( iioDev->reuseIoIdx >= 0 )
				idx = iioDev->reuseIoIdx;
			else {
				if (!iioDev->initialIoMode)
					iioDev->initialIoMode = out_capability;
				if ( iioDev->add_iio_device(out_capability|iioDev->scanCapability, chanName) < 0 )
					return false;
			}
			IIoChannel *ch = &iioDev->iio_ch[iioDev->numChannels];
			/* init. channel */
			if ( ch->init(elemName,idx) >= 0 ) {
				/* increase registered channel count */
				iioDev->numChannels++;
			}
		}
		return true;
	}
private:
	IIoDevice * iioDev;
	IIoOutputs_Traverser();
};

int IIoDevice::init_buffering() {
	int i;
	char element[PATH_MAX];
	int min;
	int max;
	int po;

	IIoChannel* ch;

	/* disable buffer */
	set_config_integer(IIO_DIR_BUFFER_EN,0);
#ifdef V_PROCESSOR_am335
	/* set continuous mode */
	shellfs_echo_with_path(pathToIIoDevice,IIO_DIR_MODE,"continuous");
#endif

	snprintf(element,sizeof(element),"%s/" IIO_DIR_SCAN_ELEMENTS, pathToIIoDevice);
	DisableScanElements_Traverser disableeltraverser(element);
	disableeltraverser.traverse();
	scanCapability = disableeltraverser.capabilities;

	ScanElements_Traverser eltraverser(element, this );
	eltraverser.traverse();

	/* error if no channel available */
	if(!numChannels) {
		DBG(LOG_ERR,"no buffered channels avaliable for %s(%s)",iioDeviceName,nameFromNameFile);
		goto err;
	}
	DBG(LOG_INFO,"%d buffered channels avaliable for %s(%s)",numChannels,iioDeviceName,nameFromNameFile);

	/* get min and max of index */
	min=-1;
	max=-1;
	for(i=0;i<numChannels;i++) {
		ch=&iio_ch[i];

		if(!ch->channelName)
			continue;

		if(min<0 || ch->index<min)
			min=ch->index;

		if(max<0 || ch->index>max)
			max=ch->index;
	}

	/* set physical order TODO: it is better to do this inside of channel object */
	po=0;
	for(int j=min;j<=max;j++) {
		for( i=0; i < numChannels; i++ ) {
			if(iio_ch[i].index==j) {
				iio_ch[i].phyidx=po++;
				break;
			}
		}
	}

	/* calc. buffer len */
	col_byte_len = numChannels*IIO_BUFFERING_BYTE_COUNT;
	read_buf_len= (IIO_DEV_BUFFER_LEN/ col_byte_len)* col_byte_len;

	/* allocate buf */
	read_buf=(char*)malloc(read_buf_len);
	if(!read_buf) {
		DBG(LOG_ERR,"failed to allocate read_buf");
		goto err;
	}

	/* set kernel driver buffer length */
	set_config_integer(IIO_DIR_BUFFER_LEN, read_buf_len);
	return 0;

err:
	return -1;
}

int IIoDevice::start_buffering() {
	char dev_name[PATH_MAX]; /* device name of iio */
	/* set proc function */

	/* enable buffer */
	if ( set_config_integer(IIO_DIR_BUFFER_EN,1) < 0 )
	{
			DBG(LOG_ERR,"iio device %s - doesn't support buffering", iioDeviceName );
			goto err;
	};

	/* open iio device node */
	snprintf(dev_name,sizeof(dev_name),"/dev/%s",iioDeviceName);
	hdev=open(dev_name,O_RDONLY|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
	if(hdev<0) {
		DBG(LOG_ERR,"failed to open iio device %s - %s", dev_name, strerror(errno));
		goto err;
	};
	return hdev;

err:
	return -1;
}

void IIoDevice::fini() {
	/* bypass if not valid */
	if(!valid)
		return;

	/* fini. buffering */
	fini_buffering();

	/* clear names */
	safeFree(pathToIIoDevice);
	safeFree(iioDeviceName);
	safeFree(nameFromNameFile);
	/* reset validation flag */
	valid=0;
}

int IIoDevice::init( const char* name, const char* bname, int _reuseIoIdx ) {
	char tmpNameFromNameFile[IIO_MAX_SYSFS_CONTENT_LEN]; /* dts name information of iio */
	char rdbName[RDB_VARIABLE_NAME_MAX_LEN];
	const char * pModeStr;
	/* make sure that this is our first time to initiate */
	if(valid) {
		DBG(LOG_ERR,"iio device already initiated");
		goto err;
	}
	memset(this,0,sizeof(*this));

	reuseIoIdx = _reuseIoIdx;
	hdev=-1;

	/* store device name */
	iioDeviceName = strdup(name);
	pathToIIoDevice = strdup(bname);

	/* get dts name */
	if(!shellfs_cat_with_path( bname, IIO_DIR_NAME, tmpNameFromNameFile, sizeof(tmpNameFromNameFile) ) ) {
		DBG(LOG_ERR,"cannot read dts name - iio_dev=%s",iioDeviceName);
		goto err;
	}
	trimEnd(tmpNameFromNameFile);
	nameFromNameFile=strdup(tmpNameFromNameFile);
	isModcommsDevice = is_modcomms_device(nameFromNameFile);
	// Get io mode for modcomms device

	snprintf(rdbName, sizeof(rdbName), "sys.sensors.io.%s.mode", nameFromNameFile);
	pModeStr = rdb_getVal(rdbName);
	if ( pModeStr ) {
		DBG(LOG_INFO,"init mode is %s for  iio_dev=%s", pModeStr, nameFromNameFile);
		initialIoMode = sscanf_io_info_cap(pModeStr);
	}

	/* check uevent */
	if(!shellfs_is_existing_with_path( bname, IIO_DIR_UEVENT ) ) {
		DBG(LOG_ERR,"uevent not found - iio_dev=%s",iioDeviceName);
	}

	/* check dev node */
	if(!shellfs_is_existing_with_path( "/dev", iioDeviceName ) ) {
		DBG(LOG_ERR,"device node not found - iio_dev=%s",iioDeviceName);
		goto err;
	}

	if (init_buffering() >= 0) {
		if ((start_buffering() < 0)) {
			DBG(LOG_ERR, "IIO buffer device not available");
		}
	}
{ // These braces required to kill error of goto bypass constructor
	IIoOutputs_Traverser traverser(pathToIIoDevice, this );
	traverser.traverse();
}
	/* set validation flag */
	valid=1;
	return 0;

err:
	fini();
	return -1;
}

void IIoDevice::resume() {
	/* resume if previoiusly buffering */
	if(prev_iio_running_stat) {
		DBG(LOG_DEBUG,"resume iio device");
		start_buffering();
	}
}

int IIoDevice::suspend() {
	/* store current buffer stat */
	prev_iio_running_stat=is_buffering();

	if(prev_iio_running_stat) {
		DBG(LOG_DEBUG,"suspend iio device");
		stop_buffering();
	}
	return 0;
}

static int max_handle;
static std::vector<void*> iioDevs;	// Our container of Io devices - vector<void*> used to avoid bloat

void iio_destroy() {
	/* fini. all iio devices */
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * pIioDev = (IIoDevice *)*it;
		pIioDev->fini();
		free(pIioDev);
	}
	iioDevs.clear();
}

// Class to scan and add IIO devices
class IIO_Traverser : public DirectoryTraverser {
public:
	IIO_Traverser():
		DirectoryTraverser(IIO_DIR_DEVICE_DIR, IIO_DIR_DEVICE_FILES)
		{};

	// This is called for every matching directory entry
	virtual bool process( shellfs_dirent_t *de ) {
		/* create iio device */
		DBG(LOG_DEBUG,"IIO_Traverser %s",de->name);
		IIoDevice * pIioDev =(IIoDevice*)calloc(1,sizeof(IIoDevice));
		if(!pIioDev) {
			DBG(LOG_ERR,"failed to allocate memory for IIoDevice object");
			return true;
		}
		if(pIioDev->init(de->name, getFullName(de->name))<0) {
			DBG(LOG_ERR,"failed to create iio device - %s",de->name);
			pIioDev->fini();
			free(pIioDev);
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

	int count = iioDevs.size();
	DBG(LOG_DEBUG,"iio device count = %d",count);
	if(!count) {
		DBG(LOG_ERR,"no iio device found");
		iio_destroy();
		return -1;
	}
	return 0;
}

int get_iio_maxhandle() {
	return max_handle;
}

void fdsset_iio(fd_set * fds) {
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		int hiiodev = iio_dev->get_buffer_handle();
		if ( hiiodev<0 )
			continue;
		if( hiiodev > max_handle )
			max_handle = hiiodev;
		FD_SET(hiiodev,fds);
	}
}

void process_iio(fd_set * fds) {
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		int hiiodev = iio_dev->get_buffer_handle();
		if ( hiiodev<0 ) {
			continue;
		}
		if( FD_ISSET(hiiodev,fds)) {
			iio_dev->proc_buffering();
		}
	}
}

int set_iio_ch_config_integer(int idx, const char* name,int value) {
	const struct io_info_t* io = &io_info[idx];
	const char* ch_name = io->chan_name;
	DBG(LOG_INFO,"set_iio_ch_config_integer (%s,%s,%d)",ch_name,name, value);
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		for(int ch=0; ch < iio_dev->numChannels; ch++) {
			const IIoChannel * pChan = &iio_dev->iio_ch[ch];
			if ( pChan->iomgrIdx == idx ) {
				int rc = iio_dev->suspend();
				if(rc >=0) {
					iio_dev->set_ch_config_integer(pChan->channelName,name,value);
					iio_dev->resume();
				}
			}
		}
	}
	return 0;
}

/* change sampling frequency */
void iio_set_config_integer(const char *name, int value) {
	DBG(LOG_DEBUG,"applying sampling frequency (freq=%d)",value);
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		iio_dev->set_config_integer(name,value);
	}
}

int iio_set_output(const struct io_info_t * io ,int val) {
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		for(int ch=0; ch < iio_dev->numChannels; ch++) {
			IIoChannel * pChan = &iio_dev->iio_ch[ch];
			if ( &io_info[pChan->iomgrIdx] == io ) {
				char fileName[PATH_MAX];
				snprintf(fileName,sizeof(fileName),"out_%s_raw", pChan->channelName );
				char tmpBuf[64]; // for formatting numbers
				snprintf(tmpBuf,sizeof(tmpBuf),"%d",val);
				return shellfs_echo_with_path( iio_dev->pathToIIoDevice,  fileName, tmpBuf );
			}
		}
	}
	return -1;
}

int iio_set_mode(const struct io_info_t * io ,int mode)
{
	int rc = -1;
	for (std::vector<void*>::const_iterator it = iioDevs.begin(); it != iioDevs.end(); ++it) {
		IIoDevice * iio_dev = (IIoDevice *)*it;
		for(int ch=0; ch < iio_dev->numChannels; ch++) {
			IIoChannel * pChan = &iio_dev->iio_ch[ch];
			int reuseIoIdx = pChan->iomgrIdx; // This is used to stop creating a new IO
			if ( &io_info[reuseIoIdx] == io ) {
				if ( !iio_dev->isModcommsDevice )
					return rc;
				const char * modeStr = snprintf_io_info_cap(mode);
				DBG(LOG_INFO,"switching %s to %s idx %d", iio_dev->nameFromNameFile, modeStr, reuseIoIdx );
				char rdbName[RDB_VARIABLE_NAME_MAX_LEN];
				snprintf(rdbName, sizeof(rdbName), "sys.sensors.io.%s.mode", iio_dev->nameFromNameFile);
				rdb_setVal(rdbName, modeStr, 1);
				const char *iioDeviceName = strdup(iio_dev->iioDeviceName);
				const char *pathToIIoDevice = strdup(iio_dev->pathToIIoDevice);
				if (iioDeviceName&&pathToIIoDevice) {
					iio_dev->fini();
					rc = iio_dev->init(iioDeviceName, pathToIIoDevice, reuseIoIdx);
				}
				safeFree(iioDeviceName);
				safeFree(pathToIIoDevice);
				return rc;
			}
		}
	}
	return rc;
}

