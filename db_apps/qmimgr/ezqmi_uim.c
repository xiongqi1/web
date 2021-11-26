/*
 * QMI user identify module service.
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

#include "ezqmi_uim.h"
#include "qmidef.h"
#include "ezqmi.h"
#include "featurehash.h"

/* These are defined in ezqmi.c */
extern int _simcard_pin_enabled;
extern int _set_idx_db(const char * dbvar, unsigned long long idx);
extern int _set_int_db(const char * dbvar, int dbval, const char * suffix);
extern int _set_reset_db(const char * dbvar);
extern const char *
_get_indexed_str(const char * str, int idx, const char * suffix);
extern int
_qmi_uim_queue(struct qmimsg_t * msg, unsigned short msg_id,
               unsigned short * tran_id, unsigned char t, unsigned short l,
               void * v, int clear);

/* local functions */
static void _reset_legacy_sim_status(void)
{
    _set_str_db("sim.status.status", "SIM not inserted", -1);
    _set_reset_db("sim.status.pin_enabled");
    _set_reset_db("sim.cmd.param.verify_left");
    _set_reset_db("sim.cmd.param.unlock_left");
}

static void _set_legacy_sim_status(unsigned char pin_state,
                                   unsigned char pin_retries,
                                   unsigned char puk_retries)
{
    const char * pin_enabled;
    _set_idx_db("sim.status.status",
                RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS,
                            (1<<16) | pin_state));
    if(pin_state == 1 || pin_state == 2) {
        pin_enabled = "Enabled";
    } else if(pin_state == 3) {
        pin_enabled = "Disabled";
    } else {
        pin_enabled = "";
    }
    _set_str_db("sim.status.pin_enabled", pin_enabled, -1);
    _simcard_pin_enabled = (pin_state == 1 || pin_state == 2);
    _set_int_db("sim.cmd.param.verify_left", pin_retries, NULL);
    _set_int_db("sim.cmd.param.unlock_left", puk_retries, NULL);
    _set_int_db("sim.status.retries_remaining", pin_retries, NULL);
    _set_int_db("sim.status.retries_puk_remaining", puk_retries, NULL);
}

static void _reset_indexed_sim_status(int idx)
{
    _set_reset_db(_get_indexed_str("sim.status.status", idx, NULL));
    _set_reset_db(_get_indexed_str("sim.status.pin_enabled", idx, NULL));
    _set_reset_db(_get_indexed_str("sim.cmd.param.verify_left", idx, NULL));
    _set_reset_db(_get_indexed_str("sim.cmd.param.unlock_left", idx, NULL));
}

static void _set_indexed_sim_status(int idx,
                                    unsigned char pin_state,
                                    unsigned char pin_retries,
                                    unsigned char puk_retries)
{
    const char * dbvar;
    const char * pin_enabled;
    SYSLOG(LOG_DEBUG, "pin%d state=%d, pin_retries=%d, puk_retries=%d",
           idx, pin_state, pin_retries, puk_retries);
    dbvar = _get_indexed_str("sim.status.status", idx, NULL);
    _set_idx_db(dbvar, RES_STR_KEY(QMIDMS, QMI_DMS_UIM_GET_PIN_STATUS,
                                   pin_state));
    if(pin_state == 1 || pin_state == 2) {
        pin_enabled = "Enabled";
    } else if(pin_state == 3) {
        pin_enabled = "Disabled";
    } else {
        pin_enabled = "";
    }
    dbvar = _get_indexed_str("sim.status.pin_enabled", idx, NULL);
    _set_str_db(dbvar, pin_enabled, -1);
    dbvar = _get_indexed_str("sim.cmd.param.verify_left", idx, NULL);
    _set_int_db(dbvar, pin_retries, NULL);
    dbvar = _get_indexed_str("sim.cmd.param.unblock_left", idx, NULL);
    _set_int_db(dbvar, puk_retries, NULL);
}

