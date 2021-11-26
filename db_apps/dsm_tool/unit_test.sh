#!/bin/sh
#
# Assumes that the Mock rdb_tool is in the current path
# I cheat and keep a copy in my home directory
# as we have not yet decided how to integrate host unit tests
# into the build system

# define this is attempting to run unit test on the host
export PATH=$PATH:/home/michael-m/sdev/mock_rdb_tool/test

DSM_EP_KEY=service.dsm.ep.conf
DSM_STREAM_KEY=service.dsm.stream.conf

set_persistent() {
    rdb_set $1 "$2"
    rdb setflags $1 p
}

set_persistent $DSM_EP_KEY.0.name "EP0"
set_persistent $DSM_EP_KEY.0.type "1"
set_persistent $DSM_EP_KEY."EP0".dev_name "/dev/ttyS2"
set_persistent $DSM_EP_KEY."EP0".bit_rate "19200"
set_persistent $DSM_EP_KEY."EP0".parity "none"
set_persistent $DSM_EP_KEY."EP0".data_bits "8"
set_persistent $DSM_EP_KEY."EP0".stop_bits "1"

set_persistent $DSM_EP_KEY.1.name "EP1"
set_persistent $DSM_EP_KEY.1.type "2"
set_persistent $DSM_EP_KEY."EP1".port_number "456"
set_persistent $DSM_EP_KEY."EP1".max_children "12"
set_persistent $DSM_EP_KEY."EP1".keep_alive "1"
set_persistent $DSM_EP_KEY."EP1".keepcnt "10"
set_persistent $DSM_EP_KEY."EP1".keepidle "12"
set_persistent $DSM_EP_KEY."EP1".keepintvl "20"

set_persistent $DSM_EP_KEY.2.name "EP2"
set_persistent $DSM_EP_KEY.2.type "4"
set_persistent $DSM_EP_KEY."EP2".port_number "1234"
set_persistent $DSM_EP_KEY."EP2".max_children "55"

set_persistent $DSM_EP_KEY.3.name "EP3"
set_persistent $DSM_EP_KEY.3.type "6"
set_persistent $DSM_EP_KEY."EP3".raw_mode "1"

set_persistent $DSM_EP_KEY.4.name "EP4"
set_persistent $DSM_EP_KEY.4.type "3"
set_persistent $DSM_EP_KEY."EP4".ip_address "192.168.20.133"
set_persistent $DSM_EP_KEY."EP4".port_number "3333"
set_persistent $DSM_EP_KEY."EP4".timeout "10"
set_persistent $DSM_EP_KEY."EP4".keep_alive "1"
set_persistent $DSM_EP_KEY."EP4".keepcnt "10"
set_persistent $DSM_EP_KEY."EP4".keepidle "12"
set_persistent $DSM_EP_KEY."EP4".keepintvl "20"

set_persistent $DSM_EP_KEY.5.name "EP5"
set_persistent $DSM_EP_KEY.5.type "5"
set_persistent $DSM_EP_KEY."EP5".ip_address "192.168.20.139"
set_persistent $DSM_EP_KEY."EP5".port_number "1234"
set_persistent $DSM_EP_KEY."EP5".timeout "5"

set_persistent $DSM_EP_KEY.6.name "EP6"
set_persistent $DSM_EP_KEY.6.type "1"
set_persistent $DSM_EP_KEY."EP6".dev_name "/dev/ttyS4"
set_persistent $DSM_EP_KEY."EP6".bit_rate "19200"
set_persistent $DSM_EP_KEY."EP6".parity "even"
set_persistent $DSM_EP_KEY."EP6".data_bits "8"
set_persistent $DSM_EP_KEY."EP6".stop_bits "1"

set_persistent $DSM_EP_KEY.11.name "EP11"
set_persistent $DSM_EP_KEY.11.type "1"
set_persistent $DSM_EP_KEY."EP11".dev_name "/dev/ttyS4"
set_persistent $DSM_EP_KEY."EP11".bit_rate "19200"
set_persistent $DSM_EP_KEY."EP11".parity "even"
set_persistent $DSM_EP_KEY."EP11".data_bits "8"
set_persistent $DSM_EP_KEY."EP11".stop_bits "1"

set_persistent $DSM_EP_KEY.7.name "EP-S1"
set_persistent $DSM_EP_KEY.7.type "2"
set_persistent $DSM_EP_KEY."EP-S1".port_number "10000"
set_persistent $DSM_EP_KEY."EP-S1".max_children "2"
set_persistent $DSM_EP_KEY."EP-S1".keep_alive "1"
set_persistent $DSM_EP_KEY."EP-S1".keepcnt "10"
set_persistent $DSM_EP_KEY."EP-S1".keepidle "12"
set_persistent $DSM_EP_KEY."EP-S1".keepintvl "20"

