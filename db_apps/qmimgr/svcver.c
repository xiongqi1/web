/*
 * QMI service version.
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
#include "svcver.h"

/* This is defined in ezqmi.c */
extern int
_request_qmi(struct qmimsg_t* msg, int serv_id, unsigned short msg_id,
             unsigned short* tran_id);

/* version numbers for all qmi services*/
struct qmi_service_version_t qmi_svc_ver[QMIUNICLIENT_SERVICE_CLIENT];

/* get the version numbers for all qmi services */
int _qmi_get_service_versions(void)
{
	int i;
	unsigned short tran_id;
	unsigned short qmi_result;
	unsigned short qmi_error;

	struct qmi_ctl_get_version_info_resp_svc_ver * resp;
	struct qmi_ctl_get_version_info_resp_svc_ver_sub * resp_sub;

	struct qmimsg_t * msg;
	struct qmimsg_t * rmsg;

	const struct qmitlv_t * tlv;

	SYSLOG(LOG_OPERATION, "###qmimux### get service version");

	// create msg
	msg = qmimsg_create();
	if(!msg) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to create a msg");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### requesting QMI_CTL_GET_VERSION_INFO");

	if(_request_qmi(msg, QMICTL, QMI_CTL_GET_VERSION_INFO, &tran_id) < 0) {
		SYSLOG(LOG_ERROR, "###qmimux### failed to request for "
               "QMI_CTL_GET_VERSION_INFO");
		goto err;
	}

	SYSLOG(LOG_OPERATION, "###qmimux### waiting for response - "
           "QMI_CTL_GET_VERSION_INFO");

	// wait for response
	rmsg = _wait_qmi_response_ex(QMICTL, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id,
                                 &qmi_result, &qmi_error, NULL, 0);
	if(!rmsg) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi response timeout");
		goto err;
	}
	if(qmi_result) {
		SYSLOG(LOG_ERROR, "###qmimux### qmi failure: result=%02x, error=%02x",
		       qmi_result, qmi_error);
		goto err;
	}

	SYSLOG(LOG_OPERATION, "check result of QMI_CTL_GET_VERSION_INFO");

	// get QMI_CTL_GET_VERSION_INFO_RESP_TYPE_SERVICE_VERSION
	tlv = _get_tlv(rmsg, QMI_CTL_GET_VERSION_INFO_RESP_TYPE_SERVICE_VERSION,
                   sizeof(*resp));
	if(!tlv) {
		SYSLOG(LOG_ERROR, "QMI_CTL_GET_VERSION_INFO_RESP_TYPE_SERVICE_VERSION "
               "not found in QMI_CTL_GET_VERSION_INFO");
		goto err;
	}

	/* get resp and versions */
	resp = (struct qmi_ctl_get_version_info_resp_svc_ver *)tlv->v;
	resp_sub = (struct qmi_ctl_get_version_info_resp_svc_ver_sub *)(resp + 1);

	for(i = 0; i < resp->service_version_list_len; i++) {
        if(resp_sub[i].qmi_svc_type < QMIUNICLIENT_SERVICE_CLIENT) {
            qmi_svc_ver[resp_sub[i].qmi_svc_type].major = resp_sub[i].major_ver;
            qmi_svc_ver[resp_sub[i].qmi_svc_type].minor = resp_sub[i].minor_ver;
        }
	}

	qmimsg_destroy(msg);

    //dump_versions();

	return 0;

err:
	qmimsg_destroy(msg);
	return -1;
}

void dump_versions(void)
{
    int i;
    for(i = 0; i < QMIUNICLIENT_SERVICE_CLIENT; i++) {
        SYSLOG(LOG_INFO, "svc=%02X, ver=%d.%d", i, qmi_svc_ver[i].major,
               qmi_svc_ver[i].minor);
    }
}

/* external functions defined in ezqmi.c */
extern int
_qmi_dms_get_iccid(int wait);

