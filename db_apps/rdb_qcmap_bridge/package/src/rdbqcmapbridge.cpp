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

#include "rdbqcmapbridge.hpp"

#include <arpa/inet.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include <elogger.hpp>
#include <estd.hpp>
#include <inet.hpp>

#include "qmiclient.hpp"
#include "qmitraits.hpp"
#include "serialization.hpp"

#include "setoperation.hpp"

namespace eqmi
{

void RdbQcMapBridge::onQcMapIndicationBringUpWwan(const qcmap_msgr_bring_up_wwan_ind_msg_v01 &msg) {}

void RdbQcMapBridge::updatePolicyStatus(int policyNo)
{
    struct policy_status_t {
        int dummy;

        std::string v4ConnProgStr;
        std::string v6ConnProgStr;

        std::string inAddrStr;
        std::string primaryDnsStr;
        std::string secondaryDnsStr;

        std::string inAddr6Str;
        std::string primaryDns6Str;
        std::string secondaryDns6Str;

        std::string ifDev;

        erdb::RdbValue v4UpStat;
        erdb::RdbValue v6UpStat;

        bool operator==(const policy_status_t &o) const
        {
            return (dummy == o.dummy) && (v4ConnProgStr == o.v4ConnProgStr) && (v6ConnProgStr == o.v6ConnProgStr) && (inAddrStr == o.inAddrStr) &&
                   (primaryDnsStr == o.primaryDnsStr) && (secondaryDnsStr == o.secondaryDnsStr) && (inAddr6Str == o.inAddr6Str) &&
                   (primaryDns6Str == o.primaryDns6Str) && (secondaryDns6Str == o.secondaryDns6Str) && (ifDev == o.ifDev) &&
                   (v4UpStat == o.v4UpStat) && (v6UpStat == o.v6UpStat);
        }
    };

    log(LOG_DEBUG, "update policy #%d status", policyNo);

    // throw exception if failed to select WWAN
    if (!qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyNo)) {
        throw estd::runtime_error(estd::format("Failed to select WWAN policy #%d to read policy. Crashing (systemd may restart this program).", policyNo));
    }

    policy_status_t oldStatus = {
        0,
    };

    int i = 0;
    while (true) {
        policy_status_t curStatus = {
            1,
        };

        log(LOG_DEBUG, "read policy #%d status (attempt=%d)", policyNo, ++i);

        // get current WWAN status
        qcmap_msgr_wwan_status_enum_v01 v4Status;
        qcmap_msgr_wwan_status_enum_v01 v6Status;

        if (qcMap->callSafe(&QcMapClient::GetWWANStatus, &v4Status, &v6Status)) {
            curStatus.v4ConnProgStr = qcMapWwanStatusToStrMap.get(v4Status, estd::BlankStringView);
            curStatus.v6ConnProgStr = qcMapWwanStatusToStrMap.get(v6Status, estd::BlankStringView);

            log(LOG_INFO, "initial wwan status for policy #%d (v4StatusStr=%s,v6StatusStr=%s)", policyNo, curStatus.v4ConnProgStr.c_str(),
                curStatus.v6ConnProgStr.c_str());
        }

        // get interface for ipv4 or ipv6 - ipv4 and ipv6 stack are always using the same interface.
        log(LOG_INFO, "trying to get rmnet interface name for policy #%d", policyNo);
        qcmap_msgr_get_wwan_iface_name_resp_msg_v01 respWwanIfNamev4;
        qcmap_msgr_get_wwan_iface_name_resp_msg_v01 respWwanIfNamev6;
        memset(&respWwanIfNamev4, 0, sizeof(respWwanIfNamev4));
        memset(&respWwanIfNamev6, 0, sizeof(respWwanIfNamev6));
        if (qcMap->callSafe(&QCMAP_Client::GetWWANIFaceName, QCMAP_MSGR_IP_FAMILY_V4_V01, &respWwanIfNamev4) ||
            qcMap->callSafe(&QCMAP_Client::GetWWANIFaceName, QCMAP_MSGR_IP_FAMILY_V6_V01, &respWwanIfNamev6)) {
            if (respWwanIfNamev4.iface_name_valid != 0) {
                log(LOG_DEBUG, "got rmnet interface(%s) for policy #%d", respWwanIfNamev4.iface_name, policyNo);
                curStatus.ifDev = respWwanIfNamev4.iface_name;
            } else if (respWwanIfNamev6.iface_name_valid != 0) {
                log(LOG_DEBUG, "got rmnet interface(%s) for policy #%d", respWwanIfNamev6.iface_name, policyNo);
                curStatus.ifDev = respWwanIfNamev6.iface_name;
            } else {
                log(LOG_DEBUG, "no rmnet interface found for policy #%d", policyNo);
            }
        }

        if (v4Status == QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01) {

            in_addr_t inAddr = 0;
            uint32 primaryDns = 0;
            in_addr_t secondaryDns = 0;

            if (qcMap->callSafe(&QcMapClient::GetIPv4NetworkConfiguration, &inAddr, &primaryDns, &secondaryDns)) {
                // little-endian for this structure of IPv4 only
                auto inAddrN = estd::hton(inAddr);
                curStatus.inAddrStr = estd::inet_ntop<AF_INET>(inAddrN);

                auto primaryDnsN = estd::hton(primaryDns);
                if (!estd::isAny<AF_INET>(primaryDns)) {
                    curStatus.primaryDnsStr = estd::inet_ntop<AF_INET>(primaryDnsN);
                }

                auto secondaryDnsN = estd::hton(secondaryDns);
                if (!estd::isAny<AF_INET>(secondaryDns)) {
                    curStatus.secondaryDnsStr = estd::inet_ntop<AF_INET>(secondaryDnsN);
                }

                log(LOG_DEBUG, "### read ipv4 policy #%d initial\ninAddrN=%s\nprimaryDnsN=%s\nsecondaryDnsN=%s\nifDev=%s", policyNo,
                    estd::inet_ntop<AF_INET>(inAddrN).c_str(), estd::inet_ntop<AF_INET>(primaryDnsN).c_str(),
                    estd::inet_ntop<AF_INET>(secondaryDnsN).c_str(), curStatus.ifDev.c_str());
            }
        }

        if (v6Status == QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01) {

            in6_addr inAddr6 = {};
            in6_addr primaryDns6 = {};
            in6_addr secondaryDns6 = {};

            if (qcMap->callSafe(&QcMapClient::GetIPv6NetworkConfiguration, &inAddr6, &primaryDns6, &secondaryDns6)) {

                curStatus.inAddr6Str = estd::inet_ntop<AF_INET6>(inAddr6);

                if (!estd::isAny<AF_INET6>(primaryDns6)) {
                    curStatus.primaryDns6Str = estd::inet_ntop<AF_INET6>(primaryDns6);
                }

                if (!estd::isAny<AF_INET6>(secondaryDns6)) {
                    curStatus.secondaryDns6Str = estd::inet_ntop<AF_INET6>(secondaryDns6);
                }

                log(LOG_DEBUG, "### read ipv6 policy #%d initial\ninAddr6=%s\nprimaryDns6=%s\nsecondaryDns6=%s\nifDev=%s", policyNo,
                    estd::inet_ntop<AF_INET6>(inAddr6).c_str(), estd::inet_ntop<AF_INET6>(primaryDns6).c_str(),
                    estd::inet_ntop<AF_INET6>(secondaryDns6).c_str(), curStatus.ifDev.c_str());
            }
        }

        curStatus.v4UpStat = erdb::RdbValue(upDownTuple, v4Status == QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01);
        curStatus.v6UpStat = erdb::RdbValue(upDownTuple, (v6Status == QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01));

        if (oldStatus == curStatus) {
            log(LOG_INFO, "### policy #%d successfully updated", policyNo);
            break;
        }

        auto &rdbPolicy = rdbPolicies.children[policyNo];

        {
            erdb::RdbScopeMutex scopeMutex(rdbPolicies);

            rdbPolicy.setChildren({ { "ifdev", curStatus.ifDev },
                                    { "connect_progress_v4", curStatus.v4ConnProgStr },
                                    { "dns1", curStatus.primaryDnsStr },
                                    { "dns2", curStatus.secondaryDnsStr },
                                    { "iplocal", curStatus.inAddrStr },
                                    { "connect_progress_v6", curStatus.v6ConnProgStr },
                                    { "ipv6_dns1", curStatus.primaryDns6Str },
                                    { "ipv6_dns2", curStatus.secondaryDns6Str },
                                    { "ipv6_ipaddr", curStatus.inAddr6Str },
                                    { "status_ipv4", curStatus.v4UpStat },
                                    { "status_ipv6", curStatus.v6UpStat } },
                                  true, true);
        }

        oldStatus = curStatus;
    }
}

