/**
 * C header file of QC diagnostic structures
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#ifndef __G_LOG_CODES_H__
#define __G_LOG_CODES_H__

#define FEATURE_LOG_EXPOSED_HEADER
#define _UINT32_DEFINED
#define _INT32_DEFINED

#include <diag/log.h>

/*

#define LOG_LTE_BASE_C       ((uint16) (0xB000 + 0x0010))

*/

typedef struct PACK() {
    uint8 log_packet_ver;      /*!< Log packet version */
    uint16 phy_cell_id;        /*!< Physical cell ID */
    uint32 dl_freq;            /*!< DL Frequency of the cell */
    uint32 ul_freq;            /*!< UL Frequency (from SIB2) */
    uint8 dl_bw;               /*!< Downlink bandwidth (from MIB) */
    uint8 ul_bw;               /*!< Uplink bandwidth */
    uint32 cell_id;            /*!< Cell identity received in SIB1 */
    uint16 tracking_area_code; /*!< Tracking area code of the cell */
    uint32 freq_band_ind;      /*!< Frequency band indicator */
    uint16 sel_plmn_mcc;       /*!< MCC of selected PLMN */
    uint8 num_mnc_digits;      /*!< Number of digits in MNC (2 or 3) */
    uint16 sel_plmn_mnc;       /*!< MNC of selected PLMN */
    uint8 allowed_access;      /*!< Allowed access on the cell - Full/Limited */
} lte_rrc_log_serv_cell_info_s;

typedef struct PACK() {
    /*! Version */
    uint16 version : 8;

    /*! Downlink bandwidth */
    uint16 dl_bandwidth : 8;

    /*! System frame number (at ref time) */
    uint16 sfn;

    ///< serving cell earfcn
    uint32 earfcn;

    ///< serving  cell_ID
    uint16 cell_ID : 9;

    ///< decode result: 1 = pass, 0= fail;
    uint16 decode_result : 1;

    /*! The phich duration configures lower limit on the size of the control
        region signaled by the PCFICH. */
    uint16 phich_duration : 3;

    /*! Parameter for determining the number of PHICH groups */
    uint16 phich_resource : 3;

    uint16 reserved0;

    ///< serving cell PSS
    uint32 pss_corr_result;

    ///< serving cell PSS
    uint32 sss_corr_result;

    /*------------------------*/
    ///< 10 ms boundary;
    uint32 ref_time[2];

    ///< PBCH payload
    uint32 mib_payload;

    ///< updated frequency offset estimate (in Hz);
    ///< invalid if freq_offset_invalid
    uint16 freq_offset;

    ///< number of antennas for cell: 1, 2, or 4;
    uint16 num_antennas : 2;

    union
    {
        /* v2 */
        uint16 reserved1 : 14;
        /* v3 */
        struct {
            uint16 coverage_mode_ce : 1;
            uint16 reserved2 : 13;
        };
    };
} lte_ml1_sm_log_serv_cell_info_pkt_s;

/*==========================================================================*/
/*! @brief
 * 2 LTE LL1 PUSCH Tx Report (Log code ID: 0xB139)
 *
 *  This log packet gives important parameters related to the PUSCH transmit
 *  reports. The log record is generated once every 20 sub-frames or every
 *  100ms, whichever occurs first.
---------------------------------------------------------------------------*/
#define LTE_LL1_LOG_UL_NUMBER_OF_PUSCH_RECORDS_FIXED 20

typedef struct {
    uint16 current_sfn_sf;                        ///< System / Sub Frame Number, as
                                                  ///< 10*SFN + SF
    uint8 ul_carrier_idx : 2;                     /// < 0: PCC
                                                  /// < 1: SCC
    uint32 ul_control_mux_bitmask_ack_cqi_ri : 3; ///< 0x0: No ACK/CQI/RI
                                                  ///< 0x1: ACK/CQI/RI exists
    uint32 frequency_hopping : 2;                 ///< 0 : Freq Hop disabled
                                                  ///< 1: Inter SF freq hopping
                                                  ///< 2 : Intra+Inter SF freq hopping
                                                  ///< 3 : Undefined
    uint32 re_transmission_index : 5;             ///< CURRENT_TX_NB
                                                  ///< 0: First transmission
                                                  ///< 1: Second transmission
                                                  ///< ...
                                                  ///< 7: Eighth transmission
    uint32 redundancy_version : 2;                ///< Range: 0 ~ 3
    uint32 mirror_hopping : 2;                    ///< 0x0: Disabled for slot 0
                                                  ///< 0x1: Enabled for slot 0
                                                  ///< 0x2: Disabled for slot 1
                                                  ///< 0x3: Enabled for slot 1

    uint32 start_resource_block_rb_for_slot_0 : 7; ///< Range: 0...110
    uint32 start_resource_block_rb_for_slot_1 : 7; ///< Range: 0...110
    uint32 number_of_rb : 7;                       ///< Range: 0...110
    uint32 reserved0 : 11;

    uint32 pusch_tb_size : 16; ///< Transport block size in bytes
    uint16 coding_rate;        ///< PUSCH coding rate in Q10 (0,1)

    uint32 rate_matched_ack_bits : 14; ///< changed to 14 bits to support up to 9600 bits in TDDCA??
    uint32 ri_payload : 5;             ///< RI raw payload
    uint32 rate_matched_ri_bits : 11;  ///< Range: 0 - 1920
    uint32 ue_srs_on : 1;              ///< 0: OFF
                                       ///< 1: ON
    uint32 srs_occasion : 1;           ///< 0: OFF
                                       ///< 1: ON
    uint32 ack_payload : 20;           ///< ACK / NACK raw payload.
                                       ///< Max length:
                                       ///< TDD: up to 20 bits
                                       ///< FDD: 2 bits non CA, up to 6 bits CA
                                       ///< FMT3, up to 20 bits
    uint32 ack_nak_inp_length0 : 4;    /// < ACK/NACK length 0
    uint32 ack_nak_inp_length1 : 4;    /// < ACK/NACK length 1
    uint32 num_ri_bits_nri : 3;        /// 0: 0 RI bits
                                       ///< 1: 1 RI bit
                                       ///< 2: 2 RI bits
                                       ///< 3: for 3DL CA
                                       ///< 4 for TM9/TM10
    uint32 reserved2 : 1;              /// < reserved bits

    uint32 pusch_mod_order : 2;    ///< Modulation order encoded as
                                   ///< follows:
                                   ///< 0: BPSK
                                   ///< 1: QPSK
                                   ///< 2: 16QAM
                                   ///< 3: 64QAM
    uint32 pusch_digital_gain : 8; ///< Digital amplitude gain in dB
                                   ///< (beta_pusch is int16. Are 8 bits
                                   ///< sufficient?)
    uint32 reserved3 : 22;         /// < reserved bits

    int32 pusch_transmit_power_dbm : 7; ///< Range: -50dBm to 23dBm, with 1dB
                                        ///< stepsize
    uint32 num_cqi_bits_ncqi : 8;       ///< Range:0~66 non-ca, 0-204 CA
    uint32 rate_matched_cqi_bits : 14;  ///< Range: 0~ 4959 non-ca, 0-14134 CA
    uint32 reserved4 : 3;

    uint32 num_dl_carriers : 2; ///< Number of enabled DL carriers in FW.
    uint32 ack_nak_idx : 12;    ///< From MSB to LSB: idx_2, idx_1, idx_0. 4 Bit for each.
                                ///< It is how each DL carrier maps to ACK/NACK payload location.
                                ///< Please show it in HEX.
    uint32 ack_nak_late : 1;    ///< 1 if Demback is late and ACK/NACK is not ready.
    uint32 csf_late : 1;        ///< 1 if CSF is not available when being got fetched.
    uint32 drop_pusch : 1;      ///< 1 if we dropped this PUSCH.
    uint32 reserved5 : 15;

    uint32 cqi_payload[7]; ///< CQI raw payload on PUSCH. raw payload size can be 204
                           ///< length is 96 bits for non-ca, 128 CA
                           ///< Please display as HEX.

    int32 tx_resampler; ///< TXR resampler value Q32

    uint32 cyclic_shift_of_dmrs_symbols_slot_0 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 cyclic_shift_of_dmrs_symbols_slot_1 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 dmrs_root_slot_0 : 11;                   ///< Range: 1...1291
    uint32 dmrs_root_slot_1 : 11;                   ///< Range: 1...1291
    uint32 reserved6 : 2;

} lte_LL1_log_ul_pusch_tx_report_pusch_records_v121_s;

typedef struct {
    uint16 current_sfn_sf;                        ///< System / Sub Frame Number, as
                                                  ///< 10*SFN + SF
    uint8 ul_carrier_idx : 2;                     /// < 0: PCC
                                                  /// < 1: SCC1
    uint32 ul_control_mux_bitmask_ack_cqi_ri : 3; ///< 0: Ack bit, 0 None 1 Exists
                                                  ///< 1 :CSF bit, 0 None 1 Exists
                                                  ///< 2:RI bit, 0 None 1 Exists
    uint32 frequency_hopping : 2;                 ///< 0 : Freq Hop disabled
                                                  ///< 1: Inter SF freq hopping
                                                  ///< 2 : Intra+Inter SF freq hopping
                                                  ///< 3 : Undefined
    uint32 re_transmission_index : 5;             ///< CURRENT_TX_NB
                                                  ///< 0: First transmission
                                                  ///< 1: Second transmission
                                                  ///< ...
                                                  ///< 7: Eighth transmission
    uint32 redundancy_version : 2;                ///< Range: 0 ~ 3
    uint32 mirror_hopping : 2;                    ///< 0x0: Disabled for slot 0
                                                  ///< 0x1: Enabled for slot 0
                                                  ///< 0x2: Disabled for slot 1
                                                  ///< 0x3: Enabled for slot 1

    uint32 start_resource_block_rb_for_slot_0 : 7; ///< Range: 0...110
    uint32 start_resource_block_rb_for_slot_1 : 7; ///< Range: 0...110
    uint32 number_of_rb : 7;                       ///< Range: 0...110
    uint8 dl_carrier_idx : 3;                      /// < 0: PCC
                                                   /// < 1: SCC1
                                                   /// < 2: SCC2
                                                   /// < 3: SCC3
                                                   /// < 4: SCC4

    uint32 reserved0 : 8;

    uint32 pusch_tb_size : 16; ///< Transport block size in bytes
    uint16 coding_rate;        ///< PUSCH coding rate in Q10 (0,1)

    uint32 rate_matched_ack_bits : 14; ///< changed to 14 bits to support up to 9600 bits in TDDCA??
    uint32 ri_payload : 5;             ///< RI raw payload
    uint32 rate_matched_ri_bits : 11;  ///< Range: 0 - 1920
    uint32 ue_srs_on : 1;              ///< 0: OFF
                                       ///< 1: ON
    uint32 srs_occasion : 1;           ///< 0: OFF
                                       ///< 1: ON

    uint32 ack_payload : 20;        ///< ACK / NACK raw payload.
                                    ///< Max length:
                                    ///< TDD: up to 20 bits
                                    ///< FDD: 2 bits non CA, up to 6 bits CA
                                    ///< FMT3, up to 20 bits
    uint32 ack_nak_inp_length0 : 4; /// < ACK/NACK length 0
    uint32 ack_nak_inp_length1 : 4; /// < ACK/NACK length 1
    uint32 num_ri_bits_nri : 3;     /// 0: 0 RI bits
                                    ///< 1: 1 RI bit
                                    ///< 2: 2 RI bits
                                    ///< 3: for 3DL CA
                                    ///< 4 for TM9/TM10
                                    ///< 5: 5 RI bits
    uint32 reserved2 : 1;           /// < reserved bits

    uint32 pusch_mod_order : 2;    ///< Modulation order encoded as
                                   ///< follows:
                                   ///< 0: BPSK
                                   ///< 1: QPSK
                                   ///< 2: 16QAM
                                   ///< 3: 64QAM
    uint32 pusch_digital_gain : 8; ///< Digital amplitude gain in dB
                                   ///< (beta_pusch is int16. Are 8 bits
                                   ///< sufficient?)
    uint32 reserved3 : 22;

    uint32 pusch_transmit_power_dbm : 7; ///< Range: -50dBm to 23dBm, with 1dB
                                         ///< stepsize
    uint32 num_cqi_bits_ncqi : 11;       ///< Range:0~66 non-ca, 0-204 CA, ~320 for 5DLCA
    uint32 rate_matched_cqi_bits : 14;   ///< Range: 0~ 4959 non-ca, 0-14134 CA

    uint32 num_dl_carriers : 2; ///< Number of enabled DL carriers in FW.
    uint32 ack_nak_idx : 12;    ///< From MSB to LSB: idx_2, idx_1, idx_0. 4 Bit for each.
                                ///< It is how each DL carrier maps to ACK/NACK payload location.
                                ///< Please show it in HEX.
    uint32 ack_nak_late : 1;    ///< 1 if Demback is late and ACK/NACK is not ready.
    uint32 csf_late : 1;        ///< 1 if CSF is not available when being got fetched.
    uint32 drop_pusch : 1;      ///< 1 if we dropped this PUSCH.
    uint32 reserved5 : 15;

    uint32 cqi_payload[11]; ///< CQI raw payload on PUSCH. raw payload size can be 204
                            ///< length is 96 bits for non-ca, 128 CA, ~320 for 5DLCA
                            ///< Please display as HEX.

    uint32 tx_resampler; ///< TXR resampler value Q32

    uint32 cyclic_shift_of_dmrs_symbols_slot_0 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 cyclic_shift_of_dmrs_symbols_slot_1 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 dmrs_root_slot_0 : 11;                   ///< Range: 1...1291
    uint32 dmrs_root_slot_1 : 11;                   ///< Range: 1...1291
    uint32 reserved6 : 2;

} lte_LL1_log_ul_pusch_tx_report_pusch_records_v124_s;

