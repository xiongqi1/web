/*
 * QMI Location service.
 *
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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

#include "g.h"

#define _GET_TLV_LOC(ptr_msg, ptr_tlv, tvl_type, struct_type, ptr_struct) \
    struct struct_type * ptr_struct= NULL; \
    do { \
        ptr_tlv = _get_tlv(ptr_msg, tvl_type, sizeof(*ptr_struct)); \
        if(ptr_tlv) \
        ptr_struct = ptr_tlv->v; \
    } while(0)

static unsigned char g_sessionId = 1;

void qmimgr_callback_on_loc(unsigned char msg_type, struct qmimsg_t* msg, unsigned short qmi_result, unsigned short qmi_error, int noti)
{
    const struct qmitlv_t* tlv;

    switch(msg->msg_id) {
        case QMI_LOC_EVENT_POSITION_REPORT_IND: {
            char dir[16];

            SYSLOG(LOG_OPERATION, "got QMI_LOC_EVENT_POSITION_REPORT");

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SESSION_STATUS, qmi_loc_event_position_report_ind_type_session_status, resp_sessionStatus);
            if(!tlv || resp_sessionStatus->sessionStatus != eQMI_LOC_SESS_STATUS_SUCCESS) {
                if (tlv)
                    SYSLOG(LOG_OPERATION, "###gps### Invalid sessionStatus=%d",resp_sessionStatus->sessionStatus);
                break;
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SESSIONID, qmi_loc_event_position_report_ind_type_sessionid, resp_sessionId);
            if(!tlv || resp_sessionId->sessionId != g_sessionId) {
                SYSLOG(LOG_OPERATION, "###gps### Invalid sessionId");
                break;
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LATITUDE, qmi_loc_event_position_report_ind_type_latitude, resp_latitude);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### latitude=%f", resp_latitude->latitude);
                _set_str_db_ex("sensors.gps.0.assisted.latitude", convert_decimal_degree_to_degree_minute(resp_latitude->latitude, 1, dir, sizeof(dir)), -1, 0);
                _set_str_db_ex("sensors.gps.0.assisted.latitude_direction", dir, -1, 0);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LONGITUDE, qmi_loc_event_position_report_ind_type_longitude, resp_longitude);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### longitude=%f", resp_longitude->longitude);
                _set_str_db_ex("sensors.gps.0.assisted.longitude", convert_decimal_degree_to_degree_minute(resp_longitude->longitude, 0, dir, sizeof(dir)), -1, 0);
                _set_str_db_ex("sensors.gps.0.assisted.longitude_direction", dir, -1, 0);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_CIRCULAR_HORIZONTAL_POSITION_UNCERTAINTY, qmi_loc_event_position_report_ind_type_circular_horizontal_position_uncertainty, resp_horUncCircular);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### horUncCircular=%f", resp_horUncCircular->horUncCircular);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_ELLIPTICAL_UNCERTAINTY_SEMI_MINOR_AXIS, qmi_loc_event_position_report_ind_type_horizontal_elliptical_uncertainty_semi_minor_axis, resp_horUncEllipseSemiMinor);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_horUncEllipseSemiMinor=%f", resp_horUncEllipseSemiMinor->horUncEllipseSemiMinor);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_ELLIPTICAL_UNCERTAINTYSEMI_MAJOR_AXIS, qmi_loc_event_position_report_ind_type_horizontal_elliptical_uncertaintysemi_major_axis, resp_horUncEllipseSemiMajor);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_horUncEllipseSemiMajor=%f", resp_horUncEllipseSemiMajor->horUncEllipseSemiMajor);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ELLIPTICAL_HORIZONTAL_UNCERTAINTY_AZIMUTH, qmi_loc_event_position_report_ind_type_elliptical_horizontal_uncertainty_azimuth, resp_horUncEllipseOrientAzimuth);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_horUncEllipseOrientAzimuth=%f", resp_horUncEllipseOrientAzimuth->horUncEllipseOrientAzimuth);
            }
            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_CONFIDENCE, qmi_loc_event_position_report_ind_type_horizontal_confidence, resp_horConfidence);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_horConfidence=%d", resp_horConfidence->horConfidence);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_RELIABILITY, qmi_loc_event_position_report_ind_type_horizontal_reliability, resp_horReliability);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_horReliability=%d", resp_horReliability->horReliability);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HORIZONTAL_SPEED, qmi_loc_event_position_report_ind_type_horizontal_speed, resp_speedHorizontal);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_speedHorizontal=%f", resp_speedHorizontal->speedHorizontal);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SPEED_UNCERTAINTY, qmi_loc_event_position_report_ind_type_speed_uncertainty, resp_speedUnc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_speedUnc=%f", resp_speedUnc->speedUnc);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_WITH_RESPECT_TO_ELLIPSOID, qmi_loc_event_position_report_ind_type_altitude_with_respect_to_ellipsoid, resp_altitudeWrtEllipsoid);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_altitudeWrtEllipsoid=%f", resp_altitudeWrtEllipsoid->altitudeWrtEllipsoid);
                _set_float_db_ex("sensors.gps.0.assisted.altitude", resp_altitudeWrtEllipsoid->altitudeWrtEllipsoid, NULL, 0);
                _set_int_db_ex("sensors.gps.0.assisted.height_of_geoid", 0, NULL, 0);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_WITH_RESPECT_TO_SEA_LEVEL, qmi_loc_event_position_report_ind_type_altitude_with_respect_to_sea_level, resp_altitudeWrtMeanSeaLevel);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_altitudeWrtMeanSeaLevel=%f", resp_altitudeWrtMeanSeaLevel->altitudeWrtMeanSeaLevel);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_UNCERTAINTY, qmi_loc_event_position_report_ind_type_vertical_uncertainty, resp_vertUnc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_vertUnc=%f", resp_vertUnc->vertUnc);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_CONFIDENCE, qmi_loc_event_position_report_ind_type_vertical_confidence, resp_vertConfidence);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_vertConfidence=%d", resp_vertConfidence->vertConfidence);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_RELIABILITY, qmi_loc_event_position_report_ind_type_vertical_reliability, resp_vertReliability);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_vertReliability=%d", resp_vertReliability->vertReliability);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_VERTICAL_SPEED, qmi_loc_event_position_report_ind_type_vertical_speed, resp_speedVertical);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_speedVertical=%f", resp_speedVertical->speedVertical);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HEADING, qmi_loc_event_position_report_ind_type_heading, resp_heading);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_heading=%f", resp_heading->heading);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_HEADING_UNCERTAINTY, qmi_loc_event_position_report_ind_type_heading_uncertainty, resp_headingUnc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_headingUnc=%f", resp_headingUnc->headingUnc);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_MAGNETIC_DEVIATION, qmi_loc_event_position_report_ind_type_magnetic_deviation, resp_magneticDeviation);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_magneticDeviation=%f", resp_magneticDeviation->magneticDeviation);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TECHNOLOGY_USED, qmi_loc_event_position_report_ind_type_technology_used, resp_technologyMask);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_technologyMask=%d", resp_technologyMask->technologyMask);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_DILUTION_OF_PRECISION, qmi_loc_event_position_report_ind_type_dilution_of_precision, resp_DOP);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### PDOP=%f, HDOP=%f, VDOP=%f", resp_DOP->PDOP,resp_DOP->HDOP,resp_DOP->VDOP);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_UTC_TIMESTAMP, qmi_loc_event_position_report_ind_type_utc_timestamp, resp_timestampUtc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_timestampUtc=%llu", resp_timestampUtc->timestampUtc);

                struct tm tm_gps;
                time_t timestamp_utc_sec;

                char date_str[32];
                char time_str[32];

                /* convert ms to sec */
                timestamp_utc_sec = (time_t)(resp_timestampUtc->timestampUtc / 1000ULL);

                /* get Y,M,D,H,M and S */
                gmtime_r(&timestamp_utc_sec, &tm_gps);

                snprintf(date_str, sizeof(date_str), "%02d%02d%02d", tm_gps.tm_mday, tm_gps.tm_mon + 1, (tm_gps.tm_year + 1900) % 100);
                snprintf(time_str, sizeof(time_str), "%02d%02d%02d", tm_gps.tm_hour, tm_gps.tm_min, tm_gps.tm_sec);

                _set_str_db_ex("sensors.gps.0.assisted.date", date_str, -1, 0);
                _set_str_db_ex("sensors.gps.0.assisted.time", time_str, -1, 0);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_LEAP_SECONDS, qmi_loc_event_position_report_ind_type_leap_seconds, resp_leapSeconds);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_leapSeconds=%d", resp_leapSeconds->leapSeconds);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_GPS_TIME, qmi_loc_event_position_report_ind_type_gps_time, resp_gpsTime);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### gpsWeek=%d, gpsTimeOfWeekMs=%d", resp_gpsTime->gpsWeek, resp_gpsTime->gpsTimeOfWeekMs);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TIME_UNCERTAINTY, qmi_loc_event_position_report_ind_type_time_uncertainty, resp_timeUnc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_timeUnc=%f", resp_timeUnc->timeUnc);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_TIME_SOURCE, qmi_loc_event_position_report_ind_type_time_source, resp_timeSrc);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_timeSrc=%d", resp_timeSrc->timeSrc);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SENSOR_DATA_USAGE, qmi_loc_event_position_report_ind_type_sensor_data_usage, resp_dataUsage);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### usageMask=%d, aidingIndicatorMask=%d", resp_dataUsage->usageMask, resp_dataUsage->aidingIndicatorMask);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_FIX_COUNT_FOR_THIS_SESSION, qmi_loc_event_position_report_ind_type_fix_count_for_this_session, resp_fixId);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_fixId=%d", resp_fixId->fixId);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_SVS_USED_TO_CALCULATE_THE_FIX, qmi_loc_event_position_report_ind_type_svs_used_to_calculate_the_fix, resp_gnssSvUsedList);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### gnssSvUsedList_len=%d, gnssSvUsedList=%d", resp_gnssSvUsedList->gnssSvUsedList_len, resp_gnssSvUsedList->gnssSvUsedList);
            }

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_POSITION_REPORT_IND_TYPE_ALTITUDE_ASSUMED, qmi_loc_event_position_report_ind_type_altitude_assumed, resp_altitudeAssumed);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### resp_altitudeAssumed=%d", resp_altitudeAssumed->altitudeAssumed);
            }
            break;
        }

        case QMI_LOC_EVENT_GNSS_SV_INFO_IND: {
            SYSLOG(LOG_OPERATION, "got QMI_LOC_EVENT_GNSS_SV_INFO");
            //::TODO
            break;
        }

        case QMI_LOC_EVENT_NMEA_IND: {
            SYSLOG(LOG_OPERATION, "got QMI_LOC_EVENT_NMEA_IND");
            //::TODO
            break;
        }

        case QMI_LOC_EVENT_ENGINE_STATE_IND: {
            SYSLOG(LOG_OPERATION, "got QMI_LOC_EVENT_ENGINE_STATE_IND");

            _GET_TLV_LOC(msg, tlv, QMI_LOC_EVENT_ENGINE_STATE_IND_TYPE_ENGINE_STATE, qmi_loc_event_engine_state_ind_type_engine_state, resp_enginstatus);
            if(tlv) {
                SYSLOG(LOG_OPERATION, "###gps### engineState=%d", resp_enginstatus->engineState);
                //::TODO
                // For Sierra MC7430,
                // if engineState is changed to 2(eQMI_LOC_ENGINE_STATE_ OFF) after QMI_LOC_START is triggered,
                // that means internel GPS function of a module is crased or stopped accdidently.
                // So all of GPS function should be restarted.
            }
            break;
        }

        // Below indications are for "Asynchronous Messaging Paradiam", so the indications are not notifications.
        // Those are responses for correspending requests.
        // The indication messages should be processed in function _qmi_easy_req_do_async_ex().
        case QMI_LOC_SET_OPERATION_MODE: {
            SYSLOG(LOG_ERROR, "unknown QMILOC Asynchronous Indication msg(0x%04x) detected", msg->msg_id);
            break;
        }

        default:
            SYSLOG(LOG_COMM, "unknown LOC msg(0x%04x) detected", msg->msg_id);
            break;
    }
}

