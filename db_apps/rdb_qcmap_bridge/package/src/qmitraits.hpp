#ifndef __QMITRAITS_H__
#define __QMITRAITS_H__

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

#include <functional>
#include <set>

#include <QCMAP_Client.h>
#include <qualcomm_mobile_access_point_msgr_v01.h>

#include <inet.hpp>

#include "serialization.hpp"
#include "setoperation.hpp"

namespace eqmi
{
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Config structure traits for QCMAP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct QcMapMsgrConfigTraitHelper
{};

template <typename T>
struct QcMapMsgrConfigTraits : public QcMapMsgrConfigTraitHelper<T>
{
    typedef std::vector<typename QcMapMsgrConfigTraitHelper<T>::ConfigType> VectorType;
    typedef std::set<typename QcMapMsgrConfigTraitHelper<T>::ConfigType *, typename QcMapMsgrConfigTraitHelper<T>::Less> PointerSetType;
    typedef SetOperation<PointerSetType> SetOperationType;

    static std::string getConfigTypeName()
    {
        return estd::demangleType<T>();
    }
};

template <>
struct QcMapMsgrConfigTraitHelper<qcmap_msgr_snat_v6_entry_config_v01>
{
    typedef qcmap_msgr_snat_v6_entry_config_v01 ConfigType;

    struct Less : public std::binary_function<ConfigType *, ConfigType *, bool>
    {
        bool operator()(const ConfigType *v1, const ConfigType *v2) const
        {
            auto v1tie = std::tie(v1->global_port, v1->private_port, v1->protocol);
            auto v2tie = std::tie(v2->global_port, v2->private_port, v2->protocol);

            if (v1tie == v2tie) {
                estd::BlindUnaryLess bl(v1->port_fwding_private_ip6_addr);
                return bl(v2->port_fwding_private_ip6_addr);
            } else {
                return v1tie < v2tie;
            }
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef DnatIpv6Deserializer Deserializer;

    static constexpr int maxConfigs = QCMAP_MSGR_MAX_SNAT_ENTRIES_V01;
    static constexpr auto qcMapGetConfigs = &QCMAP_Client::GetStaticNatConfig_Ipv6;
    static constexpr auto qcMapAddConfig = &QCMAP_Client::AddStaticNatEntry_Ipv6;
    static constexpr auto qcMapDeleteConfig = &QCMAP_Client::DeleteStaticNatEntry_Ipv6;

    static std::string getLogConfigEntry(const ConfigType &config)
    {
        return estd::format("ip=%s,port=%d,gport=%d,protocol=%d", estd::inet_ntop<AF_INET6>(config.port_fwding_private_ip6_addr).c_str(),
                            config.private_port, config.global_port, config.protocol);
    }
};

template <>
struct QcMapMsgrConfigTraitHelper<qcmap_msgr_snat_entry_config_v01>
{
    typedef qcmap_msgr_snat_entry_config_v01 ConfigType;

    struct Less : public std::binary_function<ConfigType *, ConfigType *, bool>
    {
        bool operator()(const ConfigType *v1, const ConfigType *v2) const
        {
            auto v1tie = std::tie(v1->global_port, v1->private_port, v1->protocol);
            auto v2tie = std::tie(v2->global_port, v2->private_port, v2->protocol);

            if (v1tie == v2tie) {
                estd::BlindUnaryLess bl(v1->private_ip_addr);
                return bl(v2->private_ip_addr);
            } else {
                return v1tie < v2tie;
            }
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef DnatIpv4Deserializer Deserializer;

    static constexpr int maxConfigs = QCMAP_MSGR_MAX_SNAT_ENTRIES_V01;
    static constexpr auto qcMapGetConfigs = &QCMAP_Client::GetStaticNatConfig;
    static constexpr auto qcMapAddConfig = &QCMAP_Client::AddStaticNatEntry;
    static constexpr auto qcMapDeleteConfig = &QCMAP_Client::DeleteStaticNatEntry;

    static std::string getLogConfigEntry(const ConfigType &config)
    {
        return estd::format("ip=%s,port=%d,gport=%d,protocol=%d", estd::inet_ntop<AF_INET>(estd::hton(config.private_ip_addr)).c_str(),
                            config.private_port, config.global_port, config.protocol);
    }
};

template <>
struct QcMapMsgrConfigTraitHelper<qcmap_msgr_firewall_entry_conf_t>
{
    using ConfigType = qcmap_msgr_firewall_entry_conf_t;
    // union config type including firewall_entry and firewall_handle_list
    using UConfigType = qcmap_msgr_firewall_conf_t;

