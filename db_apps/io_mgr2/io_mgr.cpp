/*
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 * The IoMgr sits between the IO ( Industrial Io (IIo) and GPIO ) and our
 * RDB. Inputs are written to the RDB. Updates to the RDB are output.
 * Also Io modes are configured.
 */

#include "io_mgr.h"
#include "iio.h"
#include "xgpio.h"
#include "timer.h"
#include "HFI.h"
#include "tcp.h"
#include "iir_lpfilter.h"
#include "utils.h"
#include <cdcs_fileselector.h>
#include <cdcs_rdb.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <signal.h>

using namespace std;

uint samplingFrequency = 20;		/* current hardware sampling frequency, default 10 Hz */
static uint rdbUpdateFrequency = 5;	/* current rdb update frequency, default 5 Hz */

static vector<void*> io_array;	// Our main container of Ios  - vector<void*> used to avoid bloat

static RdbSession ioMgrRdbSession;
static FileSelector * pIoMgrFileSelector;

static double inputVoltage;	// This stores the voltage from the active input (assumes one is active at a time)

// This is called when the RDB is changed
static void onRdbDaemonWatchdog(void *param, const char *val)
{
	RdbVar *pRdb = (RdbVar *)param;
	if (!strcmp(val, "1")) { /* bypass if already touched */
		return;
	}

	DBG(LOG_INFO,"reset watchdog rdb");
	pRdb->setVal("1");	/* clear touch */
}

// This is called when the RDB is changed
static void onRdbDbgLevel(void *parm, const char *val)
{
	int loglevel;

	/* use default loglevel if no value exists */
	if(!*val) {
		loglevel=LOG_INFO;
		DBG(LOG_INFO, "no debug level specified - use default LOG_INFO(%d)",loglevel);
	}
	else {
		loglevel=atoi(val);
		DBG(LOG_INFO,"change loglevel to %d",loglevel);
	}
	setlogmask(LOG_UPTO(loglevel));
}

// This is called when the RDB is changed
static void onRdbRdbSamplingFreq(void *parm, const char *val)
{
	DBG(LOG_DEBUG,"change to %s",val);
	int sam_freq=atoi(val);
	if(!sam_freq) {
		DBG(LOG_DEBUG,"invalid  frequency specified (cur_freq=%d)",rdbUpdateFrequency);
		return;
	}
	// This is a hack, although it was not documented anywhere the original
	// variable name had x10 in it and stored 10 times the frequency.
	// The rdb variable name does not give a clue about this
	// so has not to change behaviour - divide by 10 here (0 result doesn't matter)
	rdbUpdateFrequency=sam_freq/10;
	DBG(LOG_DEBUG,"change rdb sampling frequency to %d",rdbUpdateFrequency);
}

// This is called when the RDB is changed
static void onRdbSamplingFreq(void *parm, const char *val)
{
	DBG(LOG_DEBUG,"change to %s",val);
	int sam_freq=atoi(val);
	if(!*val) {
		// nothing to do
	}
	else if(!sam_freq) {
		DBG(LOG_DEBUG,"invalid sampling frequency specified (cur_freq=%d) - use current freq",samplingFrequency);
	} else {
		samplingFrequency=sam_freq;
	}
	iio_changeSamplingFreq(samplingFrequency);
}

static bool running = true;
// This is called when the RDB is changed
static void onRdbDaemonEnable(void *parm, const char *val)
{
	if (!*val) {
		DBG(LOG_DEBUG,"no daemon enable RDB specified");
		return;
	}

	int enabled = atoi(val);
	if (!enabled) {
		DBG(LOG_DEBUG,"daemon disabled by rdb val='%s',enabled='%d')", val, enabled);
		running = false;
	}
}

/*
 * This class is used for analog IO to massages the inputs
 * We have HW - the value as read from the ADC. Typically a 12 bit integer
 * This is scaled to RAW - the voltage at te chip pin
 * Then scaled again to ADC - the value at the connector ( before the resistive divider )
 */
class Scaler {
public:
	double getScale() { return scale; }
	double getCorrection() { return correction; }
	void setScale(double _scale) { scale = _scale; }
	void setCorrection(double _correction) { correction = _correction; }

	/* platform specific parameters */
	static const int IO_MGR_ADC_MAX	=	4095;
#ifdef V_PROCESSOR_am335
	static const double IO_MGR_ADC_PHYS_MAX	= 1.8 ;
#else
	static const double IO_MGR_ADC_PHYS_MAX	= 1.85;
#endif
	Scaler(	double _scale = RESISTOR_DIVIDER(0,1), double _correction=1, double _adcStep = IO_MGR_ADC_PHYS_MAX/IO_MGR_ADC_MAX):
		scale(_scale),
		correction(_correction),
		adcStep(_adcStep)
	{}
	int convert_value_to_adc_hw(double adc)
	{
		return adc/(scale*correction*adcStep);
	}

	void convert_adc_hw_to_value( uint adc_hw, double &adc_raw, double &adc)
	{
		adc_raw=(double)adc_hw*adcStep;
		adc= adc_raw*scale*correction;
	}
private:
	double scale;
	double correction;
	double adcStep;
};

/*
 * This Thunk macro provides a convenient way to define a static function which takes a this pointer
 * as a parameter and calls a class member function.
 * The functions are onRdb functions that are specific to the IO
 * and take single param  character *  for the rdb value
 * className is the name of the IO class
 * fn is the name of the function
 */
#define Thunk(className, fn)\
static void fn(void *parm, const char *val)\
{\
	className *pIo = (className *)parm;\
	pIo->fn##_this(val);\
	/*DBG(LOG_DEBUG,"Thunk returned");*/\
}\
void fn##_this(const char *val )

/*
 * Update an rdb variable if it has changed
 * the value can be int of float etc
 * pRDb is a pointer to the rdb variable
 * fmt is the format specifier - %d or %f etc
 * val and last_val are the new and last values
 */