typedef struct {
    uint16 current_sfn_sf;                        ///< System / Sub Frame Number, as
                                                  ///< 10*SFN + SF
    uint8 ul_carrier_idx : 2;                     /// < 0: PCC
                                                  /// < 1: SCC
    uint32 ul_control_mux_bitmask_ack_cqi_ri : 3; ///< 0x0: No ACK/CQI/RI
                                                  ///< 0x1: ACK/CQI/RI exists
    uint32 frequency_hopping : 2;                 ///< 0 : Freq Hop disabled
                                                  ///< 1: Inter SF freq hopping
                                                  ///< 2 : Intra+Inter SF freq hopping
                                                  ///< 3 : Undefined
    uint32 re_transmission_index : 5;             ///< CURRENT_TX_NB
                                                  ///< 0: First transmission
                                                  ///< 1: Second transmission
                                                  ///< ...
                                                  ///< 7: Eighth transmission
    uint32 redundancy_version : 2;                ///< Range: 0 ~ 3
    uint32 mirror_hopping : 2;                    ///< 0x0: Disabled for slot 0
                                                  ///< 0x1: Enabled for slot 0
                                                  ///< 0x2: Disabled for slot 1
                                                  ///< 0x3: Enabled for slot 1

    uint32 resource_allocation_type : 1;           ///< Range: 0 ~ 1
    uint32 start_resource_block_rb_for_slot_0 : 7; ///< Range: 0...110
    uint32 start_resource_block_rb_for_slot_1 : 7; ///< Range: 0...110
    uint32 number_of_rb : 7;                       ///< Range: 0...110
    uint32 reserved0 : 10;

    uint32 pusch_tb_size : 16; ///< Transport block size in bytes
    uint16 coding_rate;        ///< PUSCH coding rate in Q10 (0,1)

    uint32 rate_matched_ack_bits : 14; ///< changed to 14 bits to support up to 9600 bits in TDDCA??
    uint32 ri_payload : 4;             ///< RI raw payload
    uint32 rate_matched_ri_bits : 11;  ///< Range: 0 - 1920
    uint32 ue_srs_on : 1;              ///< 0: OFF
                                       ///< 1: ON
    uint32 srs_occasion : 1;           ///< 0: OFF
                                       ///< 1: ON
    uint32 reserved1 : 1;

    uint32 ack_payload : 20;        ///< ACK / NACK raw payload.
                                    ///< Max length:
                                    ///< TDD: up to 20 bits
                                    ///< FDD: 2 bits non CA, up to 6 bits CA
                                    ///< FMT3, up to 20 bits
    uint32 ack_nak_inp_length0 : 4; /// < ACK/NACK length 0
    uint32 ack_nak_inp_length1 : 4; /// < ACK/NACK length 1
    uint32 num_ri_bits_nri : 3;     /// 0: 0 RI bits
                                    ///< 1: 1 RI bit
                                    ///< 2: 2 RI bits
                                    ///< 3: for 3DL CA
                                    ///< 4 for TM9/TM10
    uint32 reserved2 : 1;           /// < reserved bits

    uint32 pusch_mod_order : 2;                       ///< Modulation order encoded as
                                                      ///< follows:
                                                      ///< 0: BPSK
                                                      ///< 1: QPSK
                                                      ///< 2: 16QAM
                                                      ///< 3: 64QAM
    uint32 pusch_digital_gain : 8;                    ///< Digital amplitude gain in dB
                                                      ///< (beta_pusch is int16. Are 8 bits
                                                      ///< sufficient?)
    uint32 start_resource_block_rb_for_cluster_1 : 7; ///< Range: 0...110
    uint32 number_of_rb_for_cluster_1 : 7;            ///< Range: 0...110
    uint32 reserved3 : 8;

    uint32 pusch_transmit_power_dbm : 7; ///< Range: -50dBm to 23dBm, with 1dB
                                         ///< stepsize
    uint32 num_cqi_bits_ncqi : 8;        ///< Range:0~66 non-ca, 0-204 CA
    uint32 rate_matched_cqi_bits : 14;   ///< Range: 0~ 4959 non-ca, 0-14134 CA
    uint32 reserved4 : 3;

    uint32 num_dl_carriers : 2; ///< Number of enabled DL carriers in FW.
    uint32 ack_nak_idx : 12;    ///< From MSB to LSB: idx_2, idx_1, idx_0. 4 Bit for each.
                                ///< It is how each DL carrier maps to ACK/NACK payload location.
                                ///< Please show it in HEX.
    uint32 ack_nak_late : 1;    ///< 1 if Demback is late and ACK/NACK is not ready.
    uint32 csf_late : 1;        ///< 1 if CSF is not available when being got fetched.
    uint32 drop_pusch : 1;      ///< 1 if we dropped this PUSCH.
    uint32 reserved5 : 15;

    uint32 cqi_payload[7]; ///< CQI raw payload on PUSCH. raw payload size can be 204
                           ///< length is 96 bits for non-ca, 128 CA
                           ///< Please display as HEX.

    uint32 tx_resampler; ///< TXR resampler value Q32

    uint32 cyclic_shift_of_dmrs_symbols_slot_0 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 cyclic_shift_of_dmrs_symbols_slot_1 : 4; ///< Cyclic shift of DMRS symbols for
                                                    ///< slot 1. Unit is samples.
                                                    ///< Range: 0 -11
    uint32 dmrs_root_slot_0 : 11;                   ///< Range: 1...1291
    uint32 dmrs_root_slot_1 : 11;                   ///< Range: 1...1291
    uint32 reserved6 : 2;

} lte_LL1_log_ul_pusch_tx_report_pusch_records_v101_s;

typedef struct {
    uint32 version : 8;           ///< Log packet version. Range: 0...255.
                                  ///< Version = 2 for the log packet
                                  ///< structure described below
    uint32 serving_cell_id : 9;   ///< Range: 0...504
    uint32 number_of_records : 5; ///< Range: 0..20
    uint32 reserved0 : 10;
    uint16 dispatch_sfn_sf; ///< System / Sub Frame Number, as 10*SFN + SF
    uint16 reserved1;
    union
    {
        lte_LL1_log_ul_pusch_tx_report_pusch_records_v101_s records_v101[LTE_LL1_LOG_UL_NUMBER_OF_PUSCH_RECORDS_FIXED];
        lte_LL1_log_ul_pusch_tx_report_pusch_records_v121_s records_v121[LTE_LL1_LOG_UL_NUMBER_OF_PUSCH_RECORDS_FIXED];
        lte_LL1_log_ul_pusch_tx_report_pusch_records_v124_s records_v124[LTE_LL1_LOG_UL_NUMBER_OF_PUSCH_RECORDS_FIXED];
    };
} lte_LL1_log_ul_pusch_tx_report_ind_struct;

/*==========================================================================*/
/*! @brief
 * 2 LTE LL1 PUCCH Tx Report (Log code ID: 0xB13C)
 *
 *  This log packet gives important parameters related to the PUCCH transmit
 *  reports. The log record is generated once every 20 sub-frames or every
 *  100ms, whichever occurs first.
---------------------------------------------------------------------------*/
#define LTE_LL1_LOG_UL_NUMBER_OF_PUCCH_RECORDS_FIXED 20
#define LTE_LL1_LOG_UL_NUMBER_OF_PUCCH_SF_RECORDS_FIXED 10

/*! @brief pucch_tx_report number_of_pucch_records_fixed struct
 * version: 101 (Log code ID: 0xB13C)
 */
typedef struct {
    uint16 current_sfn_sf; ///< System / Sub Frame Number, as
                           ///< 10*SFN + SF
    uint16 cqi_payload;    ///< Raw CQI payload on PUCCH. Max length is 13 bits

    uint8 ul_carrier_idx : 2;             /// < 0: PCC
                                          /// < 1: SCC
    uint32 format : 3;                    ///< 0x0: Format 1
                                          ///< 0x1: Format 1a
                                          ///< 0x2: Format 1b
                                          ///< 0x3: Format 2
                                          ///< 0x4: Format 2a
                                          ///< 0x5: Format 2b
                                          ///< 0x6: Format 3
    uint32 start_rb_slot_0 : 7;           ///< Range: 0...110
    uint32 start_rb_slot_1 : 7;           ///< Range: 0...110
    uint32 srs_shorting_for_2nd_slot : 1; ///< 0: normal slot
                                          ///< 1: shorten 2nd Slot
    uint32 ue_srs_on : 1;                 ///< 0: OFF
                                          ///< 1: ON
    uint32 dmrs_sequence_for_slot_0 : 5;  ///< index 0...29
    uint32 dmrs_sequence_for_slot_1 : 5;  ///< index 0...29
    uint32 reserved0 : 1;

    uint8 cyclic_shift_sequence_for_each_symbol[14]; ///< Time domain cyclic shifts per symbol per slot
    uint32 pucch_digital_gain : 8;                   ///< Digital amplitude gain in dB
    int32 pucch_transmit_power_dbm : 7;              ///< Range: -50dBm to 23dBm, with 1dB stepsize
    uint32 reserved1 : 1;

    uint32 ack_payload : 20;       ///< ACK/NACK raw payload.
    uint32 ack_payload_length : 5; ///< ACK/NAK raw payload length
    uint32 sr_bit_fmt3 : 1;        ///< SR bit in PUCCH FMT3
    uint32 num_dl_carriers : 2;    ///< Number of enabled DL carriers in FW.
    uint32 reserved2 : 4;

    uint16 n_1_pucch;        ///< Logs N_1_pucch for PUCCH fmt_1, N_2_pucch for fmt_2 and etc.
    uint16 ack_nak_idx : 12; ///< From MSB to LSB: idx_2, idx_1, idx_0. 4 Bit for each.
                             ///< It is how each DL carrier maps to ACK/NACK payload location.
                             ///< Please show it in HEX.
    uint16 ack_nak_late : 1; ///< 1 if Demback is late and ACK/NACK is not ready.
    uint16 csf_late : 1;     ///< 1 if CSF is not available when being got fetched.
    uint16 drop_pucch : 1;   ///< 1 if we dropped this PUCCH.
    uint16 reserved3 : 1;

    int32 tx_resampler; ///< TXR resampler value Q32

} lte_LL1_log_ul_pucch_tx_report_pucch_records_v101_s;