set_persistent $DSM_EP_KEY.8.name "EP-S2"
set_persistent $DSM_EP_KEY.8.type "2"
set_persistent $DSM_EP_KEY."EP-S2".port_number "10001"
set_persistent $DSM_EP_KEY."EP-S2".max_children "2"
set_persistent $DSM_EP_KEY."EP-S2".keep_alive "1"
set_persistent $DSM_EP_KEY."EP-S2".keepcnt "10"
set_persistent $DSM_EP_KEY."EP-S2".keepidle "12"
set_persistent $DSM_EP_KEY."EP-S2".keepintvl "20"

set_persistent $DSM_EP_KEY.9.name "EP-C1"
set_persistent $DSM_EP_KEY.9.type "3"
set_persistent $DSM_EP_KEY."EP-C1".ip_address "192.168.20.134"
set_persistent $DSM_EP_KEY."EP-C1".port_number "10010"
set_persistent $DSM_EP_KEY."EP-C1".timeout "0"
set_persistent $DSM_EP_KEY."EP-C1".keep_alive "1"
set_persistent $DSM_EP_KEY."EP-C1".keepcnt "10"
set_persistent $DSM_EP_KEY."EP-C1".keepidle "12"
set_persistent $DSM_EP_KEY."EP-C1".keepintvl "20"

set_persistent $DSM_EP_KEY.10.name "EP-C2"
set_persistent $DSM_EP_KEY.10.type "3"
set_persistent $DSM_EP_KEY."EP-C2".ip_address "192.168.20.134"
set_persistent $DSM_EP_KEY."EP-C2".port_number "10010"
set_persistent $DSM_EP_KEY."EP-C2".timeout "3"
set_persistent $DSM_EP_KEY."EP-C2".keep_alive "1"
set_persistent $DSM_EP_KEY."EP-C2".keepcnt "10"
set_persistent $DSM_EP_KEY."EP-C2".keepidle "12"
set_persistent $DSM_EP_KEY."EP-C2".keepintvl "20"

set_persistent $DSM_STREAM_KEY.0.name "STREAM 0"
set_persistent $DSM_STREAM_KEY.0.enabled "1"
set_persistent $DSM_STREAM_KEY.0.epa_name "EP5"
set_persistent $DSM_STREAM_KEY.0.epb_name "EP1"
set_persistent $DSM_STREAM_KEY.0.epa_mode "1"
set_persistent $DSM_STREAM_KEY.0.epb_mode "1"

set_persistent $DSM_STREAM_KEY.1.name "STREAM 1"
set_persistent $DSM_STREAM_KEY.1.enabled "1"
set_persistent $DSM_STREAM_KEY.1.epa_name "EP6"
set_persistent $DSM_STREAM_KEY.1.epb_name "EP4"
set_persistent $DSM_STREAM_KEY.1.epa_mode "4"
set_persistent $DSM_STREAM_KEY.1.epb_mode "1"

set_persistent $DSM_STREAM_KEY.2.name "STREAM 2"
set_persistent $DSM_STREAM_KEY.2.enabled "0"
set_persistent $DSM_STREAM_KEY.2.epa_name "EP5"
set_persistent $DSM_STREAM_KEY.2.epb_name "EP1"
set_persistent $DSM_STREAM_KEY.2.epa_mode "1"
set_persistent $DSM_STREAM_KEY.2.epb_mode "1"

set_persistent $DSM_STREAM_KEY.3.name "STREAM 3"
set_persistent $DSM_STREAM_KEY.3.enabled "1"
set_persistent $DSM_STREAM_KEY.3.epa_name "EP0"
set_persistent $DSM_STREAM_KEY.3.epb_name "EP3"
set_persistent $DSM_STREAM_KEY.3.epa_mode "1"
set_persistent $DSM_STREAM_KEY.3.epb_mode "1"

set_persistent $DSM_STREAM_KEY.4.name "Two clients"
set_persistent $DSM_STREAM_KEY.4.enabled "0"
set_persistent $DSM_STREAM_KEY.4.epa_name "EP-C1"
set_persistent $DSM_STREAM_KEY.4.epb_name "EP-C2"
set_persistent $DSM_STREAM_KEY.4.epa_mode "1"
set_persistent $DSM_STREAM_KEY.4.epb_mode "1"

set_persistent $DSM_STREAM_KEY.5.name "Two servers"
set_persistent $DSM_STREAM_KEY.5.enabled "0"
set_persistent $DSM_STREAM_KEY.5.epa_name "EP-S1"
set_persistent $DSM_STREAM_KEY.5.epb_name "EP-S2"
set_persistent $DSM_STREAM_KEY.5.epa_mode "1"
set_persistent $DSM_STREAM_KEY.5.epb_mode "1"

set_persistent $DSM_STREAM_KEY.6.name "Client to server"
set_persistent $DSM_STREAM_KEY.6.enabled "0"
set_persistent $DSM_STREAM_KEY.6.epa_name "EP-C1"
set_persistent $DSM_STREAM_KEY.6.epb_name "EP-S2"
set_persistent $DSM_STREAM_KEY.6.epa_mode "1"
set_persistent $DSM_STREAM_KEY.6.epb_mode "1"