template<class T>
inline void updateIfChanged( RdbVar *pRdb, const char *fmt, T val, T &last_val )
{
	if ( last_val != val ) {
		char buf[64]; /* Enough space for a double */
		snprintf( buf, sizeof(buf), fmt, val );
		pRdb->setVal(buf);
		last_val = val;
//		DBG(LOG_DEBUG,"update rdb %s=%s", pRdb->getName(), buf );
	}
}

/*
 * This is the base class for all IOs
 * and encapulates common attributes and functions
 */
class Io {
public:
	RdbSession *pRdbSession;	// Each IO gets its own session. This allows us to just subscribe to relevant RDBs
	string rdbPrefix;		// All IO RDBs are based on this - like "sys.sensors.io.xaux6"
	string name;			// IO name like "xaux6"
	string label;			// Alternate name for Modcomms like "gps-mice.1-MIO1"
	uint capabilities;		// The IO modes this can do
	uint mode;			// The IO mode currently selected
	FileSelector *pFS;		// We keep this pointer for removing ourselves from monitoring
	bool buffered;			// If buffered data is provided otherwise we need to poll
	// Our container of HFI outputs - vector<void*> used to avoid bloat
	vector<void*>HfiOutputs;

	// This routine is called when a network connection is made to the
	// HFI Output port.
	void addHfiOutput(Hfi *pHfiOutput)
	{
		HfiOutputs.push_back(pHfiOutput);
	}

	void removeHfiOutput(int fd)
	{
		for (uint i = 0; i < HfiOutputs.size(); i++) {
			Hfi *pHfiOutput = (Hfi *)HfiOutputs[i];
			if (pHfiOutput->getFd() == fd) {
				HfiOutputs.erase(HfiOutputs.begin()+i);
				delete pHfiOutput;
				return;
			}
		}
	}

	Io(const string &name, const string &label, uint _capabilities, uint _mode);
	virtual ~Io();

	virtual void closeRdb()
	{
		delete pRdbSession;
		pRdbSession = 0;
	}

	virtual void registerWith(FileSelector &fs)	// Register our RDB session and
	{						// all the RDB variables we subscribe to
		pFS = &fs;
		string modeStr =  capabilitiesToString(mode);
		DBG(LOG_DEBUG,"%s mode %s", name.c_str(), modeStr.c_str() );
		pRdbSession = new RdbSession;
		if(!pRdbSession->init() ) {
			DBG(LOG_ERR,"failed to init. rdb");
			return;
		}

		rdbPrefix = "sys.sensors.io."+name;
		RdbVar modeRdb(*pRdbSession, rdbPrefix,".mode", NULL,true);
		if (mode) {
			modeRdb.setVal(modeStr);
			if (label.size() ) {
				RdbVar(*pRdbSession, rdbPrefix,".label").setVal(label);
				RdbVar(ioMgrRdbSession, "sys.sensors.io."+label, ".mode", 0, true).setVal(modeStr);
			}
		}
		RdbVar(*pRdbSession, rdbPrefix,".cap").setVal(capabilitiesToString(capabilities) );

		pRdbSession->registerWith(fs);
		pRdbSession->addSubscriber(modeRdb,onRdbMode, this);
	}

	// This routine is the path for physical inputs to make it to the RDB ( or HFI output)
	virtual void pushSamples(uint *samples, int cnt )	// Samples are written to the RDB
	{
//		DBG(LOG_DEBUG,"Io pushSamples %s  (%u)", name.c_str(), cnt );
	}

	// This routine is the path for HFI inputs
	virtual void pushSamples(double *samples, int cnt )	// Samples are written to the RDB
	{
//		DBG(LOG_DEBUG,"Io pushSamples %s  (%u)", name.c_str(), cnt );
	}

	virtual uint readValue()
	{
		DBG(LOG_ERR,"readValue not implemented for %s", name.c_str() );
		return 0;
	}

	void poll()		// If the IO is not buffered, read the input and push it
	{
		if (isInputMode(mode)) {
			if (!buffered) {
				uint value = readValue();
				pushSamples(&value, 1);
			}
		}
	}
private:
	virtual bool changeMode(uint new_mode) = 0;

	// This is called when the RDB is changed
	Thunk(Io, onRdbMode)
	{
		DBG(LOG_INFO,"IO %s %s", name.c_str(), val );

		/* check if it is blank */
		if(!*val) {
			DBG(LOG_INFO,"blank mode detected");
			return;
		}

		/* convert str to mode */
		uint new_mode = stringToCapabilities(val);
		if(!new_mode) {
			DBG(LOG_INFO,"invalid mode detected (mode=%s)",val);
			return;
		}

		if( (new_mode & capabilities) == 0 ) {
			DBG(LOG_INFO,"IO port  not capable %x of mode=%s(%x)", capabilities, val, new_mode);
			return;
		}

		if ( new_mode == mode ) {
			DBG(LOG_DEBUG,"same Mode");
			return ;
		}

		if( !changeMode(new_mode) ) {
			DBG(LOG_INFO,"IO port failed to change to mode=%s",val);
			return;
		}
		mode=new_mode;
		if (label.size()) {
			RdbVar(ioMgrRdbSession,"sys.sensors.io."+label,".mode").setVal(val);
		}
	}
};

// This is called from the IIO Channel to provide its data
void ioPushSamples(Io *pIo, uint *samples, int cnt )
{
//	DBG(LOG_DEBUG,"ioPushSamples %s", pIo->name.c_str() );
	pIo->pushSamples(samples,cnt);
}

// This is the constructor for the base IO class
// One side effect is that "this" is added to IoMgr's container of Ios
Io::Io(const string &_name, const string &_label, uint _capabilities, uint _mode ):
	name(_name),
	label(_label),
	capabilities(_capabilities),
	mode(_mode),
	pFS(0),
	buffered(false)
{
	io_array.push_back(this); //add to IoMgr's container of Ios
}

// This is the destructor for the base IO class
// One side effect is that "this" is removed from IoMgr's container of Ios
Io::~Io()
{
	DBG(LOG_DEBUG,"%s",name.c_str());
	bool found = false;
	int idx = 0;
	for (std::vector<void*>::const_iterator it = io_array.begin(); it != io_array.end(); ++it) {
		Io * pIo = (Io *)*it;
		if ( pIo== this ) {
			found = true;
			break;
		}
		idx++;
	}
	if (found) {
		io_array.erase(io_array.begin()+idx);
	}

	delete pRdbSession;
}

