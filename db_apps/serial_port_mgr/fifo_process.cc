/**
 * @file fifo_process.cc
 * @brief Implement L3 procesing class
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

#include "serial_process.h"
#include "fifo_process.h"
#include "crc.h"
#include "utils.h"

#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

#define L3_IPC_HEADER_LEN   2
#define L2_HEADER_LEN       1
#define L2_CRC_LEN          2
#define L2_SDU_MAX_LEN      800

#define PACK_BUFFER_LEN (L2_HEADER_LEN + \
                        L2_SDU_MAX_LEN + \
                        L2_CRC_LEN)

FifoProcess::FifoProcess(FILE* fifo_out, FILE* fifo_in, unsigned char version, unsigned char service)
: m_fifo_out(fifo_out), m_fifo_in(fifo_in),
  m_header((service << SCOMM_DATALINK_PROTO_SERVICE_ID_POS) | version),
  m_bad(false)
{
    m_buffer = new unsigned char[PACK_BUFFER_LEN];
    if (!m_buffer) {
        m_bad = true;
        BLOG_ERR("no buffer was allocated fifo processing\n");
    }

#ifdef LOOP_TEST
    m_fifo_in = m_fifo_out;
#endif
}

FifoProcess::~FifoProcess()
{
    delete[] m_buffer;
}

void FifoProcess::ProcessData(FILE* serial)
{
    if (m_bad) {
        BLOG_INFO("could not process fifo data\n");
        return;
    }

    // read ipc message length
    unsigned short length;
    if (1 != fread(&length, L3_IPC_HEADER_LEN, 1, m_fifo_in)) {
        BLOG_NOTICE("could not read ipc length header\n");
        return;
    }
    length = ntohs(length);
    BLOG_DEBUG("fifo in data expected len %d\n", length);

    // check length
    if (length > L2_SDU_MAX_LEN) {
        m_bad = true;
        BLOG_ERR("ipc message is too long %u\n", (unsigned int)length);
        return;
    }

    // read ipc data
    if (1 != fread(m_buffer + L2_HEADER_LEN, length, 1, m_fifo_in)) {
        m_bad = true;
        BLOG_ERR("could not read ipc data\n");
        return;
    }

#ifdef CLI_DEBUG
    for(int z=0; z<(length); z++) {
        BLOG_DEBUG("%02X%s", m_buffer[z+L2_HEADER_LEN], ((z+1)%16 && z!=(length-1))?" ":"\n");
    }
#endif

    // add l2 header
    m_buffer[0] = m_header;

    // pad l2 crc in the end
    unsigned short crc = crc16_calculate(m_buffer, L2_HEADER_LEN + length);
    crc = htons(crc);
    memcpy(m_buffer + L2_HEADER_LEN + length, &crc, L2_CRC_LEN);

    // send data through serial port
    serial_send_data(serial, m_buffer, L2_HEADER_LEN + length + L2_CRC_LEN);
}

void FifoProcess::WriteData(unsigned char* data, unsigned short length)
{
    BLOG_DEBUG("write fifo data %d\n", length);
#ifdef CLI_DEBUG
    for(int z=0; z<length; z++) {
        BLOG_DEBUG("%02X%s", data[z], ((z+1)%16 && z!=(length-1))?" ":"\n");
    }
#endif

    unsigned short netlen = htons(length);
    if (1 != fwrite(&netlen, L3_IPC_HEADER_LEN, 1, m_fifo_out) ||
        1 != fwrite(data, length, 1, m_fifo_out)) {
        BLOG_ERR("failed to write to fifo\n");
        return;
    }

    (void)fflush(m_fifo_out);
}
