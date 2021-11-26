#ifndef __RDBQCMAPBRIDGE_HPP__
#define __RDBQCMAPBRIDGE_HPP__

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

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <deque>
#include <map>

#include <qmi_client.h>
#include <qualcomm_mobile_access_point_msgr_v01.h>
#include <wireless_data_service_v01.h>

#include <erdb.hpp>
#include <estring.hpp>
#include <inet.hpp>

#include "qmiclient.hpp"
#include "qmistruct.hpp"
#include "qmitraits.hpp"
#include "serialization.hpp"

namespace eqmi
{

/**
 * @brief bridge between RDB and QCMAP
 *
 */
class RdbQcMapBridge : public estd::Singleton<RdbQcMapBridge>
{
  public:
    void run();
    void terminate();

    RdbQcMapBridge();

  protected:
    typedef std::function<void(const qmi_client_type userHandle, unsigned int msgId, const void *indBuf, unsigned int indBufLen)>
        indication_handler_t;

    template <typename T>
    using qcmap_type_specific_indication_handler_t = typename std::function<void(RdbQcMapBridge *, const T &)>;

    template <int msgId>
    using indType = typename RawQmiMessageType<QcMapRawQmiMessageType, msgId>::indType;

    template <int msgId>
    void hookQcMapIndicationHandler(qcmap_type_specific_indication_handler_t<indType<msgId>> indicationHandler)
    {

        auto indicationBinder = [this, indicationHandler = std::move(indicationHandler)](const qmi_client_type userHandle, unsigned int msgId_,
                                                                                         const void *indBuf, unsigned int indBufLen) mutable -> void {
            indType<msgId> msg;

            qmi_client_error_type qmi_error =
                ::qmi_client_message_decode(userHandle, QMI_IDL_INDICATION, msgId_, indBuf, indBufLen, &msg, sizeof(msg));

            if (qmi_error != QMI_NO_ERR) {
                log(LOG_ERR, "cannot decode message (msgId=%s,T='%s')", qcMap->getMsgIdStr(msgId_).c_str(), estd::demangleType(msg).c_str());
                return;
            }

            log(LOG_DEBUG, "indication decoded (msgId=%s,T='%s',len=%d)", qcMap->getMsgIdStr(msgId_).c_str(), estd::demangleType(msg).c_str(),
                sizeof(msg));
            log(LOG_DEBUG, estd::dumpHex(reinterpret_cast<const char *>(&msg), sizeof(msg)).c_str());

            indicationHandler(this, msg);
        };

        auto [it, succ] = indicationHandlerMap.insert({ msgId, std::move(indicationBinder) });
        (void)it;
        assert(succ && "failed to add indication handler");
    }

    void initIndicationHandlerMap();
    void onRawQcMapIndication(const qmi_client_type userHandle, unsigned int msgId, const void *indBuf, unsigned int indBufLen);

    void onQcMapIndicationWwanStatus(const qcmap_msgr_wwan_status_ind_msg_v01 &msg);
    void onQcMapIndicationBackhaulStatus(const qcmap_msgr_backhaul_status_ind_msg_v01 &msg);
    void onQcMapIndicationBringUpWwan(const qcmap_msgr_bring_up_wwan_ind_msg_v01 &msg);
    void onQcMapIndicationTearDownWwan(const qcmap_msgr_tear_down_wwan_ind_msg_v01 &msg);
    void onQcMapIndicationPacketStatsStatus(const qcmap_msgr_packet_stats_status_ind_msg_v01 &msg);

    void configModemPolicySettings();
    void initPolicyRdbs();
    void subscribePolicyRdbs();

    void initProfile0Rdb();
    void initVlanRdb();
    void initIpPassthrough();

    template <typename ConfigType>
    void initGenericListRdb(erdb::Rdb &rdbCollection);

    template <typename ConfigType>
    void initDnatRdb(erdb::Rdb &rdbCollection);

