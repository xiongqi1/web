/**
 * @file file_buffer.cc
 * @brief Implement a simple file buffer management
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

#include "file_buffer.h"

#include "utils.h"
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include <poll.h>
#include <arpa/inet.h>

void FileBuffer::Buffer(void* ptr, size_t len, bool header)
{
    if (!len) {
        BLOG_WARNING("empty sized memory block %p\n", ptr);
        return;
    }

    BufferChunk* chunk = nullptr;

    if (header) {
        uint16_t pack_len = (uint16_t)len;
        pack_len = htons(pack_len);

        chunk = new BufferChunk(len + sizeof(pack_len));
        if (chunk) {
            uint8_t* buffer = chunk->Get();
            memcpy(buffer, &pack_len, sizeof(pack_len));
            memcpy(buffer + sizeof(pack_len), ptr, len);
        }
    }
    else {
        chunk = new BufferChunk(ptr, len);
    }

    if (!chunk || 0 == chunk->Size()) {
        BLOG_ERR("could not allocate memory %p %d\n", ptr, len);
        return;
    }

    m_chunks.push_back(chunk);
    BLOG_DEBUG("memory chunk added %p %d\n", ptr, chunk->Size());
}

int FileBuffer::OnWrite(uint16_t event)
{
    int ret =0;
    if (!(POLLOUT & event)) {
        return ret;
    }

    BLOG_DEBUG("write file buffer %d\n", m_fd);

    // write as much as possible
    for (auto& it : m_chunks) {
        void* ptr = it->Get();
        size_t len = it->Length();
        if (!len) {
            continue;
        }

        int count = write(m_fd, ptr, len);
        if (count<0) {
            BLOG_ERR("fd write error %d %d\n", m_fd, errno);
            break;
        }

        if (count>0) {
            it->Discard(count);
            ret += count;
        }

        if (it->Length()) {
            break;
        }
    }

    // remove empty chunks.
    auto iter = std::remove_if(m_chunks.begin(), m_chunks.end(), [](const BufferChunk* bc) {
        return 0 == bc->Length();
    });

    for(auto it = iter; it != m_chunks.end(); it++) {
        delete *it;
    }

    m_chunks.erase(iter, m_chunks.end());
    return ret;
}