int _qmi_gps_on_loc_reg_events(unsigned long long mask)
{
    struct qmi_easy_req_t er;
    int rc;

    unsigned long long eventMask = mask;

    rc = _qmi_easy_req_init(&er, QMILOC, QMI_LOC_REG_EVENTS);
    if(rc < 0)
        goto fini;

    qmimsg_add_tlv(er.msg, QMI_LOC_REG_EVENTS_REQ_TYPE_EVENTREGMASK, sizeof(eventMask), &eventMask);

    /* init. tlv */
    rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 1, 1);
    if(rc < 0)
        goto fini;

fini:
    _qmi_easy_req_fini(&er);
    return rc;
}

int _qmi_gps_on_loc_set_operation_mode(enum loc_operation_mode mode)
{
    struct qmi_easy_req_t er;
    struct qmi_loc_set_operation_mode_req_type_operation_mode req;
    int rc;
    const struct qmitlv_t* tlv;

    rc = _qmi_easy_req_init(&er, QMILOC, QMI_LOC_SET_OPERATION_MODE);
    if(rc < 0)
        goto fini;

    /* init. tlv */
    memset(&req, 0, sizeof(req));
    req.operationMode = mode;
    SYSLOG(LOG_OPERATION, "QMILOC: set_operation_mode=[%d]",mode);
    rc = _qmi_easy_req_do_async_ex(&er, QMI_LOC_SET_OPERATION_MODE_RESP_TYPE, sizeof(req), &req, QMIMGR_GENERIC_RESP_TIMEOUT, QMIMGR_GENERIC_RESP_TIMEOUT,0, 1);

    if(rc < 0)
        goto fini;

    struct qmi_loc_set_operation_mode_ind_type_set_operation_mode_status* resp;

    tlv = _get_tlv(er.imsg, QMI_PDS_SET_GNSS_ENGINE_ERROR_RECOVERY_REPORT_RESP_TYPE, sizeof(*resp));
    if(tlv) {
        resp = tlv->v;
        rc = (resp->set_operation_mode_status==0) ? 0 : -1;
    }

fini:
    _qmi_easy_req_fini(&er);
    return rc;

}