void RdbQcMapBridge::onQcMapIndicationWwanStatus(const qcmap_msgr_wwan_status_ind_msg_v01 &msg)
{
    erdb::RdbScopeMutex scopeMutex(rdbPolicies);

    // update policy
    auto policyNo = msg.profile_handle;
    auto &rdbPolicy = rdbPolicies.children[policyNo];

    if (msg.wwan_call_end_reason_valid != 0) {
        auto &endReason = msg.wwan_call_end_reason;
        auto &qmiWdsVerboseErrorCollection = StrRes::getInstance().qmiWdsVerboseError;

        auto it = qmiWdsVerboseErrorCollection.find(endReason.wwan_call_end_reason_type);

        std::string errorCode;
        if (it == qmiWdsVerboseErrorCollection.end()) {
            errorCode = estd::format("[errorType#%d errorCode#%d]", endReason.wwan_call_end_reason_type, endReason.wwan_call_end_reason_code);
        } else {
            auto &qmiWdsVerboseError = *it->second;
            errorCode = estd::format("[errorType#%d] %s", endReason.wwan_call_end_reason_type,
                                     qmiWdsVerboseError[endReason.wwan_call_end_reason_code].c_str());
        }

        rdbPolicy.children["pdp_result"].set(errorCode);
        log(LOG_NOTICE, "profile #%d end (%s)", policyNo, errorCode.c_str());

        // If the error type is 2 and the end reason code is 236, then QCMAP tried to put the IPv6 call on a different
        // network device to the IPv4 call.  If the wwan.0.modem_initial_connect_workaround is enabled trigger it here
        if (rdbModemInitialConnectWorkaround.get().toBool() &&
            !rdbModemInitialConnectWorkaroundDone.get().toBool() &&
            endReason.wwan_call_end_reason_type == QCMAP_MSGR_WWAN_CALL_END_TYPE_INTERNAL_V01 &&
            endReason.wwan_call_end_reason_code == WDS_VCER_INTERNAL_INTERNAL_CALL_ALREADY_PRESENT_V01) {
            //  Trigger the workaround.  At the end of this function the rdb trigger for this policy will be set
            modemInitialConnectWorkaroundTrigger = true;
        }
    }

    auto &wwan_info = msg.wwan_info;

    bool ipv4 = msg.wwan_status < QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTING_V01;
    bool wwan_info_valid = msg.wwan_info_valid;

    // get current system uptime.
    struct sysinfo info;
    long uptime = 0;
    if (sysinfo(&info) == 0) {
        uptime = info.uptime;
    }

    if (ipv4) {
        bool prev_ipv4Up = rdbPolicy.children["status_ipv4"].get(false).toBool(upDownTuple, false);
        if (!prev_ipv4Up && msg.wwan_status == QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01) {
            rdbPolicy.children["sysuptime_at_ipv4_up"].set(uptime);
        } else if (prev_ipv4Up && msg.wwan_status != QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01) {
            rdbPolicy.children["sysuptime_at_ipv4_up"].set("");
        }
        rdbPolicy.children["connect_progress_v4"].set(qcMapWwanStatusToStrMap.get(msg.wwan_status, ""));
        rdbPolicy.children["status_ipv4"].set(erdb::RdbValue(upDownTuple, msg.wwan_status == QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01), false);

        std::string inAddrStr;
        std::string primaryDnsStr;
        std::string secondaryDnsStr;

        if (wwan_info_valid != 0) {
            inAddrStr = estd::inet_ntop<AF_INET>(wwan_info.v4_addr);
            if (!estd::isAny<AF_INET>(wwan_info.v4_prim_dns_addr)) {
                primaryDnsStr = estd::inet_ntop<AF_INET>(wwan_info.v4_prim_dns_addr);
            }

            if (!estd::isAny<AF_INET>(wwan_info.v4_sec_dns_addr)) {
                secondaryDnsStr = estd::inet_ntop<AF_INET>(wwan_info.v4_sec_dns_addr);
            }

            rdbPolicy.children["ifdev"].set(wwan_info.iface_name);
            if (rdbPolicy.children["sysuptime_at_ifdev_up"].get(true).toInt() == 0) {
                rdbPolicy.children["sysuptime_at_ifdev_up"].set(uptime);
            }
        }

        rdbPolicy.setChildren({ { "dns1", primaryDnsStr }, { "dns2", secondaryDnsStr }, { "iplocal", inAddrStr } });

    } else {
        bool prev_ipv6Up = rdbPolicy.children["status_ipv6"].get(false).toBool(upDownTuple, false);
        if (!prev_ipv6Up && msg.wwan_status == QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01) {
            rdbPolicy.children["sysuptime_at_ipv6_up"].set(uptime);
        } else if (prev_ipv6Up && msg.wwan_status != QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01) {
            rdbPolicy.children["sysuptime_at_ipv6_up"].set("");
        }
        rdbPolicy.children["connect_progress_v6"].set(qcMapWwanStatusToStrMap.get(msg.wwan_status, ""));
        rdbPolicy.children["status_ipv6"].set(erdb::RdbValue(upDownTuple, msg.wwan_status == QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01), false);

        std::string inAddr6Str;
        std::string primaryDns6Str;
        std::string secondaryDns6Str;

        if (wwan_info_valid != 0) {
            inAddr6Str = estd::inet_ntop<AF_INET6>(wwan_info.v6_addr);
            if (!estd::isAny<AF_INET6>(wwan_info.v6_prim_dns_addr)) {
                primaryDns6Str = estd::inet_ntop<AF_INET6>(wwan_info.v6_prim_dns_addr);
            }
            if (!estd::isAny<AF_INET6>(wwan_info.v6_sec_dns_addr)) {
                secondaryDns6Str = estd::inet_ntop<AF_INET6>(wwan_info.v6_sec_dns_addr);
            }

            rdbPolicy.children["ifdev"].set(wwan_info.iface_name);
            if (rdbPolicy.children["sysuptime_at_ifdev_up"].get(true).toInt() == 0) {
                rdbPolicy.children["sysuptime_at_ifdev_up"].set(uptime);
            }
        }

        if (msg.wwan_status != QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01) {
            inAddr6Str = "";  // Make sure address is empty if not up
        }

        rdbPolicy.setChildren({ { "ipv6_dns1", primaryDns6Str }, { "ipv6_dns2", secondaryDns6Str } });
        // Only write value if changed
        rdbPolicy.children["ipv6_ipaddr"].set(inAddr6Str, true, true);
    }

    bool ipv4Up = rdbPolicy.children["status_ipv4"].get(false).toBool(upDownTuple, false);
    bool ipv6Up = rdbPolicy.children["status_ipv6"].get(false).toBool(upDownTuple, false);

    log(LOG_DEBUG, "get pdp status (ipv4Up=%d,ipv6Up=%d)", ipv4Up, ipv6Up);
    if (!ipv4Up && !ipv6Up) {
        rdbPolicy.children["ifdev"].set("");
        rdbPolicy.children["sysuptime_at_ifdev_up"].set("");
    } else if (ipv4Up && ipv6Up) {
        rdbPolicy.children["pdp_result"].set("");
    }
    if (ipv4Up || ipv6Up) {
        if (rdbSysUptimeRegStart.get(true).toStdString().empty()) {
            rdbSysUptimeRegStart.set(uptime);
        }
    }

    if (ipv4) {
        if (rdbPolicy.children["status_ipv4"].isChanged()) {
            rdbPolicy.children["status_ipv4"].write();
            if (ipv4Up && !ipv6Up) {
                // trigger a read to check profile settings since they may have
                // changed since bootup (e.g. because of new SIM/MBN)
                rdbProfiles.children[policyNo].children["readflag"].set("1");
            }
        }
        if (ipv4Up) {
            // status_ipv4_up is an edge trigger that ipv4 is up.
            rdbPolicy.children["status_ipv4_up"].set("");
        }
    } else {
        if (rdbPolicy.children["status_ipv6"].isChanged()) {
            rdbPolicy.children["status_ipv6"].write();
            if (ipv6Up && !ipv4Up) {
                // trigger a read to check profile settings since they may have
                // changed since bootup (e.g. because of new SIM/MBN)
                rdbProfiles.children[policyNo].children["readflag"].set("1");
            }
        }
    }

    setMtuForPolicy(policyNo);

    // If wwan.0.modem_initial_connect_workaround is enabled, and then modemInitialConnectWorkaroundTrigger
    // indicates whether the policy should be re-triggered (and the callback run to set auto-connect to true)
    if (modemInitialConnectWorkaroundTrigger) {
        rdbPolicy.children["trigger"].set("");
    }
}

void RdbQcMapBridge::onQcMapIndicationBackhaulStatus(const qcmap_msgr_backhaul_status_ind_msg_v01 &msg) {}

void RdbQcMapBridge::onQcMapIndicationTearDownWwan(const qcmap_msgr_tear_down_wwan_ind_msg_v01 &msg) {}

void RdbQcMapBridge::onRawQcMapIndication(const qmi_client_type userHandle, unsigned int msgId, const void *indBuf, unsigned int indBufLen)
{
    // qcMap may be NULL for a very short time during the constructor
    if (qcMap == nullptr) {
        log(LOG_NOTICE, "indication msgId=%d received too early", msgId);
        return;
    }

    log(LOG_DEBUG, "indication received (userHandle=0x%p,msgId=%s,indBuf=0x%p,indBufLen=%d", userHandle, qcMap->getMsgIdStr(msgId).c_str(), indBuf,
        indBufLen);

    auto it = indicationHandlerMap.find(msgId);

    if (it == indicationHandlerMap.end()) {
        log(LOG_DEBUG, "no indication handler found (msgId=%s)", qcMap->getMsgIdStr(msgId).c_str());
        return;
    }

    log(LOG_DEBUG, estd::dumpHex(static_cast<const char *>(indBuf), indBufLen).c_str());

    it->second(userHandle, msgId, indBuf, indBufLen);
}

void RdbQcMapBridge::initIndicationHandlerMap()
{
    //
    // NOTE:
    //
    // These handler functions are called in the QCCI thread context. QCMAP or QMI requests can issue
    // indications to the context, and wait for the returns.
    //
    // 1. Do own multithreading protection or dead-lock solution for each of handler functions.
    // 2. Do not call QCMAP or QMI requests in a handler function if the requests issue the indications for the handler function.
    // 3. Do not call QCMAP or QMI requests in a lock scope if the requests issue the indications that acquire the lock.
    //

    hookQcMapIndicationHandler<QMI_QCMAP_MSGR_WWAN_STATUS_IND_V01>(&RdbQcMapBridge::onQcMapIndicationWwanStatus);
    hookQcMapIndicationHandler<QMI_QCMAP_MSGR_BACKHAUL_STATUS_IND_V01>(&RdbQcMapBridge::onQcMapIndicationBackhaulStatus);
    hookQcMapIndicationHandler<QMI_QCMAP_MSGR_BRING_UP_WWAN_IND_V01>(&RdbQcMapBridge::onQcMapIndicationBringUpWwan);
    hookQcMapIndicationHandler<QMI_QCMAP_MSGR_TEAR_DOWN_WWAN_IND_V01>(&RdbQcMapBridge::onQcMapIndicationTearDownWwan);
}

void RdbQcMapBridge::terminate()
{
    running = false;
}

/**
 * @brief Initialise QCMAP policy settings aligning to policy settings in RDB.
 *
 */
