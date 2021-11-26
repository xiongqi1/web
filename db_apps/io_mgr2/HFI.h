#ifndef __HFI_H__16062016
#define __HFI_H__16062016

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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  High frequency interface (HFI) interface
 */

#include "timer.h"

#include <sys/types.h>
#include <vector>

/*
 * This structure defines the different types of data that can by passed though the
 * HFI. The floating point is used for the analogs and the int for digital IO
 */
union HfiData {
    double doubleVal;
    uint uintVal;
};

/*
 * This High frequency interface class is responsible for receiving the Io data,
 * formatting it and sending it to the file descriptor
 */
class Hfi {
public:
    Hfi(int fd, uint columns, MsTick tick); // The constructor takes the
                                            // file descriptor and the number
                                            // of samples to put on each line sent
                                            // and the time of start
    int getFd() { return fd; }
    // The Io class calls the with the data it receives
    bool pushSamples(double *samples, int cnt, const char *ioName);
    bool pushSamples(uint *samples, int cnt, const char *ioName);
    // The IoMgr calls this to decode a HFI input from the network
    static bool parseBuffer(const char *buf, uint samples[], int &numSamples);
    static bool parseBuffer(const char *buf, double samples[], int &numSamples);
    static bool parseHeader(char *buf, const char * &pName, const char * &pData);
private:
    int fd;                             // file descriptor to output to
    uint columns;                       // number of columns per line
    std::vector<HfiData> samples;        // Our buffer for accumulating data
    MsTick sessionStart;
    MsTick bufferStart;                 // Time first entry put in our buffer
    uint samplePeriod;
    bool isDataInt;
    Hfi();  // Invalidate the default constructor
    bool formatAndOutput(const char *ioName);
    bool pushSamples(HfiData *samples, int cnt, const char *ioName);
};

#endif