extern int
_qmi_dms_get_imsi(int wait);

extern int
_qmi_dms_check_pin_status(int wait);

extern int
_qmi_dms_control_pin(int pin_id, const char* pin, int action,
                     int* verify_left, int* unblock_left);

extern int
_qmi_dms_change_pin(int pin_id, const char* pin, const char* newpin,
                    int* verify_left, int* unblock_left);

extern int
_qmi_dms_verify_puk(int pin_id, const char* puk, const char* pin,
                    int* verify_left, int* unblock_left);

/* external functions defined in ezqmi_uim.c */
extern int
_qmi_uim_get_iccid(int wait);

extern int
_qmi_uim_get_imsi(int wait);

extern int
_qmi_uim_check_pin_status(int wait);

extern int
_qmi_uim_control_pin(int pin_id, const char* pin, int action,
                     int* verify_left, int* unblock_left);

extern int
_qmi_uim_change_pin(int pin_id, const char* pin, const char* newpin,
                    int* verify_left, int* unblock_left);

extern int
_qmi_uim_verify_puk(int pin_id, const char* puk, const char* pin,
                    int* verify_left, int* unblock_left);

/* global function pointers. initilized to dms version */
int (* _qmi_get_iccid)(int) = _qmi_dms_get_iccid;
int (* _qmi_get_imsi)(int) = _qmi_dms_get_imsi;
int (* _qmi_check_pin_status)(int) = _qmi_dms_check_pin_status;
int (* _qmi_control_pin)(int, const char *, int, int *, int *) =
    _qmi_dms_control_pin;
int (* _qmi_change_pin)(int, const char *, const char *, int *, int *) =
    _qmi_dms_change_pin;
int (* _qmi_verify_puk)(int, const char *, const char *, int *, int *) =
    _qmi_dms_verify_puk;

static void _qmi_use_uim(int uim_dms)
{
    if(uim_dms) {
        _qmi_get_iccid = _qmi_uim_get_iccid;
        _qmi_get_imsi = _qmi_uim_get_imsi;
        _qmi_check_pin_status = _qmi_uim_check_pin_status;
        _qmi_control_pin = _qmi_uim_control_pin;
        _qmi_change_pin = _qmi_uim_change_pin;
        _qmi_verify_puk = _qmi_uim_verify_puk;
    } else {
        _qmi_get_iccid = _qmi_dms_get_iccid;
        _qmi_get_imsi = _qmi_dms_get_imsi;
        _qmi_check_pin_status = _qmi_dms_check_pin_status;
        _qmi_control_pin = _qmi_dms_control_pin;
        _qmi_change_pin = _qmi_dms_change_pin;
        _qmi_verify_puk = _qmi_dms_verify_puk;
    }
}

void _batch_cmd_stop_gps_on_dummy()
{
        SYSLOG(LOG_ERR, "ERROR: Wireless Module does not support GPS service on QMI");
}

int _batch_cmd_start_gps_on_dummy(int agps)
{
        SYSLOG(LOG_ERR, "ERROR: Wireless Module does not support GPS service on QMI");
        return 0;
}

