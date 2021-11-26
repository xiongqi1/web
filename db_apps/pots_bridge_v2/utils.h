#ifndef __UTILS__H_20180620__
#define __UTILS__H_20180620__

/*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include <stdint.h>

#define __countof(a) (sizeof(a)/sizeof(a[0]))

#define min(X,Y) ((X)>(Y)?(Y):(X))
#define max(X,Y) ((X)<(Y)?(Y):(X))

#define __packed __attribute__((__packed__))
#define __stringify(x) #x
#define __tostring(x) __stringify(x)

#define __noop() (void)0
#define __unused(x) (void)(x)

#define strncpy_safe(d,s,n) \
    do { \
        strncpy(d,s,n); \
        d[n-1]=0; \
    } while(0)


#define strlen_static(s) (sizeof(s)-1)

uint64_t get_monotonic_msec(void);
uint64_t get_time_elapsed_msec(void);
void log_dump_hex(const char* dump_name, const void* data, int len);

#endif