void RdbQcMapBridge::configModemPolicySettings()
{
    // get profile list from WDS
    log(LOG_DEBUG, "### read modem profile list");
    auto msgProfileList = wds.makeMsg<QMI_WDS_GET_PROFILE_LIST_REQ_V01>();
    msgProfileList.req.profile_type_valid = true;
    msgProfileList.req.profile_type = WDS_PROFILE_TYPE_3GPP_V01;
    msgProfileList.send();
    log(LOG_DEBUG, "%d modem profile(s) found", msgProfileList.resp.profile_list_len);

    // collect settings of all profiles from WDS
    log(LOG_DEBUG, "### read profile settings for all modem profiles");
    std::map<int, std::shared_ptr<QmiClientWds::respType<QMI_WDS_GET_PROFILE_SETTINGS_REQ_V01>>> modemProfileSettingMap;
    for (uint32_t i = 0; i < msgProfileList.resp.profile_list_len; ++i) {
        auto &profile = msgProfileList.resp.profile_list[i];

        auto msgmodemProfileSettingMap = wds.makeMsg<QMI_WDS_GET_PROFILE_SETTINGS_REQ_V01>();
        msgmodemProfileSettingMap.req.profile.profile_index = profile.profile_index;
        msgmodemProfileSettingMap.req.profile.profile_type = profile.profile_type;

        log(LOG_DEBUG, "read modem profile #%d settings", profile.profile_index);

        msgmodemProfileSettingMap.send();
        modemProfileSettingMap.insert({ profile.profile_index, std::make_shared<QmiClientWds::respType<QMI_WDS_GET_PROFILE_SETTINGS_REQ_V01>>(
                                                                   msgmodemProfileSettingMap.resp) });
    }

    // TODO: based on "modemProfileSettingMap", construct link.profile RDBs into link.policy.

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // match each of profile policy handle to modem profile index
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /*
        policies are maintained in the following way.

        1. profile handle in a policy is used as profile ID number in modem (modem profile ID).
        2. profile handle in a policy is used as RDB policy number (RDB policy index)

        In result,

        1. policy handle is modem profile ID.
        2. policy handle is RDB policy index.

        In log,

        Use policy handle as policy index.
    */

    qcmap_msgr_wwan_policy_list_resp_msg_v01 policies;

    // create policies until the number of policies are up to the number of
    log(LOG_DEBUG, "### check policies");
    {
        qcMap->call(&QcMapClient::GetWWANPolicyList, &policies);
        int curNoOfPolicies = static_cast<int>(policies.wwan_policy_len);

        if (curNoOfPolicies != numberOfProfiles) {
            log(LOG_DEBUG, "### reconstruct policies");

            log(LOG_DEBUG, "check total number of policies and profiles (policy=%d,profile=%d)", curNoOfPolicies, modemProfileSettingMap.size());

            for (int i = 1; i < curNoOfPolicies; ++i) {
                auto &wwanProfilePolicy = policies.wwan_policy[i];

                log(LOG_DEBUG, "delete policy (policy=#%d)", wwanProfilePolicy.profile_handle);

                qcMap->call(&QcMapClient::SetWWANProfileHandlePreference, wwanProfilePolicy.profile_handle);
                qcMap->call(&QcMapClient::DeleteWWANPolicy);
            }

            for (int i = 0; i < numberOfProfiles; ++i) {
                int profileHandle = i + 1;
                log(LOG_DEBUG, "add policy (policy=#%d)", profileHandle);

                qcmap_msgr_net_policy_info_v01 newWwanPolicy;
                newWwanPolicy.tech_pref = QCMAP_MSGR_MASK_TECH_PREF_ANY_V01;
                newWwanPolicy.ip_family = QCMAP_MSGR_IP_FAMILY_V4V6_V01;
                int modemProfileIdx = rdbProfiles.children[profileHandle].children["module_profile_idx"].get().toInt(profileHandle);
                newWwanPolicy.v4_profile_id_3gpp = modemProfileIdx;
                newWwanPolicy.v6_profile_id_3gpp = modemProfileIdx;
                newWwanPolicy.v4_profile_id_3gpp2 = modemProfileIdx;
                newWwanPolicy.v6_profile_id_3gpp2 = modemProfileIdx;

                if (i == 0) {
                    qcMap->call(&QcMapClient::SetWWANProfileHandlePreference, 1);
                    qcMap->call(&QcMapClient::SetAutoconnect, false);

                    qcMap->call(&QcMapClient::UpdateWWANPolicy, QCMAP_MSGR_UPDATE_ALL_PROFILE_INDEX_V01, newWwanPolicy);
                } else {
                    qcMap->call(&QcMapClient::CreateWWANPolicy, newWwanPolicy);
                }
            }
        }

        // Update policies to match the module_profile_idx from link.profile.x
        qcMap->call(&QcMapClient::GetWWANPolicyList, &policies);
        curNoOfPolicies = static_cast<int>(policies.wwan_policy_len);

        for (int i = 0; i < numberOfProfiles; ++i) {
            int profileHandle = i + 1;
            auto &wwanProfilePolicy = policies.wwan_policy[i];
            int modemProfileIdx = rdbProfiles.children[profileHandle].children["module_profile_idx"].get().toInt(profileHandle);
            log(LOG_DEBUG, "update policy (policy=#%d)", profileHandle);

            if (wwanProfilePolicy.v4_profile_id_3gpp != modemProfileIdx || wwanProfilePolicy.v6_profile_id_3gpp != modemProfileIdx ||
                wwanProfilePolicy.v4_profile_id_3gpp2 != modemProfileIdx || wwanProfilePolicy.v6_profile_id_3gpp2 != modemProfileIdx) {
              qcmap_msgr_net_policy_info_v01 newWwanPolicy;
              newWwanPolicy.tech_pref = QCMAP_MSGR_MASK_TECH_PREF_ANY_V01;
              newWwanPolicy.ip_family = QCMAP_MSGR_IP_FAMILY_V4V6_V01;
              newWwanPolicy.v4_profile_id_3gpp = modemProfileIdx;
              newWwanPolicy.v6_profile_id_3gpp = modemProfileIdx;
              newWwanPolicy.v4_profile_id_3gpp2 = modemProfileIdx;
              newWwanPolicy.v6_profile_id_3gpp2 = modemProfileIdx;

              log(LOG_DEBUG, "update policy (policy=#%d):  modem profile indexes differ, attempting to update", profileHandle);
              qcMap->call(&QcMapClient::SetWWANProfileHandlePreference, profileHandle);

              if (!qcMap->callSafe(&QcMapClient::UpdateWWANPolicy, QCMAP_MSGR_UPDATE_ALL_PROFILE_INDEX_V01, newWwanPolicy)) {
                  // try taking down the interface and trying again
                  log(LOG_NOTICE, "update policy (policy=#%d):  failed to update policy; disabling and trying again", profileHandle);
                  triggerConnectPolicy(profileHandle, false);
                  qcMap->call(&QcMapClient::SetAutoconnect, false);
                  qcMap->callSafe(&QcMapClient::UpdateWWANPolicy, QCMAP_MSGR_UPDATE_ALL_PROFILE_INDEX_V01, newWwanPolicy);
                  // The interface will be enabled again in subscribePolicyRdbs()
                }
            }
        }
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // update link.policy.#.module_profile_idx
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    qcMap->call(&QcMapClient::GetWWANPolicyList, &policies);
    int len = static_cast<int>(policies.wwan_policy_len);

    for (int i = 0; i < len; ++i) {
        auto &wwanPolicy = policies.wwan_policy[i];
        auto policyRdbNo = i + 1;
        auto &rdbPolicy = rdbPolicies.children[policyRdbNo];

        log(LOG_DEBUG, "policy #%d (policyRdbNo=%d,modem_profile_idx=%d,policy_idx=%d)", i, policyRdbNo, wwanPolicy.v4_profile_id_3gpp,
            wwanPolicy.profile_handle);
        rdbPolicy.children["module_profile_idx"].set(wwanPolicy.v4_profile_id_3gpp);

        auto pdpType = qcMapIpFamilyToStrMap.get(wwanPolicy.ip_family, "ipv4v6");
        rdbPolicy.children["pdp_type"].set(pdpType);
    }
}

/**
 * @brief Setup policy RDBs
 *
 */
void RdbQcMapBridge::initPolicyRdbs()
{
    log(LOG_DEBUG, "start initialising Policy/Profile RDBs");

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // setup policy rdb structure
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    for (int i = 1; i <= numberOfProfiles; ++i) {
        log(LOG_DEBUG, "add policy/profile index (i=%d)", i);
        auto &policy = rdbPolicies.addChild(i);
        auto &profile = rdbProfiles.addChild(i);

        policyStatsMap.emplace(i, PolicyStats{});

        log(LOG_DEBUG, "setup policy/profile #%d Rdbs", i);

        // create persist RDBs
        policy.addChild("enable");

        // create non-persist RDBs
        auto policyVolatile = policy.addChildren({
            "connect_progress_v4",
            "connect_progress_v6",
            "dns1",
            "dns2",
            "ifdev",
            "iplocal",
            "ipv6_dns1",
            "ipv6_dns2",
            "ipv6_ipaddr",
            "trigger",
            "trigger_connect",
            "pdp_result",
            "stats_ipv4_bytes_rx",
            "stats_ipv4_bytes_tx",
            "stats_ipv4_pkts_rx",
            "stats_ipv4_pkts_tx",
            "stats_ipv4_pkts_dropped_rx",
            "stats_ipv4_pkts_dropped_tx",
            "stats_ipv6_bytes_rx",
            "stats_ipv6_bytes_tx",
            "stats_ipv6_pkts_rx",
            "stats_ipv6_pkts_tx",
            "stats_ipv6_pkts_dropped_rx",
            "stats_ipv6_pkts_dropped_tx",
            "stats_rx_kbps",
            "stats_tx_kbps",
            "stats_max_rx_kbps",
            "stats_max_tx_kbps",
            "sysuptime_at_ipv4_up", // system uptime when ipv4 up.
            "sysuptime_at_ipv6_up",
            "sysuptime_at_ifdev_up", // system uptime when interface device up.
            "status_ipv4",
            "status_ipv4_up",
            "status_ipv6",
        });

        auto a = erdb::RdbValue("");
        policyVolatile.setChildrenRdbFlags(0).setChildren("");

        // TODO: make the following RDBs persist and configurable
        auto policyPersisent = policy.addChildren({ "module_profile_idx", "pdp_type" });
        policyPersisent.setChildrenRdbFlags(0).setChildren("");

        // allow access to the link.profile RDBs
        profile.addChildren({
            "ip_handover",
            "vlan_index",
            "defaultroute",
            "mtu",
            "readflag",
            "module_profile_idx"
        });

        profile.children["ip_handover"].addChildren({"enable", "fake_wwan_address", "fake_wwan_mask", "mode", "last_wwan_ip"});
    }
}

/**
 * @brief Read connection status of all policies from QCMAP and update the policy RDBs.
 *
 */
void RdbQcMapBridge::updateAllPoliciesStatus()
{
    for (auto &[_, policy] : rdbPolicies.children) {
        int idx = policy->getNameInt();

        log(LOG_DEBUG, "update policy index (i=%d)", idx);
        updatePolicyStatus(idx);
    }
}

/**
 * @brief Subscribe policy RDBs.
 *
 */
void RdbQcMapBridge::subscribePolicyRdbs()
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // subscribe policy rdbs
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    for (auto &[_, policy] : rdbPolicies.children) {
        int idx = policy->getNameInt();

        log(LOG_DEBUG, "add policy index (i=%d)", idx);

        erdb::Rdb &rdbTrigger = policy->children["trigger"];
        erdb::Rdb &rdbEnable = policy->children["enable"];
        erdb::Rdb &rdbTriggerConnect = policy->children["trigger_connect"];

        auto policyEnableOrTrigger = [this, &rdbTrigger, &rdbEnable](erdb::Rdb &rdb) mutable {
            auto &rdbVal = rdbEnable.get();

            if (rdbVal.isAvail()) {
                erdb::Rdb *parent = rdb.getParent();
                assert(parent && "parent not configured");

                // read policy enable settings
                int policyNo = parent->getNameInt();
                bool policyEnable = rdbVal.toBool();

                ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                // enable or trigger the policy
                ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                if (&rdb == &rdbEnable) {
                    log(LOG_NOTICE, "change policy enable status (policyNo=%d,policyEnable=%d)", policyNo, policyEnable);
                    enablePolicy(policyNo, policyEnable);
                } else {

                    if (!policyEnable) {
                        log(LOG_NOTICE, "policy is not enabled, ignore the trigger (policyNo=%d, policyEnable=%d)", policyNo, policyEnable);
                    } else {
                        log(LOG_NOTICE, "trigger policy (policyNo=%d, policyEnable=%d)", policyNo, policyEnable);
                        if (rdbModemInitialConnectWorkaround.get().toBool()) {
                            // Workaround VENUS-276 and VENUS-259 by turning the modem off (then later, back on)
                            // This puts the modem in a better state
                            auto done_flag = rdbModemInitialConnectWorkaroundDone;
                            if (modemInitialConnectWorkaroundTrigger && !done_flag.get().toBool()) {
                                // disable the policy (set auto-connect to false), so that the
                                // disconnect command following will have an effect
                                enablePolicy(policyNo, false);
                                triggerConnectPolicy(policyNo, false);
                                // The policy can't be connected here again (or auto-connect enabled), as the disconnection
                                // takes some time and has to complete for the workaround to be successful.
                                //
                                // Instead, when the policy is disconnected, the onQcMapIndicationWwanStatus() callback
                                // will be called, check modemInitialConnectWorkaroundTrigger then set link.policy.n.trigger, which
                                // ends up enabling auto-connect by the call to enablePolicy(..., true) below.

                                // Do this workaraound only once per boot, this workaround is (almost) done now
                                done_flag.set("1");
                            } else if (!parent->children["status_ipv4"].get(false).toBool(upDownTuple, false)){
                              // Workaround is done, ipv4 is down, clear the trigger
                              modemInitialConnectWorkaroundTrigger = false;
                            }
                        }
                        if (!modemInitialConnectWorkaroundTrigger) {
                            enablePolicy(policyNo, true);
                        }
                    }
                }
            }
        };

        auto policyTriggerConnectCb = [this, &rdbTriggerConnect](erdb::Rdb &rdb) mutable {
            auto &rdbVal = rdbTriggerConnect.get();

            if (rdbVal.isAvail()) {
                erdb::Rdb *parent = rdb.getParent();
                assert(parent && "parent not configured");

                // read policy enable settings
                int policyNo = parent->getNameInt();
                bool policyTriggerEnable = rdbVal.toBool();

                log(LOG_NOTICE, "trigger policy (policyNo=%d, policyTriggerEnable=%d)", policyNo, policyTriggerEnable);
                triggerConnectPolicy(policyNo, policyTriggerEnable);
            }
        };

        // subscribe
        rdbTriggerConnect.subscribe(policyTriggerConnectCb, false);
        rdbEnable.subscribeForChange(policyEnableOrTrigger, false);
        rdbTrigger.subscribe(policyEnableOrTrigger);
    }
}

