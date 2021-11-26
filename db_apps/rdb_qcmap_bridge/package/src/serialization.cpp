/*
 * Serialization
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

#include "serialization.hpp"

#include <tuple>

#include <estd.hpp>
#include <inet.hpp>

namespace eqmi
{

// common deserialize functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static auto deserializeEnable = std::function([](const std::string &str, bool &en) -> bool {
    en = estd::sToT(str) != 0;
    return true;
});

static auto deserializeInt = std::function([](const std::string &str, int &out) -> bool {
    out = estd::sToT(str);

    return true;
});

// deserialise a string into a uint8_t, return true on success or false on invalid input
static auto deserializeUInt8 = std::function([](const std::string &str, uint8_t &out) -> bool {
    int v = estd::sToT(str, -1);
    if (v < 0 || v > 255) {
        return false;
    }
    out = (uint8_t)v;
    return true;
});

// deserialise a string into a uint8_t as IPv6 prefix length, return true on success or false on invalid input
static auto deserializeIp6PrefixLen = std::function([](const std::string &str, uint8_t &out) -> bool {
    int v = estd::sToT(str, -1);
    if (v < 0 || v > 128) {
        return false;
    }
    out = (uint8_t)v;
    return true;
});

// deserialise a string into a uint16_t, return true on success or false on invalid input
static auto deserializeUInt16 = std::function([](const std::string &str, uint16_t &out) -> bool {
    int v = estd::sToT(str, -1);
    if (v < 0 || v > 65535) {
        return false;
    }
    out = (uint16_t)v;
    return true;
});

// deserialise a string into a uint32_t, return true on success or false on invalid input
static auto deserializeUInt32 = std::function([](const std::string &str, uint32_t &out) -> bool {
    int64_t v = estd::sToT<int64_t>(str, -1);
    if (v < 0 || v > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    out = (uint32_t)v;
    return true;
});

// deserialise a string of IPv4 address into a uint32_t, return true on success or false on invalid input
static auto deserializeIpAddr = std::function([](const std::string &str, uint32_t &ipAddr) -> bool {
    try {
        ipAddr = estd::ntoh(estd::inet_pton<AF_INET>(str)).s_addr;
    } catch (const estd::invalid_argument&) {
        return false;
    }
    return true;
});

// deserialise a string of IPv4 address into a ps_in_addr, return true on success or false on invalid input
static auto deserializePsInAddr = std::function([](const std::string &str, ps_in_addr &psInAddr) -> bool {
    try {
        psInAddr.ps_s_addr = estd::inet_pton<AF_INET>(str).s_addr;
    } catch (const estd::invalid_argument&) {
        return false;
    }
    return true;
});

// deserialise a string of IPv6 address into a uint8_t[16], return true on success or false on invalid input
static auto deserializeIpv6Addr = std::function([](const std::string &str, uint8_t (&ipv6Addr_)[QCMAP_MSGR_IPV6_ADDR_LEN_V01]) -> bool {
    try {
        auto ipv6Addr = estd::inet_pton<AF_INET6>(str);

        static_assert(sizeof(ipv6Addr_) == sizeof(ipv6Addr), "IPv6 structure size not matching");

        *reinterpret_cast<decltype(ipv6Addr) *>(ipv6Addr_) = ipv6Addr;
    } catch (const estd::invalid_argument&) {
        return false;
    }
    return true;
});

// deserialise a string of IPv6 address into a ps_in6_addr, return true on success or false on invalid input
static auto deserializePsIn6Addr = std::function([](const std::string &str, ps_in6_addr &psIn6Addr) -> bool {
    try {
        auto ipv6Addr = estd::inet_pton<AF_INET6>(str);

        static_assert(sizeof(psIn6Addr.ps_s6_addr) == sizeof(ipv6Addr), "IPv6 structure size not matching");

        *reinterpret_cast<decltype(ipv6Addr) *>(psIn6Addr.ps_s6_addr) = ipv6Addr;
    } catch (const estd::invalid_argument&) {
        return false;
    }
    return true;
});

// deserialise a string of MAC address into a uint8_t[6], return true on success or false on invalid input
static auto deserializeMacAddr = std::function([](const std::string &str, uint8_t (&macAddr)[QCMAP_MSGR_MAC_ADDR_LEN_V01]) -> bool {
    try {
        estd::MacAddress::pton(str, macAddr);
    } catch (const estd::invalid_argument&) {
        return false;
    }
    return true;
});

static auto deserializeDhcpName = std::function([](const std::string &str, char (&devName)[QCMAP_MSGR_DEVICE_NAME_MAX_V01]) -> bool {
    estd::safeStrCpy(devName, str.c_str());
    return true;
});

static auto deserializePort = std::function([](const std::string &str, unsigned short int &gPort) -> bool {
    gPort = estd::sToT<unsigned short int>(str);
    return gPort != 0;
});

// deserialise a string of port or port range into a uint16_t[2], return true on success or false on invalid input
static auto deserializePortRange = std::function([](const std::string &str, uint16 (&prange)[2]) -> bool {
    std::vector<std::string> v = estd::split(str, ',');
    prange[0] = estd::sToT<unsigned short int>(v[0]);
    if (prange[0] == 0) {
        return false;
    }
    if (v.size() > 1) {
        uint16_t ePort = estd::sToT<unsigned short int>(v[1]);
        if (ePort == 0 || ePort < prange[0]) {
            return false;
        }
        prange[1] = ePort - prange[0];
    } else {
        prange[1] = 0;
    }
    return true;
});

static auto deserializeStr = std::function([](const std::string &str, std::string &out) -> bool {
    out = str;
    return true;
});

static auto deserializeProtocol =
    std::function([](const std::string &str, decltype(std::declval<qcmap_msgr_snat_entry_config_v01>().protocol) &proto) -> bool {
        static estd::DefValueMap<estd::StringView, ps_ip_protocol_enum_type> qcMapStrToProtocolTypeMap = {
            { "TCP", PS_IPPROTO_TCP },
            { "UDP", PS_IPPROTO_UDP },
            { "TCP/UDP", PS_IPPROTO_TCP_UDP },
            { "ICMP", PS_IPPROTO_ICMP },
            { "ICMP6", PS_IPPROTO_ICMP6 },
            { "ESP", PS_IPPROTO_ESP },
        };

        proto = qcMapStrToProtocolTypeMap.get(str.c_str(), PS_IPPROTO_NO_PROTO);
        return proto != PS_IPPROTO_NO_PROTO;
    });

// deserialise a string of firewall direction (UL|DL) into a boolean with a default of DL
static auto deserializeFwDirection =
    std::function([](const std::string &str, decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().firewall_direction) &dir) -> bool {
        static estd::DefValueMap<estd::StringView, qcmap_msgr_firewall_direction> qcMapStrToDirectionTypeMap = {
            { "UL", QCMAP_MSGR_UL_FIREWALL },
            { "DL", QCMAP_MSGR_DL_FIREWALL },
        };

        dir = qcMapStrToDirectionTypeMap.get(str.c_str(), QCMAP_MSGR_DL_FIREWALL);
        return true;
    });

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DhcpDeserializer::DhcpDeserializer()
    : DhcpDeserializerBase(std::make_tuple(deserializeDhcpName, deserializeMacAddr, deserializeIpAddr, deserializeEnable))
{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DnatIpv4Deserializer::DnatIpv4Deserializer()
    : DnatDeserializerBase(std::make_tuple(deserializeStr, deserializeEnable, deserializeInt, deserializePort, deserializeIpAddr, deserializePort,
                                           deserializeProtocol))
{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DnatIpv6Deserializer::DnatIpv6Deserializer()
    : DnatIpv6DeserializerBase(std::make_tuple(deserializeStr, deserializeEnable, deserializeInt, deserializePort, deserializeIpv6Addr,
                                               deserializePort, deserializeProtocol))
{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WanIpv4FirewallDeserializer::WanIpv4FirewallDeserializer()
    : WanIpv4FirewallDeserializerBase(
        std::make_tuple(deserializeEnable, // enable
                        deserializeInt, // policy_index
                        deserializeFwDirection, // dir
                        deserializePsInAddr, // src_addr
                        deserializePsInAddr, // src_mask
                        deserializePsInAddr, // dst_addr
                        deserializePsInAddr, // dst_mask
                        deserializeUInt8, // tos
                        deserializeUInt8, // tos_mask
                        deserializeProtocol, // proto
                        deserializePortRange, // src_port
                        deserializePortRange, // dst_port
                        deserializeUInt8, // icmp_type
                        deserializeUInt8, // icmp_code
                        deserializeUInt32 // esp_spi
            ))
{}

bool WanIpv4FirewallDeserializer::deserialize(
    const serialized_data_t &sd,
    bool &en,
    int &policy_idx,
    qcmap_msgr_firewall_entry_conf_t &fwEntry)
{
    std::array<bool, length()> valid;
    auto &ip_hdr = fwEntry.filter_spec.ip_hdr.v4;
    auto &prot_hdr = fwEntry.filter_spec.next_prot_hdr;
    uint16 sport[2], dport[2];
    decltype(prot_hdr.icmp) icmp;
    decltype(prot_hdr.esp) esp;
    if (!WanIpv4FirewallDeserializerBase::deserialize(
            sd,
            std::tie(
                en,
                policy_idx,
                fwEntry.firewall_direction,
                ip_hdr.src.addr,
                ip_hdr.src.subnet_mask,
                ip_hdr.dst.addr,
                ip_hdr.dst.subnet_mask,
                ip_hdr.tos.val,
                ip_hdr.tos.mask,
                ip_hdr.next_hdr_prot,
                sport,
                dport,
                icmp.type,
                icmp.code,
                esp.spi
                ),
            valid)) {
        return false;
    }
    if (!valid[0] || !valid[1] || !valid[2]) { // mandatory fields
        return false;
    }
    fwEntry.filter_spec.ip_vsn = IP_V4;
    ip_hdr.field_mask = 0;
    if (valid[3] && valid[4]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP4_SRC_ADDR;
    }
    if (valid[5] && valid[6]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP4_DST_ADDR;
    }
    if (valid[7] && valid[8]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP4_TOS;
    }
    if (valid[9]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP4_NEXT_HDR_PROT;
        switch(ip_hdr.next_hdr_prot) {
        case PS_IPPROTO_TCP:
            prot_hdr.tcp.field_mask = 0;
            if (valid[10]) {
                prot_hdr.tcp.field_mask |= IPFLTR_MASK_TCP_SRC_PORT;
                prot_hdr.tcp.src.port = sport[0];
                prot_hdr.tcp.src.range = sport[1];
            }
            if (valid[11]) {
                prot_hdr.tcp.field_mask |= IPFLTR_MASK_TCP_DST_PORT;
                prot_hdr.tcp.dst.port = dport[0];
                prot_hdr.tcp.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_UDP:
            prot_hdr.udp.field_mask = 0;
            if (valid[10]) {
                prot_hdr.udp.field_mask |= IPFLTR_MASK_UDP_SRC_PORT;
                prot_hdr.udp.src.port = sport[0];
                prot_hdr.udp.src.range = sport[1];
            }
            if (valid[11]) {
                prot_hdr.udp.field_mask |= IPFLTR_MASK_UDP_DST_PORT;
                prot_hdr.udp.dst.port = dport[0];
                prot_hdr.udp.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_TCP_UDP:
            prot_hdr.tcp_udp_port_range.field_mask = 0;
            if (valid[10]) {
                prot_hdr.tcp_udp_port_range.field_mask |= IPFLTR_MASK_TCP_UDP_SRC_PORT;
                prot_hdr.tcp_udp_port_range.src.port = sport[0];
                prot_hdr.tcp_udp_port_range.src.range = sport[1];
            }
            if (valid[11]) {
                prot_hdr.tcp_udp_port_range.field_mask |= IPFLTR_MASK_TCP_UDP_DST_PORT;
                prot_hdr.tcp_udp_port_range.dst.port = dport[0];
                prot_hdr.tcp_udp_port_range.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_ICMP:
            prot_hdr.icmp.field_mask = 0;
            if (valid[12]) {
                prot_hdr.icmp.field_mask |= IPFLTR_MASK_ICMP_MSG_TYPE;
                prot_hdr.icmp.type = icmp.type;
            }
            if (valid[13]) {
                prot_hdr.icmp.field_mask |= IPFLTR_MASK_ICMP_MSG_CODE;
                prot_hdr.icmp.code = icmp.code;
            }
            break;

        case PS_IPPROTO_ESP:
            prot_hdr.esp.field_mask = 0;
            if (valid[14]) {
                prot_hdr.esp.field_mask |= IPFLTR_MASK_ESP_SPI;
                prot_hdr.esp.spi = esp.spi;
            }
            break;
        }
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WanIpv6FirewallDeserializer::WanIpv6FirewallDeserializer()
    : WanIpv6FirewallDeserializerBase(
        std::make_tuple(deserializeEnable, // enable
                        deserializeInt, // policy_index
                        deserializeFwDirection, // dir
                        deserializePsIn6Addr, // src_addr
                        deserializeIp6PrefixLen, // src_prefix_len
                        deserializePsIn6Addr, // dst_addr
                        deserializeIp6PrefixLen, // dst_prefix_len
                        deserializeUInt8, // tc
                        deserializeUInt8, // tc_mask
                        deserializeEnable, // nat_enabled
                        deserializeProtocol, // proto
                        deserializePortRange, // src_port
                        deserializePortRange, // dst_port
                        deserializeUInt8, // icmp_type
                        deserializeUInt8, // icmp_code
                        deserializeUInt32 // esp_spi
            ))
{}

bool WanIpv6FirewallDeserializer::deserialize(
    const serialized_data_t &sd,
    bool &en,
    int &policy_idx,
    qcmap_msgr_firewall_entry_conf_t &fwEntry)
{
    std::array<bool, length()> valid;
    auto & ip_hdr = fwEntry.filter_spec.ip_hdr.v6;
    auto & prot_hdr = fwEntry.filter_spec.next_prot_hdr;
    bool ipv6_nat_enabled;
    uint16_t sport[2], dport[2];
    decltype(prot_hdr.icmp) icmp;
    decltype(prot_hdr.esp) esp;
    WanIpv6FirewallDeserializerBase::deserialize(
        sd,
        std::tie(
            en,
            policy_idx,
            fwEntry.firewall_direction,
            ip_hdr.src.addr,
            ip_hdr.src.prefix_len,
            ip_hdr.dst.addr,
            ip_hdr.dst.prefix_len,
            ip_hdr.trf_cls.val,
            ip_hdr.trf_cls.mask,
            ipv6_nat_enabled,
            ip_hdr.next_hdr_prot,
            sport,
            dport,
            icmp.type,
            icmp.code,
            esp.spi
            ),
        valid);
    if (!valid[0] || !valid[1] || !valid[2]) { // mandatory fields
        return false;
    }
    fwEntry.filter_spec.ip_vsn = IP_V6;
    ip_hdr.field_mask = 0;
    if (valid[3] && valid[4]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP6_SRC_ADDR;
    }
    if (valid[5] && valid[6]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP6_DST_ADDR;
    }
    if (valid[7] && valid[8]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP6_TRAFFIC_CLASS;
    }
    if (!valid[9]) { // mandatory
        return false;
    }
    ip_hdr.ipv6_nat_enabled = ipv6_nat_enabled;
    if (valid[10]) {
        ip_hdr.field_mask |= IPFLTR_MASK_IP6_NEXT_HDR_PROT;
        switch(ip_hdr.next_hdr_prot) {
        case PS_IPPROTO_TCP:
            prot_hdr.tcp.field_mask = 0;
            if (valid[11]) {
                prot_hdr.tcp.field_mask |= IPFLTR_MASK_TCP_SRC_PORT;
                prot_hdr.tcp.src.port = sport[0];
                prot_hdr.tcp.src.range = sport[1];
            }
            if (valid[12]) {
                prot_hdr.tcp.field_mask |= IPFLTR_MASK_TCP_DST_PORT;
                prot_hdr.tcp.dst.port = dport[0];
                prot_hdr.tcp.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_UDP:
            prot_hdr.udp.field_mask = 0;
            if (valid[11]) {
                prot_hdr.udp.field_mask |= IPFLTR_MASK_UDP_SRC_PORT;
                prot_hdr.udp.src.port = sport[0];
                prot_hdr.udp.src.range = sport[1];
            }
            if (valid[12]) {
                prot_hdr.udp.field_mask |= IPFLTR_MASK_UDP_DST_PORT;
                prot_hdr.udp.dst.port = dport[0];
                prot_hdr.udp.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_TCP_UDP:
            prot_hdr.tcp_udp_port_range.field_mask = 0;
            if (valid[11]) {
                prot_hdr.tcp_udp_port_range.field_mask |= IPFLTR_MASK_TCP_UDP_SRC_PORT;
                prot_hdr.tcp_udp_port_range.src.port = sport[0];
                prot_hdr.tcp_udp_port_range.src.range = sport[1];
            }
            if (valid[12]) {
                prot_hdr.tcp_udp_port_range.field_mask |= IPFLTR_MASK_TCP_UDP_DST_PORT;
                prot_hdr.tcp_udp_port_range.dst.port = dport[0];
                prot_hdr.tcp_udp_port_range.dst.range = dport[1];
            }
            break;

        case PS_IPPROTO_ICMP6:
            prot_hdr.icmp.field_mask = 0;
            if (valid[13]) {
                prot_hdr.icmp.field_mask |= IPFLTR_MASK_ICMP_MSG_TYPE;
                prot_hdr.icmp.type = icmp.type;
            }
            if (valid[14]) {
                prot_hdr.icmp.field_mask |= IPFLTR_MASK_ICMP_MSG_CODE;
                prot_hdr.icmp.code = icmp.code;
            }
            break;

        case PS_IPPROTO_ESP:
            prot_hdr.esp.field_mask = 0;
            if (valid[15]) {
                prot_hdr.esp.field_mask |= IPFLTR_MASK_ESP_SPI;
                prot_hdr.esp.spi = esp.spi;
            }
            break;
        }
    }
    return true;
}

} // namespace eqmi
