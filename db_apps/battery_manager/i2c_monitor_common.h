/**
 * @file i2c_monitor_common.h
 * @brief common utilities and definitions for i2c fuel gauge IC
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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

#include "i2c_linux.h"
#include "utils.h"

// TODO: this could be moved to default config in RDB or uci
#define MONITOR_I2C_BUSNO 0

/*
 * Utilities macro definitions
 */
#define MONITOR_WRITE(addr, val) do { \
        int ret = i2c_write_le16(&handle, addr, val); \
        if (ret < 0) { \
            BLOG_ERR("Failed to write to monitor reg %02x\n", addr); \
            return ret; \
        } \
    } while(0)

#define MONITOR_READ(addr, ret) do { \
        ret = i2c_read_le16(&handle, addr); \
        if (ret < 0) { \
            BLOG_ERR("Failed to read monitor reg %02x, err=%d\n", addr, ret); \
            return ret; \
        } \
    } while(0)

#ifdef V_BATTERY_MONITOR_max17201

/*
 * write to a register on the secondary slave address of monitor chip
 *
 * @param addr The 8-bit register address
 * @param val The 16-bit value
 * @return A negative error code on failure; fall through on success
 */
#define MONITOR_WRITE_N(addr, val) do { \
        int ret = i2c_write_le16(&nhandle, addr, val); \
        if (ret < 0) { \
            BLOG_ERR("Failed to write to monitor nreg %02x\n", addr); \
            return ret; \
        } \
    } while(0)

#endif
