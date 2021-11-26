/**
 * @file fifo_process.cc
 * @brief Implement L1/L2 processing for serial port
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

#include "utils.h"
#include "crc.h"
#include "serial_process.h"

#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define MAX_SCOMM_FRAME_END     L1_FRAME_BASE_SIZE

#define SCOMM_L2_HEADER_SIZE    1
#define SCOMM_L2_CRC_SIZE       2
#define MAX_SCOMM_L2_SDU_SIZE   800
#define MAX_SCOMM_UNESCAPED_PACKET_SIZE (SCOMM_L2_HEADER_SIZE + \
                                        MAX_SCOMM_L2_SDU_SIZE + \
                                        SCOMM_L2_CRC_SIZE)

#define MAX_SCOMM_ESCAPED_SIZE MAX_SCOMM_UNESCAPED_PACKET_SIZE

#define MAX_SCOMM_PACKET_BUFFER_SIZE (MAX_SCOMM_UNESCAPED_PACKET_SIZE + \
                                    MAX_SCOMM_ESCAPED_SIZE + \
                                    MAX_SCOMM_FRAME_END)

#define SCOMM_FRAME_END         0xC0
#define SCOMM_FRAME_ESC         0xDB
#define SCOMM_FRAME_ESC_END     0xDC
#define SCOMM_FRAME_ESC_ESC     0xDD

#define SERVICE_ID_MANAGEMENT   0x00
#define SERVICE_ID_SOCKETS      0x01

static unsigned char packet_buffer[MAX_SCOMM_PACKET_BUFFER_SIZE];
static unsigned char send_buffer[MAX_SCOMM_PACKET_BUFFER_SIZE];
static int buffer_pos = 0;

static unsigned char version = 0;
static unsigned char service = 0;
static unsigned short packet_len = 0;

/**
 * Remove escapes and frame end from buffer
 *
 * @param sfram Start of frame
 * @param eframe End of frame
 */
static void l1_decode(int sframe, int eframe) {
    int j = 0;
    int escaped = 0;
    packet_len = 0;

    BLOG_DEBUG("l1 decode [%d %d]\n", sframe, eframe);

    for(int i=sframe; i<=eframe; i++) {
        unsigned char ch = packet_buffer[i];

        switch(ch) {
        case SCOMM_FRAME_END:
            packet_len = j;
            break;

        case SCOMM_FRAME_ESC:
            escaped = 1;
            break;

        case SCOMM_FRAME_ESC_END:
            if (escaped) {
                packet_buffer[j++] = SCOMM_FRAME_END;
                escaped = 0;
            }
            else {
                packet_buffer[j++] = packet_buffer[i];
            }
            break;

        case SCOMM_FRAME_ESC_ESC:
            if (escaped) {
                packet_buffer[j++] = SCOMM_FRAME_ESC;
                escaped = 0;
            }
            else {
                packet_buffer[j++] = packet_buffer[i];
            }
            break;

        default:
            packet_buffer[j++] = packet_buffer[i];
            break;
        }

        // l2 packet is at [0, packet_len)
        if (packet_len) {
            break;
        }
    }

    BLOG_DEBUG("l2 packet length %d\n", packet_len);
}

/**
 * Process a layer 1/2 data to get layer 3 pdu
 *
 * @param sfram Start of frame
 * @param eframe End of frame
 */
static void process_l1l2(int sframe, int eframe)
{
    unsigned short recv_crc;
    unsigned short calc_crc;

    // remove l1 escapes and frame end to get l2 packet
    l1_decode(sframe, eframe);

    if (packet_len < (SCOMM_L2_HEADER_SIZE + SCOMM_L2_CRC_SIZE)) {
        BLOG_ERR("frame recieved too short %d\n", packet_len);
        packet_len = 0;
        return;
    }

    // check l2 crc
    memcpy(&recv_crc,
           packet_buffer + packet_len - SCOMM_L2_CRC_SIZE,
           SCOMM_L2_CRC_SIZE);

    recv_crc = ntohs(recv_crc);
    calc_crc =  crc16_calculate(packet_buffer, packet_len - SCOMM_L2_CRC_SIZE);

    if (recv_crc != calc_crc) {
        BLOG_ERR("crc mismatch (%04X %04X), len %d\n", (unsigned int)recv_crc,
                (unsigned int)calc_crc, packet_len - SCOMM_L2_CRC_SIZE);
        packet_len = 0;
        return;
    }

    // decode l2 header
    version = packet_buffer[0] & SCOMM_DATALINK_PROTO_VERSION_MASK;
    service = (packet_buffer[0] >> SCOMM_DATALINK_PROTO_SERVICE_ID_POS) & SCOMM_DATALINK_PROTO_VERSION_MASK;

    // exclude crc from l2 packate to get l2 pdu size
    packet_len -= SCOMM_L2_CRC_SIZE;
}