/*! @brief pucch_tx_report number_of_pucch_records_fixed struct
 * version: 121 (Log code ID: 0xB13C)
 */
typedef struct {
    uint16 current_sfn_sf; ///< System / Sub Frame Number, as
                           ///< 10*SFN + SF
    uint16 cqi_payload;    ///< Raw CQI payload on PUCCH. Max length is 13 bits

    uint8 ul_carrier_idx : 2;             /// < 0: PCC
                                          /// < 1: SCC1
    uint32 format : 3;                    ///< 0x0: Format 1
                                          ///< 0x1: Format 1a
                                          ///< 0x2: Format 1b
                                          ///< 0x3: Format 2
                                          ///< 0x4: Format 2a
                                          ///< 0x5: Format 2b
                                          ///< 0x6: Format 3
    uint32 start_rb_slot_0 : 7;           ///< Range: 0...110
    uint32 start_rb_slot_1 : 7;           ///< Range: 0...110
    uint32 srs_shorting_for_2nd_slot : 1; ///< 0: normal slot
                                          ///< 1: shorten 2nd Slot
    uint32 ue_srs_on : 1;                 ///< 0: OFF
                                          ///< 1: ON
    uint32 dmrs_sequence_for_slot_0 : 5;  ///< index 0...29
    uint32 dmrs_sequence_for_slot_1 : 5;  ///< index 0...29
    uint32 reserved0 : 1;

    uint8 cyclic_shift_sequence_for_each_symbol[14]; ///< Time domain cyclic shifts per symbol per slot
    uint32 pucch_digital_gain : 8;                   ///< Digital amplitude gain in dB
    uint32 pucch_transmit_power_dbm : 7;             ///< Range: -50dBm to 23dBm, with 1dB stepsize
    uint32 reserved1 : 1;

    uint32 ack_payload : 20;       ///< ACK/NACK raw payload.
    uint32 ack_payload_length : 5; ///< ACK/NAK raw payload length
    uint32 sr_bit_fmt3 : 1;        ///< SR bit in PUCCH FMT3
    uint32 num_dl_carriers : 2;    ///< Number of enabled DL carriers in FW.
    uint8 dl_carrier_idx : 3;      /// < 0: PCC
                                   /// < 1: SCC1
                                   /// < 2: SCC2
                                   /// < 3: SCC3
                                   /// < 4: SCC4
    uint32 reserved2 : 1;

    uint16 n_1_pucch;        ///< Logs N_1_pucch for PUCCH fmt_1, N_2_pucch for fmt_2 and etc.
    uint16 ack_nak_idx : 12; ///< From MSB to LSB: idx_2, idx_1, idx_0. 4 Bit for each.
                             ///< It is how each DL carrier maps to ACK/NACK payload location.
                             ///< Please show it in HEX.
    uint16 ack_nak_late : 1; ///< 1 if Demback is late and ACK/NACK is not ready.
    uint16 csf_late : 1;     ///< 1 if CSF is not available when being got fetched.
    uint16 drop_pucch : 1;   ///< 1 if we dropped this PUCCH.
    uint16 reserved3 : 1;

    uint32 tx_resampler; ///< TXR resampler value Q32

} lte_LL1_log_ul_pucch_tx_report_pucch_records_v121_s;

/*! @brief pucch_tx_report main struct
 */
typedef struct {
    uint32 version : 8;           ///< Log packet version. Range: 0...255.
                                  ///< Version = 2 for the log packet structure
                                  ///< described below
    uint32 serving_cell_id : 9;   ///< Range: 0...504
    uint32 number_of_records : 5; ///< Range: 0..20
    uint32 reserved0 : 10;
    uint16 dispatch_sfn_sf; ///< System / Sub Frame Number, as
                            ///< 10*SFN + SF
    uint16 reserved1;
    union
    {
        lte_LL1_log_ul_pucch_tx_report_pucch_records_v101_s records_v101[LTE_LL1_LOG_UL_NUMBER_OF_PUCCH_RECORDS_FIXED];
        lte_LL1_log_ul_pucch_tx_report_pucch_records_v121_s records_v121[LTE_LL1_LOG_UL_NUMBER_OF_PUCCH_RECORDS_FIXED];
    };
} lte_LL1_log_ul_pucch_tx_report_ind_struct;

typedef enum
{
    FDD = 0,                         ///<  FDD mode (multiplexing & bundling are the same)
    TDD_ACK_MUX,                     ///<  Multiplexing mode for TDD with single carrier
    TDD_ACK_BUNDLING,                ///<  Bundling mode for TDD with single carrier
    TDD_ACK_PUCCH_1B_CH_SEL,         ///< PUCCH FMT 1B with ch selection for TDD single cell
    TDD_CA_ACK_PUCCH_1B_CH_SEL,      ///< PUCCH FMT 1B with ch selection for TDD+CA
    ACK_PUCCH_3,                     ///< PUCCH FMT 3 for FDD+CA, TDD with single carrier and TDD+CA
    ACK_TPYE_MAX_WIDTH = 0xFFFFFFFF, ///< @internal set enum to 32bits
} lte_ml1_log_ul_ack_reporting_mode_type_e;

#define LTE_ML1_LOG_GM_PUCCH_TX_N_1_PUCCH_NUM (4)
#define LTE_ML1_LOG_GM_PUCCH_TX_ACK_TO_SEND_MASK_NUM (5)
/* Max number of CCs currently supported (including primary)*/
#define LTE_ML1_GM_LOG_MAX_CC 5

/*==========================================================================*/
/*! @brief
 *  LTE ML1 GM Tx Report (Log code ID: 0xB16D)
 *
 *  This log packet gives important parameters about transmission on PUSCH/PUCCH.
 *  The log record is generated once every 50 sub-frames or every 1 second,
 *  whichever occurs first.
---------------------------------------------------------------------------*/

/*! @brief GM Tx N_1_pucch record
 */
typedef struct {
    uint16 n_1_pucch_i : 12;
    uint16 reserved : 4;
} lte_ml1_gm_log_n_1_pucch_record_s;

/*! @brief GM PUCCH Tx Report Record definition : version 33 */
typedef struct {
    /* First word */
    uint32 reserved1;

    /* Second word */
    uint32 chan_type : 1;            ///< 0 - PUCCH
                                     ///< 1 - PUSCH
    uint32 sfn : 10;                 ///< PUCCH OTA system frame number
                                     ///< Range is 0 to 1023
    uint32 sub_fn : 4;               ///< PUCCH OTA subframe number
                                     ///< Range is 0 to 9
    uint32 total_tx_power : 8;       ///< Range is -112 dBm to 23 dBm, with 1 dB stepsize
    uint32 ack_nak_present_flag : 2; ///< 0: ACK/NAK not present
                                     ///< 1: ACK/NAK present
                                     ///< 2: Forced NAK present
    uint32 ca_mode_enabled : 1;      ///< Whether at least a Scell is configured
                                     ///< 0: CA mode disabled
                                     ///< 1: CA mode enabled

    uint32 ack_reporting_mode : 3; ///< Ack/Nak reporting mode - lte_ml1_log_ul_ack_reporting_mode_type_e

    uint32 ack_nak_len : 2; ///< ACK/NAK length
    uint32 reserved2 : 1;

    /* Third, Fourth and Fifth word */
    uint16 ack_nak_to_send_mask[LTE_ML1_LOG_GM_PUCCH_TX_ACK_TO_SEND_MASK_NUM];
    ///< The mask to indicate which DL subframe needs to ACK or NAK.
    ///< Shown only when '(."UL ACK/NAK Present" == 1 || ."UL ACK/NAK Present" == 2) && ."CA Mode Enabled" ? 5 : 0
    uint16 reserved3;

    /* Sixth word */
    uint32 beta_pucch : 16;   ///< Range is 0 to 65535
    uint32 n_1_pucch : 12;    ///< PUCCH resource for format 1/1a/1b
                              ///< Range is 0 to 2083
    uint32 n_1_pucch_num : 3; ///< Number of n_1_pucch sent t in TDD multiplexing mode
                              ///< or Carrier Aggregation mode. This field is displayed
                              ///< only when its value is non-zero.
                              ///< Range is 0 to 4
    uint32 reserved4 : 1;

    /* Seventh and Eighth words */
    lte_ml1_gm_log_n_1_pucch_record_s n_1_pucch_records[LTE_ML1_LOG_GM_PUCCH_TX_N_1_PUCCH_NUM]; ///< n_1_pucch_array[4]
                                                                                                ///< This field is displayed only when "Number of
                                                                                                ///< n_1_pucch" is non-zero.
                                                                                                ///< The number of n_1_pucch_array [i] displayed is
                                                                                                ///< "Number of n_1_pucch". Each one is 2 byte.

    /* Ninth word */
    uint32 n_2_pucch : 10;              ///< PUCCH resource for format 2/2a/2b
    uint32 n_3_pucch : 10;              ///< PUCCH resource for format 3
    uint32 sr_present_flag : 1;         ///< Scheduling Request Present
                                        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 trni_ack_flag : 1;           ///< Temp RNTI ACK Flag
                                        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 srs_present_flag : 1;        ///< Whether SRS is being scheduled
    uint32 srs_ue_or_cell_specific : 1; ///< Cell or UE specific SRS occasion
    uint32 csf_present_flag : 1;        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 reserved5 : 7;

    /* Tenth word */
    uint16 cc_max_tx_power : 8;    ///< Max Power for the CCRange is -112 dBm to 23 dBm, 1 dB step
    uint16 total_max_tx_power : 8; ///< Total Max Power for the CCRange is -112 dBm to 23 dBm, 1 dB step
    uint16 reserved6 : 16;

    /* Eleventh word */
    int32 afc_rx_freq_error; ///< AFC Rx Freq Error [Hz]
} lte_ml1_gm_log_pucch_tx_report_record_v33_s;