/*
 * Convert BCD encoded bytes to hex string.
 * Params:
 *  bcd: BCD encoded bytes
 *  len: number of bytes in bcd
 *  buf: output buffer
 *  cap: capacity of buf (including terminating nil)
 * Return:
 *  0 if success; the required buffer length - 1 if buffer is too short
 */
static int bcd_to_string(unsigned char * bcd, int len, char * buf, int cap)
{
    int i;
    if(cap - 1 < len * 2) {
        return len * 2;
    }
    for(i = 0; i < len; i++) {
        sprintf(buf++, "%x", bcd[i] & 0x0f);
        sprintf(buf++, "%x", (bcd[i] >> 4) & 0x0f);
    }
    return 0;
}

/* global functions */

/* read transparent file in the card
 * path must contain the complete path of the file (LSB first).
 * file_id is of type uint16.
 * e.g. ICCID: MF 3f00 -> EF 2fe2. cf. ETSI TS 100 977
 *          path[] = {0x00, 0x3f}, path_len = 2, file_id = 0x2fe2
 *      IMSI: MF 3f00 -> DF 7fff -> EF 6f07.
 *          path[] = {0x00, 0x3f, 0xff, 0x7f}, path_len = 4, file_id = 0x6f07
 * name should be a unique name for the file, e.g. iccid, imsi
 */
int _qmi_uim_read_transparent(unsigned char path[], int path_len,
                              unsigned short file_id, const char * name,
                              int wait)
{
    struct qmimsg_t * msg;
    unsigned short tran_id;
    struct qmi_uim_read_transparent_req_file_id * fileid = NULL;
    struct qmi_uim_read_transparent_req_read_transparent read = {0, 0};

    if(!(msg = qmimsg_create())) {
        SYSLOG(LOG_ERROR, "failed to allocate msg - QMI_UIM_READ_TRANSPARENT");
        goto err;
    }

    if(!(fileid = _malloc(sizeof(*fileid) + path_len))) {
        SYSLOG(LOG_ERROR, "Failed to allocate tlv file id");
        goto err;
    }

    fileid->file_id = file_id;
    fileid->path_len = path_len;
    memcpy(fileid->path, path, path_len);

    qmimsg_clear_tlv(msg);
    if(qmimsg_add_tlv(msg, QMI_UIM_READ_TRANSPARENT_REQ_TYPE_FILE_ID,
                      sizeof(*fileid) + path_len, fileid) < 0) {
        SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv - "
               "QMI_UIM_READ_TRANSPARENT_REQ_TYPE_FILE_ID");
        goto err;
    }
    if(qmimsg_add_tlv(msg,
                      QMI_UIM_READ_TRANSPARENT_REQ_TYPE_READ_TRANSPARENT,
                      sizeof(read), &read) < 0) {
        SYSLOG(LOG_ERROR, "failed in qmimsg_add_tlv - "
               "QMI_UIM_READ_TRANSPARENT_REQ_TYPE_READ_TRANSPARENT");
        goto err;
    }
    if(_qmi_uim_queue(msg, QMI_UIM_READ_TRANSPARENT, &tran_id,
                      0, 0, NULL, 0) < 0) {
        SYSLOG(LOG_ERROR, "Failed to queue - QMI_UIM_READ_TRANSPARENT");
        goto err;
    }
    SYSLOG(LOG_DEBUG, "QMI_UIM_READ_TRANSPARENT requested");

    // we use a unique string to identify the field to read
    if(qmi_req_footprint_add(tran_id, name, 0)) {
        SYSLOG(LOG_ERROR, "failed to add qmi req footprint");
    }

    // wait for response
    if(wait &&
       _wait_qmi_until(QMIUIM, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
        goto err;
    }

    _free(fileid);
    qmimsg_destroy(msg);
    return 0;

err:
    _free(fileid);
    qmimsg_destroy(msg);
    return -1;
}

