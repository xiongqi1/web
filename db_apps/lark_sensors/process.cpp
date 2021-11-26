/*
 * @file
 * Lark sensor data process implementation
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


#include "process.h"
#include "rdblib.h"
#include "log.h"
#include "gridvar.h"

#include "Eigen/Geometry"
#include <stdio.h>     // sprintf etc
#include <math.h>      // round etc
#include <stdlib.h>    // atof etc
#include <string>      // std::string

/**
 * Format and write string to RDB
 *
 * @param   _fmt       Format, same as printf.
 * @param   _k         RDB key to write to.
 * @param   _b         Buffer used to hold formatted value.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to write.
 * @param   ...        Extra parameters referenced in format.
 *
 *
 */
 #define FWRITE_RDB_STR(_fmt, _k, _b, _rdbs, _rc, ...)      \
    do {                                                    \
        (void)snprintf(_b, sizeof(_b), _fmt, __VA_ARGS__);  \
        if(rdb_set_string(_rdbs, _k, _b) && _rc) {          \
            LS_ERROR("rdb_set_string: %s, '%s'", _k, _b);   \
            return _rc;                                     \
        }                                                   \
    } while (0)

/**
 * Write float number to RDB with percision of two digits
 *
 * @param   _k         RDB key to write to.
 * @param   _v         Float number to write.
 * @param   _b         Buffer used to hold formatted value.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to write.
 *
 *
 */
 #define RDB_WRITE_FLOAT_P2(_k, _v, _b, _rdbs, _rc)         \
    FWRITE_RDB_STR("%.2f", _k, _b, _rdbs, _rc, _v)

/**
 * Write float number to RDB rounded to the nearest integer
 *
 * @param   _k         RDB key to write to.
 * @param   _v         Float number to write.
 * @param   _b         Buffer used to hold formatted value.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to write.
 *
 *
 */
 #define RDB_WRITE_FLOAT_PR0(_k, _v, _b, _rdbs, _rc)        \
    FWRITE_RDB_STR("%d", _k, _b, _rdbs, _rc, (int)round(_v))

/**
 * Write unsigned int number to RDB
 *
 * @param   _k         RDB key to write to.
 * @param   _v         Unsigned integer to write.
 * @param   _b         Buffer used to hold formatted value.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to write.
 *
 *
 */
 #define RDB_WRITE_UINT(_k, _v, _b, _rdbs, _rc)             \
    FWRITE_RDB_STR("%u", _k, _b, _rdbs, _rc, (unsigned int)_v)

/**
 * Read a string from RDB
 *
 * @param   _k         RDB key to read from.
 * @param   _b         Buffer where the read result is stored at.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to read.
 *
 *
 */
 #define RDB_READ_STR(_k, _b, _rdbs, _rc)                           \
    do {                                                            \
        int _ret = rdb_get_string(_rdbs, _k, _b, sizeof(_b));       \
        if(_ret) {                                                  \
            LS_ERROR("rdb_get_string: %s, ret: %d", _k, _ret);      \
            return _rc;                                             \
        }                                                           \
    } while (0)

/**
 * Read a float number from RDB
 *
 * @param   _v         Float number where the read result is stored at.
 * @param   _k         RDB key to read from.
 * @param   _b         Buffer where the read result(string) is stored at.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to read
 *
 *
 */
 #define RDB_READ_FLOAT(_v, _k, _b, _rdbs, _rc)         \
    do {                                                \
        RDB_READ_STR(_k, _b, _rdbs, _rc);               \
        _v = atof(_b);                                  \
    } while (0)

/**
 * Read an integer number from RDB
 *
 * @param   _v         Integer number where the read result is stored at.
 * @param   _k         RDB key to read from.
 * @param   _b         Buffer where the read result(string) is stored at.
 * @param   _rdbs      RDB session.
 * @param   _rc        Return code value if it fails to read.
 *
 *
 */
 #define RDB_READ_INT(_v, _k, _b, _rdbs, _rc)           \
    do {                                                \
        RDB_READ_STR(_k, _b, _rdbs, _rc);               \
        _v = atoi(_b);                                  \
    } while (0)

/**
 * Rotate an orientation given offset
 *
 * @param   _orien     An array of three float numbers representing orientation.
 * @param   _rot       An array of three float numbers representing offset.
 *
 *
 */
 #define ROTATE(_orien, _rot)       \
    do {                            \
        _orien[0] += _rot[0];       \
        _orien[1] += _rot[1];       \
        _orien[2] += _rot[2];       \
    } while (0)