void RdbQcMapBridge::getPolicyStats(erdb::Rdb &policy)
{
    erdb::Rdb &rdbEnable = policy.children["enable"];
    bool policyEnable = rdbEnable.get().toBool();
    if (!policyEnable) {
        return;
    }

    int policyNo = policy.getNameInt();
    log(LOG_DEBUG, "getting stats policy function (policyNo=%d)", policyNo);

    if (qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyNo)) {
        qcmap_msgr_wwan_status_enum_v01 v4Status;
        qcmap_msgr_wwan_status_enum_v01 v6Status;

        if (qcMap->callSafe(&QcMapClient::GetWWANStatus, &v4Status, &v6Status)) {
            log(LOG_DEBUG, "wwan status (v4Status=%d,v6Status=%d)", v4Status, v6Status);
        }

        // get current system uptime.
        struct sysinfo info;
        long uptime = 0;
        if (sysinfo(&info) == 0) {
            uptime = info.uptime;
        }

        bool ipConnected = false;
        wwan_stats_bytes_t rx_bytes = 0;
        wwan_stats_bytes_t tx_bytes = 0;

        if (v4Status == QCMAP_MSGR_WWAN_STATUS_CONNECTED_V01) {
            ipConnected = true;
            qcmap_msgr_ip_family_enum_v01 ipFamily = QCMAP_MSGR_IP_FAMILY_V4_V01;
            qcmap_msgr_wwan_statistics_type_v01 wwanStats{};
            bool succGet = qcMap->callSafe(&QcMapClient::GetWWANStatistics, ipFamily, &wwanStats);

            if (!succGet) {
                log(LOG_ERR, "failed to get WWAN IPv4 stats (policyNo=%d)", policyNo);
            } else {
                log(LOG_DEBUG, "received WWAN IPv4 stats (policyNo=%d)", policyNo);
                rx_bytes += wwanStats.bytes_rx;
                tx_bytes += wwanStats.bytes_tx;
                policy.setChildren({
                    { "stats_ipv4_bytes_rx", wwanStats.bytes_rx },
                    { "stats_ipv4_bytes_tx", wwanStats.bytes_tx },
                    { "stats_ipv4_pkts_rx", wwanStats.pkts_rx },
                    { "stats_ipv4_pkts_tx", wwanStats.pkts_tx },
                    { "stats_ipv4_pkts_dropped_rx", wwanStats.pkts_dropped_rx },
                    { "stats_ipv4_pkts_dropped_tx", wwanStats.pkts_dropped_tx },
                });

                // If the interface is UP status but corresponding RDB variable is not set then
                // set the variable here.
                bool ipv4Up = policy.children["status_ipv4"].get(false).toBool(upDownTuple, false);
                if (!ipv4Up) {
                    policy.children["status_ipv4"].set(erdb::RdbValue(upDownTuple, true), false);
                }
                // If the interface is UP but the uptime is not set then set to current system
                // up time. There is same logic in WMMD for variants which do not use QCMAP_ConnectionManager
                auto ipv4UpTime = policy.children["sysuptime_at_ipv4_up"].get(false).toStdString();
                if (ipv4UpTime.size()<=0) {
                    policy.children["sysuptime_at_ipv4_up"].set(uptime);
                    if (policy.children["sysuptime_at_ifdev_up"].get(true).toInt() == 0) {
                        policy.children["sysuptime_at_ifdev_up"].set(uptime);
                    }
                }

            }
        }

        if (v6Status == QCMAP_MSGR_WWAN_STATUS_IPV6_CONNECTED_V01) {
            ipConnected = true;
            qcmap_msgr_ip_family_enum_v01 ipFamily = QCMAP_MSGR_IP_FAMILY_V6_V01;
            qcmap_msgr_wwan_statistics_type_v01 wwanStats{};
            bool succGet = qcMap->callSafe(&QcMapClient::GetWWANStatistics, ipFamily, &wwanStats);

            if (!succGet) {
                log(LOG_ERR, "failed to get WWAN IPv6 stats (policyNo=%d)", policyNo);
            } else {
                log(LOG_DEBUG, "received WWAN IPv6 stats (policyNo=%d)", policyNo);
                rx_bytes += wwanStats.bytes_rx;
                tx_bytes += wwanStats.bytes_tx;
                policy.setChildren({
                    { "stats_ipv6_bytes_rx", wwanStats.bytes_rx },
                    { "stats_ipv6_bytes_tx", wwanStats.bytes_tx },
                    { "stats_ipv6_pkts_rx", wwanStats.pkts_rx },
                    { "stats_ipv6_pkts_tx", wwanStats.pkts_tx },
                    { "stats_ipv6_pkts_dropped_rx", wwanStats.pkts_dropped_rx },
                    { "stats_ipv6_pkts_dropped_tx", wwanStats.pkts_dropped_tx },
                });

                // If the interface is UP status but corresponding RDB variable is not set then
                // set the variable here.
                bool ipv6Up = policy.children["status_ipv6"].get(false).toBool(upDownTuple, false);
                if (!ipv6Up) {
                    policy.children["status_ipv6"].set(erdb::RdbValue(upDownTuple, true), false);
                }
                // If the interface is UP but the uptime is not set then set to current system
                // up time. There is same logic in WMMD for variants which do not use QCMAP_ConnectionManager
                auto ipv6UpTime = policy.children["sysuptime_at_ipv6_up"].get(false).toStdString();
                if (ipv6UpTime.size()<=0) {
                    policy.children["sysuptime_at_ipv6_up"].set(uptime);
                    if (policy.children["sysuptime_at_ifdev_up"].get(true).toInt() == 0) {
                        policy.children["sysuptime_at_ifdev_up"].set(uptime);
                    }
                }

            }
        }

        auto& policyStats = policyStatsMap[policyNo];
        if (ipConnected) {
            if (rx_bytes >= policyStats.rx_bytes) {
                // unable to use function round from the library math
                // estd defines macro "log", which conflicts with the same macro in math
                auto rx_kbps_d = (double)(rx_bytes - policyStats.rx_bytes)*8/(pollSecs*1000);
                wwan_stats_bytes_t rx_kbps = (wwan_stats_bytes_t)(rx_kbps_d + 0.5);
                if (rx_kbps > policyStats.max_rx_kbps) {
                    policyStats.max_rx_kbps = rx_kbps;
                    policy.children["stats_max_rx_kbps"].set(policyStats.max_rx_kbps);
                }
                policy.children["stats_rx_kbps"].set(rx_kbps);
            }
            if (tx_bytes >= policyStats.tx_bytes) {
                auto tx_kbps_d = (double)(tx_bytes - policyStats.tx_bytes)*8/(pollSecs*1000);
                wwan_stats_bytes_t tx_kbps = (wwan_stats_bytes_t)(tx_kbps_d + 0.5);
                if (tx_kbps > policyStats.max_tx_kbps) {
                    policyStats.max_tx_kbps = tx_kbps;
                    policy.children["stats_max_tx_kbps"].set(policyStats.max_tx_kbps);
                }
                policy.children["stats_tx_kbps"].set(tx_kbps);
            }
        } else {
            policy.children["stats_rx_kbps"].set("", true, true);
            policy.children["stats_tx_kbps"].set("", true, true);
        }
        policyStats.rx_bytes = rx_bytes;
        policyStats.tx_bytes = tx_bytes;
    }
}

bool RdbQcMapBridge::enablePolicy(int policyNo, bool policyEnable)
{
    log(LOG_DEBUG, "auto connect setting triggered (policyNo=%d,policyEnable=%d)", policyNo, policyEnable);
    bool succ = false;

    qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyNo);

    boolean curPolicyEnable;
    bool succGet = qcMap->callSafe(&QcMapClient::GetAutoconnect, &curPolicyEnable);

    if (!succGet || (!curPolicyEnable != !policyEnable)) {
        log(LOG_NOTICE, "apply new auto connect setting (policyNo=%d,policyEnable=%d)", policyNo, policyEnable);
        succ = qcMap->callSafe(&QcMapClient::SetAutoconnect, policyEnable);
    } else {
        log(LOG_DEBUG, "no auto connect setting changed (policyNo=%d,policyEnable=%d)", policyNo, policyEnable);
    }

    return succ;
}

bool RdbQcMapBridge::triggerConnectPolicy(int policyNo, bool policyTriggerEnable)
{
    bool succ = false;

    if (!qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyNo)) {
        log(LOG_ERR, "RdbQcMapBridge::triggerConnectPolicy failed to SetWWANProfileHandlePreference (policyNo=%d)", policyNo);
        return succ;
    }

    auto msgmodemProfileSettingMap = wds.makeMsg<QMI_WDS_GET_PROFILE_SETTINGS_REQ_V01>();
    msgmodemProfileSettingMap.req.profile.profile_index = policyNo;
    msgmodemProfileSettingMap.req.profile.profile_type = WDS_PROFILE_TYPE_3GPP_V01;
    if (!msgmodemProfileSettingMap.sendSafe()) {
        log(LOG_NOTICE, "RdbQcMapBridge::triggerConnectPolicy failed to get modem profile (policyNo=%d)", policyNo);
        return succ;
    }

    auto &resp = msgmodemProfileSettingMap.resp;
    auto callType = profilePdpTypeToQcMapCallType.get((resp.pdn_type_valid != 0) ? resp.pdn_type : WDS_PROFILE_PDN_TYPE_IPV4_IPV6_V01,
                                                      QCMAP_MSGR_WWAN_CALL_TYPE_V4V6_V01);

    if (policyTriggerEnable) {
        succ = qcMap->callSafe(&QcMapClient::ConnectBackHaul, callType);
    } else {
        succ = qcMap->callSafe(&QcMapClient::DisconnectBackHaul, callType);
    }

    return succ;
}

/**
 * @brief Initialise IP passthrough RDBs
 *
 */
void RdbQcMapBridge::initIpPassthrough()
{
    auto ipPassthroughRdbNodesSubscribeOnly = rdbIpPassthrough.addChildren({ "mac_address", "client_device_name" });
    erdb::Rdb &rdbEnable = rdbIpPassthrough.addChild("enable");
    erdb::Rdb &rdbTrigger = rdbIpPassthrough.addChild("trigger_enable").setRdbFlags(0).set("");
    rdbIpPassthrough.addChild("fake_wwan_address").setRdbFlags(0).set("");
    rdbIpPassthrough.addChild("last_wwan_ip");

    for (auto &[_, profile]: rdbProfiles.children) {
        auto handleIpPassthrough = [this, &rdbEnable, &rdbTrigger, profile](erdb::Rdb &rdb) {
            rdbIpPassthrough.readChildren();

            int policyIndex = profile->getNameInt();

            qcMap->call(&QcMapClient::SetWWANProfileHandlePreference, policyIndex);

            // Don't call SetIPPassthroughConfig on disabled profiles, since they're likely mapped to bridge0 (default), and
            // that's probably in use for something else.
            bool profileEnabled = rdbPolicies.children[policyIndex].children["enable"].get().toBool();
            bool isDefaultProfile = profile->children["defaultroute"].get().toBool();

            // Don't read from rdb database.  Use the same vlan index that initVLanRbd setup
            int vlanIndex = profile->children["vlan_index"].get(false).toInt(-1);
            bool vlansEnabled = rdbVlanCollection.children["enable"].get(false).toBool();
            int numVlan = rdbVlanCollection.children["num"].get(false).toInt(0);
            int vlanId = 0;
            if (vlansEnabled && vlanIndex >= 0 && vlanIndex < numVlan) {
                if (rdbVlanCollection.children[vlanIndex].children["enable"].get(false).toBool()) {
                    vlanId = rdbVlanCollection.children[vlanIndex].children["id"].get(false).toInt();
                }
            }
            if (!isDefaultProfile && vlanId == 0) {
                // A non-default profile can't use vlanId == 0 (bridge0), that's for the default profile only
                // treat profile as if it's disabled
                profileEnabled = false;
            }
            log(LOG_INFO, "IP passthrough: profile: %d, vlan id: %d", policyIndex, vlanId);
            qcMap->call(&QCMAP_Client::SelectLANBridge, vlanId);

            // get IP passthrough config from QCMAP
            qcmap_msgr_ip_passthrough_mode_enum_v01 enableState;
            qcmap_msgr_ip_passthrough_config_v01 ipPassthroughConfig;
            bool gotIpPassthroughConfig = qcMap->callSafe(&QCMAP_Client::GetIPPassthroughConfig, &enableState, &ipPassthroughConfig);

            // get IP passthrough config from RDB
            auto macAddress = rdbIpPassthrough.children["mac_address"].get(false).toStdString();
            auto devName = rdbIpPassthrough.children["client_device_name"].get(false).toStdString();
            bool ipPassthroughEnable;
            // Only use the trigger rdb val if it's non-empty (i.e. trigger_enable.set("") re-triggers the current enabled state)
            if (rdb.getRdbName() == rdbEnable.getRdbName() || (rdb.getRdbName() == rdbTrigger.getRdbName() && rdb.get().isSet())) {
                ipPassthroughEnable = rdb.get().toBool();
            } else {
                ipPassthroughEnable = rdbEnable.get().toBool();
            }
            // Also only enable if the link.profile.x.ip_handover.enable says so
            ipPassthroughEnable = ipPassthroughEnable && profile->children["ip_handover"].children["enable"].get().toBool();

            log(LOG_DEBUG, "IP passthrough triggered by RDB \"%s\", enable order = %d", rdb.getRdbName().c_str(), ipPassthroughEnable);

            estd::BlindComp bc(ipPassthroughConfig);
            auto newIpPassthroughConfig = bc.clone();

            // setup QCMAP IP passthrough config
            if (ipPassthroughEnable) {
                estd::safeStrCpy(newIpPassthroughConfig.client_device_name, "");
                estd::MacAddress::pton(std::string(""), newIpPassthroughConfig.mac_addr);
                newIpPassthroughConfig.device_type = QCMAP_MSGR_DEVICE_TYPE_ANY_V01;
                // If the mode is eth, supply mac address
                if (profile->children["ip_handover"].children["mode"].get().toStdString() == "eth") {
                    estd::safeStrCpy(newIpPassthroughConfig.client_device_name, devName.c_str());
                    estd::MacAddress::pton(macAddress, newIpPassthroughConfig.mac_addr);
                    newIpPassthroughConfig.device_type = QCMAP_MSGR_DEVICE_TYPE_ETHERNET_V01;
                    // If the MAC address is empty, disable IP passthrough until a new MAC address is provided
                    if (estd::MacAddress::isEmpty(newIpPassthroughConfig.mac_addr)) {
                        ipPassthroughEnable = false;
                    }
                }
            }
            qcmap_msgr_ip_passthrough_mode_enum_v01 newState =
                ipPassthroughEnable ? QCMAP_MSGR_IP_PASSTHROUGH_MODE_UP_V01 : QCMAP_MSGR_IP_PASSTHROUGH_MODE_DOWN_V01;

            bool ipPassthroughConfigChg = !bc.isIdentical(newIpPassthroughConfig);

            // Check the current state. If it's the same as the curernt config, then the
            // call to SetIPPassthroughConfig() may be skipped
            boolean current_state = false;
            qcMap->callSafe(&QCMAP_Client::GetIPPassthroughState, &current_state);
            log(LOG_INFO, "IP passthrough: current state: %d, config changed: %d", current_state, ipPassthroughConfigChg);
            ipPassthroughConfigChg = ipPassthroughConfigChg || (current_state != ipPassthroughEnable);

            // apply IP passthrough settings if changed
            if (profileEnabled && (!gotIpPassthroughConfig || (enableState != newState) || ipPassthroughConfigChg)) {
                log(LOG_INFO, "apply new IP passthrough settings (en=%d,macAddress=%s,devName=%s)", ipPassthroughEnable, macAddress.c_str(),
                    devName.c_str());

                // If ip passthrough is currently enabled, and will still be enabled after the change (e.g. because of a change in MAC address)
                // then disable ip passthrouh first, so that the new MAC address is applied properly for the dnsmasq processes
                if (enableState == newState && ipPassthroughEnable) {
                    auto disable = QCMAP_MSGR_IP_PASSTHROUGH_MODE_DOWN_V01;
                    qcMap->callSafe(&QCMAP_Client::SetIPPassthroughConfig, disable, ipPassthroughConfigChg, &newIpPassthroughConfig);
                }

                qcMap->callSafe(&QCMAP_Client::SetIPPassthroughConfig, newState, ipPassthroughConfigChg, &newIpPassthroughConfig);
            } else {
                log(LOG_INFO, "keep IP passthrough settings (en=%d,macAddress=%s,devName=%s)", ipPassthroughEnable, macAddress.c_str(), devName.c_str());
            }

            std::string fake_wwan_address;
            auto iplocal = rdbPolicies.children[policyIndex].children["iplocal"].get(false).toStdString();
            if (ipPassthroughEnable) {
                // Get the fake address assigned to the WWAN device. Either GetIPPassthroughState() said the
                // passthrough state is enabled, or SetIPPassthroughConfig() was called.  In both cases,
                // the WWAN device has the fake IPv4 address now  (unless the WWAN device is down.  But then
                // the WWAN_STATUS callback will trigger this bit of code again)
                auto network = estd::NetworkInterface<AF_INET>();
                auto interface = rdbPolicies.children[policyIndex].children["ifdev"].get(false).toStdString();
                fake_wwan_address = estd::inet_ntop<AF_INET>(network.get(interface));
                log(LOG_INFO, "IP passthrough interface: %s local ip: %s fake wwan address: %s", interface.c_str(),
                    iplocal.c_str(), fake_wwan_address.c_str());
                if (fake_wwan_address == "0.0.0.0" || fake_wwan_address == iplocal) {
                    fake_wwan_address = "";
                }
            }
            profile->children["ip_handover"].children["fake_wwan_address"].set(fake_wwan_address);
            profile->children["ip_handover"].children["last_wwan_ip"].set(iplocal);
            if (isDefaultProfile) {
                // The current profile is the default, set the global fake_wwan_address and last_wwan_ip
                rdbIpPassthrough.children["fake_wwan_address"].set(fake_wwan_address);
                rdbIpPassthrough.children["last_wwan_ip"].set(iplocal);
            }
        };

        // Note: all rdbs that are subscribed here have to be copied and added to rdbIpPassthroughTriggerList
        // (each erdb::Rdb should only have one callback subscribed)

        // subscribe IP passthrough RDBs
        for (auto &[_, rdb] : ipPassthroughRdbNodesSubscribeOnly.children) {
            rdbIpPassthroughTriggerList.push_back(*rdb);
            auto &s_rdb = rdbIpPassthroughTriggerList.back();
            s_rdb.subscribeForChange(handleIpPassthrough, false);
        }
        rdbIpPassthroughTriggerList.push_back(rdbTrigger);
        auto &s_rdbTrigger = rdbIpPassthroughTriggerList.back();
        s_rdbTrigger.subscribe(handleIpPassthrough, false);
        // Subscribe ipv4 up
        int policyIndex =  profile->getNameInt();
        rdbPolicies.children[policyIndex].children["status_ipv4_up"].subscribe(handleIpPassthrough, false);
        // subscribe global enable
        rdbIpPassthroughTriggerList.push_back(rdbEnable);
        auto &s_rdbEnable = rdbIpPassthroughTriggerList.back();
        s_rdbEnable.subscribeForChange(handleIpPassthrough, false);
        // subscibe and execute now
        profile->children["ip_handover"].children["enable"].subscribe(handleIpPassthrough);
    }
}