/*! @brief GM PUCCH Tx Report Record definition : version 26 */
typedef struct {
    /* First word */
    uint32 chan_type : 1;            ///< 0 - PUCCH
                                     ///< 1 - PUSCH
    uint32 sfn : 10;                 ///< PUCCH OTA system frame number
                                     ///< Range is 0 to 1023
    uint32 sub_fn : 4;               ///< PUCCH OTA subframe number
                                     ///< Range is 0 to 9
    uint32 total_tx_power : 8;       ///< Range is -112 dBm to 23 dBm, with 1 dB stepsize
    uint32 ack_nak_present_flag : 2; ///< 0: ACK/NAK not present
                                     ///< 1: ACK/NAK present
                                     ///< 2: Forced NAK present
    uint32 ca_mode_enabled : 1;      ///< Whether at least a Scell is configured
                                     ///< 0: CA mode disabled
                                     ///< 1: CA mode enabled

    uint32 ack_reporting_mode : 3; ///< Ack/Nak reporting mode - lte_ml1_log_ul_ack_reporting_mode_type_e

    uint32 ack_nak_len : 2; ///< ACK/NAK length
    uint32 reserved0 : 1;

    /* Second word */
    uint32 ack_nak_to_send_mask : 9;
    uint32 ack_nak_to_send_mask_scell_1 : 9; ///< This field is printed only when
                                             ///< "CA Mode Enabled" is 1
    uint32 ack_nak_to_send_mask_scell_2 : 9; ///< This field is printed only when
                                             ///< "CA Mode Enabled" is 1
    uint32 reserved1 : 5;

    /* Third word */
    uint32 beta_pucch : 16; ///< Range is 0 to 65535
    uint32 reserved2 : 1;
    uint32 n_1_pucch : 12;    ///< PUCCH resource for format 1/1a/1b
                              ///< Range is 0 to 2083
    uint32 n_1_pucch_num : 3; ///< Number of n_1_pucch sent t in TDD multiplexing mode
                              ///< or Carrier Aggregation mode. This field is displayed
                              ///< only when its value is non-zero.
                              ///< Range is 0 to 4
    /* Fourth and Fifth words */
    lte_ml1_gm_log_n_1_pucch_record_s n_1_pucch_records[LTE_ML1_LOG_GM_PUCCH_TX_N_1_PUCCH_NUM]; ///< n_1_pucch_array[4]
                                                                                                ///< This field is displayed only when "Number of
                                                                                                ///< n_1_pucch" is non-zero.
                                                                                                ///< The number of n_1_pucch_array [i] displayed is
                                                                                                ///< "Number of n_1_pucch". Each one is 2 byte.

    /* Sixth word */
    uint32 n_2_pucch : 10;              ///< PUCCH resource for format 2/2a/2b
    uint32 n_3_pucch : 10;              ///< PUCCH resource for format 3
    uint32 sr_present_flag : 1;         ///< Scheduling Request Present
                                        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 trni_ack_flag : 1;           ///< Temp RNTI ACK Flag
                                        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 srs_present_flag : 1;        ///< Whether SRS is being scheduled
    uint32 srs_ue_or_cell_specific : 1; ///< Cell or UE specific SRS occasion
    uint32 csf_present_flag : 1;        ///< 0x0 - Not present
                                        ///< 0x1 - Present
    uint32 padding : 7;                 ///< Reserved

    /* Seventh word */
    int32 afc_rx_freq_error; ///< AFC Rx Freq Error [Hz]
} lte_ml1_gm_log_pucch_tx_report_record_v26_s;

typedef enum
{
    MOD_BPSK = 0,
    MOD_QPSK = 1,
    MOD_16QAM = 2,
    MOD_64QAM = 3,
    MOD_TYPE_MAX_WIDTH = 0xFFFFFFFF, ///< @internal set enum to 32bits
} lte_ml1_log_modulation_type_e;

/*! @brief
    Packet ID: LOG_LTE_ML1_PUSCH_POWER_CONTROL_LOG_C  (0xB16E)
*/
#define LTE_ML1_LOG_PUSCH_POWER_CONTROL_RECORD_VERSION 24
#define LTE_ML1_LOG_PUSCH_POWER_CONTROL_MAX_CNT 50
typedef struct {
    /*! ********** per record **************/

    uint32 cell_index : 3;
    uint32 sfn : 10;
    uint32 sub_fn : 4;
    int32 pusch_tx_power : 8;
    uint32 dci_format : 4;
    uint32 tx_type : 2;
    uint32 reserved0 : 1;

    uint32 num_rbs : 8;
    uint32 transport_block_size : 14;
    uint32 dl_path_loss : 8;
    uint32 reserved1 : 2;

    int32 f_i : 10;
    uint32 tpc : 5;
    int32 pusch_actual_tx_power : 8;
    uint32 max_power : 8;
    uint32 reserved2 : 1;

} lte_ml1_log_pusch_power_control_record_s;

typedef struct {
    /*!***************** Version *****************/
    uint32 version : 8; // 32bit word1 start

    /*!***************** Number of records available *****************/
    uint32 reserved0 : 16;
    uint32 num_records : 8; // 32bit word1 end

    /*!***************** records *****************/
    /*! VARIABLE SIZE ALLOCATION based on num of records field */
    lte_ml1_log_pusch_power_control_record_s record[LTE_ML1_LOG_PUSCH_POWER_CONTROL_MAX_CNT];
} lte_ml1_log_pusch_power_control_records_s;

#define MAX_MCS_INDEX 32 /// 0~31

/*==========================================================================*/
/*! @brief
 * LTE GM TX Report (0xB16D)
 * This log packet gives important parameters about transmission on
 * PUSCH/PUCCH. The log record is generated once every 50 sub-frames or
 * every 1 second, whichever occurs first.
---------------------------------------------------------------------------*/

/*! @brief GM PUSCH Tx Report Record definition : version 33 */
typedef struct {
    /* First word */
    uint32 reserved1;

    /* Second word */
    uint32 chan_type : 1;   ///< 0 - PUCCH
                            ///< 1 - PUSCH
    uint32 cell_idx : 3;    ///< Cell Index; Range 0 to 7
    uint32 sfn : 10;        ///< PUSCH OTA system frame number
                            ///< Range is 0 to 1023
    uint32 sub_fn : 4;      ///< PUSCH OTA subframe number
                            ///< Range is 0 to 9
    uint32 trblk_size : 14; ///< PUSCH transport block size in bytes

    /* Third word */
    uint32 csf_present_flag : 1;     ///< Whether CSF is being scheduled
    uint32 ack_nak_present_flag : 2; ///< Whether UL ACK/NAK is present
                                     ///< 0: ACK/NAK not present
                                     ///< 1: ACK/NAK present
                                     ///< 2: Forced NAK present
    uint32 ca_mode_enabled : 1;      ///< Whether at least a Scell is configured
                                     ///< 0: CA mode disabled
                                     ///< 1: CA mode enabled
    uint32 ack_reporting_mode : 3;   ///< Ack/Nak reporting mode - lte_ml1_log_ul_ack_reporting_mode_type_e
    uint32 reserved2 : 25;

    /* Fourth, Fifth and Sixth word */
    uint16 ack_nak_to_send_mask[LTE_ML1_LOG_GM_PUCCH_TX_ACK_TO_SEND_MASK_NUM];
    ///< The mask to indicate which DL subframe needs to ACK or NAK.
    ///< Shown only when '(."UL ACK/NAK Present" == 1 || ."UL ACK/NAK Present" == 2) && ."CA Mode Enabled" ? 5 : 0	uint32 reserved1: 16;
    uint16 reserved3;

    /* Seventh word */
    uint32 dci_0_present : 1;   ///< Whether this PUSCH is based on DCI 0
                                ///< 0: DCI 0 grant not present
                                ///< 1: DCI 0 grant present
    uint32 w_ul_dai : 4;        ///< This field is printed only when "CA Mode Enabled" is 1
    uint32 n_bundled : 4;       ///< The total number of subframes in current bundling.
                                ///< This field is displayed only when
                                ///< ACK/NAK Reporting Mode is 0x1 or 0x2.
                                ///< Range: 1 to 9
    uint32 end_of_bundling : 4; ///< The very last subframe number of current bundling.
                                ///< This field is displayed only
                                ///< when ACK/NAK Reporting Mode is 0x1 or 0x2.
                                ///< Range: 0 to 9
    uint32 ack_nak_len : 3;     ///< ACK/NAK length
                                ///< This field is printed only when it is greater than 0
    uint32 beta_pusch : 16;     ///< Range is 0 to 65535

    /* Eighth word */
    uint32 total_tx_power : 8;    ///< Range is -112 dBm to 23 dBm, with 1 dB stepsize
    uint32 cyclic_shift_dmrs : 3; ///< Cyclic Shift DMRS
    uint32 rb_start : 7;          ///< Starting resource block number (1-110)
    uint32 rv : 2;                ///< Redundancy Version
    uint32 mod_type : 2;          // lte_ml1_log_modulation_type_e
    uint32 num_rbs : 7;           ///< Number of contiguous resource blocks (1-110)
    uint32 harq_id : 3;           ///< Harq index

    /* Nineth word */
    uint32 retx_index : 5;              ///< Retransmission index
    uint32 hopping_flag : 1;            ///< Freq Hopping Flag
                                        ///< 0x0 - Disabled
                                        ///< 0x1 - Enabled
    uint32 harq_ack_offset : 4;         ///< I_harq_ack_offset
    uint32 cqi_offset : 4;              ///< I_cqi_offset
    uint32 ri_offset : 4;               ///< I_ri_offset
    uint32 hopping_payload : 2;         ///< Used to determine VRB to PRB mapping
                                        ///< Range is 0 to 3
    uint32 srs_present_flag : 1;        ///< Whether SRS is being scheduled
    uint32 srs_ue_or_cell_specific : 1; ///< Cell or UE specific SRS occasion
    uint32 n_dmrs : 3;                  ///< N_dmrs
    uint32 antenna_num : 2;             ///< Number of antenna
    uint32 tti_bundl_index : 3;         ///< TTI Bundle Index
                                        ///< Range: 0 to 3
                                        ///< 0x7 - Invalid
    uint32 eib_index : 1;               ///< EIB Index; Range: 0 to 1
    uint32 alloc_type : 1;              ///< Resource allocation type - 0 or 1

    /* Tenth word */
    uint32 mcs_index : 5;  ///< The index for mcs
    uint32 rb_start_2 : 7; ///< Starting resource block number for second cluster(1-110)
    uint32 num_rbs_2 : 7;  ///< Number of contiguous resource blocks for second cluster (1-110)
    uint32 reserved4 : 13;

    /* Eleventh word */
    uint8 cc_max_tx_power;    ///< Max Power for the CCRange is -112 dBm to 23 dBm, 1 dB step
    uint8 total_max_tx_power; ///< Total Max Power for the CCRange is -112 dBm to 23 dBm, 1 dB step
    uint16 reserved5;

    /* Twelve word */
    int32 afc_rx_freq_error; ///< AFC Rx Freq Error [Hz]
} lte_ml1_gm_log_pusch_tx_report_record_v33_s;