/**
 * Write orientation into RDB
 *
 * @param   _rdbs      RDB session.
 * @param   _ori       An array of three float numbers representing orientation.
 * @param   _status    Accuracy status of the orientation.
 * @param   _key       RDB key to write to.
 * @param   _rc        Return code value if it fails to read.
 *
 *
 */
 #define WRITE_ORIENTATION(_rdbs, _ori, _status, _key, _rc)             \
    do {                                                                \
        char _buffer[32];                                               \
        FWRITE_RDB_STR("%.2f;%.2f;%.2f;%d", _key, _buffer,              \
                _rdbs, _rc, _ori[0], _ori[1], _ori[2], (int)_status);   \
    } while (0)

// accuracy estimation level greater or eqal is recogized that the sensor
// is calibrated
// TBD
#define CAL_CNFDT_LEVEL                     (2)

// Last azimuth and tilt data as most precise data prior to
// the actual implementation is ready
static int last_azimuth = 0;
static int last_tilt = 0;

// unit function declarations

/**
 * Create all RDB keys might be used.
 *
 * @param[out]   rdbs      RDB session.
 *
 * @retval       0         ok
 * @retval       non-zero  fail
 */
 static int create_rdb_keys(rdb_session* rdbs);

/**
 * Get GPS data of antenna from RDB
 *
 * @param        rdbs      RDB session.
 * @param[out]   gps       longituide/latitude/altituide.
 * @param[out]   date      GPS date, DDMMY.
 * @param[out]   status    GPS status.
 *
 * @retval       0         ok
 * @retval       non-zero  fail
 */
 static int rdb_get_gps_location(rdb_session* rdbs, Eigen::Vector3f& gps, std::string& date, int& status);

/**
 * Get ideal mechanical design rotation required to transform orientation of IMU to antenna.
 *
 * @param[out]   rot       azimuth/pitch/roll.
 *
 * @retval       0         ok
 * @retval       non-zero  fail
 */
 static int get_ideal_rotation_to_antenna(Eigen::Vector3f& rot);

/**
 * Get compensate offset to apply upon ideal rotation from RDB
 *
 * @param        rdbs      RDB session.
 * @param        cal       azimuth/pitch/roll.
 *
 * @retval       0         ok
 * @retval       non-zero  fail
 */
 static int rdb_get_calibration(rdb_session* rdbs, Eigen::Vector3f& cal);

/**
 * Write accelerometer reading(without offset) to RDB
 *
 * @param        rdbs      RDB session.
 * @param        acc       Accelermeter reading.
 * @param        status    Accelermeter status.
 *
 * @retval       0         ok
 * @retval       non-zero  fail
 */
static int rdb_set_uncal_acc(rdb_session* rdbs, const Eigen::Vector3f& acc, int status);

/**
 * Write antenna orientation into RDB
 *
 * @param        rdbs      RDB session.
 * @param        ori       Anntena orientation(true north): azimuth/pitch/roll
 * @param        status    Status of orientation.
 * @param        bearing   Megnetic bearing requested by external interface.
 * @param        gps_valid Whether gps data are valid
 * @retval       0         ok
 * @retval       non-zero  fail
 */
static int rdb_set_ant_orien(rdb_session* rdbs, const Eigen::Vector3f& ori, int status, int bearing, bool gps_valid);

// interface functions implementation