/*
 * This class adds specialization for digital IO but is abstract
 */
class DigIo : public Io {
protected:
	bool	dio_invert;
	int	d_out;
public:
	DigIo(const string &name, const string &label, uint capabilities, uint mode, bool invert_, int d_out_ ):
		Io(name,label,capabilities,mode),
		dio_invert(invert_),
		d_out(d_out_),
		pDinRdb(0),
		last_d_in(0)
	{
		DBG(LOG_DEBUG,"%s mode %s", name.c_str(), capabilitiesToString(mode).c_str() );
	}

	virtual ~DigIo(){
		delete pDinRdb;
	}

	virtual void closeRdb()
	{
		delete pDinRdb;
		pDinRdb = 0;
		Io::closeRdb();
	}

	virtual void registerWith(FileSelector &fs)	// Register our RDB session and
	{						// all the RDB variables we subscribe to
		Io::registerWith(fs);
		RdbVar d_invertRdb(*pRdbSession, rdbPrefix,".d_invert","0",true);
		d_invertRdb.setVal(dio_invert?"1":"0");
		pRdbSession->addSubscriber( d_invertRdb, onRdbDioInvert, this );
		if (mode & digitalInputMode) {
			pDinRdb = new RdbVar(*pRdbSession, rdbPrefix,".d_in","0");
		}
		if (mode & digitalOutputMode) {
			RdbVar d_outRdb(*pRdbSession, rdbPrefix,".d_out");
			// The RDB d_out variable always overrides the defaults
			if ( d_outRdb.getVal().length() == 0 ) {
				DBG(LOG_DEBUG,"No Rdb Output %s", name.c_str() );
				d_outRdb.setVal(d_out?"1":"0");
			}
			pRdbSession->addSubscriber(d_outRdb, onRdbDout, this );
		}
	}

	virtual bool outputValue(int val)
	{
		DBG(LOG_DEBUG,"DoutIo::outputValue %s", name.c_str() );
		return false;
	}

	virtual void pushSamples(uint *samples, int cnt )
	{
		if ( isOutputMode(mode)) {		// This only happens with data from the HFI
			outputValue(samples[cnt-1]);	// Just output the last value
			return;
		}
		if (HfiOutputs.size()) {
			for ( uint i = 0; i < HfiOutputs.size(); i++ ) {
				Hfi *pHfiOutput = (Hfi *)HfiOutputs[i];
				if (!pHfiOutput->pushSamples(samples,cnt, name.c_str())) {
					HfiOutputs.erase(HfiOutputs.begin()+i);
					delete pHfiOutput;
				}
			}
		}
		uint d_in = samples[cnt-1];
		if(dio_invert)
			d_in = !d_in;
		updateIfChanged(pDinRdb,"%d",d_in,last_d_in);
	}

private:
	RdbVar *pDinRdb;
	uint last_d_in;

	// This is called when the RDB is changed
	Thunk(DigIo,onRdbDioInvert)
	{
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		int invert=atoi(val);
		dio_invert=invert;
	}

	// This is called when the RDB is changed
	Thunk(DigIo,onRdbDout)
	{
		int out=atoi(val);
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		if (!outputValue(out)) {
			DBG(LOG_DEBUG,"failed DoutIo::outputValue %s %s", name.c_str(), val );
		}
		d_out = out;
	}
};

/*
 * This class provides extra functionality for GPIO IO
 */
class GpioIo : public DigIo {
public:
	GpioIo(const string & name, const string & label, uint capabilities, uint _mode, int _gpio, bool invert, int val):
		DigIo(name,label,capabilities,_mode,invert,val),
		gpio(_gpio)
	{
		DBG(LOG_DEBUG,"%s mode %s %d %d %d", label.c_str(), capabilitiesToString(_mode).c_str(), _gpio, invert ? 1: 0, val );
		setMode(_mode);
	}

private:
	virtual bool changeMode(uint new_mode)
	{
		DBG(LOG_DEBUG,"");
		if (!setMode(new_mode) ) {
			return false;
		}
		closeRdb();	// unsubscribes everything
		mode = new_mode;
		registerWith(*pFS); // Now register and subscribe for new mode
		return true;
	}

	bool setMode(uint _mode)
	{
		DBG(LOG_DEBUG,"");
		return gpio.setMode(_mode);
	}

	virtual bool outputValue(int val)
	{
		DBG(LOG_DEBUG,"for %s - %d", name.c_str(), val );
		gpio.writeValue(val);
		return true;
	}
	virtual uint readValue()
	{
//		DBG(LOG_ERR,"GpioDinIo::readValue for %s - gpio %d", name.c_str(), gpio );
		return gpio.readValue();
	}
	Gpio	gpio;
};

/*
 * This class is for analog inputs. All the analog inputs come from IIO devices
 */
class AinIo : public DigIo {
public:
	IIoChannel *pIIoChan;
	AinIo(const string &name, const string &label, IIoChannel *pIIoChan, Scaler _scaler, uint capabilities = IO_INFO_CAP_MASK_AIN | IO_INFO_CAP_MASK_VDIN, uint mode = IO_INFO_CAP_MASK_AIN);
	virtual ~AinIo();
	virtual void closeRdb()
	{
		delete pHwRdb;
		pHwRdb = 0;
		delete pRawRdb;
		pRawRdb = 0;
		delete pAdcRdb;
		pAdcRdb = 0;
		DigIo::closeRdb();
	}