/**
 * @brief Apply LAN config
 *
 */
void RdbQcMapBridge::applyLanConfig(int vlanId, const erdb::Rdb *rdbAddress, const erdb::Rdb *rdbNetmask, const erdb::Rdb *rdbEnable,
                                    const erdb::Rdb *rdbRange, const erdb::Rdb *rdbLease)
{
    assert(rdbAddress != nullptr && rdbNetmask != nullptr && rdbRange != nullptr && rdbLease != nullptr && "missing parameters");

    auto address = rdbAddress->get(false).toStdString();
    auto netmask = rdbNetmask->get(false).toStdString();
    auto range = estd::split(rdbRange->get(false).toStdString(), ',');
    auto lease = rdbLease->get(false).toInt(-1);

    log(LOG_DEBUG, "start to apply LAN config (vlanId=%d, range.size=%d)", vlanId, range.size());

    if (address.empty() || netmask.empty()) {
        log(LOG_ERR, "One of RDBs is not available. LAN/DHCP cannot be configured");
        return;
    }

    if (range.size() == 1) {
        range.push_back(range[0]);
    };

    bool dhcpValid = (range.size() == 2) && !range[0].empty() && !range[1].empty() && (lease >= 0);
    auto enable = (rdbEnable == nullptr) ? dhcpValid : rdbEnable->get(false).toBool();

    if (enable && !dhcpValid) {
        log(LOG_ERR, "incorrect range LAN/DHCP used. LAN/DHCP cannot be configured (range=%s)", estd::concat(range, ",").c_str());
        enable = false;
    }

    try {
        log(LOG_DEBUG, "get LAN/DHCP parameters (vlanId=%d, address=%s,netmask=%s,enable=%d,range='%s',lease=%d)", vlanId, address.c_str(),
            netmask.c_str(), enable, estd::concat(range, ",").c_str(), lease);

        auto lanConfig = qcMap->getLanConfig(vlanId);
        estd::BlindComp bc(lanConfig);

        qcmap_msgr_lan_config_v01 newLanConfig = bc.clone();
        newLanConfig.enable_dhcp = enable;
        newLanConfig.gw_ip = estd::ntoh(estd::inet_pton<AF_INET>(address)).s_addr;
        newLanConfig.netmask = estd::ntoh(estd::inet_pton<AF_INET>(netmask)).s_addr;

        if (dhcpValid) {
            newLanConfig.dhcp_config.dhcp_start_ip = estd::ntoh(estd::inet_pton<AF_INET>(range[0])).s_addr;
            newLanConfig.dhcp_config.dhcp_end_ip = estd::ntoh(estd::inet_pton<AF_INET>(range[1])).s_addr;
            newLanConfig.dhcp_config.lease_time = lease;
        }

        if (!bc.isIdentical(newLanConfig)) {
            log(LOG_NOTICE, "apply new LAN/DHCP settings (vlanId=%d,address=%s,netmask=%s,enable=%d,range='%s',lease=%d)", vlanId, address.c_str(),
                netmask.c_str(), enable, estd::concat(range, ",").c_str(), lease);

            qcMap->setLanConfig(vlanId, newLanConfig);

            qcMap->call(&QCMAP_Client::ActivateLAN);

            // Trigger ipv6 template, if the ipv6 address is already up
            // Take the lock to avoid writing incorrect values
            erdb::RdbScopeMutex scopeMutex(rdbPolicies);
            for (auto &[_, policy]: rdbPolicies.children) {
                if (policy->children["ipv6_ipaddr"].get(false).isSet()) {
                    policy->children["ipv6_ipaddr"].write();
                }
            }
        } else {
            log(LOG_DEBUG, "no LAN/DHCP settings changed");
        }
    }
    catch (const estd::invalid_argument &e) {
        log(LOG_ERR, "failed in preparing LAN/DHCP arguments for SetLANConfig()");
    }
    catch (const estd::runtime_error &e) {
        log(LOG_ERR, "failed to set LAN/DHCP. Crashing (systemd may restart this program).");
        throw;
    }
}

/**
 * @brief Initiate DNAT RDBs
 *
 * TODO: use this function as a generic function
 *
 */

