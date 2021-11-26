/**
 * @file i2c_charger_common.h
 * @brief common utilities and definitions for i2c charger IC
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
#define CHARGER_I2C_BUSNO 0

/*
 * Utilities macro definitions
 */
#define CHARGER_UPDATE_BITS(addr, mask, val) do { \
        int ret = i2c_update_bits(&handle, addr, mask, val); \
        if (ret < 0) { \
            BLOG_ERR("Failed to update charger reg=%02x, mask=%02x, val=%02x\n", addr, mask, val); \
            return ret; \
        } \
    } while(0)

#define CHARGER_WRITE(addr, val) do { \
        int ret = i2c_write_8(&handle, addr, val); \
        if (ret < 0) { \
            BLOG_ERR("Failed to write charger reg=%02x, val=%02x\n", addr, val); \
            return ret; \
        } \
    } while(0)

#define CHARGER_READ(addr, ret) do { \
        ret = i2c_read_8(&handle, addr); \
        if (ret < 0) { \
            BLOG_ERR("Failed to read charger reg=%02x\n", addr); \
            return ret; \
        } \
    } while(0)