/*! @brief GM PUSCH Tx Report Record definition : version 26 */
typedef struct {
    /* First word */
    uint32 chan_type : 1;   ///< 0 - PUCCH
                            ///< 1 - PUSCH
    uint32 cell_idx : 3;    ///< Cell Index; Range 0 to 7
    uint32 sfn : 10;        ///< PUSCH OTA system frame number
                            ///< Range is 0 to 1023
    uint32 sub_fn : 4;      ///< PUSCH OTA subframe number
                            ///< Range is 0 to 9
    uint32 trblk_size : 14; ///< PUSCH transport block size in bytes

    /* Second word */
    uint32 csf_present_flag : 1;     ///< Whether CSF is being scheduled
    uint32 ack_nak_present_flag : 2; ///< Whether UL ACK/NAK is present
                                     ///< 0: ACK/NAK not present
                                     ///< 1: ACK/NAK present
                                     ///< 2: Forced NAK present
    uint32 ca_mode_enabled : 1;      ///< Whether at least a Scell is configured
                                     ///< 0: CA mode disabled
                                     ///< 1: CA mode enabled
    uint32 ack_reporting_mode : 3;   ///< Ack/Nak reporting mode - lte_ml1_log_ul_ack_reporting_mode_type_e

    uint32 ack_nak_to_send_mask : 9;         ///< The mask to indicate which DL subframe needs to ACK or NAK.
                                             ///< This array is printed only when
                                             ///< "UL ACK/NAK Present Flag" is 1 or 2,
                                             ///< and "ACK/NAK Reporting Mode" is 1-6.
    uint32 ack_nak_to_send_mask_scell_1 : 9; ///< This field is printed only when
                                             ///< "CA Mode Enabled" is 1
    uint32 reserved2 : 7;

    /* Third word */
    uint32 ack_nak_to_send_mask_scell_2 : 9; ///< This field is printed only when
                                             ///< "CA Mode Enabled" is 1
    uint32 dci_0_present : 1;                ///< Whether this PUSCH is based on DCI 0
                                             ///< 0: DCI 0 grant not present
                                             ///< 1: DCI 0 grant present
    uint32 w_ul_dai : 4;                     ///< This field is printed only when "CA Mode Enabled" is 1
    uint32 n_bundled : 4;                    ///< The total number of subframes in current bundling.
                                             ///< This field is displayed only when
                                             ///< ACK/NAK Reporting Mode is 0x1 or 0x2.
                                             ///< Range: 1 to 9
    uint32 end_of_bundling : 4;              ///< The very last subframe number of current bundling.
                                             ///< This field is displayed only
                                             ///< when ACK/NAK Reporting Mode is 0x1 or 0x2.
                                             ///< Range: 0 to 9
    uint32 ack_nak_len : 3;                  ///< ACK/NAK length
                                             ///< This field is printed only when it is greater than 0
    uint32 reserved3 : 7;

    /* Fourth word */
    uint32 beta_pusch : 16;       ///< Range is 0 to 65535
    uint32 total_tx_power : 8;    ///< Range is -112 dBm to 23 dBm, with 1 dB stepsize
    uint32 cyclic_shift_dmrs : 3; ///< Cyclic Shift DMRS
    uint32 reserved4 : 5;

    /* Fifth word */
    uint32 rb_start : 7;        ///< Starting resource block number (1-110)
    uint32 rv : 2;              ///< Redundancy Version
    uint32 mod_type : 2;        // lte_ml1_log_modulation_type_e
    uint32 num_rbs : 7;         ///< Number of contiguous resource blocks (1-110)
    uint32 harq_id : 3;         ///< Harq index
    uint32 retx_index : 5;      ///< Retransmission index
    uint32 hopping_flag : 1;    ///< Freq Hopping Flag
                                ///< 0x0 - Disabled
                                ///< 0x1 - Enabled
    uint32 harq_ack_offset : 4; ///< I_harq_ack_offset
    uint32 reserved5 : 1;

    /* Sixth word */
    uint32 cqi_offset : 4;              ///< I_cqi_offset
    uint32 ri_offset : 4;               ///< I_ri_offset
    uint32 hopping_payload : 2;         ///< Used to determine VRB to PRB mapping
                                        ///< Range is 0 to 3
    uint32 srs_present_flag : 1;        ///< Whether SRS is being scheduled
    uint32 srs_ue_or_cell_specific : 1; ///< Cell or UE specific SRS occasion
    uint32 n_dmrs : 3;                  ///< N_dmrs
    uint32 antenna_num : 2;             ///< Number of antenna
    uint32 tti_bundl_index : 3;         ///< TTI Bundle Index
                                        ///< Range: 0 to 3
                                        ///< 0x7 - Invalid
    uint32 eib_index : 1;               ///< EIB Index; Range: 0 to 1
    uint32 mcs_index : 5;               ///< The index for mcs
    uint32 padding : 6;                 ///< To pad 6 bits to align PUCCH and PUSCH tx records. Not displayed.

    /* Sevent word */
    uint32 alloc_type : 1; ///< Resource allocation type - 0 or 1
    uint32 rb_start_2 : 7; ///< Starting resource block number for second cluster(1-110)
    uint32 num_rbs_2 : 7;  ///< Number of contiguous resource blocks for second cluster (1-110)
    uint32 reserved6 : 17;

    /* Eighth word */
    int32 afc_rx_freq_error; ///< AFC Rx Freq Error [Hz]
} lte_ml1_gm_log_pusch_tx_report_record_v26_s;

/*! @brief GM Tx Report Log packet union */
typedef union
{
    lte_ml1_gm_log_pucch_tx_report_record_v26_s pucch_tx_report_v26;
    lte_ml1_gm_log_pusch_tx_report_record_v26_s pusch_tx_report_v26;
    lte_ml1_gm_log_pucch_tx_report_record_v33_s pucch_tx_report_v33;
    lte_ml1_gm_log_pusch_tx_report_record_v33_s pusch_tx_report_v33;
} lte_ml1_gm_log_tx_report_u;

/*! @brief GM Tx Report Log packet struct */
typedef struct {
    lte_ml1_gm_log_tx_report_u tx_report;
} lte_ml1_gm_log_tx_report_record_s;

/*! @brief This defines the radio bearer configuration index type
 */
typedef uint8 lte_rb_cfg_idx_t;

/*! @brief Maximum Number of active Signalling Radio Bearers supported in UE
    TBD: Need to confirm (shall we consider only AM/UM SRBs here?)
*/
#define LTE_MAX_ACTIVE_SRB 2

/*! @brief Maximum Number of active Data Radio Bearers supported in UE
    TBD: Need to change the value of this macro as per decision in 3gpp
*/
#define LTE_MAX_ACTIVE_DRB 8

/*! @brief Maximum Number of Active Radio Bearers (SRB + DRB)
 */
#define LTE_MAX_ACTIVE_RB (LTE_MAX_ACTIVE_SRB + LTE_MAX_ACTIVE_DRB)

/*! @brief Type for a 32-bit Unique Message ID (UMID).
 */
typedef uint32 msgr_umid_type;

/*! @brief Type for a TECH/MODULE pair
 */
typedef uint16 msgr_tech_module_type;

typedef uint8 msgr_proc_t;

typedef uint8 msgr_priority_t;
typedef msgr_priority_t msgr_priority_type;

/*! @brief Type for message header.
 */
#ifdef __GNUC__
typedef struct ALIGN(8)
#else
typedef struct
#endif // __GNUC__
{
    msgr_umid_type id;           /*!< Unique Message ID (UMID) */
    msgr_tech_module_type src;   /*!< Sending module */
    msgr_proc_t proc;            /*!< Sending proc */
    uint8 num_attach;            /*!< Number of attachments */
    msgr_priority_type priority; /*!< Priority of the message */
    uint8 inst_id;               /*!< Generic unsigned user data field.
                                    Appropriate conversion to be
                                    performed by user to pass data
                                    like sys_modem_as_id_e_type
                                    inside this field */
    uint8 options_mask;          /*!< Bit field to set optional params */
} msgr_hdr_s;
typedef msgr_hdr_s msgr_hdr_struct_type;

/*! @brief RLCUL statistics per radio bearer
 */
typedef struct {
    lte_rb_cfg_idx_t rb_cfg_idx;    /*!< RB configuration index: unique */
    uint32 num_new_data_pdus_txed;  /*!< Total number of new RLC PDUs txed */
    uint32 new_data_pdu_bytes_txed; /*!< Total new data pdu bytes transmitted
                                       including header */
    uint32 num_sdus_txed;           /*!< Total number of new RLC SDUs txed */
    uint32 sdu_bytes_txed;          /*!< Total number of new RLC SDUs txed in
                                       bytes */
    uint32 num_pdus_re_txed;        /*!< Total number of re-txed RLC PDUs */
    uint32 pdu_bytes_re_txed;       /*!< Total pdu bytes re-transmitted */
} lte_rlcul_rb_stats_s;

/*! @brief RLCUL statistics
 */
typedef struct {
    uint32 num_active_rb;                             /*!< number of active RB's */
    lte_rlcul_rb_stats_s rb_stats[LTE_MAX_ACTIVE_RB]; /*!< stats per RB only
         the first num_active_rb elements in the array are valid */
} lte_rlcul_stats_s;

/*! @brief statistics request message for RLCUL
 */
typedef struct {
    msgr_hdr_struct_type msg_hdr; /*!< common message router header */
    lte_rlcul_stats_s *stats_ptr; /*!< pointer to the statistics */
} lte_rlcul_stats_req_s;

/* Bitfields may not be ANSI, but all our compilers
** recognize it and *should* optimize it.
** Not that bit-packed structures are only as long as they need to be.
** Even though we call it uint32, it is a 16 bit structure.
*/
typedef struct {
    uint16 id : 12;
    uint16 reserved : 1;
    uint16 payload_len : 2; /* payload length (0, 1, 2, see payload) */
    uint16 time_trunc_flag : 1;
} event_id_type;

/* The event payload follows the event_type structure */
typedef struct {
    uint8 length;
    uint8 payload[1]; /* 'length' bytes */
} event_payload_type;

/* NOTE: diag_event_type and event_store_type purposely use the same
   format, except that event_store type is preceeded by a Q link
   and the event ID field has a union for internal packet formatting.
   If either types are changed, the service will not function properly. */

struct event_store_type {
    uint8 cmd_code; /* 96 (0x60)            */
    uint16 length;  /* Length of packet     */
    union
    {
        event_id_type event_id_field;
    } event_id;
    uint32 ts_lo; /* Time stamp */
    uint32 ts_hi;

    event_payload_type payload;
} __attribute__((__packed__));

typedef struct PACK() {
    byte log_version; /*!< Log packet version number*/
    byte context_type;
    byte bearer_id;
    byte bearer_state;
    byte connection_id;
    word sdf_id;
    // byte ota_context_data[LTE_NAS_OTA_MSG_MAX_SIZE];
    boolean lbi_valid;
    byte lbi;
    byte rb_id;
    byte eps_qos[10]; /* Refer logging ICD section 8.1.6 for formatting of this data*/
} lte_nas_esm_bearer_context_info_T;

typedef struct PACK() {
    byte log_version; /*!< Log packet version number*/
    byte bearer_id;
    byte bearer_state;
    byte connection_id;
} lte_nas_esm_bearer_context_state_T;

/*==========================================================================*/
/*! @brief
 * 5 PUCCH CSF Log (Log code ID: 0xB14d)
 *
 *  This log packet gives important parameters related to the CSF log results
 *  in PUCCH periodic reporting mode. The log record is generated on sub-frames
 *  receiving CSF reporting request in periodic reporting mode from ML.
---------------------------------------------------------------------------*/
#define LTE_LL1_LOG_CSF_PUCCH_VERSION 101

/*! @brief pucch_csf_log main struct
 */
typedef struct {
    uint32 version : 8;                           ///< Range: 0...255
    uint32 start_system_sub_frame_number_sfn : 4; ///< Range 0..9
    uint32 start_system_frame_number : 10;        ///< Range 0..1023
    uint32 reserved0 : 1;
    uint32 pucch_reporting_mode : 2; ///< Range: 0..3
                                     ///< 0: MODE_1_0
                                     ///< 1: MODE_1_1
                                     ///< 2: MODE 2_0
                                     ///< 3: MODE 2_1
    uint32 pucch_report_type : 4;    ///< Range: 0..9
                                     ///< 0x0: TYPE_1A_SBCQI_PMI2
                                     ///< 0x1: TYPE_1_SBCQI
                                     ///< 0x2: TYPE_2_WBCQI_PMI
                                     ///< 0x3: TYPE_3_RI
                                     ///< 0x4: TYPE_4_WBCQI
                                     ///< 0x5: TYPE_5_RI_WBPMI1
                                     ///< 0x6: TYPE_6_RI_PTI
                                     ///< 0x7: TYPE_2A_WBPMI
                                     ///< 0x8: TYPE_2B_WBCQI_WBPMI1_WBCQI2
                                     ///< 0x9: TYPE_2C_WBCQI_WBPMI1_WBPMI2_WBCQI2

    uint32 size_bwp : 3;           ///< Range: 0...4.
                                   ///< This field is valid only when pucch_report_type = type 1
    uint32 number_of_subbands : 4; ///< Range: 1..14
    uint32 bwp_index : 3;          ///< Range: 0...4
                                   /// This field is valid only when pucch_report_type = type 1
    uint32 reserved1 : 1;
    uint32 subband_label : 2; ///< Range: 0..3
                              ///< This field is valid only when pucch_report_type = type 1
    uint32 cqi_cw0 : 4;       ///< Range: 0..15
                              ///< Wideband CQI for CW0 in type 2 & type 4 reporting
                              ///< Subband  CQI for CW0 in type 1 reporting
                              ///< This field is valid only when pucch_report_type = type 1 or type 2 or type4
    uint32 cqi_cw1 : 4;       ///< Range: 0...15
                              ///< Wideband CQI for CW1 in type 2
                              ///< Subband  CQI for CW1 in type 1
                              ///< This field is valid when pucch_report_type = type 1 and pucch_reporting mode = 3 (Mode 2_1) and
                              ///< This field is valid when pucch_report_type = type 2 and pucch_reporting mode = 1 (Mode 1_1)
    uint32 wideband_pmi : 4;  ///< Range: 0...15 for 4 Tx, Rank 1 & 2
                              ///< Range: 0...1 for 2 Tx, Rank 2
                              ///< Range: 0...3 for 1 Tx, Rank 1
                              ///< This field is valid only when pucch_report_type = type 2

    uint32 carrier_index : 4;           ///< 0-PCC, 1-SCC
    uint32 csf_tx_mode : 4;             ///< Range: 1..9
                                        ///< 0: TM_INVALID
                                        ///< 1: TM_SINGLE_ANT_PORT0
                                        ///< 2: TM_TD_RANK1
                                        ///< 3: TM_OL_SM
                                        ///< 4: TM_CL_SM
                                        ///< 5: TM_MU_MIMO
                                        ///< 6: TM_CL_RANK1_PC
                                        ///< 7: TM_SINGLE_ANT_PORT5
                                        ///< 8: TM8
                                        ///< 9: TM9
    uint32 pucch_reporting_submode : 2; ///< Range: 0..2
                                        ///< Invalid submode
                                        ///< submode 1
                                        ///< submode 2
    uint32 num_csirs_ports : 4;         ///< Range: 1...8

    uint32 wideband_pmi_1 : 4; ///< Range: 0..15
                               /// range 0-15, valid when num_csirs_ports=8

    uint32 pti : 1;              ///< Range:0..1
                                 /// range 0..1, valid when num_csirs_ports=8 and pucch_report_type=TYPE_6_RI_PTI
    uint32 csi_meas_set_idx : 1; ///< CSI0 vs. CSI1; or sometimese we say set1 vs. set2
    uint32 rank_index : 2;       ///< Range: 0...3
                                 ///< This field is valid only when pucch_report_type = type 3
                                 ///< 0: Rank 1
                                 ///< 1: Rank 2
                                 ///< 2: Rank 3
                                 ///< 3: Rank 4
    uint32 reserved2 : 20;

} lte_LL1_log_csf_pucch_report_ind_struct_v101;