void serial_process_data(int serial, FifoProcess* mgmt, FifoProcess* socks)
{
    FifoProcess* fifo;
    unsigned char* tail;
    int count = 0;

    tail = packet_buffer + buffer_pos;
    count = MAX_SCOMM_PACKET_BUFFER_SIZE - buffer_pos;

    BLOG_DEBUG("to read %d bytes\n", count);
    count = read(serial, tail, count);
    BLOG_DEBUG("got %d bytes\n", count);

    if (count <= 0) {
        BLOG_ERR("can't read from serial port: %d\n", errno);
        return;
    }

#ifdef CLI_DEBUG
    for(int z = 0; z < count; z++) {
        BLOG_DEBUG("%02X%s", *(tail + z), ((z+1)%16 && z!=(count-1))?" ":"\n");
    }
#endif

    buffer_pos += count;
    int sframe = -1;         // start of a frame
    int eframe = -1;         // end of a frame
    int last_processed = -1; // last byte examed

    for(int i=0; i<buffer_pos; i++) {
        if (SCOMM_FRAME_END != packet_buffer[i]) {
            // mark the begining of a frame
            if (sframe == -1) {
                sframe = i;
            }
            continue;
        }
        else {
            // seek the FRAME_END in the
            // begining of the unprocessed buffer

            if (sframe == -1) {
                last_processed = i;
                continue;
            }
        }

        eframe = i;
        last_processed = eframe;

        // process layer 1 and layer 2
        process_l1l2(sframe, eframe);

        // reset for next frame
        sframe = -1;
        eframe = -1;

        // skip to remaing data if current packet is invalid
        if (!packet_len) {
            continue;
        }

        if (version != SCOMM_DATALINK_PROTO_VERSION) {
            BLOG_ERR("unexpected version %d\n", version);
            continue;
        }

        if (service != SERVICE_ID_MANAGEMENT && service != SERVICE_ID_SOCKETS) {
            BLOG_ERR("unsupported service met %u\n", (unsigned int)service);
            continue;
        }

        // forward l3 data to corresponding service excluding l2 header
        fifo = (service == SERVICE_ID_MANAGEMENT) ? mgmt : socks;
        fifo->WriteData(packet_buffer + SCOMM_L2_HEADER_SIZE,
                        packet_len - SCOMM_L2_HEADER_SIZE);
    }

    // move remaining data at the begining of buffer
    last_processed++;
    buffer_pos -= last_processed;
    if (buffer_pos && last_processed) {
        memcpy(packet_buffer, packet_buffer + last_processed, buffer_pos);
    }

    BLOG_DEBUG("serial remaining data length: %d\n", buffer_pos);
}

void serial_send_data(FILE* serial, const unsigned char* data, int len)
{
    int i = 0;
    int j = 0;

    BLOG_DEBUG("write serial l2 length %d\n", len);

    // escape
    for (i=0; i<len; i++) {
        unsigned char ch = data[i];
        switch (ch) {
        case SCOMM_FRAME_END:
            send_buffer[j++] = SCOMM_FRAME_ESC;
            send_buffer[j++] = SCOMM_FRAME_ESC_END;
            break;

        case SCOMM_FRAME_ESC:
            send_buffer[j++] = SCOMM_FRAME_ESC;
            send_buffer[j++] = SCOMM_FRAME_ESC_ESC;
            break;

        default:
            send_buffer[j++] = ch;
            break;
        }
    }

    // add frame end
    int pad_len = L1_FRAME_BASE_SIZE - j % L1_FRAME_BASE_SIZE;
    for(i=0; i<pad_len; i++) {
        send_buffer[j++] = SCOMM_FRAME_END;
    }

    // send data to serial port
    if (1 != fwrite(send_buffer, j, 1, serial)) {
        BLOG_ERR("could not write to serial port\n");
    }
    (void)fflush(serial);

#ifdef CLI_DEBUG
    for(int z=0; z<j; z++) {
        BLOG_DEBUG("%02X%s", send_buffer[z], ((z+1)%16 && z!=(j-1))?" ":"\n");
    }
#endif
}