	virtual void registerWith(FileSelector &fs)
	{
		char buf[32];
		DigIo::registerWith(fs);
		if (mode & analogOutputMode) {
			pRdbSession->addSubscriber( RdbVar(*pRdbSession, rdbPrefix,".dac","0"), onRdbAout, this );
			pHwRdb = 0;
			pRawRdb = 0;
			pAdcRdb = 0;
		}
		else {
			pHwRdb = new RdbVar(*pRdbSession, rdbPrefix,".adc_hw","0");
			pRawRdb = new RdbVar(*pRdbSession, rdbPrefix,".adc_raw","0");
			pAdcRdb = new RdbVar(*pRdbSession, rdbPrefix,".adc","0");

			pRdbSession->addSubscriber( RdbVar(*pRdbSession, rdbPrefix,".d_in_threshold", "0",true), onRdbDinThreshold, this );
		}
		snprintf( buf, sizeof(buf), "%f",scaler.getScale() );
		RdbVar scaleVar(*pRdbSession, rdbPrefix,".scale");
		scaleVar.setVal(buf);
		pRdbSession->addSubscriber( scaleVar, onRdbScale, this );

		pRdbSession->addSubscriber( RdbVar(*pRdbSession, rdbPrefix,".hardware_gain", "1"), onRdbHardwareGain, this );

		snprintf( buf, sizeof(buf), "%f",scaler.getCorrection() );
		RdbVar correctionVar(*pRdbSession, rdbPrefix,".correction", buf ,true);
		correctionVar.setVal(buf);
		pRdbSession->addSubscriber(correctionVar, onRdbCorrection, this);

		pIIoChan->attach(this, mode);
		pIIoChan->pIIoDevice->registerWith(&fs);
		if (mode & analogInputMode) {
			if ( pIIoChan->capabilities & IO_INFO_CAP_MASK_BUFFERED ) {
				buffered = true;
				pIIoChan->pIIoDevice->startBuffering();
			}
		}
	}
private:
	RdbVar *pHwRdb;
	RdbVar *pRawRdb;
	RdbVar *pAdcRdb;
	Scaler scaler;
	double last_adc;
	double last_adc_raw;
	uint last_adc_hw;
	double d_in_threshold;
	int hardware_gain;
	uint down_sampling_cntr;
	Iir_lpfilter lpf;

	virtual bool changeMode(uint new_mode)
	{
		return false;
	}

	virtual void pushSamples(uint *samples, int cnt)
	{
		if ( isOutputMode(mode)) {		// This only happens with data from the HFI
			outputValue(samples[cnt-1]);	// Just output the last value
			return;
		}

		// When configure as a VDIN don't HFI output here
		// This will happen in the Dio class
		if ((mode != IO_INFO_CAP_MASK_VDIN ) && HfiOutputs.size()) {
			double adc_raw;
			double convertedSamples[cnt];
			for (int i = 0; i < cnt; i++) {
				scaler.convert_adc_hw_to_value(samples[i], adc_raw, convertedSamples[i]);
			}
			for ( uint i = 0; i < HfiOutputs.size(); i++ ) {
				Hfi *pHfiOutput = (Hfi *)HfiOutputs[i];
				if (!pHfiOutput->pushSamples(convertedSamples,cnt, name.c_str())) {
					HfiOutputs.erase(HfiOutputs.begin()+i);
					delete pHfiOutput;
				}
			}
		}
		uint down_sampling_divider = 1;
		if (rdbUpdateFrequency) down_sampling_divider = samplingFrequency/rdbUpdateFrequency;

		for(int i = 0; i < cnt; i++) {
			/* apply low-pass filter - frequencies from 0% to 0.25% of the sampling frequency are allowed */
			int64_t v = lpf.filter(samples[i]);
			if ( v < 0 ) v  = 0;
			if(down_sampling_cntr++ == 0)
				updateRdbs(v);

			if ( down_sampling_cntr >= down_sampling_divider )
				down_sampling_cntr = 0;
		}
	}

	// This routine is the path for HFI inputs
	virtual void pushSamples(double *samples, int cnt)	// Samples are written to the RDB
	{
		uint isamples[cnt];
		for(int i = 0; i < cnt; i++) {
			isamples[i] = scaler.convert_value_to_adc_hw(samples[i]);
//			DBG(LOG_DEBUG,"scale float %.3f %d", samples[i], isamples[i]);
		}
		pushSamples(isamples, cnt);
	}

	void updateRdbs(uint adc_hw)
	{
		double adc;
		double adc_raw;

		updateIfChanged(pHwRdb,"%u",adc_hw,last_adc_hw);

		scaler.convert_adc_hw_to_value(adc_hw, adc_raw, adc);
		/* digital input by virtual digital input */
		if( mode == IO_INFO_CAP_MASK_VDIN ) {
			uint d_in = adc > d_in_threshold;
//			DBG(LOG_DEBUG,"vdin %f %f -> %d'", adc, d_in_threshold, d_in);
			DigIo::pushSamples(&d_in,1);
		}

		updateIfChanged(pRawRdb,"%f",adc_raw,last_adc_raw);
		updateIfChanged(pAdcRdb,"%0.2f",adc,last_adc);

		uint powerCapabilities = capabilities & IO_MGR_POWERSOURCE_MASK;
		if ( powerCapabilities == 0) {
			return;
		}
		static uint powerSource = 0;
		uint newPowerSource = powerSource;

		if ( adc > 5 ) {
			newPowerSource |= powerCapabilities;
		}
		else {
			newPowerSource &= ~powerCapabilities;
		}

		// Update the voltage value if we're the active source or no other has claimed it
		if ( (0 == newPowerSource) || (newPowerSource == powerCapabilities) ) {
			inputVoltage = adc;
		}

		if (powerSource != newPowerSource) {
			string powerString = powerToString(newPowerSource);
			// This is infrequent enough that creating the RDB variable each time is not significant
			RdbVar(ioMgrRdbSession, "sys.sensors.info.powersource").setVal(powerString);
			DBG(LOG_INFO,"power source changed to'%s'",powerString.c_str());
			powerSource = newPowerSource;
		}
	}

	// This is called when the RDB is changed
	Thunk(AinIo,onRdbDinThreshold)
	{
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		char* valptr;
		double  _d_in_threshold=strtod(val,&valptr);
		if(valptr==val) {
			DBG(LOG_DEBUG,"invalid d_in_threshold detected - keep current %f",d_in_threshold);
			return;
		}

		/* store d_in_threshold */
		d_in_threshold = _d_in_threshold;
		DBG(LOG_DEBUG,"new d_in_threshold set - %f", _d_in_threshold);

		#warning [TODO] immediately update virtual digital output status based on the new threshold instead of waiting for a new value
	}