typedef struct {
    uint32 version : 8;                           ///< Range: 0...255
    uint32 start_system_sub_frame_number_sfn : 4; ///< Range 0..9
    uint32 start_system_frame_number : 10;        ///< Range 0..1023

    uint32 carrier_index : 4;
    uint32 scell_index : 4;
    uint32 reserved1 : 1;

    uint32 pucch_reporting_mode : 2; ///< Range: 0..3
                                     ///< 0: MODE_1_0
                                     ///< 1: MODE_1_1
                                     ///< 2: MODE 2_0
                                     ///< 3: MODE 2_1
    uint32 pucch_report_type : 4;    ///< Range: 0..9
                                     ///< 0x0: TYPE_1A_SBCQI_PMI2
                                     ///< 0x1: TYPE_1_SBCQI
                                     ///< 0x2: TYPE_2_WBCQI_PMI
                                     ///< 0x3: TYPE_3_RI
                                     ///< 0x4: TYPE_4_WBCQI
                                     ///< 0x5: TYPE_5_RI_WBPMI1
                                     ///< 0x6: TYPE_6_RI_PTI
                                     ///< 0x7: TYPE_2A_WBPMI
                                     ///< 0x8: TYPE_2B_WBCQI_WBPMI1_WBCQI2
                                     ///< 0x9: TYPE_2C_WBCQI_WBPMI1_WBPMI2_WBCQI2

    uint32 size_bwp : 3;           ///< Range: 0...4.
                                   ///< This field is valid only when pucch_report_type = type 1
    uint32 number_of_subbands : 4; ///< Range: 1..14
    uint32 bwp_index : 3;          ///< Range: 0...4

    uint32 alt_cqi_table_data : 2; /// Range: 0..1 0=> legacy table. 1=> ALT table

    uint32 subband_label : 2; ///< Range: 0..3
                              ///< This field is valid only when pucch_report_type = type 1
    uint32 cqi_cw0 : 4;       ///< Range: 0..15
                              ///< Wideband CQI for CW0 in type 2 & type 4 reporting
                              ///< Subband  CQI for CW0 in type 1 reporting
                              ///< This field is valid only when pucch_report_type = type 1 or type 2 or type4
    uint32 cqi_cw1 : 4;       ///< Range: 0...15
                              ///< Wideband CQI for CW1 in type 2
                              ///< Subband  CQI for CW1 in type 1
                              ///< This field is valid when pucch_report_type = type 1 and pucch_reporting mode = 3 (Mode 2_1) and
                              ///< This field is valid when pucch_report_type = type 2 and pucch_reporting mode = 1 (Mode 1_1)
    uint32 wideband_pmi : 4;  ///< Range: 0...15 for 4 Tx, Rank 1 & 2
                              ///< Range: 0...1 for 2 Tx, Rank 2
                              ///< Range: 0...3 for 1 Tx, Rank 1
                              ///< This field is valid only when pucch_report_type = type 2

    uint32 csf_tx_mode : 4; ///< Range: 1..9
                            ///< 0: TM_INVALID
                            ///< 1: TM_SINGLE_ANT_PORT0
                            ///< 2: TM_TD_RANK1
                            ///< 3: TM_OL_SM
                            ///< 4: TM_CL_SM
                            ///< 5: TM_MU_MIMO
                            ///< 6: TM_CL_RANK1_PC
                            ///< 7: TM_SINGLE_ANT_PORT5
                            ///< 8: TM8
                            ///< 9: TM9

    uint32 pucch_reporting_submode : 2; ///< Range: 0..2
                                        ///< Invalid submode
                                        ///< submode 1
                                        ///< submode 2
    uint32 num_csirs_ports : 6;         ///< Range: 1...32

    uint32 wideband_pmi_1 : 10; ///< Range: 0..1024, range 0-15, valid when num_csirs_ports=8
                                ///< Shown only when '(5 == ."PUCCH Report Type" || 7 == ."PUCCH
                                ///< Report Type") || 9 == ."PUCCH Report Type"

    uint32 pti : 1;              ///< Range:0..1
                                 /// range 0..1, valid when num_csirs_ports=8 and pucch_report_type=TYPE_6_RI_PTI
    uint32 csi_meas_set_idx : 1; ///< CSI0 vs. CSI1; or sometimese we say set1 vs. set2
    uint32 rank_index : 2;       ///< Range: 0...3
                                 ///< This field is valid only when pucch_report_type = type 3
                                 ///< 0: Rank 1
                                 ///< 1: Rank 2
                                 ///< 2: Rank 3
                                 ///< 3: Rank 4
    uint32 reserved2 : 20;

} lte_LL1_log_csf_pucch_report_ind_struct_v162;

/*==========================================================================*/
/*! @brief
 * 6 PUSCH CSF Log (Log code ID: 0xB14e)
 *
 *  This log packet gives important parameters related to the CSF log results
 *  in PUSCH aperiodic reporting mode. The log record is generated on sub-
 *  frames receiving CSF reporting request in aperiodic PUSCH  reporting mode
 *  from ML.
---------------------------------------------------------------------------*/
//#ifdef LTE_LL1_FEATURE_THOR_HORXD_LOG
#define LTE_LL1_LOG_CSF_PUSCH_VERSION 101 // new version
//#else
//#define LTE_LL1_LOG_CSF_PUSCH_VERSION 42
//#endif
/*! @brief pusch_csf_log main struct
 */
typedef struct {
    uint32 version : 8;                           ///< Range: 0...255
    uint32 start_system_sub_frame_number_sfn : 4; ///< Range 0..9
    uint32 start_system_frame_number : 10;        ///< Range 0..1023
    uint32 pusch_reporting_mode : 3;              ///< Range: 0..4
                                                  ///< 0: MODE_APERIODIC_RM12
                                                  ///< 1: MODE_APERIODIC_RM20
                                                  ///< 2: MODE_APERIODIC_RM22
                                                  ///< 3: MODE_APERIODIC_RM30
                                                  ///< 4: MODE_APERIODIC_RM31
    uint32 csi_meas_set_idx : 1;                  ///< CSI0 vs. CSI1; or sometimese we say set1 vs. set2
    uint32 rank_index : 2;                        ///< 0: Rank 1
                                                  ///< 1: Rank 2
                                                  ///< 2: Rank 3
                                                  ///< 3: Rank 4
    uint32 wideband_pmi_1 : 4;                    ///< Range: 0..15
                                                  /// range 0-15, valid when num_csirs_ports=8  uint32 number_of_subbands
                                                  /// end of first word
    uint32 number_of_subbands : 5;                ///< Range: 1..28
    uint32 wideband_cqi_cw0 : 4;                  ///< Range: 0...15
                                                  ///< This field is valid for all pusch_reporting_modes
    uint32 wideband_cqi_cw1 : 4;                  ///< Range: 0...15
                                                  ///< This field is valid only when pusch_reporting_mode = 0 or 2 or 4
                                                  ///< i.e, in reporting modes 1-2, 2-2 & 3-1
    uint32 subband_size_k : 4;                    ///< [4, 4, 6, 8]  for reporting modes conforming to
                                                  ///< Table 7.2.1-3 of 36.213
                                                  ///< [2,2,3,4 ] for reporiting modes conforming to
                                                  ///< Table 7.2.1-5 of 36.213
    uint32 size_m : 3;                            ///< [1,3,5,6]
                                                  ///< Table 7.2.1-5 of 36.213
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 and 2-2
    uint32 single_wb_pmi : 4;                     ///< Range: 0...15 for 4 Tx, Rank1  & Rank2
                                                  ///< Range: 0...1 for 2 Tx, Rank2
                                                  ///< Range: 0...3 for 2 Tx, Rank1
                                                  ///< This field is valid only when pusch_reporting_mode = 2 or 4
                                                  ///< i.e, in reporting modes 2-2 & 3-1
    uint32 single_mb_pmi : 4;                     ///< Range: 0...15 for 4 Tx, Rank1  & Rank2
                                                  ///< Range: 0...1 for 2 Tx, Rank2
                                                  ///< Range: 0...3 for 2 Tx, Rank1
                                                  ///< This field is valid only when pusch_reporting_mode = 2
                                                  ///< i.e, in reporting mode 2-2
    uint32 csf_tx_mode : 4;                       ///< Range: 0..9
                                                  ///< 0: TM_INVALID
                                                  ///< 1: TM_SINGLE_ANT_PORT0
                                                  ///< 2: TM_TD_RANK1
                                                  ///< 3: TM_OL_SM
                                                  ///< 4: TM_CL_SM
                                                  ///< 5: TM_MU_MIMO
                                                  ///< 6: TM_CL_RANK1_PC
                                                  ///< 7: TM_SINGLE_ANT_PORT5
                                                  ///< 8: TM8
                                                  ///< 9: TM9, end of second word

    uint32 subband_cqi_cw0_sb0 : 4;  ///< (4 bits per SubBand  x Max_num_SubBands)
                                     ///< Max_num_SubBands = 14 for 2Tx
                                     ///< Max_num_SubBands = 10 for 4Tx
                                     ///< OR
                                     ///< M_SubBand_CQI_CW0 in reporting modes 2-0 &  2-2
                                     ///< Range: 0...15
                                     ///< This field is valid only when pusch_reporting_mode = 1 or 2 or 3 or 4
                                     ///< i.e, in reporting modes 2-0, 2-2, 3-0 & 3-1
    uint32 subband_cqi_cw0_sb1 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb2 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb3 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb4 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb5 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb6 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb7 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb8 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb9 : 4;  ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb10 : 4; ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb11 : 4; ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb12 : 4; ///< This field is valid only when pusch_reporting_mode = 3 or 4
    uint32 subband_cqi_cw0_sb13 : 4; ///< This field is valid only when pusch_reporting_mode = 3 or 4

    uint32 subband_cqi_cw1_sb0 : 4;  ///< (4 bits per SubBand  x Max_num_SubBands)
                                     ///< Max_num_SubBands = 14 for 2Tx
                                     ///< Max_num_SubBands = 10 for 4Tx
                                     ///< OR
                                     ///< M_SubBand_differential_CQI_CW1 in reporting mode 2-2
                                     ///< Range: 0...15
                                     ///< This field is valid only when pusch_reporting_mode = 2 or 4
                                     ///< i.e, in reporting modes 2-2 & 3-1
    uint32 subband_cqi_cw1_sb1 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb2 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb3 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb4 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb5 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb6 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb7 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb8 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb9 : 4;  ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb10 : 4; ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb11 : 4; ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb12 : 4; ///< This field is valid only when pusch_reporting_mode = 4
    uint32 subband_cqi_cw1_sb13 : 4; ///< This field is valid only when pusch_reporting_mode = 4
    uint32 reserved3 : 16;

    uint32 position_of_m_select_subbands_sb0 : 5; ///< num_SubBands (max  = 28), 1st best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 position_of_m_select_subbands_sb1 : 5; /// 2nd best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 position_of_m_select_subbands_sb2 : 5; /// 3rd best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 position_of_m_select_subbands_sb3 : 5; /// 4th best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 position_of_m_select_subbands_sb4 : 5; /// 5th best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 position_of_m_select_subbands_sb5 : 5; /// 6th best subband among M
                                                  ///< This field is valid only when pusch_reporting_mode = 1 or 2
                                                  ///< i.e, in reporting modes 2-0 & 2-2
    uint32 reserved4 : 2;
    uint32 multi_sb_pmi_sb0 : 4;  ///< = max( 40, 14, 28)  bits total
                                  ///< ( 4 pmi bits per SubBand x max_num_SubBands =
                                  ///< 10) for 4 Tx
                                  ///< (1 pmi bit per SubBand x max_num_SubBands = 14)
                                  ///< for 1/2 Tx, Rank 2
                                  ///< (2 pmi bits per SubBand x max_num_SubBands =
                                  ///< 14) for 1/2 Tx, Rank 1
                                  ///< This field is valid only when pusch_reporting_mode = 0
                                  ///< i.e, in reporting modes 1-2
    uint32 multi_sb_pmi_sb1 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb2 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb3 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb4 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb5 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb6 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb7 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb8 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb9 : 4;  ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb10 : 4; ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb11 : 4; ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb12 : 4; ///< This field is valid only when pusch_reporting_mode = 0
    uint32 multi_sb_pmi_sb13 : 4; ///< This field is valid only when pusch_reporting_mode = 0
    uint32 carrier_index : 4;     ///< 0-PCC, 1-SCC
    uint32 num_csirs_ports : 4;   ///< Range: 1...8
} lte_LL1_log_csf_pusch_report_ind_struct;