    void initWanFirewallSettings(void);
    void initWanFirewallRdb(erdb::Rdb &rdbCollection);

    void doRdbExamples();
    void doQcMapTest();

    bool enablePolicy(int policyNo, bool policyEnable);
    bool triggerConnectPolicy(int policyNo, bool policyTriggerEnable);
    void updatePolicyStatus(int policyNo);
    void updateAllPoliciesStatus();
    void getPolicyStats(erdb::Rdb &policy);

    void applyLanConfig(int vlanId, const erdb::Rdb *rdbAddress, const erdb::Rdb *rdbNetmask, const erdb::Rdb *rdbEnable, const erdb::Rdb *rdbRange,
                        const erdb::Rdb *rdbLease);

    void poll();

    bool setMtuForPolicy(int policyIndex);
    void subscribeProfileMtu();

  private:
    typedef std::map<int, indication_handler_t> indication_handler_map_t;
    indication_handler_map_t indicationHandlerMap;

    bool running = true;

    std::shared_ptr<QcMapClient> qcMap;
    QmiClientWds wds;

    unsigned int pollSecs;

    static constexpr int numberOfProfiles = 6;
    static constexpr const char *localNwInterface = "eth0";

    using wwan_stats_bytes_t = decltype(qcmap_msgr_wwan_statistics_type_v01::bytes_rx);

    struct PolicyStats
    {
        wwan_stats_bytes_t rx_bytes;
        wwan_stats_bytes_t tx_bytes;
        wwan_stats_bytes_t max_rx_kbps;
        wwan_stats_bytes_t max_tx_kbps;
    };
    std::map<int, PolicyStats> policyStatsMap;

    // If wwan.0.modem_initial_connect_workaround is enabled, this re-triggers the policy after it
    // is disconnected during the workaround
    bool modemInitialConnectWorkaroundTrigger = false;

    //
    // NOTE:
    //
    // The mutex lock must be acquired to access the following children of rdbPolicy since the RDBs are written by QCCI thread context
    //
    //     connect_progress_v4
    //     connect_progress_v6
    //     dns1
    //     dns2
    //     ifdev
    //     iplocal
    //     ipv6_dns1
    //     ipv6_dns2
    //     ipv6_ipaddr
    //     pdp_result
    //     status_ipv4
    //     status_ipv6
    //     sysuptime_at_ifdev_up
    //     sysuptime_at_ipv4_up
    //     sysuptime_at_ipv6_up
    //
    erdb::Rdb rdbPolicies = erdb::Rdb("link.policy", PERSIST, DEFAULT_PERM, true);

