#ifndef LEDS_RDB_H_10405501082018
#define LEDS_RDB_H_10405501082018
/**
 * @file leds_rdb.h
 *
 * Header file for RDB interfaces.
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
static const uint32_t TIMEOUT_INFINITY_MSEC = 0x80000000;

/**
 * Initialise RDB session
 */
void initRdb();

/**
 * Release RDB session
 */
void releaseRdb();

/**
 * Get current value of an RDB variable
 *
 * This function returns a pointer to a statically allocated buffer, so is not threadsafe.  The
 * caller is responsible for copying the contents of this buffer to another location if multiple
 * calls to this function for different RDB variables are to be made within a single context.
 *
 * @param rdbName RDB variable of interest
 * @return Value of the RDB variable if found, or NULL otherwise
 */
const char *getRdb(const char *rdbName);

/**
 * Subscribe to notifications for an RDB variable
 *
 * This function will create the RDB variable (with an empty value) if the variable does not
 * already exist.
 *
 * @param rdbName RDB variable of interest
 * @return true if the RDB variable was successfully subscribed, false otherwise.
 */
bool subscribeRdb(const char *rdbName);

/**
 * Unsubscribe from notifications for an RDB variable
 *
 * @param rdbName RDB variable of interest
 * @return true if the RDB variable was successfully unsubscribed, false otherwise.
 */
bool unsubscribeRdb(const char *rdbName);

/**
 * Wait for a notification to a subscribed RDB variable
 *
 * If multiple subscribed RDB variables are notified simultaneously, this function will return
 * each one in turn, where the order will depend on the underlying RDB drivers.
 *
 * This function returns a pointer to a statically allocated buffer, so is not threadsafe.  The
 * caller is responsible for copying the contents of this buffer to another location if multiple
 * calls to this function for different RDB variables are to be made within a single context.
 * @note This RDB name buffer is a different buffer to that used for getRdb.
 *
 * @param timeoutMsec Maximum duration (in msec) to wait for a notification, where 0 means to
 *        check and return immediately, while a value of TIMEOUT_INFINITY_MSEC (or greater) will
 *        wait indefinitely
 * @return Name of a notified the RDB variable, or NULL otherwise
 */
const char *waitForRdb(uint32_t timeoutMsec = TIMEOUT_INFINITY_MSEC);

#endif  // LEDS_RDB_H_10405501082018