    // needed for set operation
    struct Less
    {
        bool operator()(const ConfigType *v1, const ConfigType *v2) const
        {
            if (v1->firewall_direction < v2->firewall_direction) {
                return true;
            }
            if (v1->firewall_direction > v2->firewall_direction) {
                return false;
            }
            auto & spec1 = v1->filter_spec;
            auto & spec2 = v2->filter_spec;
            if (spec1.ip_vsn < spec2.ip_vsn) {
                return true;
            }
            if (spec1.ip_vsn > spec2.ip_vsn) {
                return false;
            }
            uint8 next_hdr_prot = PS_IPPROTO_NO_PROTO;

            if (spec1.ip_vsn == IP_V4) {
                auto & hdr1 = spec1.ip_hdr.v4;
                auto & hdr2 = spec2.ip_hdr.v4;
                if (hdr1.field_mask < hdr2.field_mask) {
                    return true;
                }
                if (hdr1.field_mask > hdr2.field_mask) {
                    return false;
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP4_SRC_ADDR) != 0) {
                    int r = estd::safeMemCmp(hdr1.src.addr, hdr2.src.addr);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                    r = estd::safeMemCmp(hdr1.src.subnet_mask, hdr2.src.subnet_mask);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP4_DST_ADDR) != 0) {
                    int r = estd::safeMemCmp(hdr1.dst.addr, hdr2.dst.addr);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                    r = estd::safeMemCmp(hdr1.dst.subnet_mask, hdr2.dst.subnet_mask);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP4_TOS) != 0) {
                    auto v1tie = std::tie(hdr1.tos.val, hdr1.tos.mask);
                    auto v2tie = std::tie(hdr2.tos.val, hdr2.tos.mask);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP4_NEXT_HDR_PROT) != 0) {
                    if (hdr1.next_hdr_prot < hdr2.next_hdr_prot) {
                        return true;
                    }
                    if (hdr1.next_hdr_prot > hdr2.next_hdr_prot) {
                        return false;
                    }
                    next_hdr_prot = hdr1.next_hdr_prot;
                }
            } else if (spec1.ip_vsn == IP_V6) {
                auto & hdr1 = spec1.ip_hdr.v6;
                auto & hdr2 = spec2.ip_hdr.v6;
                if (hdr1.field_mask < hdr2.field_mask) {
                    return true;
                }
                if (hdr1.field_mask > hdr2.field_mask) {
                    return false;
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP6_SRC_ADDR) != 0) {
                    int r = estd::safeMemCmp(hdr1.src.addr, hdr2.src.addr);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                    if (hdr1.src.prefix_len < hdr2.src.prefix_len) {
                        return true;
                    }
                    if (hdr1.src.prefix_len > hdr2.src.prefix_len) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP6_DST_ADDR) != 0) {
                    int r = estd::safeMemCmp(hdr1.dst.addr, hdr2.dst.addr);
                    if (r < 0) {
                        return true;
                    }
                    if (r > 0) {
                        return false;
                    }
                    if (hdr1.dst.prefix_len < hdr2.dst.prefix_len) {
                        return true;
                    }
                    if (hdr1.dst.prefix_len > hdr2.dst.prefix_len) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP6_TRAFFIC_CLASS) != 0) {
                    auto v1tie = std::tie(hdr1.trf_cls.val, hdr1.trf_cls.mask);
                    auto v2tie = std::tie(hdr2.trf_cls.val, hdr2.trf_cls.mask);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                if ((hdr1.field_mask & IPFLTR_MASK_IP6_NEXT_HDR_PROT) != 0) {
                    if (hdr1.next_hdr_prot < hdr2.next_hdr_prot) {
                        return true;
                    }
                    if (hdr1.next_hdr_prot > hdr2.next_hdr_prot) {
                        return false;
                    }
                    next_hdr_prot = hdr1.next_hdr_prot;
                }
                if (hdr1.ipv6_nat_enabled < hdr2.ipv6_nat_enabled) {
                    return true;
                }
                if (hdr1.ipv6_nat_enabled > hdr2.ipv6_nat_enabled) {
                    return false;
                }
            }