int _qmi_uim_get_iccid(int wait)
{
    // ICCID: MF 3f00 -> EF 2fe2. cf. ETSI TS 100 977
    unsigned char path[] = {0x00, 0x3f}; // LSB first
    return _qmi_uim_read_transparent(path, sizeof(path), 0x2fe2, "iccid", wait);
}

int _qmi_uim_get_imsi(int wait)
{
    // ICCID: MF 3f00 -> DF 7fff -> EF 6f07. cf. ETSI TS 100 977 & TS 102 221
    unsigned char path[] = {0x00, 0x3f, 0xff, 0x7f}; // LSB first
    return _qmi_uim_read_transparent(path, sizeof(path), 0x6f07, "imsi", wait);
}

// only initiate request here, while response is handled by callback
int _qmi_uim_check_pin_status(int wait)
{
    struct qmimsg_t* msg;
    unsigned short tran_id;

    SYSLOG(LOG_DEBUG, "checking pin status - QMI_UIM_GET_CARD_STATUS");

    // create msg
    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to create a msg - QMI_UIM_GET_CARD_STATUS");
        goto err;
    }

    if(_request_qmi(msg, QMIUIM, QMI_UIM_GET_CARD_STATUS, &tran_id) < 0) {
        SYSLOG(LOG_ERROR, "qmi request failed - QMI_UIM_GET_CARD_STATUS");
        goto err;
    }

    // wait for response
    if(wait &&
       _wait_qmi_until(QMIUIM, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id, 0) <= 0) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
        goto err;
    }

    qmimsg_destroy(msg);
    return 0;

err:
    qmimsg_destroy(msg);
    return -1;
}

int _qmi_uim_control_pin(int pin_id, const char* pin, int action,
                         int* verify_left, int* unblock_left)
{
    void* v;
    struct qmi_uim_set_pin_protection_req* prot_req = NULL;
    struct qmi_uim_verify_pin_req* verify_req = NULL;
    struct qmimsg_t* msg;
    struct qmimsg_t* rmsg;
    unsigned short tran_id;
    int pin_len;
    const struct qmitlv_t* tlv;

    unsigned short msg_id;
    unsigned char t;
    unsigned short l;

    /*
      action
        0 = disable pin
        1 = enable pin
        2 = verify pin
	*/

    if(verify_left)
        *verify_left = -1;
    if(unblock_left)
        *unblock_left = -1;

    // create msg
    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to create a msg");
        goto err;
    }

    // get pin length
    pin_len = strlen(pin);
    if(pin_len > 255) {
        SYSLOG(LOG_ERROR, "too long pin - pin_len=%d", pin_len);
        goto err;
    }

    if(action == 2) {
        // create prot_req
        verify_req = _malloc(sizeof(*verify_req) + pin_len);
        if(!verify_req) {
            SYSLOG(LOG_ERROR, "failed to allocate verify_req - pin_len=%d",
                   pin_len);
            goto err;
        }

        // build request
        verify_req->pin_id = (unsigned char)(pin_id == 0 ? 1 : 2);
        verify_req->pin_length = (unsigned char)pin_len;
        strncpy((char*)verify_req->pin_value, pin, pin_len);

        // set trans parameters
        msg_id = QMI_UIM_VERIFY_PIN;
        t = QMI_UIM_VERIFY_PIN_REQ_TYPE;
        l = sizeof(*verify_req) + pin_len;
        v = verify_req;
    } else {
        // create prot_req
        prot_req = _malloc(sizeof(*prot_req) + pin_len);
        if(!prot_req) {
            SYSLOG(LOG_ERROR, "failed to allocate prot_req - pin_len=%d",
                   pin_len);
            goto err;
        }

        // build request
        prot_req->pin_id = (unsigned char)(pin_id == 0 ? 1 : 2);
        prot_req->pin_operation = (unsigned char)(action == 0 ? 0 : 1);
        prot_req->pin_length = (unsigned char)pin_len;
        strncpy((char*)prot_req->pin_value, pin, pin_len);

        // set trans parameters
        msg_id = QMI_UIM_SET_PIN_PROTECTION;
        t = QMI_UIM_SET_PIN_PROTECTION_REQ_TYPE;
        l = sizeof(*prot_req) + pin_len;
        v = prot_req;
    }

    unsigned short qmi_result;
    unsigned short qmi_error;

    if(_qmi_uim_queue(msg, msg_id, &tran_id, t, l, v, 1) < 0) {
        goto err;
    }

    // wait for response
    rmsg = _wait_qmi_response(QMIUIM, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id,
                              &qmi_result, &qmi_error, NULL);
    if(!rmsg) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
        goto err;
    }

    // process error
    if(qmi_result) {
        struct qmi_uim_pin_resp* resp;

        tlv = _get_tlv(rmsg, QMI_UIM_SET_PIN_PROTECTION_RESP_TYPE,
                       sizeof(struct qmi_uim_pin_resp));
        if(!tlv) {
            SYSLOG(LOG_ERROR, "mandatory resp type not found");
            goto err;
        }

        resp = (struct qmi_uim_pin_resp*)tlv->v;

        if(verify_left) {
            *verify_left = resp->verify_retries_left;
        }
        if(unblock_left) {
            *unblock_left = resp->unblock_retries_left;
        }
    }

    SYSLOG(LOG_OPERATION, "QMI_UIM_VERIFY_PIN/SET_PIN_PROTECTION_RESP_TYPE "
           "successful");

    _free(prot_req);
    _free(verify_req);

    qmimsg_destroy(msg);
    return 0;

