#ifndef LEDS_SYSFS_H_10405501082018
#define LEDS_SYSFS_H_10405501082018
/**
 * @file leds_sysfs.h
 *
 * Header file for LED sysfs interfaces.
 *
 *//*
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
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
#include <stdint.h>

/*
 * Constants
 */
const char SYSFS_PATH_LEDS[] = "/sys/class/leds";
const char SYSFS_LEDS_ENTRY_TRIGGER[]= "trigger";
const uint32_t SYSFS_TIMEOUT_MSEC = 10;


/**
 * Write to a LED-specific SYSFS entry
 *
 * @param ledName Name of the LED in the /sys/class/leds file system.
 * @param entry Name of the entry to write to in the /sys/class/leds/<ledName> directory.
 * @param value Value to write to the entry.
 * @param timeoutMsec Timeout in msec to wait for the entry to be writeable.
 * @return true if successful, or false if the entry in the file system was not writeable before
 *         the timeout was reached.
 */
bool setSysLed(const char *ledName, const char *entry, const char *value, uint32_t timeoutMsec = SYSFS_TIMEOUT_MSEC);
bool setSysLed(const char *ledName, const char *entry, int value, uint32_t timeoutMsec = SYSFS_TIMEOUT_MSEC);

/**
 * Check existance of a LED-specific SYSFS entry
 *
 * @param ledName Name of the LED in the /sys/class/leds file system.
 * @param entry Name of the entry to look for in the /sys/class/leds/<ledName> directory.
 * @param timeoutMsec Timeout in msec to wait for the entry to be accessible.
 * @return true if the entry was found, or false otherwise.
 */
bool checkSysLed(const char *ledName, const char *entry = SYSFS_LEDS_ENTRY_TRIGGER, uint32_t timeoutMsec = SYSFS_TIMEOUT_MSEC);

#endif  // LEDS_SYSFS_H_10405501082018
