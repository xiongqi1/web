/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#ifndef RDB_UTIL_H_20090325_
#define RDB_UTIL_H_20090325_

#include "rdb_ops.h"

extern char wwan_prefix[];
extern char gps_prefix[];

// Build up a full RDB variable name from two components.
//
// Special behaviour if the path is RDB_GPS_PREFIX or RDB_SYSLOG_PREFIX.
//
// Examples:
//
//   Assuming wwan_prefix == "wwan.0" and gps_prefix == "sensors.gps.0" then:
//     rdb_name(RDB_GPS_PREFIX, RDB_GPS_GPSONE_EN) == "sensors.gps.0.gpsone.enable"
//     rdb_name(RDB_SYSLOG_PREFIX, RDB_SYSLOG_MASK) == "service.syslog.option.mask"
//     rdb_name(RDB_ROAMING, "") == "wwan.0.system_network_status.roaming"
//     rdb_name("", RDB_ROAMING) == "wwan.0.system_network_status.roaming" too
//
// Warnings:
// * Returns a pointer to the function scope static buffer.  So the value must be used before
//   subsequent calls.
// * Accesses global variable wwan_prefix.
// * Risk buffer overruns if string is longer than 256 characters.
const char* rdb_name( const char* path, const char* name );

int rdb_set_single_int(const char* name, int value);
void rdb_create_and_get_single(const char* name, char *value, int vlen);

#endif /* RDB_UTIL_H_20090325_ */