/*==========================================================================*/
/*! @brief
* LTE LL1 PDSCH Demapper Configuration (Log code ID: 0xB126)
*
  ==========================================================================*/

#define LTE_LL1_LOG_PDSCH_DEMAPPER_CFG_VERSION 121 // new version

/*! @brief lte_ll1_pdsch_demapper_cfg_ind_struct main struct
 */
typedef struct {
    uint32 version : 8;
    uint32 serving_cell_id : 9;
    uint32 sub_frame_number : 4;
    uint32 system_frame_number : 10;
    uint32 reserved_0 : 1;
    uint32 fill_words_0[10];
    uint32 mu_receiver_mode : 2;
    uint32 pmi_index : 4;
    uint32 transmission_scheme : 4;
    uint32 port_enabled : 2;
    uint32 bmod_fd_sym_index : 4;
    uint32 reserved_1 : 16;
    uint32 fill_words_1[2];
} lte_ll1_pdsch_demapper_cfg_ind_struct;

/*==========================================================================*/
/*! @brief
* LTE ML1 LTE Downlink Common Configuration (Log code ID: 0xB160)
*
  ==========================================================================*/

#define LTE_ML1_LOG_LTE_DL_COMMON_CFG_VERSION 2 // new version

/*! @brief lte_ll1_dl_common_cfg_ind_struct main struct
 */
typedef struct {
    uint32 version : 8;
    uint32 reserved_0 : 24;
    uint32 fill_words_0;
    uint32 mib_info_present : 1;
    uint32 mib_info : 6;
    uint32 pdsch_config_present : 1;
    uint32 reference_signal_power : 8;
    uint32 p_b : 8;
    uint32 reserved_1 : 8;
    uint32 fill_words_1[2];
} lte_ll1_dl_common_cfg_ind_struct;

/*! @brief Subpacket struct for serv cell measurement result*/
typedef struct PACK() {
    /*! Physical cell ID, 0 ~ 504*/
    uint16 phy_cell_id : 9;
    /*! Serving cell index: 0..7 */
    uint16 serv_cell_index : 3;
    uint32 is_serv_cell : 1;
    uint32 reserved_13 : 19;

    /*! System frame time when measurements are made, 0 ~ 1023*/
    uint32 current_sfn : 10;
    /*! System subframe time when measurements are made, 0 ~ 9*/
    uint32 current_subfn : 4;
    uint32 reserved_46 : 18;

    /*!Measurement done on restricted subframe , 0 or 1 */
    uint32 is_subfm_restricted : 1;
    /*! cell timing for rx antenna 0, 0 ~ 307199*/
    uint32 cell_timing_0 : 19;
    uint32 reserved_84 : 12;

    /*! cell timing for rx antenna 1, 0 ~ 307199*/
    uint32 cell_timing_1 : 19;
    /*! SFN for rx antenna 0 corresponding to cell timing, 0 ~ 1023*/
    uint32 cell_timing_sfn_0 : 10;
    uint32 reserved_125 : 3;

    /*! SFN for rx antenna 1 corresponding to cell timing, 0 ~ 1023*/
    uint32 cell_timing_sfn_1 : 10;
    /*! Inst RSRP value for antenna 0, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_0 : 12;
    uint32 reserved_150 : 22;

    /*! Inst RSRP value for antenna 1, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_1 : 12;
    uint32 reserved_184 : 20;

    /*! Inst RSRP value for antenna 2, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_2 : 12;
    uint32 reserved_216 : 8;

    /*! Inst RSRP value for antenna 3, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_3 : 12;
    /*! Inst RSRP value combined across tx-rx pairs, -140 ~ -40 dBm*/
    uint32 inst_rsrp : 12;
    // QC doc bug : 20 bits of reserved field missing
    uint32 reserved_248 : 20;
    /*! Filtered RSRP value */
    uint32 filtered_rsrp : 12;
    uint32 reserved_280 : 8;

    /*! Inst RSRQ value for antenna 0, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_0 : 10;
    uint32 reserved_298 : 10;

    /*! Inst RSRQ value for antenna 1, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_1 : 10;
    uint32 reserved_318 : 12;

    /*! Inst RSRQ value for antenna 2, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_2 : 10;
    /*! Inst RSRQ value for antenna 3, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_3 : 10;
    // QC doc bug : 2 bits of reserved field missing
    uint32 reserved_350 : 2;

    /*! Inst RSRQ value combined across tx-rx pairs, -30 ~ 0 dBm*/
    uint32 inst_rsrq : 10;
    uint32 reserved_362 : 10;
    /*! Filtered RSRQ value */
    uint32 filtered_rsrq : 12;

    /*! Inst RSSI value for antenna 0, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_0 : 11;
    /*! Inst RSSI value for antenna 1, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_1 : 11;
    uint32 reserved_406 : 10;

    /*! Inst RSSI value for antenna 2, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_2 : 11;
    /*! Inst RSSI value for antenna 3, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_3 : 11;
    uint32 reserved_438 : 10;

    /*! Inst RSRQ value combined across tx-rx pairs, -110 ~ -10 dBm*/
    uint32 inst_rssi : 11;
    uint32 reserved_459 : 5;
    uint32 reserved_464[3];

    /*! Residual frequency error */
    int16 res_freq_error;
    uint32 reserved_576[2];

    /*! FTL-SNR value for antenna 0, -20 to 30 dBm*/
    uint32 ftl_snr_rx_0 : 9;
    /*! FTL-SNR value for antenna 1, -20 to 30 dBm*/
    uint32 ftl_snr_rx_1 : 9;
    uint32 reserved_658 : 14;
    /*! FTL-SNR value for antenna 2, -20 to 30 dBm*/
    uint32 ftl_snr_rx_2 : 9;
    /*! FTL-SNR value for antenna 3, -20 to 30 dBm*/
    uint32 ftl_snr_rx_3 : 9;
    uint32 reserved_690[3];
    // QC doc bug : 14 bits of reserved field missing
    uint32 reserved_786 : 14;

    /*! Projected SIR in Q_format 4 */
    int32 projected_sir;
    /*! Post IC RSRQ */
    uint32 post_ic_rsrq;

    /*! CINR value for antenna 0, -20 to 45 dB */
    int32 cinr_rx_0;
    /*! CINR value for antenna 1, -20 to 45 dB */
    int32 cinr_rx_1;
    /*! CINR value for antenna 2, -20 to 45 dB */
    int32 cinr_rx_2;
    /*! CINR value for antenna 3, -20 to 45 dB */
    int32 cinr_rx_3;
} lte_ml1_sm_log_meas_result_per_cell_v33_35_s;

typedef struct PACK() {
    /*! Physical cell ID, 0 ~ 504*/
    uint16 phy_cell_id : 9;
    /*! Serving cell index: 0..7 */
    uint16 serv_cell_index : 3;
    uint32 is_serv_cell : 1;
    uint32 reserved_0 : 19;

    /*! System frame time when measurements are made, 0 ~ 1023*/
    uint32 current_sfn : 10;
    /*! System subframe time when measurements are made, 0 ~ 9*/
    uint32 current_subfn : 4;
    uint32 reserved_1 : 18;

    /*!Measurement done on restricted subframe , 0 or 1 */
    uint32 is_subfm_restricted : 1;
    /*! cell timing for rx antenna 0, 0 ~ 307199*/
    uint32 cell_timing_0 : 19;
    uint32 reserved_2 : 12;

    /*! cell timing for rx antenna 1, 0 ~ 307199*/
    uint32 cell_timing_1 : 19;
    /*! SFN for rx antenna 0 corresponding to cell timing, 0 ~ 1023*/
    uint32 cell_timing_sfn_0 : 10;
    uint32 reserved_3 : 3;

    /*! SFN for rx antenna 1 corresponding to cell timing, 0 ~ 1023*/
    uint32 cell_timing_sfn_1 : 10;
    /*! Inst RSRP value for antenna 0, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_0 : 12;
    uint32 reserved_4 : 22;

    /*! Inst RSRP value for antenna 1, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_1 : 12;
    uint32 reserved_5 : 20;

    /*! Inst RSRP value for antenna 2, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_2 : 12;
    uint32 reserved_6 : 8;

    /*! Inst RSRP value for antenna 3, -140 ~ -40 dBm*/
    uint32 inst_rsrp_rx_3 : 12;
    /*! Inst RSRP value combined across tx-rx pairs, -140 ~ -40 dBm*/
    uint32 inst_rsrp : 12;

    /*! Inst RSRQ value for antenna 0, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_0 : 10;
    uint32 reserved_7 : 10;

    /*! Inst RSRQ value for antenna 1, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_1 : 10;
    uint32 reserved_8 : 10;

    /*! Inst RSRQ value for antenna 2, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_2 : 10;
    uint32 reserved_9 : 2;

    /*! Inst RSRQ value for antenna 3, -30 to 0 dBm*/
    uint32 inst_rsrq_rx_3 : 10;
    /*! Inst RSRQ value combined across tx-rx pairs, -30 ~ 0 dBm*/
    uint32 inst_rsrq : 10;

    /*! Inst RSSI value for antenna 0, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_0 : 11;
    /*! Inst RSSI value for antenna 1, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_1 : 11;

    /*! Inst RSSI value for antenna 2, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_2 : 11;
    /*! Inst RSSI value for antenna 3, -110 ~ -10 dBm*/
    uint32 inst_rssi_rx_3 : 11;
    uint32 reserved_10 : 10;

    /*! Inst RSRQ value combined across tx-rx pairs, -110 ~ -10 dBm*/
    uint32 inst_rssi : 11;
    uint32 reserved_11 : 5;
    uint32 reserved_11_0[3];

    /*! Residual frequency error */
    int16 res_freq_error;
    uint32 reserved_12[2];

    /*! FTL-SNR value for antenna 0, -20 to 30 dBm*/
    uint32 ftl_snr_rx_0 : 9;
    /*! FTL-SNR value for antenna 1, -20 to 30 dBm*/
    uint32 ftl_snr_rx_1 : 9;
    /*! FTL-SNR value for antenna 2, -20 to 30 dBm*/
    uint32 ftl_snr_rx_2 : 9;
    /*! FTL-SNR value for antenna 3, -20 to 30 dBm*/
    uint32 ftl_snr_rx_3 : 9;
    uint32 reserved_13 : 28;
    uint32 reserved_13_0[3];

    /*! Projected SIR in Q_format 4 */
    int32 projected_sir;
    /*! Post IC RSRQ */
    uint32 post_ic_rsrq;

    /*! CINR value for antenna 0, -20 to 45 dB */
    int32 cinr_rx_0;
    /*! CINR value for antenna 1, -20 to 45 dB */
    int32 cinr_rx_1;
    /*! CINR value for antenna 2, -20 to 45 dB */
    int32 cinr_rx_2;
    /*! CINR value for antenna 3, -20 to 45 dB */
    int32 cinr_rx_3;
} lte_ml1_sm_log_meas_result_per_cell_v22_s;