	// This is called when the RDB is changed
	Thunk(AinIo,onRdbScale)
	{
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		char* valptr;
		double scale = strtod(val,&valptr);
		if(valptr==val) {
			DBG(LOG_DEBUG,"invalid scale detected - keep current %f", scaler.getScale());
			return;
		}
		scaler.setScale(scale);
		DBG(LOG_DEBUG,"new scale detected set - %f", scale);
	}

	// This is called when the RDB is changed
	Thunk(AinIo,onRdbHardwareGain)
	{
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		char* valptr;
		int _hardware_gain=strtod(val,&valptr);
		if(valptr==val) {
			DBG(LOG_DEBUG,"invalid hardware_gain detected - keep current %d",hardware_gain);
			return;
		}

		hardware_gain=_hardware_gain;
		#warning [TODO]  hardware_gain
#if 0 //ndef V_PROCESSOR_am335
		/*
		 * There are no "hardwaregain" entries in TI ADC driver (kernel v3.2.0), so this should not be applied on TI AM335.
		 * Moreover, after suspend/resume buffer in set_iio_ch_config_integer, the daemon won't receive ADC data from driver.
		 * Hence exclude V_PROCESSOR_am335 from this code.
		 * Of course, this can be different if TI AM335 ADC driver is upgraded/changed.
		 */
		if( set_iio_ch_config_integer(pidx,"hardwaregain",hardware_gain)>=0) {
			/* switch hardware gain */
			DBG(LOG_DEBUG,"switch hardware gain of channel (%s) to %d",io->io_name,hardware_gain);
		}
#endif
		DBG(LOG_DEBUG,"new hardware_gain set - %d",hardware_gain);
	}

	// This is called when the RDB is changed
	Thunk(AinIo,onRdbCorrection)
	{
		DBG(LOG_DEBUG,"Io %s %s", name.c_str(), val );
		char* valptr;
		double correction=strtod(val,&valptr);
		if(valptr==val) {
			DBG(LOG_DEBUG,"invalid correction detected - keep current %f", scaler.getCorrection() );
			return;
		}
		scaler.setCorrection(correction);
		DBG(LOG_DEBUG,"new correction set - %f", correction);
	}

	// This is called when the RDB is changed
	Thunk(AinIo,onRdbAout)
	{
		DBG(LOG_DEBUG,"onRdbAout %s %s", name.c_str(), val );
		int out=atoi(val);
		if (!outputValue(out)) {
			DBG(LOG_DEBUG,"failed AinIo::outputValue %s %s", name.c_str(), val );
		}
	}
};

AinIo::AinIo(const string &name, const string &label, IIoChannel *_pIioChan, Scaler _scaler, uint capabilities, uint mode):
	DigIo(name, label, capabilities, mode, false, 0 ),
	pIIoChan(_pIioChan),
	scaler(_scaler),
	last_adc(0),
	last_adc_raw(0),
	last_adc_hw(0),
	d_in_threshold(0),
	down_sampling_cntr(0)
{
	DBG(LOG_DEBUG,"%s %s", name.c_str(), capabilitiesToString(mode).c_str() );
}

AinIo::~AinIo()
{
	DBG(LOG_DEBUG,"~AinIo()");
	if (mode & analogOutputMode) {
		return;
	}
	pIIoChan->pIIoDevice->stopBuffering();
	pIIoChan->detach();
	delete pAdcRdb;
	delete pRawRdb;
	delete pHwRdb;
}

#ifdef V_MODCOMMS
static string modcommsExcludeList;

// Generate a label for the IO that will be displayed on the WebUI
// orgLabel is read from /sys and is returned if things go wrong
// pMio is a point to either "MIO" or "DIO"
static string genLabel(const string &orgLabel, const char *pMio )
{
	// orgLabel is of form io-mice.2-3
	// but for branding we want io-mice.1-MIO3
	// Note also the name from /sys file has a slot number
	// We need to convert this slot into a logical board number using RDB variable

	// Get a local copy for working with
	int len = orgLabel.size();
	char buf[len+1];
	memcpy(buf,orgLabel.c_str(),len);
	buf[len]=0;

	char *pDot = strchr(buf,'.');
	if (pDot) {
		pDot++;
		char *pDash = strchr(pDot,'-');
		if (pDash) {
			*pDash++ = 0;	// We now have the string "2" from above
			RdbVar labelRdb(ioMgrRdbSession,string("modcomms.slot.")+pDot,".iolabel");
			string label = labelRdb.getVal();
			if ( label.size() ) {
				return label+"-"+pMio+pDash;
			}
		}
	}
	return orgLabel;
}

/*
 * This class is for the Modcomms IO
 * most of the functionality is in the inherited classes
 * The mode change is the main new functionality
 */
class ModCommsIo: public AinIo {
public:
	ModCommsIo(const string &name, const string &label, IIoChannel *_pIioChan, Scaler _scaler, uint capabilities, uint mode):
		AinIo( name,label, _pIioChan, _scaler, capabilities, mode )
	{
		DBG(LOG_DEBUG,"%s %s", name.c_str(), capabilitiesToString(mode).c_str() );
	}
private:
	virtual bool changeMode(uint new_mode)
	{
		DBG(LOG_DEBUG,"changeMode");
		pIIoChan->detach();
		if ( !pIIoChan->attach(this,new_mode) ) {
			IIoChannel *pNewIIoChan = pIIoChan->pIIoDevice->findChannel(new_mode);
			if (!pNewIIoChan) {
				DBG(LOG_DEBUG,"Failed to find new channel");
				pIIoChan->attach(this,mode);
				return false;
			}
			pIIoChan = pNewIIoChan;
		}
		closeRdb();
		mode = new_mode;
		pIIoChan->attach(this,mode);
		registerWith(*pFS);
		return true;
	}

