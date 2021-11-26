#ifndef PROCESS_14001104032019
#define PROCESS_14001104032019
/*
 * @file
 * Lark sensor data process header
 *
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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

#include "Eigen/Geometry"        //for Eigen::Vector3f
#include "rdblib.h"

// (external) RDB keys for true north antenna orientation
//#############################################################################
#define EXT_RDBK_ANT_ORIEN_AZIMUTH            "owa.orien.azimuth"
#define EXT_RDBK_ANT_ORIEN_TILT               "owa.orien.tilt"
#define EXT_RDBK_ANT_ORIEN_ROLL               "owa.orien.roll"
/* status to indicate unit calibration's status */
#define EXT_RDBK_ANT_ORIEN_STATUS             "owa.orien.status"
/* inst_status from sensor reading */
#define EXT_RDBK_ANT_ORIEN_INST_STATUS        "owa.orien.inst_status"
#define EXT_RDBK_ANT_ORIEN_MAG_BEARING        "owa.mag.bearing"

// (external) GPS data used for magnetic north and true north conversion
//#############################################################################
#define EXT_RDBK_ANT_GPS_LONGITUDE         "sensors.gps.0.common.longitude_degrees"
#define EXT_RDBK_ANT_GPS_LATITUDE          "sensors.gps.0.common.latitude_degrees"
#define EXT_RDBK_ANT_GPS_ALTITUDE          "sensors.gps.0.common.height_of_geoid"
#define EXT_RDBK_ANT_GPS_DATE              "sensors.gps.0.common.date"
#define EXT_RDBK_ANT_GPS_STATUS            "sensors.gps.0.common.status"

// RDB keys for uncalibrated(offset to be apply) accelerameter data
//#############################################################################
#define RDBK_UNCAL_ACC_X                    "imu.uncal.acc.x"
#define RDBK_UNCAL_ACC_Y                    "imu.uncal.acc.y"
#define RDBK_UNCAL_ACC_Z                    "imu.uncal.acc.z"
#define RDBK_UNCAL_ACC_STATUS               "imu.uncal.acc.status"

// RDB keys for imu/enclosure calibration offset
//#############################################################################
#define RDBK_OFFSET_IMU_ENCL_YAW            "offset.imu.enclosure.yaw"
#define RDBK_OFFSET_IMU_ENCL_PITCH          "offset.imu.enclosure.pitch"
#define RDBK_OFFSET_IMU_ENCL_ROLL           "offset.imu.enclosure.roll"

// magnetic orientation data combined for debug, including:
// - imu, the sensors reporting orientation
// - pcb, NIT PCB board
// - enclosure, the NIT unit
// - antenna, OWA antenna
//#############################################################################
#define RDBK_IMU_MAG_ORIEN                  "imu.mag.orien"
#define RDBK_PCB_MAG_ORIEN                  "pcb.mag.orien"
#define RDBK_ENCL_MAG_ORIEN                 "enclosure.mag.orien"
#define RDBK_ANT_MAG_ORIEN                  "antenna.mag.orien"

// WWM grid varation coefficient file version
#define RDBK_WMM_VERSION                    "sw.wmm.version"

// RDB bridge synchronisation status
#define RDBK_RDB_SYNC_STATUS                "service.rdb_bridge.connection_status"

// RDB REST API GPS data
#define RDBK_RDB_REST_GPS                   "owa.get.sensors.gps"


// Interface functions

/**
 * Process sensor data, perform offset/rotation/conversion and update RDB
 *
 * @param[in]   rdbs        RDB session.
 * @param[in]   acc         Accelermeter readings.
 * @param[in]   acc_status	Accelermeter readings satus.
 * @param[in]   ori         Orientation fused with reading from multiple sensors.
                            This parameter is passed by value as the implementation will
                            modify the passed in parameter copy.
 * @param[in]   ori_status  Orientation status.
 *
 * @retval   0	       ok
 * @retval   non-zero  fail
 */
 int process_readings(
    rdb_session * rdbs,
    const Eigen::Vector3f& acc,
    int acc_status,
    Eigen::Vector3f ori,
    int ori_status
);

/**
 * Get most precise data
 *
 * @param[out]   azimuth      Azimuth of OWA
 * @param[out]   tilt         Tilt of OWA
 *
 */
void get_precise_data(int &azimuth, int& tilt);

/**
 * Set the orientation calibration RDB key
 * @param[in] rdbs     RDB session.
 * @param[in] status   the orientation's status be written into RDB key
 *
 * @retval 0         ok
 * @retval non-zero  fail
*/
int rdb_set_ori_inst_status(rdb_session* rdbs, int status);

#endif  //PROCESS_14001104032019