void (* _batch_cmd_stop_gps)(void) = _batch_cmd_stop_gps_on_dummy;
int (* _batch_cmd_start_gps)(int) = _batch_cmd_start_gps_on_dummy;
static void _qmi_use_gps(unsigned short major)
{
    if (major >= 2) {
        _batch_cmd_stop_gps = _batch_cmd_stop_gps_on_loc;
        _batch_cmd_start_gps = _batch_cmd_start_gps_on_loc;
    } else if (major >= 1) {
        _batch_cmd_stop_gps = _batch_cmd_stop_gps_on_pds;
        _batch_cmd_start_gps = _batch_cmd_start_gps_on_pds;
    }

}
/* check if qmi service versions are compatible */
int _qmi_check_versions(void)
{
    unsigned short major, minor;
    unsigned short rmajor, rminor;

    if(_qmi_get_service_versions()) {
        SYSLOG(LOG_WARNING, "Failed to get service versions. "
               "This might be a very old device. You are warned.");
        return 0;
    }

    if(is_enabled_feature(FEATUREHASH_CMD_BANDSEL)) {
        // bandsel needs DMS and NAS commands
        major = qmi_svc_ver[QMIDMS].major;
        minor = qmi_svc_ver[QMIDMS].minor;
        // QMI_DMS_GET_BAND_CAPABILITY was introduced in v1.3
        rmajor = 1, rminor = 3;
        if(!major && !minor) {
            SYSLOG(LOG_WARNING, "QMIDMS version is unknown. v%d.%d is required",
                   rmajor, rminor);
        } else if(major != rmajor || minor < rminor) {
            SYSLOG(LOG_ERROR, "QMIDMS v%d.%d is not compatible with the "
                   "required v%d.%d", major, minor, rmajor, rminor);
            return -1;
        }
        SYSLOG(LOG_INFO, "QMIDMS v%d.%d is OK. v%d.%d is required",
               major, minor, rmajor, rminor);

        major = qmi_svc_ver[QMINAS].major;
        minor = qmi_svc_ver[QMINAS].minor;
        // QMI_NAS_GET_SYSTEM_SELECTION_PREFERENCE was introduced in v1.1
        rmajor = 1, rminor = 1;
        if(!major && !minor) {
            SYSLOG(LOG_WARNING, "QMINAS version is unknown. v%d.%d is required",
                   rmajor, rminor);
        } else if(major != rmajor || minor < rminor) {
            SYSLOG(LOG_ERROR, "QMINAS v%d.%d is not compatible with the "
                   "required v%d.%d", major, minor, rmajor, rminor);
            return -1;
        }
        SYSLOG(LOG_INFO, "QMINAS v%d.%d is OK. v%d.%d is required",
               major, minor, rmajor, rminor);
    }

    /*
     * Two services might implement UIM functionality: DMS and UIM.
     * Although Qualcomm deprecated DMS for UIM usage, there are many modules
     * that have not had a separate UIM service yet.
     * So we check UIM service version number and set the function pointers to
     * the new UIM service or legacy DMS-UIM.
     */
    if(is_enabled_feature(FEATUREHASH_CMD_SIMCARD)) {
        // QMI_UIM
        major = qmi_svc_ver[QMIUIM].major;
        minor = qmi_svc_ver[QMIUIM].minor;
        // QMI_UIM_* was introduced in v1.0
        rmajor = 1, rminor = 0;
        if(!major && !minor) {
            _qmi_use_uim(0);
            SYSLOG(LOG_WARNING, "QMIUIM version is unknown. Use QMIDMS");
        } else if(major != rmajor || minor < rminor) {
            _qmi_use_uim(0);
            SYSLOG(LOG_WARNING, "QMIUIM v%d.%d is not compatible with the "
                   "required v%d.%d. Use QMIDMS", major, minor, rmajor, rminor);
        } else {
            _qmi_use_uim(1);
            SYSLOG(LOG_INFO, "QMIUIM v%d.%d is OK. v%d.%d is required",
                   major, minor, rmajor, rminor);
        }
    }

    if(is_enabled_feature(FEATUREHASH_CMD_GPS)) {
        // QMI_UIM
        major = qmi_svc_ver[QMILOC].major > qmi_svc_ver[QMIPDS].major ? qmi_svc_ver[QMILOC].major : qmi_svc_ver[QMIPDS].major;
        minor = qmi_svc_ver[QMILOC].major > qmi_svc_ver[QMIPDS].major ? qmi_svc_ver[QMILOC].minor : qmi_svc_ver[QMIPDS].minor;

        SYSLOG(LOG_INFO, "QMI GPS Service version: %d.%d", major, minor);
        _qmi_use_gps(major);
    }

    return 0;
}