#define LTE_MAX_NUM_CRS_IC_CELLS 3

typedef struct PACK() {
    /*! E-ARFCN, 0 ~ 39649 */
    uint32 earfcn;

    uint16 num_cells;
    uint16 horxd_mode : 1;
    uint16 reserved : 15;

    /* LTE_Ml1GenLog_IdleModeNbrCellMeasRequest_Version 7 is too old so
     * does not support. Only support Version 22 and 33 */
    union
    {
        lte_ml1_sm_log_meas_result_per_cell_v33_35_s meas_result_v35[1 + LTE_MAX_NUM_CRS_IC_CELLS];
        lte_ml1_sm_log_meas_result_per_cell_v33_35_s meas_result_v33[1 + LTE_MAX_NUM_CRS_IC_CELLS];
        lte_ml1_sm_log_meas_result_per_cell_v22_s meas_result_v22[1 + LTE_MAX_NUM_CRS_IC_CELLS];
    };
} lte_ml1_sm_log_serv_cell_meas_response_subpkt_s;

/*! @brief Serving Cell Measurement & Evaluation
 */
typedef struct {
    /*! Version info */
    uint32 version : 8;
    /*! standards version */
    uint32 standards_version : 8;

    uint32 reserved0 : 16;

    /*! E-ARFCN */
    uint32 earfcn;

    /*! Physical cell ID, 0 - 504 */
    uint16 phy_cell_id : 9;
    /*! Serving cell priority */
    uint16 serv_layer_prio : 4;
    /*! reserved */
    uint32 reserved1 : 19;

    /*! measured RSRP, -140 ~ -40 dBm*/
    uint32 measured_rsrp : 12;
    /*! reserved */
    uint32 reserved2 : 12;

    /*! averge RSRP, -140 ~ -40 dBm*/
    uint32 avg_rsrp : 12;
    /*! reserved */
    uint32 reserved3 : 12;

    /*! measured RSRQ, -30 ~ 0*/
    uint32 measured_rsrq : 10;
    /*! reserved */
    uint32 reserved4 : 10;

    /*! averge RSRQ, -30 ~ 0 */
    uint32 avg_rsrq : 10;

    /*! measured RSSI, -110 ~ -10 */
    uint32 measured_rssi : 11;
    /*! reserved */
    uint32 reserved5 : 11;

    /*! Qrxlevmin */
    uint32 q_rxlevmin : 6;
    /*! P_max, unit of db, -30 to 33, 0x00=-30, 0x1=-29, ...0x3F=33,
        0x40=NP for not present */
    uint32 p_max : 7;
    /*! max UE Tx Power */
    uint32 max_ue_tx_pwr : 6;
    /*! Srxlev */
    uint32 s_rxlev : 7;
    /*! num drxs S < 0 */
    uint32 num_drx_S_fail : 6;

    /*! S_intra_search*/
    uint32 s_intra_search : 6;
    /*! S_non_intra_search */
    uint32 s_non_intr_search : 6;
    /*! meas rules updated or not */
    uint32 meas_rules_updated : 1;
    /*! Measurement rules */
    uint32 meas_rules : 4;
    /*! reserved */
    uint32 reserved6 : 15;

#ifdef FEATURE_LTE_REL9
    /*! Range -34 to -3 */
    uint32 q_qualmin : 7;
    /*! Range -35 to +34 */
    uint32 s_qual : 7;
    /*! Range 0 to +31 */
    uint32 s_intra_search_q : 6;
    /*! Range 0 to +31 */
    uint32 s_non_intra_search_q : 6;
    uint32 reserved7 : 6;
#endif
} lte_ml1_sm_log_idle_serv_meas_eval_s;

/*! @brief
  The system/sub-frame number type
*/
typedef struct {
    uint16 sub_fn : 4;
    uint16 sys_fn : 10;
    uint16 xxx : 2;
} lte_sfn_s;

/*! @brief OTA message log structure
 */
typedef struct PACK() {
    uint8 log_packet_ver;         /*!< Log packet version */
    uint8 rrc_rel;                /*!< RRC release number */
    uint8 rrc_ver;                /*!< RRC version number; if 8.x.y, left 4 bits are for x,
                                     right 4 bits are for y */
    uint8 rb_id;                  /*!< Radio bearer id */
    uint16 phy_cell_id;           /*!< Physical cell ID */
    uint32 freq;                  /*!< Frequency of the cell */
    lte_sfn_s sfn;                /*!< SFN on which message was sent/received - 0 if N/A */
    uint8 pdu_num;                /*!< Identifies the message type
                                     2 - BCCH_DL_SCH Message
                                     3 - PCCH Message
                                     4 - DL_CCCH Message
                                     5 - DL_DCCH Message
                                     6 - UL_CCCH Message
                                     7 - UL_DCCH Message */
    uint32 sib_mask_in_si;        /*!< Bitmask of available sibs in BCCH_DL_SCH
                                            Will be 0 if no SIBS are availabe in BCCH_DL_SCH
                                            or if the msg is not of type BCCH_DL_SCH*/
    uint16 encoded_msg_len;       /*!< Length of ASN.1 encoded message */
    uint8 encoded_msg_first_byte; /*!< First byte of the ASN.1 encoded message
                                     sent/received (present only when message
                                     length <= LTE_RRC_LOG_MAX_ENCODED_MSG_LEN) */
} lte_rrc_log_ota_msg_s_v13;

/*
    Serial Interface Control Document for Long Term Evolution (LTE)
    80-VP457-6 Rev. AK
*/

typedef struct PACK() {
    uint8 log_packet_ver;         /*!< Log packet version */
    uint8 rrc_rel;                /*!< RRC release number */
    uint8 rrc_ver;                /*!< RRC version number; if 8.x.y, left 4 bits are for x,
                                       right 4 bits are for y */
    uint8 nr_rrc_rel;             /*!< NR RRC release number*/
    uint8 nr_rrc_ver;             /*NR RRC version number*/
    uint8 rb_id;                  /*!< Radio bearer id */
    uint16 phy_cell_id;           /*!< Physical cell ID */
    uint32 freq;                  /*!< Frequency of the cell */
    lte_sfn_s sfn;                /*!< SFN on which message was sent/received - 0 if N/A */
    uint8 pdu_num;                /*!< Identifies the message type
                                       2 - BCCH_DL_SCH Message
                                       3 - PCCH Message
                                       4 - DL_CCCH Message
                                       5 - DL_DCCH Message
                                       6 - UL_CCCH Message
                                       7 - UL_DCCH Message */
    uint32 sib_mask_in_si;        /*!< Bitmask of available sibs in BCCH_DL_SCH
                                              Will be 0 if no SIBS are availabe in BCCH_DL_SCH
                                              or if the msg is not of type BCCH_DL_SCH*/
    uint16 encoded_msg_len;       /*!< Length of ASN.1 encoded message */
    uint8 encoded_msg_first_byte; /*!< First byte of the ASN.1 encoded message
                                       sent/received (present only when message
                                       length <= LTE_RRC_LOG_MAX_ENCODED_MSG_LEN) */
} lte_rrc_log_ota_msg_s_v26;

#define LTE_NAS_OTA_MSG_MAX_SIZE 1000

typedef struct PACK() {
    byte log_version;
    byte std_version;
    byte std_major_version;
    byte std_minor_version;
    byte in_ota_raw_data[LTE_NAS_OTA_MSG_MAX_SIZE]; /*!< Incoming NAS ESM plain OTA message */
} lte_nas_esm_plain_in_ota_msg_T;
typedef struct PACK() {
    byte log_version;
    byte std_version;
    byte std_major_version;
    byte std_minor_version;
    byte in_ota_raw_data[LTE_NAS_OTA_MSG_MAX_SIZE]; /*!< Incoming NAS ESM plain protected OTA message */
} lte_nas_esm_plain_out_ota_msg_T;

typedef struct PACK() {
    byte log_version;
    byte std_version;
    byte std_major_version;
    byte std_minor_version;
    byte in_ota_raw_data[LTE_NAS_OTA_MSG_MAX_SIZE]; /*!< Incoming NAS EMM plain OTA message */
} lte_nas_emm_plain_in_ota_msg_type;

typedef struct PACK() {
    byte log_version;
    byte std_version;
    byte std_major_version;
    byte std_minor_version;
    byte in_ota_raw_data[LTE_NAS_OTA_MSG_MAX_SIZE]; /*!< Incoming NAS EMM plain protected OTA message */
} lte_nas_emm_plain_out_ota_msg_type;

typedef struct PACK() {
    uint16_t minor;
    uint16_t major;
} nr5g_struct_version_t;

typedef struct PACK() {
    uint8_t sleep;
    uint8_t beam_change;
    uint8_t signal_change;
    uint8_t dl_dynamic_cfg_change;
    uint8_t dl_config;
    uint8_t ul_config : 4;
    uint8_t ml1_state_change : 4;
    uint16_t reserved;
} nr5g_log_fields_change_t;

typedef struct PACK() {
    uint8_t slot;
    uint8_t numerology : 4;
    uint8_t reserved1 : 4;
    uint16_t frame : 12;   /* TODO: frame size is less than 12 bits but the actual size is unknown */
    uint8_t reserved2 : 4; /* TODO: this reserved size is bigger than 4 bits but the actual size is unknown */
} nr5g_timestamp_t;

#define LTE_NAS_PARSER_EMM_HEADER_TYPE_UNKNOWN 0
#define LTE_NAS_PARSER_EMM_HEADER_TYPE_COMMON 1
#define LTE_NAS_PARSER_EMM_HEADER_TYPE_SERVICE_REQUEST 2

#define LTE_NAS_PARSER_ESM_HEADER_TYPE_UNKNOWN 0
#define LTE_NAS_PARSER_ESM_HEADER_TYPE_COMMON 1

#endif