err:
    _free(prot_req);
    _free(verify_req);

    qmimsg_destroy(msg);
    return -1;
}

int _qmi_uim_change_pin(int pin_id, const char* pin, const char* newpin,
                        int* verify_left, int* unblock_left)
{
    struct qmi_uim_change_pin_req * req = NULL;
    struct qmi_uim_change_pin_req2 * req2;
    struct qmimsg_t * msg;
    struct qmimsg_t * rmsg;
    unsigned short tran_id;
    const struct qmitlv_t * tlv;
    unsigned short qmi_result;

    int pin_len;
    int newpin_len;

    unsigned short l;

    if(verify_left) {
        *verify_left = -1;
    }
    if(unblock_left) {
        *unblock_left = -1;
    }

	// create msg
    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to create a msg");
        goto err;
    }

    pin_len = strlen(pin);
    newpin_len = strlen(newpin);

    // get pin length
    if(pin_len > 255 || newpin_len > 255) {
        SYSLOG(LOG_ERROR, "too long pin - pin_len=%d, newpin_len=%d",
               pin_len, newpin_len);
        goto err;
    }

    // create prot_req
    l = sizeof(*req) + pin_len + sizeof(*req2) + newpin_len;
    req = _malloc(l);
    if(!req) {
        SYSLOG(LOG_ERROR, "failed to allocate req - pin_len=%d", pin_len);
        goto err;
    }
    // get req2
    req2 = (struct qmi_uim_change_pin_req2 *)(req->old_pin_value + pin_len);

    // build request
    req->pin_id = (unsigned char)(pin_id == 0 ? 1 : 2);
    req->old_pin_length = (unsigned char)pin_len;
    strncpy((char*)req->old_pin_value, pin, pin_len);
    req2->new_pin_length = (unsigned char)newpin_len;
    strncpy((char*)req2->new_pin_value, newpin, newpin_len);

    if(_qmi_uim_queue(msg, QMI_UIM_CHANGE_PIN, &tran_id,
                      QMI_UIM_CHANGE_PIN_REQ_TYPE, l, req, 1) < 0) {
        goto err;
    }

    // wait for response
    rmsg = _wait_qmi_response(QMIUIM, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id,
                              &qmi_result, NULL, NULL);
    if(!rmsg) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
        goto err;
    }

    // process error
    if(qmi_result) {
        struct qmi_uim_pin_resp * resp;

        tlv = _get_tlv(rmsg, QMI_UIM_CHANGE_PIN_RESP_TYPE, sizeof(*resp));
        if(!tlv) {
            SYSLOG(LOG_ERROR, "mandatory resp type not found");
            goto err;
        }

        resp = (struct qmi_uim_pin_resp*)tlv->v;

        if(verify_left) {
            *verify_left = resp->verify_retries_left;
        }
        if(unblock_left) {
            *unblock_left = resp->unblock_retries_left;
        }
    }

    SYSLOG(LOG_OPERATION, "QMI_UIM_CHANGE_PIN success");
    _free(req);

    qmimsg_destroy(msg);
    return 0;

