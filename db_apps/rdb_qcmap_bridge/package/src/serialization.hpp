#ifndef __SERIALIZATION_HPP__
#define __SERIALIZATION_HPP__

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

#include <tuple>
#include <type_traits>

#include <comdef.h>
#include <qcmap_client_util.h>
#include <qcmap_firewall_util.h>
#include <qualcomm_mobile_access_point_msgr_v01.h>

#include <estring.hpp>

namespace eqmi
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using DhcpDeserializerBase = estd::Deserializer<decltype(std::declval<qcmap_msgr_dhcp_reservation_v01>().client_device_name),
                                                decltype(std::declval<qcmap_msgr_dhcp_reservation_v01>().client_mac_addr),
                                                decltype(std::declval<qcmap_msgr_dhcp_reservation_v01>().client_reserved_ip), bool>;

class DhcpDeserializer : public DhcpDeserializerBase
{
  public:
    DhcpDeserializer();

    bool deserialize(const serialized_data_t &sd, qcmap_msgr_dhcp_reservation_v01 &dhcpRes, bool &en)
    {
        return DhcpDeserializerBase::deserialize(sd, std::tie(dhcpRes.client_device_name, dhcpRes.client_mac_addr, dhcpRes.client_reserved_ip, en));
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using DnatDeserializerBase = estd::Deserializer<std::string, bool, int, decltype(std::declval<qcmap_msgr_snat_entry_config_v01>().global_port),
                                                decltype(std::declval<qcmap_msgr_snat_entry_config_v01>().private_ip_addr),
                                                decltype(std::declval<qcmap_msgr_snat_entry_config_v01>().private_port),
                                                decltype(std::declval<qcmap_msgr_snat_entry_config_v01>().protocol)>;

class DnatIpv4Deserializer : public DnatDeserializerBase
{
  public:
    DnatIpv4Deserializer();

    bool deserialize(const serialized_data_t &sd, bool &en, std::string &name, int &policyIndex, qcmap_msgr_snat_entry_config_v01 &dnatEntry)
    {
        try {
            return DnatDeserializerBase::deserialize(
                sd, std::tie(name, en, policyIndex, dnatEntry.global_port, dnatEntry.private_ip_addr, dnatEntry.private_port, dnatEntry.protocol));
        }
        catch (const estd::invalid_argument &e) {
            return false;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using DnatIpv6DeserializerBase = estd::Deserializer<std::string, bool, int, decltype(std::declval<qcmap_msgr_snat_v6_entry_config_v01>().global_port),
                                                    decltype(std::declval<qcmap_msgr_snat_v6_entry_config_v01>().port_fwding_private_ip6_addr),
                                                    decltype(std::declval<qcmap_msgr_snat_v6_entry_config_v01>().private_port),
                                                    decltype(std::declval<qcmap_msgr_snat_v6_entry_config_v01>().protocol)>;

class DnatIpv6Deserializer : public DnatIpv6DeserializerBase
{
  public:
    DnatIpv6Deserializer();

    bool deserialize(const serialized_data_t &sd, bool &en, std::string &name, int &policyIndex, qcmap_msgr_snat_v6_entry_config_v01 &dnatEntry)
    {
        try {
            return DnatIpv6DeserializerBase::deserialize(sd,
                                                         std::tie(name, en, policyIndex, dnatEntry.global_port, dnatEntry.port_fwding_private_ip6_addr,
                                                                  dnatEntry.private_port, dnatEntry.protocol));
        }
        catch (const estd::invalid_argument &e) {
            return false;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using WanIpv4FirewallDeserializerBase = estd::Deserializer<
  bool,
  int,
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().firewall_direction),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.src.addr),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.src.subnet_mask),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.dst.addr),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.dst.subnet_mask),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.tos.val),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.tos.mask),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v4.next_hdr_prot),
  uint16_t [2],
  uint16_t [2],
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.icmp.type),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.icmp.code),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.esp.spi)
  >;

class WanIpv4FirewallDeserializer : public WanIpv4FirewallDeserializerBase
{
  public:
    WanIpv4FirewallDeserializer();

    bool deserialize(const serialized_data_t &sd,
                     bool &en,
                     int &policy_idx,
                     qcmap_msgr_firewall_entry_conf_t &fwEntry);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using WanIpv6FirewallDeserializerBase = estd::Deserializer<
  bool,
  int,
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().firewall_direction),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.src.addr),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.src.prefix_len),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.dst.addr),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.dst.prefix_len),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.trf_cls.val),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.trf_cls.mask),
  bool,
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.ip_hdr.v6.next_hdr_prot),
  uint16_t [2],
  uint16_t [2],
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.icmp.type),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.icmp.code),
  decltype(std::declval<qcmap_msgr_firewall_entry_conf_t>().filter_spec.next_prot_hdr.esp.spi)
  >;

class WanIpv6FirewallDeserializer : public WanIpv6FirewallDeserializerBase
{
  public:
    WanIpv6FirewallDeserializer();

    bool deserialize(const serialized_data_t &sd,
                     bool &en,
                     int &policy_idx,
                     qcmap_msgr_firewall_entry_conf_t &fwEntry);
};

} // namespace eqmi
#endif