int _qmi_gps_on_loc_stop (unsigned char id)
{
    struct qmi_easy_req_t er;
    int rc;

    unsigned char sessionid = id;

    rc = _qmi_easy_req_init(&er, QMILOC, QMI_LOC_STOP);
    if(rc < 0)
        goto fini;

    qmimsg_add_tlv(er.msg, QMI_LOC_START_REQ_TYPE_SESSION_ID, sizeof(sessionid), &sessionid);

    /* init. tlv */
    rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 1, 0);
    if(rc < 0)
        goto fini;

fini:
    _qmi_easy_req_fini(&er);
    return rc;
}

int _qmi_gps_on_loc_start (unsigned char id)
{
    struct qmi_easy_req_t er;
    int rc;

    unsigned char sessionid = id;
    unsigned int fixRecurrence = eQMI_LOC_RECURRENCE_PERIODIC;
    unsigned int intermediateReportState = eQMI_LOC_INTERMEDIATE_REPORTS_ON;
    unsigned int minInterval = 1000; //units: milliseconds

    rc = _qmi_easy_req_init(&er, QMILOC, QMI_LOC_START);
    if(rc < 0)
        goto fini;

    qmimsg_add_tlv(er.msg, QMI_LOC_START_REQ_TYPE_SESSION_ID, sizeof(sessionid), &sessionid);
    qmimsg_add_tlv(er.msg, QMI_LOC_START_REQ_TYPE_RECURRENCE_TYPE, sizeof(fixRecurrence), &fixRecurrence);
    qmimsg_add_tlv(er.msg, QMI_LOC_START_REQ_TYPE_INTERMEDIATE_REPORTS, sizeof(intermediateReportState), &intermediateReportState);
    qmimsg_add_tlv(er.msg, QMI_LOC_START_REQ_TYPE_MINIMUM_INTERVAL_REPORTS, sizeof(minInterval), &minInterval);

    /* init. tlv */
    rc = _qmi_easy_req_do_ex(&er, 0, 0, NULL, QMIMGR_GENERIC_RESP_TIMEOUT, 1, 0);
    if(rc < 0)
        goto fini;

fini:
    _qmi_easy_req_fini(&er);
    return rc;
}