int process_readings(
    rdb_session * rdbs,
    const Eigen::Vector3f& acc,
    int acc_status,
    Eigen::Vector3f ori,
    int ori_status
)
{
    static int rdb_keys_created = 0;
    if(!rdb_keys_created &&
        !(rdb_keys_created = !create_rdb_keys(rdbs))) {
        return 2;
    }

    static int wwm_version_recorded = 0;
    if(!wwm_version_recorded) {
        char buffer[32];
        FWRITE_RDB_STR("%s", RDBK_WMM_VERSION, buffer, rdbs, 2, WMMVersion().c_str());
        wwm_version_recorded = 1;
    }

    // write acc uncalibrated data to rdb
    // database is NOT locked to maintain a persistent view of related data
    // as performance cost might be high due the update frequency & global lock

    // this is used only in ATS test/calbiration
    (void)rdb_set_uncal_acc(rdbs, acc, acc_status);

    WRITE_ORIENTATION(rdbs, ori, ori_status, RDBK_IMU_MAG_ORIEN, 3);
    WRITE_ORIENTATION(rdbs, ori, ori_status, RDBK_PCB_MAG_ORIEN, 4);

    // offset need to apply
    static Eigen::Vector3f offset;
    static int offset_loaded = 0;

    if(!offset_loaded &&
        !(offset_loaded = !rdb_get_calibration(rdbs, offset))) {
        return 5;
    }
    ROTATE(ori, offset);
    WRITE_ORIENTATION(rdbs, ori, ori_status, RDBK_ENCL_MAG_ORIEN, 6);

    // mechncial design transformations
    static Eigen::Vector3f rot;
    static int rot_got = 0;

    if(rot_got &&
        !(rot_got = !get_ideal_rotation_to_antenna(rot))) {
        return 7;
    }
    ROTATE(ori, rot);
    WRITE_ORIENTATION(rdbs, ori, ori_status, RDBK_ANT_MAG_ORIEN, 8);

    int mag_bearing = int(round(ori[0])) % 360;

    // get gps data fromm antenna(through rdb)
    Eigen::Vector3f gps;
    std::string date;
    int gps_status;

    if(rdb_get_gps_location(rdbs, gps, date, gps_status) || !gps_status) {
        LS_INFO("%s", "GPS status not ready");
        // calibration status and bearing do not depend on gps, we always need them
        (void)rdb_set_ant_orien(rdbs, ori, ori_status, mag_bearing, false);
        return 12;
    } else {
        // convert to true north system
        ori[0] = fmod(ori[0] + GridVarCal(gps[0], gps[1], gps[2], date.c_str()), 360.0);
        ori[0] = (ori[0] < 0)?(ori[0] + 360.0) : ori[0];

        // update true north orientation to rdb
        (void)rdb_set_ant_orien(rdbs, ori, ori_status, mag_bearing, true);
    }

    return 0;
}

//  static function implmentations

int create_rdb_keys(rdb_session* rdbs)
{
    typedef struct {
        const char* key;
        const char* data;
    } key_data;

    const key_data init[] = {
        // probably not a good place to really create these gps keys,
        // however through this process, we ensure all the key we're
        // going to use would exist
        { EXT_RDBK_ANT_GPS_LONGITUDE,       "0.0" },
        { EXT_RDBK_ANT_GPS_LATITUDE,        "0.0" },
        { EXT_RDBK_ANT_GPS_ALTITUDE,        "0.0" },
        { EXT_RDBK_ANT_GPS_DATE,            "010119" },
        { EXT_RDBK_ANT_GPS_STATUS,          "waiting" },

        { EXT_RDBK_ANT_ORIEN_AZIMUTH,       "0" },
        { EXT_RDBK_ANT_ORIEN_TILT,          "0" },
        { EXT_RDBK_ANT_ORIEN_ROLL,          "0" },
        { EXT_RDBK_ANT_ORIEN_STATUS,        "1" },
        { EXT_RDBK_ANT_ORIEN_INST_STATUS,   "0" },
        { EXT_RDBK_ANT_ORIEN_MAG_BEARING,   "0" },

        { RDBK_OFFSET_IMU_ENCL_YAW,         "0.0" },
        { RDBK_OFFSET_IMU_ENCL_PITCH,       "0.0" },
        { RDBK_OFFSET_IMU_ENCL_ROLL,        "0.0" },

        { RDBK_UNCAL_ACC_X,                 "0.0" },
        { RDBK_UNCAL_ACC_Y,                 "0.0" },
        { RDBK_UNCAL_ACC_Z,                 "0.0" },
        { RDBK_UNCAL_ACC_STATUS,            "0.0" },

        { RDBK_IMU_MAG_ORIEN,               "0.0;0.0;0.0;0" },
        { RDBK_PCB_MAG_ORIEN,               "0.0;0.0;0.0;0" },
        { RDBK_ENCL_MAG_ORIEN,              "0.0;0.0;0.0;0" },
        { RDBK_ANT_MAG_ORIEN,               "0.0;0.0;0.0;0" }
    };

    int size = sizeof(init)/sizeof(init[0]);

    int flags = CREATE;
    int perm = DEFAULT_PERM;

    for(int i=0; i<size; i++) {
        const key_data* item = init + i;
        int ret = rdb_create(rdbs, item->key, item->data,
            strlen(item->data) + 1, flags, perm);

        if(ret && ret != -EEXIST) {
            LS_ERROR("rdb_create, %s, %s, %d", item->key, item->data, ret);
            return 1;
        }
    }

    return 0;
}