	virtual bool outputValue(int val)
	{
		DBG(LOG_DEBUG,"outputValue %s", name.c_str() );
		return pIIoChan->outputValue(val);
	}
};

/*
 * Because we built hardware that is not consistent
 * Some Channels have special properties like power sources
 * This routine attempts to take care of the specifics.
 */
static void customizeMio(const string &label, IIoChannel *pCh,
	Scaler &scaler, uint &capabilities)
{
	if ((label.find("gps-mice") != string::npos) || (label.find("gps-can-mice") != string::npos)) {
		// There's only one channel for the gps-mice for VIN
		scaler.setScale(RESISTOR_DIVIDER(300,20));
		capabilities |= IO_MGR_POWERSOURCE_NMA1500;
	}
	else if ((label.find("aeris-mice") != string::npos) && (label.find("MIO1") != string::npos)) {
		// The current input needs to be scale
		scaler.setScale(26.1);
	}
}

static int auxCnt = 0;
static string ignitionSrc;

static string nextXauxName()
{
	char buf[16];
	snprintf(buf, sizeof(buf), "xaux%d", ++auxCnt);
	return buf;
}

static bool excludedDevice(const string & name)
{
	bool excluded = modcommsExcludeList.find(name) != std::string::npos;
	DBG(LOG_DEBUG,"Modcomms device %s - %s",name.c_str(), excluded?"excluded":"" );
	return excluded;
}

/*
 * This is called from IIO.CPP as it enumerates its IIO devices and finds Modcomms devices
 * Determine the initial mode based on Modcomms RDB variable
 * and then create the Io
 */
void iioCreate(IIoDevice *pIIoDev)
{
	string label = genLabel(pIIoDev->nameFromNameFile,"MIO");
	bool excluded = modcommsExcludeList.find(label) != std::string::npos;
	if (excluded ) {
		return;
	}
	// ModComms have their own mode variable based on label as the Xaux name can vary
	RdbVar ioModeVar(ioMgrRdbSession,"sys.sensors.io."+label,".mode");
	string ioMode = ioModeVar.getVal();
	DBG(LOG_DEBUG,"Modcomms device %s  Rdb(%s)  mode - %s",
		pIIoDev->deviceName.c_str(),ioModeVar.getName(), ioMode.c_str() );
	int mode = stringToCapabilities(ioMode.c_str());
	if ( mode == 0 ) // No mode specified, select one from the capabilities
	{
		uint capabilities = pIIoDev->capabilities;
		for ( uint mask = 1; true ; mask <<= 1 ) {
			if ( mask == IO_INFO_CAP_MASK_BUFFERED ) {
				break;
			}
			if ( mask &  capabilities) {
				mode = mask;
				break;
			}
		}
	}
	if (mode) {
		for (std::vector<void*>::const_iterator it = pIIoDev->iioChannels.begin(); it != pIIoDev->iioChannels.end(); ++it) {
			IIoChannel * pCh = (IIoChannel *)*it;
			if ( pCh->capabilities & mode) {
				DBG(LOG_DEBUG,"Modcomms channel %s %s supports modes - %s", pCh->name.c_str(),ioMode.c_str(),
					capabilitiesToString(pCh->pIIoDevice->capabilities).c_str() );
				uint capabilities = pCh->pIIoDevice->capabilities;
				Scaler scaler(RESISTOR_DIVIDER(300,36.5),1,0.001);
				customizeMio(label, pCh,scaler, capabilities);
				new ModCommsIo(nextXauxName(), label, pCh, scaler, capabilities, mode);
				break;
			}
		}
	}
	else {
		DBG(LOG_DEBUG,"Modcomms invalid mode - %s",ioMode.c_str() );
	}
}

/*
 * Because we built hardware that is not consistent
 * Some GPIOs are not bidirectional and some need to be set to certain default values.
 * This routine attempts to take care of the specifics
 */
static void customizeGpio(const string &label, int ioNumber,
	string &name, uint &ioMode, uint &capabilities, int &defaultOutput, bool &invert )
{
	if (label.find("rf-mice") != string::npos) {
		DBG(LOG_DEBUG,"customizeGpio rf-mice");
		switch (ioNumber) {
		case 0:	// This is an output only
		case 1:	// This is an output only
		case 2:	// This is an output only
			ioMode = IO_INFO_CAP_MASK_DOUT;
			capabilities = IO_INFO_CAP_MASK_DOUT;
			defaultOutput = 1;
			break;
		case 3:	// This is an output only
			ioMode = IO_INFO_CAP_MASK_DOUT;
			capabilities = IO_INFO_CAP_MASK_DOUT;
			defaultOutput = 0;
			break;
		case 4:	// This is an input only
		case 5:	// This is an input only
			ioMode = IO_INFO_CAP_MASK_DIN;
			capabilities = IO_INFO_CAP_MASK_DIN;
			break;
		}
	}
	else if ((label.find("gps-mice") != string::npos) ||(label.find("gps-can-mice") != string::npos)) {
		DBG(LOG_DEBUG,"customizeGpio gps-mice");
		// We want GPS mice Io to not appear on the Gui. So give it a name so it doesn't get an XAUX name
		name = label;
		switch (ioNumber) {
		case 0:	// This is an output only
			ioMode = IO_INFO_CAP_MASK_DOUT;
			capabilities = IO_INFO_CAP_MASK_DOUT;
			defaultOutput = 1;
			break;
		case 1:	// This is an output only
			ioMode = IO_INFO_CAP_MASK_DOUT;
			capabilities = IO_INFO_CAP_MASK_DOUT;
			defaultOutput = 0;
			break;
		case 3:	// This is an input only and possible ignition
			if (ignitionSrc.compare("ignNMA1500") == 0) {
				name = "ign";
			}
			// Fall through for rest of inputs
		case 2:	// This is an input only
		case 4:	// This is an input only
			ioMode = IO_INFO_CAP_MASK_DIN;
			capabilities = IO_INFO_CAP_MASK_DIN;
			break;
		}
	}
}

/*
 * This is called from XGPIO.CPP as it enumerates its GPIO devices and finds Modcomms devices
 * Determine the initial mode based on Modcomms RDB variable
 * and then create the Io
 */