void _batch_cmd_stop_gps_on_loc()
{
    _qmi_gps_on_loc_stop (g_sessionId);
}

int _batch_cmd_start_gps_on_loc(int agps)
{
    // QMI_LOC_EVENT_MASK_ENGINE_STATE is temporarily commented out.
    // unsigned long long eventMask = QMI_LOC_EVENT_MASK_POSITION_REPORT | QMI_LOC_EVENT_MASK_ENGINE_STATE;
    unsigned long long eventMask = QMI_LOC_EVENT_MASK_POSITION_REPORT;

    /*
       eQMI_LOC_OPER_MODE_DEFAULT=1,
       eQMI_LOC_OPER_MODE_MSB=2,         // Working
       eQMI_LOC_OPER_MODE_MSA=3,         // Not Working (error status: eQMI_LOC_SESS_STATUS_GENERAL_FAILURE)
       eQMI_LOC_OPER_MODE_STANDALONE=4,  // Working
       eQMI_LOC_OPER_MODE_CELL_ID=5,     // Working
       eQMI_LOC_OPER_MODE_WWAN=6         // Working
     */
    if (_qmi_gps_on_loc_set_operation_mode(agps ? eQMI_LOC_OPER_MODE_MSB : eQMI_LOC_OPER_MODE_STANDALONE) < 0) {
        SYSLOG(LOG_ERR, "###gps### failed to set operation mode");
        goto err;
    }

    if (_qmi_gps_on_loc_reg_events(eventMask) < 0) {
        SYSLOG(LOG_ERR, "###gps### failed to register events");
        goto err;
    }

    if (_qmi_gps_on_loc_start(g_sessionId) < 0) {
        SYSLOG(LOG_ERR, "###gps### failed to start gps engine");
        goto err;
    }
/*
    if(qmimgr_ctrl_gps_nmea(1) < 0) {
        SYSLOG(LOG_ERR, "###gps### failed to start nmea stream");
        goto err;
    }
*/
    return 0;
err:
    return -1;
}