template <typename ConfigType>
void RdbQcMapBridge::initDnatRdb(erdb::Rdb &rdbCollection)
{
    typedef QcMapMsgrConfigTraits<ConfigType> ConfigTraits;

    log(LOG_DEBUG, "subscribe RDBs (configTypeName=%s)", ConfigTraits::getConfigTypeName().c_str());

    rdbCollection.addChild("trigger").setRdbFlags(0).set("").subscribe([this, &rdbCollection](erdb::Rdb &rdb) {
        typename ConfigTraits::SetOperationType setOp;
        typename ConfigTraits::Deserializer ds;

        auto configTypeStr = ConfigTraits::getConfigTypeName();
        auto configTypeName = configTypeStr.c_str();

        log(LOG_DEBUG, "RDB triggered (configTypeName=%s)", configTypeName);

        // read RDBs
        auto rdbDnatEnumCollection = rdbCollection.enumChildren(erdb::Rdb::ChildPattern::number);
        auto dnatEnumCollectionCount = rdbDnatEnumCollection.children.size();
        log(LOG_INFO, "RDB config found (configTypeName=%s,size=%d)", configTypeName, dnatEnumCollectionCount);
        rdbDnatEnumCollection.readChildren();

        // perform Settings per policy
        for (auto &[childName, rdbPolicy] : rdbPolicies.children) {

            // select policy
            int policyIndex = rdbPolicy->getNameInt();
            log(LOG_DEBUG, "select policy (configTypeName=%s,policyIndex=%d)", configTypeName, policyIndex);
            qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyIndex);

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // collect DNAT rules from RDB
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            typename ConfigTraits::VectorType dnatConfigEntryCollectionFromRdb;

            dnatConfigEntryCollectionFromRdb.reserve(dnatEnumCollectionCount);
            for (auto &[i, rdbDnat] : rdbDnatEnumCollection.children) {

                auto rawDnatRule = rdbDnat->get(false).toStringView();

                typename ConfigTraits::ConfigType tmpDnatConfigEntry;
                bool en;
                std::string name;
                int policyIndexForRule;

                auto succ = ds.deserialize(estd::split(rawDnatRule, ','), en, name, policyIndexForRule, tmpDnatConfigEntry);

                if (!succ) {
                    log(LOG_ERR, "incorrect format of RDB (configTypeName=%s,policyIndex=%d,rdb=%s,val=%s)", configTypeName, policyIndex,
                        rdbDnat->getDispName().c_str(), rawDnatRule.c_str());
                } else if (policyIndex != policyIndexForRule) {
                    log(LOG_DEBUG, "skip RDB config, index not matching (configTypeName=%s,policyIndex=%d,rdb=%s,val=%s)", configTypeName,
                        policyIndex, rdbDnat->getDispName().c_str(), rawDnatRule.c_str());
                } else if (!en) {
                    log(LOG_DEBUG, "skip RDB config, rule is disabled (configTypeName=%s,policyIndex=%d,rdb=%s,val=%s)", configTypeName, policyIndex,
                        rdbDnat->getDispName().c_str(), rawDnatRule.c_str());
                } else {
                    log(LOG_DEBUG, "collect RDB config (configTypeName=%s,policyIndex=%d,%s)", configTypeName, policyIndex,
                        ConfigTraits::getLogConfigEntry(tmpDnatConfigEntry).c_str());

                    dnatConfigEntryCollectionFromRdb.emplace_back(tmpDnatConfigEntry);
                }
            }
            auto dnatConfigEntrySetFromRdb = setOp.doPtrTransfer(dnatConfigEntryCollectionFromRdb);

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // collect DNAT rules form QCMAP
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            rdbCollection.clearChildren(erdb::Rdb::ChildPattern::number);
            auto dnatConfigEntrySetFromQcMap = setOp.get();
            typename ConfigTraits::ConfigType dnatConfigEntryCollectionFromQcMap[ConfigTraits::maxConfigs];
            int numOfDnatConfigEntries = 0;
            if (qcMap->callSafe(ConfigTraits::qcMapGetConfigs, dnatConfigEntryCollectionFromQcMap, &numOfDnatConfigEntries)) {
                log(LOG_INFO, "QCMAP configs found (configTypeName=%s,policyIndex=%d,size=%d)", configTypeName, policyIndex, numOfDnatConfigEntries);
                for (int i = 0; i < numOfDnatConfigEntries; ++i) {
                    auto &dnatEntry = dnatConfigEntryCollectionFromQcMap[i];
                    log(LOG_DEBUG, "collect QCMAP config (configTypeName=%s,policyIndex=%d, i=%d,%s)", configTypeName, policyIndex, i,
                        ConfigTraits::getLogConfigEntry(dnatEntry).c_str());
                    dnatConfigEntrySetFromQcMap.emplace(&dnatEntry);
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // reconfig DNAT rules
            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // /etc/cdcd/conf/mgr_templates/wan_admin_access.temlate clears all the DNAT ip6tables rules, so get QCMAP to remove
            // all the DNAT entries too.  Then add all the entries configured in RDB back in
            auto dnatConfigEntryToDelete = dnatConfigEntrySetFromQcMap;
            auto dnatConfigEntryToAdd = dnatConfigEntrySetFromRdb;
            log(LOG_INFO, "reconfig (configTypeName=%s,policyIndex=%d,qcmap=%d,rdb=%d,delete=%d,add=%d)", configTypeName, policyIndex,
                dnatConfigEntrySetFromQcMap.size(), dnatConfigEntrySetFromRdb.size(), dnatConfigEntryToDelete.size(), dnatConfigEntryToAdd.size());

            for (auto &dnatConfigEntry : dnatConfigEntryToDelete) {
                log(LOG_DEBUG, "delete config (configTypeName=%s,policyIndex=%d,%s)", configTypeName, policyIndex,
                    ConfigTraits::getLogConfigEntry(*dnatConfigEntry).c_str());
                qcMap->callSafe(ConfigTraits::qcMapDeleteConfig, dnatConfigEntry);
            }

            for (auto &dnatConfigEntry : dnatConfigEntryToAdd) {
                log(LOG_DEBUG, "add config (configTypeName=%s,policyIndex=%d,%s)", configTypeName, policyIndex,
                    ConfigTraits::getLogConfigEntry(*dnatConfigEntry).c_str());
                qcMap->callSafe(ConfigTraits::qcMapAddConfig, dnatConfigEntry);
            }
        }
    });
}

// initialise WAN firewall settings (enable & policy)
void RdbQcMapBridge::initWanFirewallSettings(void)
{
    log(LOG_DEBUG, "start initialising WAN firewall settings RDBs");
    for (int i = 1; i <= numberOfProfiles; i++) {
        log(LOG_DEBUG, "add child %d to %s", i, rdbWanFirewallSettings.getDispName().c_str());
        auto &child = rdbWanFirewallSettings.addChild(i);
        child.addChildren({ "enable", "policy" });
    }
    auto fwHandler = [this](erdb::Rdb &rdb) {
        static estd::DefValueMap<estd::StringView, boolean> qcMapStrToPolicyMap = {
            { "allow", 1 },
            { "deny", 0 },
        };
        auto rdbDispName = rdbWanFirewallSettings.getDispName();
        log(LOG_DEBUG, "RDB triggered %s", rdbDispName.c_str());

        for (auto &[name, rdbPolicy] : rdbPolicies.children) {
            int policyIndex = rdbPolicy->getNameInt();
            log(LOG_DEBUG, "select policy (RDB=%s,policyIndex=%d)", rdbDispName.c_str(), policyIndex);
            if (!qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyIndex)) {
                log(LOG_ERR, "failed to select policy %d", policyIndex);
                continue;
            }

            // read from global RDBs
            boolean rdbEn = rdbWanFirewallSettings.children["enable"].get().toInt();
            boolean rdbAllowed = qcMapStrToPolicyMap.get(rdbWanFirewallSettings.children["policy"].get().toStringView(), 0);

            // read from policy specific RDBs
            auto &child = rdbWanFirewallSettings.children[name];
            auto &childValEn = child.children["enable"].get();
            auto &childValPolicy = child.children["policy"].get();
            boolean childEn = childValEn.toInt();
            boolean childAllowed = qcMapStrToPolicyMap.get(childValPolicy.toStringView(), 0);

            if (childValEn.isSet()) {
                log(LOG_DEBUG, "use policy %d specific enable RDB %d", policyIndex, childEn);
                rdbEn = childEn;
            } else {
                log(LOG_DEBUG, "use global enable RDB %d", rdbEn);
            }
            if (childValPolicy.isSet()) {
                log(LOG_DEBUG, "use policy %d specific allowed RDB %d", policyIndex, childAllowed);
                rdbAllowed = childAllowed;
            } else {
                log(LOG_DEBUG, "use global allowed RDB %d", rdbAllowed);
            }

            // read from QCMAP
            boolean qcmapEn, qcmapAllowed;
            if (qcMap->callSafe(&QCMAP_Client::GetFirewall, &qcmapEn, &qcmapAllowed)) {
                if (rdbEn != qcmapEn || (rdbEn && rdbAllowed != qcmapAllowed)) {
                    log(LOG_NOTICE, "firewall settings changed: RDB=(%d,%d) QCMAP=(%d,%d). Updating", rdbEn, rdbAllowed, qcmapEn, qcmapAllowed);
                    if (!qcMap->callSafe(&QCMAP_Client::SetFirewall, rdbEn, rdbAllowed)) {
                        log(LOG_ERR, "failed to set firewall settings for policy %d", policyIndex);
                    }
                } else {
                    log(LOG_INFO, "firewall settings not changed: en=%d,allowed=%d", rdbEn, rdbAllowed);
                }
            } else {
                log(LOG_ERR, "failed to get firewall settings from QCMAP for policy %d", policyIndex);
            }
        }
    };

    rdbWanFirewallSettings.addChild("enable").subscribe(fwHandler, false);
    rdbWanFirewallSettings.addChild("policy");
    rdbWanFirewallSettings.addChild("trigger").setRdbFlags(0).set("").subscribe(fwHandler);
}