void gpioCreate(const string &name, int baseGpio, int cnt )
{
	for ( int i = 0; i< cnt; i++, baseGpio++ ) {
		char buf[16];
		snprintf( buf, sizeof(buf), "-%d", i+1 );
		string label = genLabel(name + buf, "DIO");
		if (!excludedDevice(label)) {
			string ioModeStr = RdbVar(ioMgrRdbSession,"sys.sensors.io."+label,".mode").getVal();
			DBG(LOG_DEBUG,"Modcomms GPIO device %s - %s", label.c_str(), ioModeStr.c_str() );
			uint ioMode = stringToCapabilities(ioModeStr.c_str());
			if (ioMode == 0) {
				ioMode = IO_INFO_CAP_MASK_DIN;
			}
			uint capabilities = IO_INFO_CAP_MASK_DIN|IO_INFO_CAP_MASK_DOUT;
			int defaultOutput = 0;
			bool invert = false;
			string rdbName;
			customizeGpio(label, i, rdbName, ioMode, capabilities, defaultOutput, invert );
			if (rdbName.size() == 0)
				rdbName = nextXauxName();
			// We do not need to keep track of the pointers as the constructor adds them to our container
			new GpioIo(rdbName, label, capabilities, ioMode, baseGpio, invert, defaultOutput);
		}
	}
}
#endif

class HfiTcpServer : public TcpServer {
public:
	HfiTcpServer(FileSelector *pFS);
	virtual ~HfiTcpServer(){/*DBG(LOG_DEBUG,"~");*/}
private:
	MsTick sessionStart;
	virtual void onConnected();
	virtual void onDisconnected();
	virtual void OnDataRead(char *buf,int len);
};

HfiTcpServer::HfiTcpServer(FileSelector *pFS_):
TcpServer(pFS_)
{
}

void HfiTcpServer::onConnected()
{
//	DBG(LOG_DEBUG," ???");
	sessionStart = getNowMs();
}

static Io *getIoByName(const char * pName)
{
	for (std::vector<void*>::const_iterator it = io_array.begin(); it != io_array.end(); ++it) {
		Io * pIo = (Io *)*it;
		if ((pIo->name.compare(pName) == 0) || (pIo->label.compare(pName) == 0)) {
			return pIo;
		}
	}
	return 0;
}

// This routine is called when data is received from the network
// There are three commands processed
//	+ioName		add Io to list of those output
//	-ioName		remove Io from list of those output
//	ioName,ain,...	the legacy command to feed analogs into the IoMgr
void HfiTcpServer::OnDataRead(char *buf,int len)
{
	char *pCr = strchr(buf,'\n');
	if (pCr) {
		*pCr = 0;
	}

//	DBG(LOG_INFO,"received %s", buf);
	if (buf[0] == '+') {
		Io * pIo = getIoByName(buf+1);
		if (pIo) {
//			DBG(LOG_INFO,"add %s", buf+1);
			// Digital inputs are slow compared to analog so just one column for them
			int columns = (digitalInputMode&pIo->mode) ? 1: 10;
			Hfi * pHfi = new Hfi(getFd(), columns, sessionStart);
			pIo->addHfiOutput(pHfi);
		}
		return;
	}
	else if (buf[0] == '-') {
//		DBG(LOG_INFO,"remove %s", buf+1);
		Io * pIo = getIoByName(buf+1);
		if (pIo) {
			pIo->removeHfiOutput(getFd());
		}
		return;
	}

	const char * pName;
	const char * pData;
	if (!Hfi::parseHeader(buf, pName, pData)) {
		return;
	}

	Io * pIo = getIoByName(pName);
	if (pIo) {
		int numSamples = 20;
		if ((digitalInputMode|digitalOutputMode)&pIo->mode) {
			uint samples[numSamples];
			if (!Hfi::parseBuffer(pData, samples, numSamples)) {
				return;
			}
			pIo->pushSamples(samples, numSamples);
		}
		else {
			double samples[numSamples];
			if (!Hfi::parseBuffer(pData, samples, numSamples)) {
				return;
			}
			pIo->pushSamples(samples, numSamples);
		}
	}
	else {
		DBG(LOG_INFO," Io %s not found", pName);
	}
}

void HfiTcpServer::onDisconnected()
{
//	DBG(LOG_DEBUG," ???");
	// Remove ourselves from the IO outputter
	for (std::vector<void*>::const_iterator it = io_array.begin(); it != io_array.end(); ++it) {
		Io * pIo = (Io *)*it;
		pIo->removeHfiOutput(getFd());
	}
	delete this;
}

class HfiTcpListener: public TcpListener {
public:
	HfiTcpListener(){}
	virtual ~HfiTcpListener() {/*DBG(LOG_DEBUG,"~");*/}
private:
	void onConnect()
	{
//		DBG(LOG_DEBUG,"");
		HfiTcpServer *pHfiTcpServer = new HfiTcpServer(getFS());
		pHfiTcpServer->onConnect(getSock());
	}
};

static HfiTcpListener * pHfiTcpListener;

// This is called when the RDB is changed
static void onRdbHfiAddress(void *param, const char *val)
{
	delete pHfiTcpListener;
	pHfiTcpListener = 0;

	DBG(LOG_DEBUG,"address %s", val);
	/* bypass if no ip address is specified */
	int len = strlen(val);
	if(len == 0) {
		DBG(LOG_DEBUG,"ip address not specified (server='')");
		return;
	}

	char buf[len+1]; // local copy so we can change things
	strcpy(buf,val);
	int port = 30000;
	char *pColon = strchr(buf,':');
	if (pColon) {
		*pColon++ = 0;
		port = atoi(pColon);
		if (port == 0) {
			return;
		}
	}

	if(!HfiTcpListener::isValidAddress(buf)) {
		DBG(LOG_ERR,"incorrect ip address specified (server='',val='%s')", val);
		return;
	}
	pHfiTcpListener = new HfiTcpListener();
	DBG(LOG_DEBUG,"listen %s:%d", buf,port);
	pHfiTcpListener->listen(buf,port,pIoMgrFileSelector);
}