err:
    _free(req);

    qmimsg_destroy(msg);
    return -1;
}

int _qmi_uim_verify_puk(int pin_id, const char* puk, const char* pin,
                        int* verify_left, int* unblock_left)
{
    struct qmi_uim_unblock_pin_req * req = NULL;
    struct qmi_uim_unblock_pin_req2 * req2;
    struct qmimsg_t * msg;
    struct qmimsg_t * rmsg;
    unsigned short tran_id;
    const struct qmitlv_t * tlv;

    int puk_len;
    int pin_len;

    unsigned short qmi_result;

    unsigned short l;

    if(verify_left) {
        *verify_left = -1;
    }
	if(unblock_left) {
        *unblock_left = -1;
    }

    // create msg
    msg = qmimsg_create();
    if(!msg) {
        SYSLOG(LOG_ERROR, "failed to create a msg");
        goto err;
    }

    puk_len = strlen(puk);
    pin_len = strlen(pin);

    // get pin length
    if(pin_len > 255 || puk_len > 255) {
        SYSLOG(LOG_ERROR, "too long pin - pin_len=%d, puk_len=%d",
               pin_len, puk_len);
        goto err;
    }

    // create prot_req
    l = sizeof(*req) + puk_len + sizeof(*req2) + pin_len;
    req = _malloc(l);
    if(!req) {
        SYSLOG(LOG_ERROR, "failed to allocate req - pin_len=%d", pin_len);
        goto err;
    }
    // get req2
    req2 = (struct qmi_uim_unblock_pin_req2*)(req->puk_value + puk_len);

    // build request
    req->pin_id = (unsigned char)(pin_id == 0 ? 1 : 2);
    req->puk_length = (unsigned char)puk_len;
    strncpy((char*)req->puk_value, puk, puk_len);
    req2->new_pin_length = (unsigned char)pin_len;
    strncpy((char*)req2->new_pin_value, pin, pin_len);

    if(_qmi_uim_queue(msg, QMI_UIM_UNBLOCK_PIN, &tran_id,
                      QMI_UIM_UNBLOCK_PIN_REQ_TYPE, l, req, 1) < 0) {
        goto err;
    }

    // wait for response
    rmsg = _wait_qmi_response(QMIUIM, QMIMGR_GENERIC_RESP_TIMEOUT, tran_id,
                              &qmi_result, NULL, NULL);
    if(!rmsg) {
        SYSLOG(LOG_ERROR, "qmi response timeout (or qmi failure result)");
        goto err;
    }

    // process error
    if(qmi_result) {
        struct qmi_uim_pin_resp* resp;

        tlv = _get_tlv(rmsg, QMI_UIM_UNBLOCK_PIN_RESP_TYPE, sizeof(*resp));
        if(!tlv) {
            SYSLOG(LOG_ERROR, "mandatory resp type not found");
            goto err;
        }

        resp = (struct qmi_uim_pin_resp*)tlv->v;

        if(verify_left) {
            *verify_left = resp->verify_retries_left;
        }
        if(unblock_left) {
            *unblock_left = resp->unblock_retries_left;
        }
    }

    SYSLOG(LOG_OPERATION, "QMI_UIM_CHANGE_PIN success");
    _free(req);

    qmimsg_destroy(msg);
    return 0;

err:
    _free(req);

    qmimsg_destroy(msg);
    return -1;
}

