#ifndef QMI_BANDSEL_H_12065610052016
#define QMI_BANDSEL_H_12065610052016
/*
 * Band selection using QMI.
 * This module defines important data structures and functions of bandsel.
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

#include "qmidef.h"

#define QMI_NAS_BAND_GSM_450		0x00010000 // GSM 450
#define QMI_NAS_BAND_GSM_480		0x00020000 // GSM 480
#define QMI_NAS_BAND_GSM_750		0x00040000 // GSM 750
#define QMI_NAS_BAND_GSM_850		0x00080000 // GSM 850
#define QMI_NAS_BAND_GSM_900_EXT	0x00000100 // GSM 900 (Extended)
#define QMI_NAS_BAND_GSM_900_PRI	0x00000200 // GSM 900 (Primary)
#define QMI_NAS_BAND_GSM_900_RAIL	0x00100000 // GSM 900 (Railways)
#define QMI_NAS_BAND_GSM_1800		0x00000080
#define QMI_NAS_BAND_GSM_1900		0x00200000
#define QMI_NAS_BAND_GSM_900		(QMI_NAS_BAND_GSM_900_EXT|QMI_NAS_BAND_GSM_900_PRI|QMI_NAS_BAND_GSM_900_RAIL)
#define QMI_NAS_BAND_GSM_ALL		(QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_GSM_480|QMI_NAS_BAND_GSM_750|QMI_NAS_BAND_GSM_850|QMI_NAS_BAND_GSM_900|QMI_NAS_BAND_GSM_1800|QMI_NAS_BAND_GSM_1900)
#define QMI_NAS_BAND_WCDMA_850		0x04000000
#define QMI_NAS_BAND_WCDMA_900		0x0002000000000000ULL
#define QMI_NAS_BAND_WCDMA_1900		0x00800000
#define QMI_NAS_BAND_WCDMA_2100		0x00400000
#define QMI_NAS_BAND_WCDMA_1800		0x01000000 // WCDMA 1800
#define QMI_NAS_BAND_AWS			0x02000000 // WCDMA 1700 (US)
#define QMI_NAS_BAND_WCDMA_800		0x08000000 // WCDMA 800 (JP)
#define QMI_NAS_BAND_WCDMA_1700_JP	0x0004000000000000ULL // WCDMA 1700 (JP)
#define QMI_NAS_BAND_WCDMA_2600		0x0001000000000000ULL // WCDMA 2600
#define QMI_NAS_BAND_WCDMA_850_JP	0x1000000000000000ULL // WCDMA 850 (JP)
#define QMI_NAS_BAND_WCDMA_1500		0x2000000000000000ULL // WCDMA 1500
#define QMI_NAS_BAND_WCDMA_ALL		(QMI_NAS_BAND_WCDMA_850|QMI_NAS_BAND_WCDMA_900|QMI_NAS_BAND_WCDMA_1900|QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_1800|QMI_NAS_BAND_AWS|QMI_NAS_BAND_WCDMA_800|QMI_NAS_BAND_WCDMA_1700_JP|QMI_NAS_BAND_WCDMA_2600|QMI_NAS_BAND_WCDMA_850_JP|QMI_NAS_BAND_WCDMA_1500)
#define QMI_NAS_BAND_GSM_WCDMA_ALL	(QMI_NAS_BAND_GSM_ALL|QMI_NAS_BAND_WCDMA_ALL)

// individual band should only use hex codes smaller than this
#define QMI_NAS_MIN_BAND_GROUP_HEX 240

#define QMI_NAS_LTE_BAND_1   0x00000001ULL
#define QMI_NAS_LTE_BAND_2   0x00000002ULL
#define QMI_NAS_LTE_BAND_3   0x00000004ULL
#define QMI_NAS_LTE_BAND_4   0x00000008ULL
#define QMI_NAS_LTE_BAND_5   0x00000010ULL
#define QMI_NAS_LTE_BAND_6   0x00000020ULL
#define QMI_NAS_LTE_BAND_7   0x00000040ULL
#define QMI_NAS_LTE_BAND_8   0x00000080ULL
#define QMI_NAS_LTE_BAND_9   0x00000100ULL
#define QMI_NAS_LTE_BAND_10  0x00000200ULL
#define QMI_NAS_LTE_BAND_11  0x00000400ULL
#define QMI_NAS_LTE_BAND_12  0x00000800ULL
#define QMI_NAS_LTE_BAND_13  0x00001000ULL
#define QMI_NAS_LTE_BAND_14  0x00002000ULL
#define QMI_NAS_LTE_BAND_17  0x00010000ULL
#define QMI_NAS_LTE_BAND_18  0x00020000ULL
#define QMI_NAS_LTE_BAND_19  0x00040000ULL
#define QMI_NAS_LTE_BAND_20  0x00080000ULL
#define QMI_NAS_LTE_BAND_21  0x00100000ULL
#define QMI_NAS_LTE_BAND_23  0x00400000ULL
#define QMI_NAS_LTE_BAND_24  0x00800000ULL
#define QMI_NAS_LTE_BAND_25  0x01000000ULL
#define QMI_NAS_LTE_BAND_26  0x02000000ULL
#define QMI_NAS_LTE_BAND_28  0x08000000ULL
#define QMI_NAS_LTE_BAND_29  0x10000000ULL
#define QMI_NAS_LTE_BAND_32  0x20000000ULL
#define QMI_NAS_LTE_BAND_33  0x0000000100000000ULL
#define QMI_NAS_LTE_BAND_34  0x0000000200000000ULL
#define QMI_NAS_LTE_BAND_35  0x0000000400000000ULL
#define QMI_NAS_LTE_BAND_36  0x0000000800000000ULL
#define QMI_NAS_LTE_BAND_37  0x0000001000000000ULL
#define QMI_NAS_LTE_BAND_38  0x0000002000000000ULL
#define QMI_NAS_LTE_BAND_39  0x0000004000000000ULL
#define QMI_NAS_LTE_BAND_40  0x0000008000000000ULL
#define QMI_NAS_LTE_BAND_41  0x0000010000000000ULL
#define QMI_NAS_LTE_BAND_42  0x0000020000000000ULL
#define QMI_NAS_LTE_BAND_43  0x0000040000000000ULL
#define QMI_NAS_LTE_BAND_125 0x1000000000000000ULL
#define QMI_NAS_LTE_BAND_126 0x2000000000000000ULL
#define QMI_NAS_LTE_BAND_127 0x4000000000000000ULL
#define QMI_NAS_LTE_BAND_ALL 0xFFFFFFFFFFFFFFFFULL

struct band_info_t {
	int hex;
	const char* name;
	unsigned long long code;
};

struct band_group_info_t {
    int hex;
    const char* name;
    unsigned long long code;
    unsigned long long lte_code;
};

extern const struct band_info_t band_info_qmi[];
extern const int band_info_qmi_len;
extern const struct band_info_t lte_band_info_qmi[];
extern const int lte_band_info_qmi_len;
extern const struct band_group_info_t band_group_info_qmi[];
extern const int band_group_info_qmi_len;

extern int
_qmi_update_band_list_helper(const struct qmi_band_bit_mask * mask,
                             const struct band_info_t band_info[],
                             int band_info_len,
                             char band_list[],
                             char **p_bl, int *cap_bl,
                             char current_band[],
                             char **p_cb, int *cap_cb);

extern int
_qmi_update_band_list_groups(const struct qmi_band_bit_mask * mask,
                             const struct qmi_band_bit_mask * lte_mask,
                             const struct band_group_info_t band_group_info[],
                             int band_group_info_len,
                             char band_list[],
                             char **p_bl, int *cap_bl,
                             char current_band[],
                             char **p_cb, int *cap_cb);

extern int
_qmi_build_band_list_from_masks(const struct qmi_band_bit_mask * mask,
                                const struct qmi_band_bit_mask * lte_mask,
                                char * band_list, int cap_bl,
                                char * current_band, int cap_cb);

extern int
_qmi_mode_band_to_curband(const unsigned short * mode,
                          const struct qmi_band_bit_mask * band,
                          const struct qmi_band_bit_mask * lte_band,
                          char * curband, int len);

extern int
_qmi_band_mask_lookup(const char * band,
                      const struct band_info_t band_info[],
                      int band_info_len,
                      unsigned short * mode_pref,
                      struct qmi_band_bit_mask * band_pref);

extern int
_qmi_band_mask_group_lookup(const char * band,
                            const struct band_group_info_t group_info[],
                            int group_info_len,
                            unsigned short * mode_pref,
                            struct qmi_band_bit_mask * band_pref,
                            struct qmi_band_bit_mask * lte_band_pref);

extern int
_qmi_build_masks_from_band(const char * band,
                           unsigned short * mode_pref,
                           struct qmi_band_bit_mask * band_pref,
                           struct qmi_band_bit_mask * lte_band_pref);

void
build_custom_band_masks(void);

void
custom_band_limit(struct qmi_band_bit_mask * band,
                  struct qmi_band_bit_mask * lte_band);
#endif