    erdb::Rdb rdbProfile0 = erdb::Rdb("link.profile.0", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbProfiles = erdb::Rdb("link.profile", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbDhcp = erdb::Rdb("service.dhcp", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbIpPassthrough = erdb::Rdb("service.ip_handover", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbDhcpResCollection = erdb::Rdb("service.dhcp.static", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbVlanCollection = erdb::Rdb("vlan", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbDnatIpv4Collection = erdb::Rdb("service.firewall.dnat", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbDnatIpv6Collection = erdb::Rdb("service.firewall.dnat_ipv6", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbSysUptimeRegStart = erdb::Rdb("wwan.0.system_network_status.sysuptime_at_reg_start", 0, DEFAULT_PERM);
    erdb::Rdb rdbWanFirewallSettings = erdb::Rdb("service.firewall.wan", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbWanIpv4FirewallCollection = erdb::Rdb("service.firewall.wan.ipv4", PERSIST, DEFAULT_PERM);
    erdb::Rdb rdbWanIpv6FirewallCollection = erdb::Rdb("service.firewall.wan.ipv6", PERSIST, DEFAULT_PERM);

    // rdb values used for the modem initial connect workaround
    erdb::Rdb rdbModemInitialConnectWorkaround = erdb::Rdb("wwan.0.modem_initial_connect_workaround");
    erdb::Rdb rdbModemInitialConnectWorkaroundDone = erdb::Rdb("wwan.0.modem_initial_connect_workaround.done");

    // Used to hold the rdbs that trigger the ip passthrough callback.
    // Note: a container is needed that doesn't invalidate references as more are added
    std::deque<erdb::Rdb> rdbIpPassthroughTriggerList = {};

    estd::DefValueMap<wds_pdp_type_enum_v01, qcmap_msgr_ip_family_enum_v01> pdpTypeToQcMapIpFamilyMap = {
        { WDS_PDP_TYPE_PDP_IPV4_V01, QCMAP_MSGR_IP_FAMILY_V4_V01 },
        { WDS_PDP_TYPE_PDP_IPV6_V01, QCMAP_MSGR_IP_FAMILY_V6_V01 },
        { WDS_PDP_TYPE_PDP_IPV4V6_V01, QCMAP_MSGR_IP_FAMILY_V4V6_V01 },
    };

    estd::DefValueMap<wds_pdp_type_enum_v01, qcmap_msgr_wwan_call_type_v01> pdpTypeToQcMapCallType = {
        { WDS_PDP_TYPE_PDP_IPV4_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V4_V01 },
        { WDS_PDP_TYPE_PDP_IPV6_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V6_V01 },
        { WDS_PDP_TYPE_PDP_IPV4V6_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V4V6_V01 },
    };

    estd::DefValueMap<wds_profile_pdn_type_enum_v01, qcmap_msgr_wwan_call_type_v01> profilePdpTypeToQcMapCallType = {
        { WDS_PROFILE_PDN_TYPE_IPV4_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V4_V01 },
        { WDS_PROFILE_PDN_TYPE_IPV6_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V6_V01 },
        { WDS_PROFILE_PDN_TYPE_IPV4_IPV6_V01, QCMAP_MSGR_WWAN_CALL_TYPE_V4V6_V01 },
    };

    estd::DefValueMap<qcmap_msgr_ip_family_enum_v01, estd::StringView> qcMapIpFamilyToStrMap = {
        { QCMAP_MSGR_IP_FAMILY_V4_V01, "ipv4" },
        { QCMAP_MSGR_IP_FAMILY_V6_V01, "ipv6" },
        { QCMAP_MSGR_IP_FAMILY_V4V6_V01, "ipv4v6" },
    };

    estd::DefValueMap<std::string, qcmap_msgr_wwan_call_type_v01> strToQcMapCallTypeMap = {
        { "ipv4", QCMAP_MSGR_WWAN_CALL_TYPE_V4_V01 },
        { "ipv6", QCMAP_MSGR_WWAN_CALL_TYPE_V6_V01 },
        { "ipv4v6", QCMAP_MSGR_WWAN_CALL_TYPE_V4V6_V01 },
    };

    estd::DefValueMap<qcmap_msgr_wwan_status_enum_v01, estd::StringView> qcMapWwanStatusToStrMap = {
        { QCMAP_MSGR_WWAN_STATUS_CONNECTING_V01, "connecting" },
        { QCMAP_MSGR_WWAN_STATUS_CONNECTING_FAIL_V01, "connecting fail" },
        { QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01, "connected" },
        { QCMAP_MSGR_WWAN_STATUS_DISCONNECTING_V01, "disconnecting" },
        { QCMAP_MSGR_WWAN_STATUS_DISCONNECTING_FAIL_V01, "disconnecting fail" },
        { QCMAP_MSGR_WWAN_STATUS_DISCONNECTED_V01, "disconnected" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTING_V01, "connecting" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTING_FAIL_V01, "connecting fail" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01, "connected" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_DISCONNECTING_V01, "disconnecting" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_DISCONNECTING_FAIL_V01, "disconnecting fail" },
        { QCMAP_MSGR_WWAN_STATUS_IPV6_DISCONNECTED_V01, "disconnected" },
    };

    std::tuple<estd::StringView, estd::StringView> upDownTuple = { "up", "down" };
};
} // namespace eqmi
#endif
