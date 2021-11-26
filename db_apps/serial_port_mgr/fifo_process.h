/**
 * @file fifo_process.h
 * @brief L3 procesing class interface
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
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

#ifndef FIFO_PROCESS_H_10253121082019
#define FIFO_PROCESS_H_10253121082019

/**
 * FIFO processing class
 *
 * Read and write data on FIFO connecting to layer3 services
 */
class FifoProcess {
public:
    /**
     * Constructor
     *
     * @param   fifo_out    File pointer to outgoing FIFO
     * @param   fifo_in     File pointer to incoming FIFO
     * @param   version     L2 version number
     * @param   service     L3 service identity number
     *
     */
    FifoProcess(FILE* fifo_out, FILE* fifo_in, unsigned char version, unsigned char service);

    /**
     * Destructor
     *
     */
    ~FifoProcess();

public:
    /**
     * Read data from FIFO, encode and send through serial port
     *
     * @param   serial      File pointer to serial port
     *
     */
    void ProcessData(FILE* serial);

    /**
     * Write data to FIFO
     *
     * @param   data        Pointer to data to be written
     * @param   len         Length of data to be written
     *
     */
    void WriteData(unsigned char* data, unsigned short len);

protected:

    FILE* m_fifo_out;           // FIFO out file
    FILE* m_fifo_in;            // FIFO in file
    unsigned char m_header;     // Layer 2 header
    unsigned char* m_buffer;    // Internal buffer for reading data from FIFO
    bool m_bad;                 // Flag to mark if an instannce is in order
};

#endif // FIFO_PROCESS_H_10253121082019