int rdb_get_gps_location(rdb_session* rdbs, Eigen::Vector3f& gps, std::string& date, int& status )
{
    char buff[16];

    RDB_READ_STR(RDBK_RDB_SYNC_STATUS, buff, rdbs, 6);
    if (strncmp(buff, "synchronised", sizeof(buff))) {
        // gps data is invalid unless rdb_bridge is synchronised
        int ret = rdb_get_string(rdbs, RDBK_RDB_REST_GPS, buff, sizeof(buff));
        if (ret || strncmp(buff, "1", sizeof(buff))) {
            // and there's not GPS from the REST API
            status = 0;
            return 0;
        }
    }

    RDB_READ_FLOAT(gps[0], EXT_RDBK_ANT_GPS_LONGITUDE, buff, rdbs, 1);
    RDB_READ_FLOAT(gps[1], EXT_RDBK_ANT_GPS_LATITUDE, buff, rdbs, 2);
    RDB_READ_FLOAT(gps[2], EXT_RDBK_ANT_GPS_ALTITUDE, buff, rdbs, 3);

    RDB_READ_STR(EXT_RDBK_ANT_GPS_DATE, buff, rdbs, 4);
    date = buff;

    RDB_READ_STR(EXT_RDBK_ANT_GPS_STATUS, buff, rdbs, 5);
    status = std::string(buff) == "success";

    return 0;
}

int get_ideal_rotation_to_antenna(Eigen::Vector3f& rot)
{
    // TBD need finalize with hardware design
    rot[0] = 0.0;
    rot[1] = 0.0;
    rot[2] = 0.0;

    return 0;
}

int rdb_get_calibration(rdb_session* rdbs, Eigen::Vector3f& cal)
{
    char buff[16];

    RDB_READ_FLOAT(cal[0], RDBK_OFFSET_IMU_ENCL_YAW, buff, rdbs, 1);
    RDB_READ_FLOAT(cal[1], RDBK_OFFSET_IMU_ENCL_PITCH, buff, rdbs, 2);
    RDB_READ_FLOAT(cal[2], RDBK_OFFSET_IMU_ENCL_ROLL, buff, rdbs, 3);

    return 0;
}

int rdb_set_uncal_acc(rdb_session* rdbs, const Eigen::Vector3f& acc, int status )
{
    char buffer[16];

    RDB_WRITE_FLOAT_P2(RDBK_UNCAL_ACC_X, acc[0], buffer, rdbs, 1);
    RDB_WRITE_FLOAT_P2(RDBK_UNCAL_ACC_Y, acc[1], buffer, rdbs, 2);
    RDB_WRITE_FLOAT_P2(RDBK_UNCAL_ACC_Z, acc[2], buffer, rdbs, 3);

    // status ranges from 0-3 representing differnt confident level
    RDB_WRITE_UINT(RDBK_UNCAL_ACC_STATUS, status, buffer, rdbs, 4);

    return 0;
}

int rdb_set_ant_orien(
    rdb_session* rdbs,
    const Eigen::Vector3f& ori,
    int status,
    int mag_bearing,
    bool gps_valid)
{
    char buffer[16];

    if (gps_valid) {
        RDB_WRITE_FLOAT_PR0(EXT_RDBK_ANT_ORIEN_AZIMUTH, ori[0], buffer, rdbs, 1);

        last_azimuth = (int) round(ori[0]);
        last_tilt = (int) round(ori[1]);

        status = (status >= CAL_CNFDT_LEVEL) ? 0 : 1;
    } else {
        status = (status >= CAL_CNFDT_LEVEL) ? 2 : 1;
    }

    RDB_WRITE_FLOAT_PR0(EXT_RDBK_ANT_ORIEN_TILT, ori[1], buffer, rdbs, 2);
    RDB_WRITE_FLOAT_PR0(EXT_RDBK_ANT_ORIEN_ROLL, ori[2], buffer, rdbs, 3);
    RDB_WRITE_UINT(EXT_RDBK_ANT_ORIEN_STATUS, status, buffer, rdbs, 4);
    RDB_WRITE_UINT(EXT_RDBK_ANT_ORIEN_MAG_BEARING, mag_bearing, buffer, rdbs, 5);

    return 0;
}

// TODO To be implemented with precise data
void get_precise_data(int &azimuth, int& tilt)
{
    azimuth = last_azimuth;
    tilt = last_tilt;
}

int rdb_set_ori_inst_status(rdb_session* rdbs, int status)
{
    char buffer[16];
    RDB_WRITE_UINT(EXT_RDBK_ANT_ORIEN_INST_STATUS, status, buffer, rdbs, 1);
    return 0;
}