static bool rescanIo = false;

void sigHandler(int signo)
{
	DBG(LOG_DEBUG,"signal %d received",signo);
	if(signo==SIGUSR2) {
		rescanIo = true;
	}
	else if(signo==SIGTERM || signo==SIGINT) {
		running=false;
	}
}


// Periodic timer
void TimerFd::onTimeOut()
{
	for (std::vector<void*>::const_iterator it = io_array.begin(); it != io_array.end(); ++it) {
		Io * pIo = (Io *)*it;
		pIo->poll();
	}

	char buf[16];	// We're going to store a float to 2 decimal places xx.xx
	snprintf(buf, sizeof(buf), "%2.2f", inputVoltage);
	RdbVar(ioMgrRdbSession, "sys.sensors.info.DC_voltage").setVal(buf);
}

static void initIo(FileSelector &fileSelector)
{
	iio_create();
	iio_changeSamplingFreq(samplingFrequency);

	// We do not need to keep track of the pointers of the Io as the constructor adds them to our container
#if defined(V_IOMGR_ioext1)
	new AinIo("vin", "", iio_findChannel("voltage1"), Scaler(RESISTOR_DIVIDER(300,30), 1 ), IO_MGR_POWERSOURCE_DCJACK );
	if (ignitionSrc.compare("ignNMA1500") != 0) {
		new GpioIo("ign", "", IO_INFO_CAP_MASK_DIN, IO_INFO_CAP_MASK_DIN, GPIO_BANK_PIN(2,16), true, 0);
	}
#elif defined(V_IOMGR_ioext4)
	new AinIo("vin", "", iio_findChannel("voltage1"), Scaler(RESISTOR_DIVIDER(300,20), 1.01180438449 /* DCjack+POE */ ), IO_MGR_POWERSOURCE_DCJACK );
	if (ignitionSrc.compare("ignNMA1500") != 0) {
		new GpioIo("ign", "", IO_INFO_CAP_MASK_DIN, IO_INFO_CAP_MASK_DIN, GPIO_BANK_PIN(2,9), true, 0);
	}
	// TODO -add the other IO for this family  (the 6200 has PoE and Xaux 1 to 3). We don't need this yet for Aeris - needs a new Xaux Io class for this
#endif

#ifdef V_MODCOMMS
	modcommsExcludeList = RdbVar(ioMgrRdbSession,"modcomms.iomgr.exclude").getVal();
	DBG(LOG_DEBUG,"ModComms exclude %s",modcommsExcludeList.c_str());
	gpio_addModcomms();
	iio_addModcomms();
#endif
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", auxCnt);
	// This Rdb variable says how many Ios are managed. ( Used by WebUI Io page)
	RdbVar(ioMgrRdbSession, "sys.sensors.info.lastio").setVal(buf);

	DBG(LOG_DEBUG,"main: registering Ios");
	for (std::vector<void*>::const_iterator it = io_array.begin(); it != io_array.end(); ++it) {
		Io * pIo = (Io *)*it;
		pIo->registerWith(fileSelector);
	}
}

static void destroyIo()
{
	DBG(LOG_DEBUG,"main:iio_delete Io");
	for (std::vector<void*>::const_reverse_iterator it = io_array.rbegin(); it != io_array.rend(); ++it) {
		Io * pIo = (Io *)*it;
		delete pIo;
	}
	io_array.clear();
	auxCnt = 0;
	DBG(LOG_DEBUG,"main:iio_destroy");
	iio_destroy();
}

int main(int argc,char *argv[])
{
	DBG(LOG_INFO,"started");
		/* start rdb */
	if(!ioMgrRdbSession.init() ) {
		DBG(LOG_ERR,"failed to init. rdb");
		return -1;
	}

	/* override signals */
	signal(SIGINT,sigHandler);
	signal(SIGTERM,sigHandler);
	signal(SIGPIPE,sigHandler);
	signal(SIGUSR2,sigHandler);

	FileSelector fileSelector(1000);
	pIoMgrFileSelector = &fileSelector;
	ioMgrRdbSession.addSubscriber( RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.mgr.enable"),onRdbDaemonEnable, NULL);
	RdbVar rdbDaemonWatchdog(ioMgrRdbSession, "sys.sensors.iocfg.mgr.watchdog");
	ioMgrRdbSession.addSubscriber(rdbDaemonWatchdog,onRdbDaemonWatchdog, &rdbDaemonWatchdog);
	ioMgrRdbSession.addSubscriber(RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.sampling_freq"),onRdbSamplingFreq, NULL);
	ioMgrRdbSession.addSubscriber(RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.rdb_sampling_freq"), onRdbRdbSamplingFreq, NULL);
	ioMgrRdbSession.addSubscriber(RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.mgr.debug"), onRdbDbgLevel, NULL);

	ignitionSrc = RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.ignition").getVal();
	DBG(LOG_DEBUG,"ignition source %s",ignitionSrc.c_str());

	TimerFd timer(1000,fileSelector);
	ioMgrRdbSession.registerWith(fileSelector);

	ioMgrRdbSession.addSubscriber(RdbVar(ioMgrRdbSession, "sys.sensors.iocfg.hfi.ipaddr", "127.0.0.1:30000"), onRdbHfiAddress, NULL);

	initIo(fileSelector);

	RdbVar rdbDaemonReady(ioMgrRdbSession, "sys.sensors.iocfg.mgr.ready");
	rdbDaemonReady.setVal("1");
	while(running) {
		fileSelector.processReadyFiles();
		if (rescanIo) {
			rescanIo = false;
			destroyIo();
			// There is a lot of udev activity
			// as a Modcomms board is initialized
			// So let's wait till things settle
			sleep(15);
			initIo(fileSelector);
		}
	}
	rdbDaemonReady.setVal("0");

	destroyIo();

	Gpio::close();
	ioMgrRdbSession.unregister();
	delete pHfiTcpListener;

	pIoMgrFileSelector = 0;

	DBG(LOG_INFO,"stopped");
	return 0;
}
