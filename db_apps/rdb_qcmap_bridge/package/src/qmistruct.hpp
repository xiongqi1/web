#ifndef __QMISTRUCT_HPP__
#define __QMISTRUCT_HPP__
/*
 * RDB QCMAP bridge
 *
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

#include <qmi_client.h>
#include <qualcomm_mobile_access_point_msgr_v01.h>
#include <wireless_data_service_v01.h>

#include "setoperation.hpp"

namespace eqmi
{

// TODO: generate these specialized templates from Qualcomm header files

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, int msgId>
struct RawQmiMessageType
{};

struct WdsRawQmiMessageType
{};

struct QcMapRawQmiMessageType
{};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <>
struct RawQmiMessageType<WdsRawQmiMessageType, QMI_WDS_GET_PROFILE_LIST_REQ_V01>
{
    typedef wds_get_profile_list_req_msg_v01 reqType;
    typedef wds_get_profile_list_resp_msg_v01 respType;
};

template <>
struct RawQmiMessageType<WdsRawQmiMessageType, QMI_WDS_GET_PROFILE_SETTINGS_REQ_V01>
{
    typedef wds_get_profile_settings_req_msg_v01 reqType;
    typedef wds_get_profile_settings_resp_msg_v01 respType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_CREATE_WWAN_POLICY_REQ_V01>
{
    typedef qcmap_msgr_create_wwan_policy_req_msg_v01 reqType;
    typedef qcmap_msgr_create_wwan_policy_resp_msg_v01 respType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_WWAN_STATUS_IND_V01>
{
    typedef qcmap_msgr_wwan_status_ind_msg_v01 indType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_BACKHAUL_STATUS_IND_V01>
{
    typedef qcmap_msgr_backhaul_status_ind_msg_v01 indType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_BRING_UP_WWAN_REQ_V01>
{
    typedef qcmap_msgr_bring_up_wwan_req_msg_v01 reqType;
    typedef qcmap_msgr_bring_up_wwan_resp_msg_v01 respType;
    typedef qcmap_msgr_bring_up_wwan_ind_msg_v01 indType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_TEAR_DOWN_WWAN_REQ_V01>
{
    typedef qcmap_msgr_tear_down_wwan_req_msg_v01 reqType;
    typedef qcmap_msgr_tear_down_wwan_resp_msg_v01 respType;
    typedef qcmap_msgr_tear_down_wwan_ind_msg_v01 indType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_PACKET_STATS_STATUS_IND_V01>
{
    typedef qcmap_msgr_packet_stats_status_ind_msg_v01 indType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_GET_WWAN_IFACE_NAME_REQ_V01>
{
    typedef qcmap_msgr_get_wwan_iface_name_req_msg_v01 reqType;
    typedef qcmap_msgr_get_wwan_iface_name_resp_msg_v01 respType;
};

template <>
struct RawQmiMessageType<QcMapRawQmiMessageType, QMI_QCMAP_MSGR_GET_PDN_TO_VLAN_MAPPINGS_REQ_V01>
{
    typedef qcmap_msgr_get_pdn_to_vlan_mappings_req_msg_v01 reqType;
    typedef qcmap_msgr_get_pdn_to_vlan_mappings_resp_msg_v01 respType;
};

} // namespace eqmi
#endif