            if (next_hdr_prot == PS_IPPROTO_NO_PROTO) {
                return false;
            }
            auto & hdr1 = spec1.next_prot_hdr;
            auto & hdr2 = spec2.next_prot_hdr;
            switch (next_hdr_prot) {
            case PS_IPPROTO_TCP:
                if (hdr1.tcp.field_mask < hdr2.tcp.field_mask) {
                    return true;
                }
                if (hdr1.tcp.field_mask > hdr2.tcp.field_mask) {
                    return false;
                }
                if ((hdr1.tcp.field_mask & IPFLTR_MASK_TCP_SRC_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.tcp.src.port, hdr1.tcp.src.range);
                    auto v2tie = std::tie(hdr2.tcp.src.port, hdr2.tcp.src.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                if ((hdr1.tcp.field_mask & IPFLTR_MASK_TCP_DST_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.tcp.dst.port, hdr1.tcp.dst.range);
                    auto v2tie = std::tie(hdr2.tcp.dst.port, hdr2.tcp.dst.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                break;

            case PS_IPPROTO_UDP:
                if (hdr1.udp.field_mask < hdr2.udp.field_mask) {
                    return true;
                }
                if (hdr1.udp.field_mask > hdr2.udp.field_mask) {
                    return false;
                }
                if ((hdr1.udp.field_mask & IPFLTR_MASK_UDP_SRC_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.udp.src.port, hdr1.udp.src.range);
                    auto v2tie = std::tie(hdr2.udp.src.port, hdr2.udp.src.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                if ((hdr1.udp.field_mask & IPFLTR_MASK_UDP_DST_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.udp.dst.port, hdr1.udp.dst.range);
                    auto v2tie = std::tie(hdr2.udp.dst.port, hdr2.udp.dst.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                break;

            case PS_IPPROTO_ICMP:
            case PS_IPPROTO_ICMP6:
                if (hdr1.icmp.field_mask < hdr2.icmp.field_mask) {
                    return true;
                }
                if (hdr1.icmp.field_mask > hdr2.icmp.field_mask) {
                    return false;
                }
                if ((hdr1.icmp.field_mask & IPFLTR_MASK_ICMP_MSG_TYPE) != 0) {
                    if (hdr1.icmp.type < hdr2.icmp.type) {
                        return true;
                    }
                    if (hdr1.icmp.type > hdr2.icmp.type) {
                        return false;
                    }
                }
                if ((hdr1.icmp.field_mask & IPFLTR_MASK_ICMP_MSG_CODE) != 0) {
                    if (hdr1.icmp.code < hdr2.icmp.code) {
                        return true;
                    }
                    if (hdr1.icmp.code > hdr2.icmp.code) {
                        return false;
                    }
                }
                break;

            case PS_IPPROTO_ESP:
                if (hdr1.esp.field_mask < hdr2.esp.field_mask) {
                    return true;
                }
                if (hdr1.esp.field_mask > hdr2.esp.field_mask) {
                    return false;
                }
                if ((hdr1.esp.field_mask & IPFLTR_MASK_ESP_SPI) != 0) {
                    if (hdr1.esp.spi < hdr2.esp.spi) {
                        return true;
                    }
                    if (hdr1.esp.spi > hdr2.esp.spi) {
                        return false;
                    }
                }
                break;

            case PS_IPPROTO_TCP_UDP:
                if (hdr1.tcp_udp_port_range.field_mask < hdr2.tcp_udp_port_range.field_mask) {
                    return true;
                }
                if (hdr1.tcp_udp_port_range.field_mask > hdr2.tcp_udp_port_range.field_mask) {
                    return false;
                }
                if ((hdr1.tcp_udp_port_range.field_mask & IPFLTR_MASK_TCP_UDP_SRC_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.tcp_udp_port_range.src.port, hdr1.tcp_udp_port_range.src.range);
                    auto v2tie = std::tie(hdr2.tcp_udp_port_range.src.port, hdr2.tcp_udp_port_range.src.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                if ((hdr1.tcp_udp_port_range.field_mask & IPFLTR_MASK_TCP_UDP_DST_PORT) != 0) {
                    auto v1tie = std::tie(hdr1.tcp_udp_port_range.dst.port, hdr1.tcp_udp_port_range.dst.range);
                    auto v2tie = std::tie(hdr2.tcp_udp_port_range.dst.port, hdr2.tcp_udp_port_range.dst.range);
                    if (v1tie < v2tie) {
                        return true;
                    }
                    if (v1tie > v2tie) {
                        return false;
                    }
                }
                break;
            }
            return false;
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr int maxConfigs = QCMAP_MSGR_MAX_FIREWALL_ENTRIES_V01;
    static constexpr auto qcMapGetHandles = &QCMAP_Client::GetFireWallHandlesList;
    static constexpr auto qcMapGetEntry = &QCMAP_Client::GetFireWallEntry;
    static constexpr auto qcMapAddEntry = &QCMAP_Client::AddFireWallEntry;
    static constexpr auto qcMapDeleteEntry = &QCMAP_Client::DeleteFireWallEntry;

    // used for debug log only
    static std::string getLogConfigEntry(const ConfigType &config)
    {
        auto res = estd::format("handle=%d,dir=%d,ipv%d", config.firewall_handle, config.firewall_direction, config.filter_spec.ip_vsn);
        uint8 next_hdr_prot = PS_IPPROTO_NO_PROTO;
        if (config.filter_spec.ip_vsn == IP_V4) {
            auto & hdr = config.filter_spec.ip_hdr.v4;
            if ((hdr.field_mask & IPFLTR_MASK_IP4_SRC_ADDR) != 0) {
                res += estd::format(",src.addr=%s,src.mask=%s", estd::inet_ntop<AF_INET>(hdr.src.addr.ps_s_addr).c_str(), estd::inet_ntop<AF_INET>(hdr.src.subnet_mask.ps_s_addr).c_str());
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP4_DST_ADDR) != 0) {
                res += estd::format(",dst.addr=%s,dst.mask=%s", estd::inet_ntop<AF_INET>(hdr.dst.addr.ps_s_addr).c_str(), estd::inet_ntop<AF_INET>(hdr.dst.subnet_mask.ps_s_addr).c_str());
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP4_TOS) != 0) {
                res += estd::format(",tos.val=%02x,tos.mask=%02x", hdr.tos.val, hdr.tos.mask);
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP4_NEXT_HDR_PROT) != 0) {
                res += estd::format(",proto=%d", hdr.next_hdr_prot);
                next_hdr_prot = hdr.next_hdr_prot;
            }
        } else {
            auto & hdr = config.filter_spec.ip_hdr.v6;
            if ((hdr.field_mask & IPFLTR_MASK_IP6_SRC_ADDR) != 0) {
                res += estd::format(",src.addr=%s,src.prefix=%d", estd::inet_ntop<AF_INET6>(hdr.src.addr.ps_s6_addr).c_str(), hdr.src.prefix_len);
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP6_DST_ADDR) != 0) {
                res += estd::format(",dst.addr=%s,dst.prefix=%d", estd::inet_ntop<AF_INET6>(hdr.dst.addr.ps_s6_addr).c_str(), hdr.dst.prefix_len);
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP6_TRAFFIC_CLASS) != 0) {
                res += estd::format(",tc.val=%02x,tc.mask=%02x", hdr.trf_cls.val, hdr.trf_cls.mask);
            }
            if ((hdr.field_mask & IPFLTR_MASK_IP6_NEXT_HDR_PROT) != 0) {
                res += estd::format(",proto=%d", hdr.next_hdr_prot);
                next_hdr_prot = hdr.next_hdr_prot;
            }
            res += estd::format(",ipv6_nat=%d", hdr.ipv6_nat_enabled);
        }
        auto & hdr = config.filter_spec.next_prot_hdr;
        switch(next_hdr_prot) {
        case PS_IPPROTO_TCP:
            if ((hdr.tcp.field_mask & IPFLTR_MASK_TCP_SRC_PORT) != 0) {
                res += estd::format(",tcp.src.port=%d,%d", hdr.tcp.src.port, hdr.tcp.src.range);
            }
            if ((hdr.tcp.field_mask & IPFLTR_MASK_TCP_DST_PORT) != 0) {
                res += estd::format(",tcp.dst.port=%d,%d", hdr.tcp.dst.port, hdr.tcp.dst.range);
            }
            break;

        case PS_IPPROTO_UDP:
            if ((hdr.udp.field_mask & IPFLTR_MASK_UDP_SRC_PORT) != 0) {
                res += estd::format(",udp.src.port=%d,%d", hdr.udp.src.port, hdr.udp.src.range);
            }
            if ((hdr.udp.field_mask & IPFLTR_MASK_UDP_DST_PORT) != 0) {
                res += estd::format(",udp.dst.port=%d,%d", hdr.udp.dst.port, hdr.udp.dst.range);
            }
            break;

        case PS_IPPROTO_TCP_UDP:
            if ((hdr.tcp_udp_port_range.field_mask & IPFLTR_MASK_TCP_UDP_SRC_PORT) != 0) {
                res += estd::format(",tcpudp.src.port=%d,%d", hdr.tcp_udp_port_range.src.port, hdr.tcp_udp_port_range.src.range);
            }
            if ((hdr.tcp_udp_port_range.field_mask & IPFLTR_MASK_TCP_UDP_DST_PORT) != 0) {
                res += estd::format(",tcpudp.dst.port=%d,%d", hdr.tcp_udp_port_range.dst.port, hdr.tcp_udp_port_range.dst.range);
            }
            break;

        case PS_IPPROTO_ICMP:
        case PS_IPPROTO_ICMP6:
            if ((hdr.icmp.field_mask & IPFLTR_MASK_ICMP_MSG_TYPE) != 0) {
                res += estd::format(",icmp.type=%d", hdr.icmp.type);
            }
            if ((hdr.icmp.field_mask & IPFLTR_MASK_ICMP_MSG_CODE) != 0) {
                res += estd::format(",icmp.code=%d", hdr.icmp.code);
            }
            break;

        case PS_IPPROTO_ESP:
            if ((hdr.esp.field_mask & IPFLTR_MASK_ESP_SPI) != 0) {
                res += estd::format(",esp.spi=%d", hdr.esp.spi);
            }
            break;
        }
        return res;
    }
};

template <>
struct QcMapMsgrConfigTraitHelper<qcmap_msgr_dhcp_reservation_v01>
{
    typedef qcmap_msgr_dhcp_reservation_v01 ConfigType;

    struct Less : public std::binary_function<ConfigType *, ConfigType *, bool>
    {
        bool operator()(const ConfigType *v1, const ConfigType *v2) const
        {
            auto v1tie = std::tie(v1->client_reserved_ip, v1->enable_reservation);
            auto v2tie = std::tie(v2->client_reserved_ip, v2->enable_reservation);

            if (v1tie == v2tie) {
                auto macAddrCmp = estd::safeMemCmp(v1->client_mac_addr, v2->client_mac_addr);

                if (macAddrCmp == 0) {
                    return estd::safeStrCmp(v1->client_device_name, v2->client_device_name) < 0;
                } else {
                    return macAddrCmp < 0;
                }
            } else {
                return v1tie < v2tie;
            }
        }
    };

    typedef DhcpDeserializer Deserializer;

    static constexpr int maxConfigs = QCMAP_MSGR_MAX_DHCP_RESERVATION_ENTRIES_V01;
    static constexpr auto qcMapGetConfigs = &QCMAP_Client::GetDHCPReservRecords;
    static constexpr auto qcMapAddConfig = &QCMAP_Client::AddDHCPReservRecord;
    static constexpr auto qcMapDeleteConfig = [](QcMapClient &self, ConfigType *config, qmi_error_type_v01 *qmi_err_num) -> boolean {
        return self.DeleteDHCPReservRecord(&config->client_reserved_ip, qmi_err_num);
    };

    static std::string getLogConfigEntry(const ConfigType &config)
    {
        return estd::format("devName=%s,mac=%s,ip=%s,en=%d", config.client_device_name, estd::MacAddress::ntop(config.client_mac_addr).c_str(),
                            estd::inet_ntop<AF_INET>(estd::hton(config.client_reserved_ip)).c_str(), config.enable_reservation);
    }
};

} // namespace eqmi

#endif