// initialise WAN firewall rules RDBs
void RdbQcMapBridge::initWanFirewallRdb(erdb::Rdb &rdbCollection)
{
    using ConfigType = qcmap_msgr_firewall_entry_conf_t;
    using ConfigTraits = QcMapMsgrConfigTraits<ConfigType>;

    log(LOG_DEBUG, "subscribe RDBs (configTypeName=%s)", ConfigTraits::getConfigTypeName().c_str());
    // the root RDB name: service.firewall.wan.ipv4 or ipv6
    const std::string &rdbRoot = rdbCollection.getName();
    bool ipv6 = rdbRoot[rdbRoot.size() - 1] == '6';
    rdbCollection.addChild("trigger").setRdbFlags(0).set("").subscribe([this, &rdbCollection, ipv6](erdb::Rdb &rdb) {
        typename ConfigTraits::SetOperationType setOp;
        WanIpv4FirewallDeserializer ds4;
        WanIpv6FirewallDeserializer ds6;
        auto configTypeStr = ConfigTraits::getConfigTypeName();
        auto configTypeName = configTypeStr.c_str();

        log(LOG_DEBUG, "RDB triggered (configTypeName=%s)", configTypeName);

        // enumerate the direct children of rdbRoot
        rdbCollection.clearChildren(erdb::Rdb::ChildPattern::number);
        auto rdbEnumCollection = rdbCollection.enumChildren(erdb::Rdb::ChildPattern::number);
        auto enumCollectionCount = rdbEnumCollection.children.size();
        log(LOG_INFO, "RDB config found (configTypeName=%s,size=%d)", configTypeName, enumCollectionCount);

        // read grandchildren
        std::initializer_list<erdb::Rdb> children;
        if (ipv6) {
            children = { "enable", "policy_index", "dir",
                         "src_addr", "src_prefix_len",
                         "dst_addr", "dst_prefix_len",
                         "tc", "tc_mask",
                         "nat_enabled", "proto",
                         "src_port", "dst_port",
                         "icmp_type", "icmp_code", "esp_spi" };
        } else {
            children = { "enable", "policy_index", "dir",
                         "src_addr", "src_mask",
                         "dst_addr", "dst_mask",
                         "tos", "tos_mask",
                         "proto",
                         "src_port", "dst_port",
                         "icmp_type", "icmp_code", "esp_spi" };
        }
        // collect serialised data from all grandchildren into a map
        std::map<estd::StringView, std::vector<std::string>> sdCollection;
        for (auto &[name, rdbFw] : rdbEnumCollection.children) {
            log(LOG_INFO, "WAN Firewall rule: %s.%s", rdbCollection.getRdbName().c_str(), name.c_str());

            rdbFw->addChildren(children);
            rdbFw->readChildren();
            std::vector<std::string> sd;
            sd.reserve(children.size());
            for (auto &rdb : children) {
                std::string s = rdbFw->children[rdb.getName()].get(false).toStdString();
                sd.push_back(s);
            }
            sdCollection.emplace(name, sd);
        }

        // process for each link policy
        for (auto &[_, rdbPolicy] : rdbPolicies.children) {
            int policyIndex = rdbPolicy->getNameInt();
            log(LOG_DEBUG, "select policy (configTypeName=%s,policyIndex=%d)", configTypeName, policyIndex);
            qcMap->callSafe(&QcMapClient::SetWWANProfileHandlePreference, policyIndex);

            bool en;
            int policyIdxRdb;
            typename ConfigTraits::VectorType fwConfigEntryCollectionFromRdb;
            fwConfigEntryCollectionFromRdb.reserve(enumCollectionCount);
            for (auto &[name, rdbFw] : rdbEnumCollection.children) {
                log(LOG_DEBUG, "collect from RDB %s, %s", rdbFw->getDispName().c_str(), name.c_str());
                typename ConfigTraits::ConfigType tmpFwConfigEntry;
                auto &sd = sdCollection[name];
                auto succ = ipv6 ? ds6.deserialize(sd, en, policyIdxRdb, tmpFwConfigEntry) : ds4.deserialize(sd, en, policyIdxRdb, tmpFwConfigEntry);

                if (policyIdxRdb == 0) {
                    log(LOG_INFO, "policyIdxRdb==0, applying to all policies");
                    policyIdxRdb = policyIndex;
                }
                if (!succ) {
                    log(LOG_ERR, "incorrect format of RDB (configTypeName=%s,rdb=%s)", configTypeName, rdbFw->getDispName().c_str());
                } else if (policyIndex != policyIdxRdb) {
                    log(LOG_DEBUG, "skip RDB config, index not matching (configTypeName=%s,policyIndex=%d,rdb=%s,policyIdxRdb=%d)", configTypeName,
                        policyIndex, rdbFw->getDispName().c_str(), policyIdxRdb);
                } else if (!en) {
                    log(LOG_DEBUG, "skip RDB config, rule is disabled (configTypeName=%s,rdb=%s)", configTypeName, rdbFw->getDispName().c_str());
                } else {
                    log(LOG_DEBUG, "collect RDB config (configTypeName=%s,rdb=%s,%s)", configTypeName, rdbFw->getDispName().c_str(),
                        ConfigTraits::getLogConfigEntry(tmpFwConfigEntry).c_str());
                    fwConfigEntryCollectionFromRdb.emplace_back(tmpFwConfigEntry);
                }
            }
            auto fwConfigEntrySetFromRdb = setOp.doPtrTransfer(fwConfigEntryCollectionFromRdb);
            log(LOG_INFO, "collected %d RDB configs", fwConfigEntrySetFromRdb.size());

            // get config from qcmap
            auto fwConfigEntrySetFromQcMap = setOp.get();
            // qcMapGetEntry does not clear entry before filling it.
            // We must do it before invocation, otherwise field masks could be wrong
            // resulting in invalid iptable rules
            typename ConfigTraits::ConfigType fwConfigEntryCollectionFromQcMap[ConfigTraits::maxConfigs] = {};
            typename ConfigTraits::UConfigType fwHandles;
            auto &handleList = fwHandles.extd_firewall_handle_list;
            fwHandles.extd_firewall_handle_list.ip_family = ipv6 ? IP_V6 : IP_V4;
            if (qcMap->callSafe(ConfigTraits::qcMapGetHandles, &handleList)) {
                log(LOG_INFO, "QCMAP configs found (configTypeName=%s,size=%d)", configTypeName, handleList.num_of_entries);
                for (int i = 0; i < handleList.num_of_entries; i++) {
                    auto &entry = fwConfigEntryCollectionFromQcMap[i];
                    entry.filter_spec.ip_vsn = ipv6 ? IP_V6 : IP_V4;
                    entry.firewall_handle = handleList.handle_list[i];
                    if (qcMap->callSafe(ConfigTraits::qcMapGetEntry, &entry)) {
                        log(LOG_DEBUG, "collect QCMAP config (configTypeName=%s,i=%d,%s)", configTypeName, i,
                            ConfigTraits::getLogConfigEntry(entry).c_str());
                        fwConfigEntrySetFromQcMap.emplace(&entry);
                    }
                }
            }

            // reconfig firewall rules
            auto fwConfigEntryToDelete = setOp.getDifference(fwConfigEntrySetFromQcMap, fwConfigEntrySetFromRdb);
            auto fwConfigEntryToAdd = setOp.getDifference(fwConfigEntrySetFromRdb, fwConfigEntrySetFromQcMap);
            log(LOG_INFO, "reconfig (configTypeName=%s,qcmap=%d,rdb=%d,delete=%d,add=%d)", configTypeName, fwConfigEntrySetFromQcMap.size(),
                fwConfigEntrySetFromRdb.size(), fwConfigEntryToDelete.size(), fwConfigEntryToAdd.size());

            for (auto &entry : fwConfigEntryToDelete) {
                log(LOG_DEBUG, "delete config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*entry).c_str());
                if (!qcMap->callSafe(ConfigTraits::qcMapDeleteEntry, entry->firewall_handle)) {
                    log(LOG_ERR, "failed to delete config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*entry).c_str());
                }
            }

            for (auto &entry : fwConfigEntryToAdd) {
                log(LOG_DEBUG, "add config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*entry).c_str());
                typename ConfigTraits::UConfigType addEntry;
                addEntry.extd_firewall_entry = *entry;
                if (!qcMap->callSafe(ConfigTraits::qcMapAddEntry, &addEntry)) {
                    log(LOG_ERR, "failed to add config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*entry).c_str());
                }
            }
        }
    });
}

/**
 * @brief Initiates DHCP Reserved RDBs.
 *
 * TODO: use this function as a generic function
 */
template <typename ConfigType>
void RdbQcMapBridge::initGenericListRdb(erdb::Rdb &rdbCollection)
{
    typedef QcMapMsgrConfigTraits<ConfigType> ConfigTraits;

    log(LOG_DEBUG, "subscribing RDBs (configTypeName=%s)", ConfigTraits::getConfigTypeName().c_str());

    rdbCollection.addChild("trigger").setRdbFlags(0).set("").subscribe([this, &rdbCollection](erdb::Rdb &rdb) {
        auto configTypeStr = ConfigTraits::getConfigTypeName();
        auto configTypeName = configTypeStr.c_str();

        log(LOG_INFO, "RDB triggered (configTypeName=%s)", configTypeName);

        // read RDBs
        rdbCollection.clearChildren(erdb::Rdb::ChildPattern::number);
        auto rdbEnumCollection = rdbCollection.enumChildren(erdb::Rdb::ChildPattern::number);
        log(LOG_INFO, "RDB configs found (count=%d)", rdbEnumCollection.children.size());
        rdbEnumCollection.readChildren();

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // collect configs from RDB
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        typename ConfigTraits::VectorType configCollectionFromRdb;
        typename ConfigTraits::Deserializer ds;
        typename ConfigTraits::ConfigType tmpConfig;
        bool en;
        for (auto &[i, rdbDhcpRes] : rdbEnumCollection.children) {
            auto rdbConfigStr = rdbDhcpRes->get(false).toStdString();

            try {
                auto succ = ds.deserialize(estd::split(rdbConfigStr, ','), tmpConfig, en);

                if (!succ) {
                    log(LOG_ERR, "incorrect format in RDB (rdb=%s,val=%s)", rdbDhcpRes->getDispName().c_str(), rdbConfigStr.c_str());
                } else if (!en) {
                    log(LOG_DEBUG, "RDB config disabled (rdb=%s,val=%s)", rdbDhcpRes->getDispName().c_str(), rdbConfigStr.c_str());
                } else {
                    tmpConfig.enable_reservation = true;
                    log(LOG_DEBUG, "collect DHCP reservation (rdb=%s,%s)", rdbDhcpRes->getDispName().c_str(),
                        ConfigTraits::getLogConfigEntry(tmpConfig).c_str());
                    configCollectionFromRdb.emplace_back(tmpConfig);
                }
            }
            catch (const estd::invalid_argument &e) {
                log(LOG_ERR, "incorrect format of DHCP reservation field (rdb=%s,v='%s')", rdbDhcpRes->getDispName().c_str(), rdbConfigStr.c_str());
            }
        }
        typename ConfigTraits::SetOperationType setOp;
        auto dhcpReservationSetFromRdb = setOp.doPtrTransfer(configCollectionFromRdb);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // collect DHCP reservations from QCMAP
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        auto dhcpReservationSetFromQcMap = setOp.get();
        typename ConfigTraits::ConfigType dhcpReservationCollection[ConfigTraits::maxConfigs];
        uint32_t numOfDhcpReservation;
        if (qcMap->callSafe(ConfigTraits::qcMapGetConfigs, dhcpReservationCollection, &numOfDhcpReservation)) {
            log(LOG_INFO, "QCMAP config found (type=%s,count=%d)", configTypeName, numOfDhcpReservation);
            for (uint32_t i = 0; i < numOfDhcpReservation; ++i) {
                auto &dhcpReservation = dhcpReservationCollection[i];
                log(LOG_DEBUG, "collect QCMAP DHCP reservation (type=%s,i=%d,%s)", configTypeName, i,
                    ConfigTraits::getLogConfigEntry(dhcpReservation).c_str());
                dhcpReservationSetFromQcMap.emplace(&dhcpReservation);
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // reconfig DNAT rules
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        auto dhcpReservationToDelete = setOp.getDifference(dhcpReservationSetFromQcMap, dhcpReservationSetFromRdb);
        auto dhcpReservationToAdd = setOp.getDifference(dhcpReservationSetFromRdb, dhcpReservationSetFromQcMap);
        log(LOG_INFO, "reconfig (configTypeName=%s,qcmap=%d,rdb=%d,delete=%d,add=%d)", configTypeName, dhcpReservationSetFromQcMap.size(),
            dhcpReservationSetFromRdb.size(), dhcpReservationToDelete.size(), dhcpReservationToAdd.size());

        for (auto &dhcpReservation : dhcpReservationToDelete) {
            log(LOG_DEBUG, "delete config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*dhcpReservation).c_str());
            qcMap->callSafe(ConfigTraits::qcMapDeleteConfig, dhcpReservation);
        }

        for (auto &dhcpReservation : dhcpReservationToAdd) {
            log(LOG_DEBUG, "add config (configTypeName=%s,%s)", configTypeName, ConfigTraits::getLogConfigEntry(*dhcpReservation).c_str());
            qcMap->callSafe(ConfigTraits::qcMapAddConfig, dhcpReservation);
        }
    });
} // namespace eqmi

/**
 * @brief Initiates VLAN RDBs.
 *
 */
void RdbQcMapBridge::initVlanRdb()
{
    log(LOG_DEBUG, "start initialising VLAN RDBs");

    rdbVlanCollection.addChild("num");

    auto vlanHanlder = [this](erdb::Rdb &rdb) {
        rdbVlanCollection.clearChildren(erdb::Rdb::ChildPattern::number);

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // collect RDB VLANs
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        qcmap_msgr_vlan_config_v01 localVlanConfig = {};
        estd::safeStrCpy(localVlanConfig.local_iface, localNwInterface);

        // collect configured VLAN Rdbs
        std::map<int, erdb::Rdb *> vlanIdToVlanRdbMap;

        auto vlanEnable = rdbVlanCollection.children["enable"].get().toBool();
        auto vlanNumOfChildren = rdbVlanCollection.children["num"].get().toInt(0);
        log(LOG_DEBUG, "got RDB VLAN config (en=%d,len=%d)", vlanEnable, vlanNumOfChildren);

        if (vlanEnable) {
            // read RDBs
            for (int childIndex = 0; childIndex < vlanNumOfChildren; ++childIndex) {
                auto &vlanRdb = rdbVlanCollection.addChild(childIndex);
                // TODO: add admin_access_allowed
                vlanRdb.addChildren({ "name", "enable", "interface", "id", "address", "netmask", "ipa_offload", "dhcp" });
                vlanRdb.children["dhcp"].addChildren({ "range", "lease" });
                vlanRdb.readChildren();
                vlanRdb.children["dhcp"].readChildren();
            }

            for (int childIndex = 0; childIndex < vlanNumOfChildren; ++childIndex) {
                auto &vlanRdb = rdbVlanCollection.children[childIndex];
                int vlanId = vlanRdb.children["id"].get(false).toInt(-1);
                if (vlanRdb.children["enable"].get(false).toBool() && (vlanId >= 0)) {

                    log(LOG_DEBUG, "add RDB VLAN config (id=%d)", vlanId);
                    vlanIdToVlanRdbMap.emplace(vlanId, &vlanRdb);
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // collect QCMAP VLANs
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        qcmap_msgr_vlan_conf_t vlanConfigCollection;
        std::map<int, const qcmap_msgr_vlan_config_v01 *> vlanIdToVlanConfigMap;

        if (qcMap->callSafe(&QCMAP_Client::GetVLANConfig, &vlanConfigCollection)) {

            log(LOG_DEBUG, "got current QCMAP VLAN config (len=%d)", vlanConfigCollection.vlan_config_list_len);
            for (auto i = 0; i < vlanConfigCollection.vlan_config_list_len; ++i) {
                const auto &vlanConfig = vlanConfigCollection.vlan_config_list[i];

                log(LOG_DEBUG, "check QCMAP VLAN config (if=%s,id=%d,offload=%d)", vlanConfig.local_iface, vlanConfig.vlan_id,
                    vlanConfig.ipa_offload);
                if (estd::safeStrCmp(vlanConfig.local_iface, localNwInterface) == 0) {
                    log(LOG_DEBUG, "add QCMAP VLAN config (if=%s,id=%d,offload=%d)", vlanConfig.local_iface, vlanConfig.vlan_id,
                        vlanConfig.ipa_offload);
                    vlanIdToVlanConfigMap.emplace(vlanConfig.vlan_id, &vlanConfig);
                }
            }
        } else {
            log(LOG_ERR, "failed in Client::GetVLANConfig()");
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // apply RDB VLANs
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        log(LOG_DEBUG, "got RDB conf map (size=%d)", vlanIdToVlanRdbMap.size());
        log(LOG_DEBUG, "got QCMAP conf map (size=%d)", vlanIdToVlanConfigMap.size());

        // remove QCMAP-configured VLANs that are not configured in RDBs
        for (auto &[vlanId, vlanConfig] : vlanIdToVlanConfigMap) {
            if (vlanIdToVlanRdbMap.find(vlanId) == vlanIdToVlanRdbMap.end()) {
                if (estd::safeStrCmp(vlanConfig->local_iface, localNwInterface) == 0) {
                    log(LOG_DEBUG, "orphan VLAN config found, delete VLAN config (id=%d)", vlanId);
                    qcMap->callSafe(static_cast<boolean (QCMAP_Client::*)(qcmap_msgr_vlan_config_v01,qmi_error_type_v01*)>(&QCMAP_Client::DeleteVLANConfig), *vlanConfig);
                } else {
                    log(LOG_DEBUG, "keep orphan VLAN config (iface=%s)", vlanConfig->local_iface);
                }
            }
        }

        // update VLAN settings
        for (auto &[vlanId, vlanRdb] : vlanIdToVlanRdbMap) {

            qcmap_msgr_vlan_config_v01 vlanConfig{};

            auto it = vlanIdToVlanConfigMap.find(vlanId);
            if (it == vlanIdToVlanConfigMap.end()) {
                log(LOG_DEBUG, "new VLAN config RDB found, add VLAN config (id=%d,rdb=%s)", vlanId, vlanRdb->getDispName().c_str());
            } else {
                log(LOG_DEBUG, "existing VLAN config RDB found, update VLAN config (id=%d,rdb=%s)", vlanId, vlanRdb->getDispName().c_str());
                vlanConfig = *it->second;
            }

            estd::safeStrCpy(vlanConfig.local_iface, localNwInterface);
            vlanConfig.ipa_offload = vlanRdb->children["ipa_offload"].get(false).toBool(true);
            vlanConfig.vlan_id = static_cast<int16_t>(vlanId);

            log(LOG_INFO, "setting new RDB VLAN config (if=%s,id=%d,ipa=%d,rdb=%s)", vlanConfig.local_iface, vlanConfig.vlan_id,
                vlanConfig.ipa_offload, vlanRdb->getDispName().c_str());

            bool ipaOffloaded;
            qcMap->callSafe(&QcMapClient::SetVLANConfig, vlanConfig, &ipaOffloaded);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // collect QCMAP-configured VLAN Mappings
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        qcmap_msgr_pdn_to_vlan_mapping_v01 vlanMappingCollection[QCMAP_MAX_NUM_BACKHAULS_V01];
        int numOfVlanMappings;
        std::map<int, qcmap_msgr_pdn_to_vlan_mapping_v01 *> vlanIdToVlanMappingMap;
        if (qcMap->callSafe(&QCMAP_Client::GetPDNtoVLANMappings, vlanMappingCollection, &numOfVlanMappings)) {
            log(LOG_DEBUG, "got VLAN mappings (size=%d)", numOfVlanMappings);
            for (int i = 0; i < numOfVlanMappings; ++i) {
                auto &vlanMapping = vlanMappingCollection[i];

                if (vlanMapping.vlan_id > 0) {
                    log(LOG_DEBUG, "add QCMAP VLAN Mapping (id=%d, pdn=%d)", vlanMapping.vlan_id, vlanMapping.profile_handle);
                    vlanIdToVlanMappingMap.emplace(vlanMapping.vlan_id, &vlanMapping);
                } else {
                    log(LOG_DEBUG, "skip default QCMAP VLAN Mapping (id=%d, pdn=%d)", vlanMapping.vlan_id, vlanMapping.profile_handle);
                }
            }
        }

        // Get the mappings from the policy to the vlan and vlanId
        std::map<int, unsigned int> vlanIdToPDNMap;
        if (vlanEnable) {
            for (auto &[_, profile]: rdbProfiles.children) {
                unsigned int policyIndex = static_cast<unsigned int>(profile->getNameInt());
                int vlanIndex = profile->children["vlan_index"].get().toInt(-1);
                if (vlanIndex >= 0 && vlanIndex < vlanNumOfChildren) {
                    int vlanId = rdbVlanCollection.children[vlanIndex].children["id"].get().toInt(-1);
                    bool enabled = rdbVlanCollection.children[vlanIndex].children["enable"].get().toBool();
                    if (enabled && vlanId >= 0) {
                        vlanIdToPDNMap[vlanId] = policyIndex;
                    }
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // apply RDB-configured VLANs mappings
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        const auto uintDef = std::numeric_limits<unsigned int>::max();

        // remove VLAN map that we have not configured in RDBs
        for (auto &[vlanId, vlanMapping] : vlanIdToVlanMappingMap) {

            auto it = vlanIdToVlanRdbMap.find(vlanId);
            auto it_policyIndex = vlanIdToPDNMap.find(vlanId);
            auto policyIndex = it_policyIndex == vlanIdToPDNMap.end() ? uintDef : it_policyIndex->second;
            if ((it == vlanIdToVlanRdbMap.end()) || (policyIndex != vlanMapping->profile_handle)) {
                log(LOG_INFO, "delete VLAN mapping (id=%d, pdn=%d)", vlanMapping->vlan_id, vlanMapping->profile_handle);
                qcMap->callSafe(&QCMAP_Client::DeletePDNToVLANMapping, vlanMapping->vlan_id, vlanMapping->profile_handle);
            } else {
                log(LOG_INFO, "keep VLAN mapping (id=%d, pdn=%d)", vlanMapping->vlan_id, vlanMapping->profile_handle);
            }
        }

        // add VLAN maps
        for (auto &[vlanId, vlanRdb] : vlanIdToVlanRdbMap) {
            if (vlanIdToVlanMappingMap.find(vlanId) == vlanIdToVlanMappingMap.end()) {
                auto it = vlanIdToPDNMap.find(vlanId);
                if (it != vlanIdToPDNMap.end()) {
                    log(LOG_INFO, "add new VLAN mapping (rdb=%s, id=%d, pdn=%d)", vlanRdb->getDispName().c_str(), vlanId, it->second);
                    qcMap->callSafe(&QCMAP_Client::AddPDNToVLANMapping, vlanId, it->second);
                } else {
                    log(LOG_INFO, "vlan_index not configured, skip new VLAN mapping (rdb=%s, id=%d)", vlanRdb->getDispName().c_str(), vlanId);
                }
            }
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // apply QCMAP-configured LAN
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        log(LOG_DEBUG, "apply DHCP/LAN settings for bridges");
        for (auto &[vlanId, vlanRdb] : vlanIdToVlanRdbMap) {
            // If the VLAN is mapped to the default profile, the bridge interface will be bridge0, so use the address and
            // dhcp settings from link.profile.0
            auto it = vlanIdToPDNMap.find(vlanId);
            log(LOG_INFO, "apply DHCP/LAN for vlanId: %i, PDN: %i", vlanId, it == vlanIdToPDNMap.end() ? -1 : it->second);
            if (it != vlanIdToPDNMap.end() && rdbProfiles.children[it->second].children["defaultroute"].get().toBool()) {
                auto &children = rdbProfile0.children;
                auto &dhcp = rdbDhcp.children;
                applyLanConfig(vlanId, &children["address"], &children["netmask"], nullptr, &dhcp["range.0"], &dhcp["lease.0"]);
            } else  {
                auto &children = vlanRdb->children;
                auto &dhcp = children["dhcp"].children;
                applyLanConfig(vlanId, &children["address"], &children["netmask"], nullptr, &dhcp["range"], &dhcp["lease"]);
            }
        }

        // Trigger ip passthrough, since VLAN <=> PDN mappings might have changed
        if (rdbIpPassthrough.children.contains("trigger_enable")) {
            rdbIpPassthrough.children["trigger_enable"].set("");
        }
    };

    rdbVlanCollection.addChild("enable");
    // setup vlan triggers
    // Don't set this one to "", it causes unneccessary triggers to ipv6_data_vlan.template
    rdbVlanCollection.addChild("trigger").setRdbFlags(0).subscribe(vlanHanlder);

    // subscribe changes to vlan_index, to reconfigure the VLAN <=> PDN mappings
    for (auto &[_, profile]: rdbProfiles.children) {
        rdbIpPassthroughTriggerList.push_back(profile->children["vlan_index"]);
        auto &vl_rdb = rdbIpPassthroughTriggerList.back();
        vl_rdb.subscribe(vlanHanlder, false);
    }
}

/**
 * @brief Initiates link.profile.0 RDBs
 *
 */
void RdbQcMapBridge::initProfile0Rdb()
{
    log(LOG_DEBUG, "start initialising Profile RDBs");

    auto dhcpHandler = [this](const erdb::Rdb &rdb) {
        log(LOG_DEBUG, "start LAN/DHCP handler");

        rdbProfile0.readChildren();
        rdbDhcp.readChildren();

        applyLanConfig(0, &rdbProfile0.children["address"], &rdbProfile0.children["netmask"], &rdbDhcp.children["enable"],
                       &rdbDhcp.children["range.0"], &rdbDhcp.children["lease.0"]);
        // Re-triger IP Passthrough, if setup
        if (rdbIpPassthrough.children.contains("trigger_enable")) {
            rdbIpPassthrough.children["trigger_enable"].set("");
        }
    };

    // setup link.proifle.0 rdbs
    rdbProfile0.addChild("dev").set("bridge0");
    auto rdbProfile0Triggers = rdbProfile0.addChildren({ "address", "netmask" });
    rdbProfile0Triggers.runForChildren([this, &dhcpHandler](erdb::Rdb &rdb) {
        rdb.subscribeForChange(dhcpHandler, false);
    });

    // setup dhcp rdbs
    rdbDhcp.addChildren({ "lease.0", "range.0" });
    // NOTE: we may not need dhcp::enable RDB as a trigger.
    rdbDhcp.addChild("enable").subscribe(dhcpHandler, false);
    rdbDhcp.addChild("trigger").setRdbFlags(0).set("").subscribe(dhcpHandler);
}

void RdbQcMapBridge::poll()
{
    for (auto &[_, policy] : rdbPolicies.children) {
        getPolicyStats(*policy);
    }
}

void RdbQcMapBridge::run()
{
    erdb::Session &rdbSess = erdb::Session::getInstance();
    auto pollIntervalSecs = std::chrono::seconds(pollSecs);
    auto pollTimeout = std::chrono::steady_clock::now() + pollIntervalSecs;

    while (running) {
        struct timeval selectTimeout;
        auto now = std::chrono::steady_clock::now();
        // check whether poll timer has expired
        if (now < pollTimeout) {
            // timer has not expired, calculate timeout for select
            auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(pollTimeout - now);
            selectTimeout.tv_sec = durationUs.count() / 1000000;
            selectTimeout.tv_usec = durationUs.count() - selectTimeout.tv_sec * 1000000;
        } else {
            poll();
            // reset poll timeout
            pollTimeout = now + pollIntervalSecs;
            continue;
        }

        ::fd_set rfds;

        FD_ZERO(&rfds);

        int sessionFd = rdbSess.setfdSet(rfds);
        int rc = ::select(sessionFd + 1, &rfds, NULL, NULL, &selectTimeout);

        if (rc > 0) {
            rdbSess.processSubscription(rfds);
        }
    }
}

bool RdbQcMapBridge::setMtuForPolicy(int policyIndex)
{
    auto mtuSize = rdbProfiles.children[policyIndex].children["mtu"].get(true).toInt(-1);
    if (mtuSize < 0) {
        log(LOG_NOTICE, "failed to get mtu size from rdb for profile %d", policyIndex);
        return false;
    }
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        log(LOG_ERR, "failed to open socket");
        return false;
    }
    bool succ = false;
    auto interface = rdbPolicies.children[policyIndex].children["ifdev"].get(false).toStdString();
    struct ifreq ifMtu;
    estd::safeStrCpy(ifMtu.ifr_name, interface.c_str());
    if (ioctl(fd, SIOCGIFMTU, &ifMtu) < 0) {
        log(LOG_ERR, "ioctl failed to get mtu");
    } else if (ifMtu.ifr_mtu != mtuSize) {
        ifMtu.ifr_mtu = mtuSize;
        log(LOG_NOTICE, "setting mtu to %d on %s", mtuSize, interface.c_str());
        if (ioctl(fd, SIOCSIFMTU, &ifMtu) < 0) {
            log(LOG_ERR, "ioctl failed to set mtu");
        } else {
            succ = true;
        }
    }
    close(fd);
    return succ;
}

void RdbQcMapBridge::subscribeProfileMtu()
{
    for (auto &[_, profile] : rdbProfiles.children) {
        int policyIndex = profile->getNameInt();
        auto mtuHandler = [this, policyIndex](const erdb::Rdb &rdb) {
            setMtuForPolicy(policyIndex);
        };
        profile->children["mtu"].subscribe(mtuHandler, false);
    }
}

RdbQcMapBridge::RdbQcMapBridge() : indicationHandlerMap(), qcMap(), wds(), pollSecs(1), policyStatsMap{}
{
    log(LOG_DEBUG, "create QCMAP client");
    qcMap = std::make_shared<QcMapClient>([this](qmi_client_type userHandle, unsigned int msgId, void *indBuf, unsigned int indBufLen) {
        log(LOG_DEBUG, "call onRawQcMapIndication");
        onRawQcMapIndication(userHandle, msgId, indBuf, indBufLen);
    });

    log(LOG_DEBUG, "initiate QCMAP indication handle map");
    initIndicationHandlerMap();

    log(LOG_DEBUG, "enable QCMAP client");
    qcMap->EnableMobileAP();

    log(LOG_DEBUG, "register indiciations with QCMAP");
    uint64_t indToRegMask = 0;

    /*
        NOTE:

        It appears that the PACKET_STATS_STATUS_IND indication is not to monitor interfaces. Instead, it seems to be for packet statistics.
        QCMap client does not receive PACKET_STATS_STATUS_IND indications when an RMNET interface gets connected.

        So, in order to get the network interface information, instead of using PACKET_STATS_STATUS_IND,

    */

    // initiate policy settings
    initPolicyRdbs();
    configModemPolicySettings();
    subscribePolicyRdbs();

    // start receiving the WWAN indication before reading policy from modem.
    indToRegMask |= WWAN_STATUS_IND;
    qcMap->RegisterForIndications(indToRegMask);
    updateAllPoliciesStatus();

    // initiate LAN/DHCP settings
    initProfile0Rdb();
    initGenericListRdb<qcmap_msgr_dhcp_reservation_v01>(rdbDhcpResCollection);

    // initiate VLAN settings
    initVlanRdb();
    // initiate IP passthrough
    initIpPassthrough();

    subscribeProfileMtu();

    // initiate NAT RDBs
    initDnatRdb<qcmap_msgr_snat_entry_config_v01>(rdbDnatIpv4Collection);
    initDnatRdb<qcmap_msgr_snat_v6_entry_config_v01>(rdbDnatIpv6Collection);
    // initialise WAN firewall RDBs
    initWanFirewallSettings();
    initWanFirewallRdb(rdbWanIpv4FirewallCollection);
    initWanFirewallRdb(rdbWanIpv6FirewallCollection);
}
} // namespace eqmi
