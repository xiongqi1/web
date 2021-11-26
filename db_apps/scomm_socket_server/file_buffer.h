/**
 * @file file_buffer.h
 * @brief A simple file buffer management
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

#ifndef FILE_BUFFER_H_10253127082019
#define FILE_BUFFER_H_10253127082019

#include "utils.h"

#include <vector>
#include <string.h> 
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/**
 * Buffer chunk
 *
 * Manage a chunk of memory buffer
 */

class BufferChunk {
public:
    /**
     * Constructor - allocate buffer of required length
     *
     * @param   len         Size of the buffer chunk
     *
     */
    BufferChunk(size_t len)
    :m_pos(0), m_size(0) {
        m_buffer = new uint8_t[len];
        if (m_buffer) {
            m_size = len;
        }
    }

    /**
     * Constructor - copy buffer to construct a buffer chunk object
     *
     * @param   ptr         Memory pointer to the buffer to copy from
     * @param   len         Size of the original buffer
     *
     */
     BufferChunk(const void * ptr, size_t len)
    :m_pos(0), m_size(0) {
        m_buffer = new uint8_t[len];
        if (m_buffer) {
            memcpy(m_buffer, ptr, len);
            m_size = len;
        }
     }

    /**
     * Destructor
     *
     */
     ~BufferChunk() {
        delete[] m_buffer;
     }

public:
    /**
     * Get memory buffer pointer
     *
     * @return      Memory pointer
     */
    uint8_t* Get() {
        return m_buffer + m_pos;
    }

    /**
     * Get undiscarded memory buffer length
     *
     * @return      Length of the undiscarded memory buffer
     */
    size_t Length() const {
        return m_size - m_pos;
    }

    /**
     * Get memory buffer chunk size
     *
     * @return      Size of the memory buffer chunk
     */
    size_t Size() const {
        return m_size;
    }

    /**
     * Discard memory buffer
     *
     * @param   Len         Length of memory requested to be discarded
     */
    void Discard(size_t len) {
#ifdef CLI_DEBUG
        for(size_t z=0; z<len; z++) {
            BLOG_DEBUG_DATA("%02X%s", m_buffer[m_pos+z], ((z+1)%16 && z!=(len-1))?" ":"\n");
        }
#endif
        m_pos += len;
        if (m_pos > m_size) {
            m_pos = m_size;
        }
    }

protected:
    uint8_t* m_buffer;     // memory buffer pointer
    size_t m_pos;       // undiscarded memory position
    size_t m_size;      // size of memory buffer
};

/**
 * A dynamic file buffer
 *
 * Manage file write buffer in dynamic
 */

class FileBuffer {
public:
    /**
     * Constructor
     *
     * @param    fd         File descriptor of the underlying file
     */
    FileBuffer(int fd)
    : m_fd(fd){
        int flags = fcntl(m_fd, F_GETFL, 0);
        fcntl(m_fd, F_SETFD, flags | O_NONBLOCK);
    }

    /**
     * Destructor
     *
     */
    ~FileBuffer() {
        for(auto& ic: m_chunks) {
            delete ic;
        }
        (void)close(m_fd);
    }

public:

    /**
     * Buffer a block of data
     *
     * @param       ptr         Memory pointer of data to be buffered
     * @param       len         Length of memory block
     * @param       header      Pad 2 uint8_t long length header
     */
    void Buffer(void* ptr, size_t len, bool header = false);

    /**
     * Write to underlying file when it is ready to accept more data
     *
     * @param       event       Returned poll events of respected file
     * @return      Number of bytes written.
     */
    int OnWrite(uint16_t event);

    /**
     * Check if the buffer is empty
     *
     * @retval true     The buffer is empty
     * @retval false    The buffer is not empty
     */
    bool IsEmpty() {
        return m_chunks.size() == 0;
    }

protected:
    std::vector<BufferChunk*> m_chunks;  // memory chunks
    int m_fd;                            // underly file descriptor
};

#endif //FILE_BUFFER_H_10253127082019