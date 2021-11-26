/*
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "qmiclient.hpp"

#include <syslog.h>

#include <qmi_client.h>

#include <elogger.hpp>
#include <estd.hpp>
#include <estdexcept.hpp>
#include <inet.hpp>

namespace eqmi
{
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void QcMapClient::invokeIndicationCallback(qmi_client_type user_handle, unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len,
                                           void *ind_cb_data)
{
    log(LOG_DEBUG, "QcMapClient::invokeIndicationCallback enter (ind_cb_data=%p)", ind_cb_data);
    auto &qcMapClient = *static_cast<QcMapClient *>(ind_cb_data);

    log(LOG_DEBUG, "QcMapClient::invokeIndicationCallback is called (qcMapClient=%p)", &qcMapClient);

    auto &indicationCallback = qcMapClient.indicationCallback;

    log(LOG_DEBUG, "checking invokeIndicationCallback (indicationCallback=%p)", &indicationCallback);
    if (indicationCallback) {
        log(LOG_DEBUG, "call invokeIndicationCallback");
        indicationCallback(user_handle, msg_id, ind_buf, ind_buf_len);
    }
}

void QcMapClient::EnableMobileAP()
{
    qmi_error_type_v01 qmiError;

    bool succ = QCMAP_Client::EnableMobileAP(&qmiError);

    if (!succ && (qmiError != QMI_ERR_NO_EFFECT_V01)) {
        throw runtime_error(estd::format("failed in EnableMobileAP (error=%s)", STR_RES_QMI_ERR(qmiError)));
    }
}

void QcMapClient::RegisterForIndications(uint64_t indRegMask)
{
    qmi_error_type_v01 qmiError;

    bool succ = QCMAP_Client::RegisterForIndications(&qmiError, indRegMask);

    if (!succ && (qmiError != QMI_ERR_NO_EFFECT_V01)) {
        throw runtime_error(estd::format("failed in RegisterForIndications (error=%s)", STR_RES_QMI_ERR(qmiError)));
    }
}

void QcMapClient::logLanConfig(estd::StringView desc, int vlanId, const qcmap_msgr_lan_config_v01 &lanConf)
{
    log(LOG_DEBUG,
        "%s\n"
        "vlanId=%d\n"
        "gw_ip=%s\n"
        "netmask=%s\n"
        "enable_dhcp=%d\n"
        "dhcp_start_ip=%s\n"
        "dhcp_end_ip=%s\n"
        "lease=%d",
        desc.c_str(), vlanId, estd::inet_ntop<AF_INET>(estd::hton(lanConf.gw_ip)).c_str(),
        estd::inet_ntop<AF_INET>(estd::hton(lanConf.netmask)).c_str(), lanConf.enable_dhcp,
        estd::inet_ntop<AF_INET>(estd::hton(lanConf.dhcp_config.dhcp_start_ip)).c_str(),
        estd::inet_ntop<AF_INET>(estd::hton(lanConf.dhcp_config.dhcp_end_ip)).c_str(), lanConf.dhcp_config.lease_time);
}

qcmap_msgr_lan_config_v01 QcMapClient::getLanConfig(int vlanId)
{
    // Note: lan bridge must exist, or this will throw exception.
    // Use SetVLANConfig to create the lan bridge
    call(&QCMAP_Client::SelectLANBridge, vlanId);

    qcmap_msgr_lan_config_v01 lanConf;
    call(&QCMAP_Client::GetLANConfig, &lanConf);

    logLanConfig("* existing Lan Settings", vlanId, lanConf);

    return lanConf;
}

void QcMapClient::setLanConfig(int vlanId, const qcmap_msgr_lan_config_v01 &lanConf)
{
    log(LOG_DEBUG, "apply new LAN/DHCP settings (vlanId=%d)", vlanId);
    call(&QCMAP_Client::SelectLANBridge, vlanId);
    call(&QCMAP_Client::SetLANConfig, lanConf);

    logLanConfig("* new Lan Settings", vlanId, lanConf);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QmiClientWds::QmiClientWds() : QmiClientBase(), msgIds()
{
    qmi_client_error_type qmi_error;
    qmi_idl_service_object_type wds_qmi_idl_service_object;

    wds_qmi_idl_service_object = wds_get_service_object_v01();
    if (wds_qmi_idl_service_object == NULL) {
        throw runtime_error("QCMAP service object(wds) not available.");
    }

    auto wdsInd = [](qmi_client_type userHandle, unsigned int msgId, void *indBuf, unsigned int indBufLen, void *indCbData) {
        // TODO: add wdsInd
    };

    void *indCbData = NULL;

    try {
        qmi_error = qmi_client_init_instance(wds_qmi_idl_service_object, QMI_CLIENT_INSTANCE_ANY, wdsInd, indCbData, &qmiOsParams, defaultTimeOut,
                                             &clientHandle);

        if (qmi_error != QMI_NO_ERR) {
            throw runtime_error(estd::format("Failed to init wds qmi client - %s", STR_RES_ERR(qmi_error)));
        }
    }
    catch (...) {
        if (clientHandle != nullptr) {
            qmi_client_release(clientHandle);
        }
        throw;
    }
};

QmiClientWds::~QmiClientWds()
{
    if (clientHandle != nullptr) {
        qmi_client_release(clientHandle);
    }
}
} // namespace eqmi