void _reset_imsi_db(void)
{
    _set_reset_db("imsi.plmn_mcc");
    _set_reset_db("imsi.plmn_mnc");
    _set_reset_db("imsi.msin");
}

void qmimgr_callback_on_uim(unsigned char msg_type,struct qmimsg_t* msg,
                            unsigned short qmi_result, unsigned short qmi_error,
                            int noti, unsigned short tran_id)
{
    const struct qmitlv_t* tlv;

    switch(msg->msg_id) {
    case QMI_UIM_READ_TRANSPARENT: {
        void *data;
        int data_len;
        struct qmi_uim_read_transparent_resp_read_result * resp;
        if(!is_enabled_feature(FEATUREHASH_UNSPECIFIED)) {
            SYSLOG(LOG_COMM, "got QMI_UIM_READ_TRANSPARENT - feature not "
                   "enabled (%s)", FEATUREHASH_UNSPECIFIED);
            break;
        }

        SYSLOG(LOG_COMM, "got QMI_UIM_READ_TRANSPARENT");
        tlv = _get_tlv(msg, QMI_UIM_READ_TRANSPARENT_RESP_TYPE_READ_RESULT,
                       sizeof(*resp));

        if(qmi_req_footprint_remove(tran_id, &data, &data_len)) {
            if(data) {
                if(tlv) {
                    resp = (struct qmi_uim_read_transparent_resp_read_result *)tlv->v;
                    if(!strncmp(data, "iccid", data_len)) {
                        char buf[21]; // 10 bytes of bcd coded iccid
                        if(bcd_to_string(resp->content, resp->content_len,
                                         buf, sizeof(buf))) {
                            SYSLOG(LOG_ERROR, "bad ICCID discarded");
                        } else {
                            SYSLOG(LOG_DEBUG, "set rdb simICCID = %s", buf);
                            _set_str_db("system_network_status.simICCID", buf,
                                        resp->content_len * 2);
                        }
                    } else if(!strncmp(data, "imsi", data_len)) {
                        char buf[17]; // 8 bytes of bcd coded imsi
                        int len = resp->content[0];
                        if(len > 8 ||
                           bcd_to_string(resp->content+1, len,
                                         buf, sizeof(buf))) {
                            SYSLOG(LOG_ERROR, "bad IMSI discarded");
                        } else {
                            char mcc[4], mnc[4];
                            char * imsi = buf + 1; // skip the parity byte
                            SYSLOG(LOG_DEBUG, "UIM got IMSI: %s", imsi);
                            if(_get_mcc_mnc(imsi, strlen(imsi), mcc, mnc) < 0) {
                                SYSLOG(LOG_ERROR, "incorrect format of IMSI -"
                                                  " %s", imsi);
                                _reset_imsi_db();
                            } else if(!atoi(mcc) || !atoi(mnc)) {
                                SYSLOG(LOG_ERROR, "incorrect mcc(%s) or mnc(%s)"
                                       " from imsi %s", mcc, mnc, imsi);
                                _reset_imsi_db();
                            } else {
                                _set_str_db("imsi.plmn_mcc", mcc, sizeof(mcc));
                                _set_str_db("imsi.plmn_mnc", mnc, sizeof(mnc));
                                // imsi.msin is the full imsi, who said that?
                                _set_str_db("imsi.msin", imsi, len*2);
                            }
                        }
                    }
                } else {
                    SYSLOG(LOG_ERROR, "failed to get %s: result=%02x, "
                           "error=%02x", data, qmi_result, qmi_error);
                    if(!strncmp(data, "iccid", data_len)) {
                        _set_reset_db("system_network_status.simICCID");
                    } else if(!strncmp(data, "imsi", data_len)) {
                        _reset_imsi_db();
                    }
                }
            }
            _free(data);
        } else {
            SYSLOG(LOG_WARNING, "unmatched request tran_id %x, discarded",
                   tran_id);
        }

        break;
    }

    case QMI_UIM_GET_CARD_STATUS: {
        struct qmi_uim_get_card_status_resp_card_status * resp;
        SYSLOG(LOG_COMM, "got QMI_UIM_GET_CARD_STATUS");

        tlv = _get_tlv(msg, QMI_UIM_GET_CARD_STATUS_RESP_TYPE_CARD_STATUS,
                       sizeof(*resp));
        if(tlv) {
            int i, j;
            int slot_idx, app_idx;
            struct qmi_uim_get_card_status_resp_card_status_slot * resp_slot;
            struct qmi_uim_get_card_status_resp_card_status_app * resp_app;
            struct qmi_uim_get_card_status_resp_card_status_app2 * resp_app2;

            resp = (struct qmi_uim_get_card_status_resp_card_status *)tlv->v;
            slot_idx = (resp->index_gw_pri >> 8) & 0xff; // primary sim
            app_idx = resp->index_gw_pri & 0xff;
            resp_slot = resp->slots;
            for(i = 0; i < resp->num_slot; i++) {
                resp_app = resp_slot->apps;
                for(j = 0; j < resp_slot->num_app; j++) {
                    resp_app2 =
                        (struct qmi_uim_get_card_status_resp_card_status_app2 *)
                          (resp_app->aid_value + resp_app->aid_len);

                    if(i == slot_idx && j == app_idx) {
                        SYSLOG(LOG_DEBUG, "SIM found in slot %d, app %d", i, j);
                        if(resp_slot->card_state != 1) { // SIM not present
                            SYSLOG(LOG_WARNING,
                                   "SIM not present in slot %d, app %d", i, j);
                            _reset_legacy_sim_status();
                            _reset_indexed_sim_status(0);
                            _reset_indexed_sim_status(1);
                            break;
                        }
                        // SIM is present
                        if(resp_app2->univ_pin == 0) { // PIN1 is used
                            _set_legacy_sim_status(resp_app2->pin1_state,
                                                   resp_app2->pin1_retries,
                                                   resp_app2->puk1_retries);
                            _set_indexed_sim_status(0, resp_app2->pin1_state,
                                                    resp_app2->pin1_retries,
                                                    resp_app2->puk1_retries);
                        } else { // UPIN is used for PIN1
                            _set_legacy_sim_status(resp_slot->upin_state,
                                                   resp_slot->upin_retries,
                                                   resp_slot->upuk_retries);
                            _set_indexed_sim_status(0, resp_slot->upin_state,
                                                    resp_slot->upin_retries,
                                                    resp_slot->upuk_retries);
                        }
                        _set_indexed_sim_status(1, resp_app2->pin2_state,
                                                resp_app2->pin2_retries,
                                                resp_app2->puk2_retries);
                        break;
                    }

                    resp_app =
                        (struct qmi_uim_get_card_status_resp_card_status_app *)
                          (resp_app2 + 1);
                }
                if(j < resp_slot->num_app) {
                    break;
                }
                resp_slot =
                    (struct qmi_uim_get_card_status_resp_card_status_slot *)
                      resp_app;
            }
            if(i >= resp->num_slot) {
                SYSLOG(LOG_ERROR, "Failed to find primary SIM @ slot %d, "
                       "app %d", slot_idx, app_idx);
            }
        } else { // failed to get tlv, treat it as SIM not inserted
            SYSLOG(LOG_WARNING, "Failed to get tlv "
                   "QMI_UIM_GET_CARD_STATUS_RESP_TYPE_CARD_STATUS");
            _reset_legacy_sim_status();
            _reset_indexed_sim_status(0);
            _reset_indexed_sim_status(1);
        }

		break;
    }
    }
}

