/*
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
 *  High frequency interface (HFI) interface
 */

#include "HFI.h"
#include "io_mgr.h"
#include "timer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// The constructor takes the
// file descriptor and the number
// of samples to put on each line sent
// startTime_ gives the time that the client connected as each output has a
// a session timestamp
Hfi::Hfi(int fd_, uint columns_, MsTick startTime_):
	fd(fd_),
	columns(columns_),
	sessionStart(startTime_)
{
	samples.reserve(columns);
	samplePeriod = 1000/samplingFrequency;
//	DBG(LOG_DEBUG," %d %d", samplingFrequency, samplePeriod);
}

// This routine is called by the Io class when Io data is received.
// It accumulates the samples until we have enough to send
// All the data given is processed - this may include push it onto the network
// and/or saving it in our buffer
bool Hfi::pushSamples(double *samples_, int cnt, const char *ioName)
{
	HfiData hfiData[cnt];
	isDataInt = false;
	for(int i = 0; i < cnt; i++) { // Store in our local buffer
		hfiData[i].doubleVal = samples_[i];
	}
	return pushSamples(hfiData, cnt, ioName);
}

bool Hfi::pushSamples(uint *samples_, int cnt, const char *ioName)
{
	HfiData hfiData[cnt];
	isDataInt = true;
	for(int i = 0; i < cnt; i++) { // Store in our local buffer
		hfiData[i].uintVal = samples_[i];
	}
	return pushSamples(hfiData, cnt, ioName);
}

bool Hfi::pushSamples(HfiData *samples_, int cnt, const char *ioName)
{
	MsTick sessionTime = getNowMs() - sessionStart;
	MsTick samplesSpan = samplePeriod*(cnt-1);	// These next few lines
	MsTick sampleTime = 0;				// are to determine time
	if ( samplesSpan <  sessionTime )		// of the first sample
		sampleTime = sessionTime - samplesSpan;

//	DBG(LOG_DEBUG," pushSamples - %llu: %d %d", sampleTime, samplePeriod, cnt);

	int startOffset = 0;
	while (cnt > 0) {
		int space = columns - samples.size();
//		DBG(LOG_DEBUG," space %d", space);
		if (space <= 0) {	// If buffer is full send it
//			DBG(LOG_DEBUG," flush %d", space);
			if (!formatAndOutput(ioName)) {
				return false;
			}
			samples.clear();
			continue;
		}
		int copied = space;	// How much to copy
		if (copied > cnt) {
			copied = cnt;
		}
//		DBG(LOG_DEBUG," copied %d", copied);
		for(int i = 0; i < copied; i++) { // Store in our local buffer
			if (samples.size() == 0) {
//				DBG(LOG_DEBUG," bufferStart - %llu: %d", sampleTime, i);
				bufferStart = sampleTime;
			}
			samples.push_back(samples_[startOffset + i]);
			sampleTime += samplePeriod;
		}
		startOffset += copied;
		cnt -= copied;
	}
	return true;
}

// The HFI format is
// <IO name>, <IO type : ain>, <msec time-stamp since beginning of session>: <value>, <value>, ... (minimum 1 entry and maximum 10 entries)
//
//example streams)
//vin,ain,150:12.03,16.16,0.06,0.13,0.12,12.02,16.17,0.06,0.15,0.12,12.03,16.17,0.06,0.15,0.12,12.02

// This returns false if there was a problem writing
bool Hfi::formatAndOutput(const char *ioName)
{
	char line[256];
	/* print prefix */
	int len = snprintf(line, sizeof(line), "%s,%s,%llu:", ioName, isDataInt ? "dio":"aio", bufferStart);

	/* print pay-load */
	int count = samples.size();
	for(int i=0; i < count; i++) {
		if (isDataInt) {
			len += snprintf(line+len, sizeof(line)-len, "%d,", samples[i].uintVal);
		}
		else {
			len += snprintf(line+len, sizeof(line)-len, "%0.2f,", samples[i].doubleVal);
		}
	}
	if (len == sizeof(line)) {
		DBG(LOG_ERR, "line buffer is not large enough to format data");
		return true;
	}
	line[len-1] = '\n';	// Substitute that last , for a \n
//	DBG(LOG_DEBUG," HFI line - %s", line );

	if ( write(fd,line,len) < 0) {
//		DBG(LOG_DEBUG," write fail ???");
		return false;
	}
	return true;
}

// This routine performs the reverse of the format routine above
// parameters are
// buf - the input buffer to be parsed
// samples - an array of floats to save the parsed data
// numSamples - on entry this has the size of samples.
//   On exit it has the number of floats parsed (if function returns true)
// pname id set to the pointer of the Io name

bool Hfi::parseHeader(char *buf, const char * &pName, const char * &pData)
{
	char *pComma = strchr(buf,',');
	if (!pComma) {
//		DBG(LOG_DEBUG,"No ',' <%s>",buf);
		return false;
	}
	*pComma++ = 0;
	char *pComma2 = strchr(pComma,',');
	if (!pComma2) {
//		DBG(LOG_DEBUG,"No ',,' <%s>",pComma);
		return false;
	}
	*pComma2++ = 0;
	char *pColon = strchr(pComma2,':');
	if (!pColon) {
//		DBG(LOG_DEBUG,"No ':' <%s>",buf);
		return false;
	}
	*pColon = 0;

	// Return the name of the IO and the start of the payload
	pName = buf;
	pData = pColon+1;

	return true;
}

bool Hfi::parseBuffer(const char *buf, double samples_[], int &numSamples)
{
	int maxSamples = numSamples;	// Get the size of buffer;
	int numParsed = 0;
	for (const char * pNext = buf; true;  ) {
		samples_[numParsed] = strtod(pNext, NULL);
//		DBG(LOG_DEBUG,"float %.3f %d", samples_[numParsed], numParsed);
		if ( ++numParsed >= maxSamples) {
			break;
		}
		pNext = strchr(pNext,',');
		if (!pNext) {
			break;
		}
		pNext++;	// Past the ',' read for next float
	}
	numSamples = numParsed;
	return numParsed > 0;
}

bool Hfi::parseBuffer(const char *buf, uint samples_[], int &numSamples)
{
	int maxSamples = numSamples;	// Get the size of buffer;
	int numParsed = 0;
	for (const char * pNext = buf; true;  ) {
		samples_[numParsed] = strtoul(pNext, NULL,0);
//		DBG(LOG_DEBUG,"uint %d %d", samples_[numParsed], numParsed);
		if ( ++numParsed >= maxSamples) {
			break;
		}
		pNext = strchr(pNext,',');
		if (!pNext) {
			break;
		}
		pNext++;	// Past the ',' read for next float
	}
	numSamples = numParsed;
	return numParsed > 0;
}
