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

#include "bandsel.h"
#include "custom/custom.h"
#include "qmimsg.h"
#include "ezqmi.h"
#include "config.h"
#include "minilib.h"
#include <stdio.h>
#include <stdlib.h>

/* full list of GSM and WCDMA bands: hex, name, code */
const struct band_info_t band_info_qmi[] = {
    {40, "GSM 450", QMI_NAS_BAND_GSM_450},
    {41, "GSM 480", QMI_NAS_BAND_GSM_480},
    {42, "GSM 750", QMI_NAS_BAND_GSM_750},
    {43, "GSM 850", QMI_NAS_BAND_GSM_850},
    {44, "GSM 900 (Extended)", QMI_NAS_BAND_GSM_900_EXT},
    {45, "GSM 900 (Primary)", QMI_NAS_BAND_GSM_900_PRI},
    {46, "GSM 900 (Railways)", QMI_NAS_BAND_GSM_900_RAIL},
    {47, "GSM 1800", QMI_NAS_BAND_GSM_1800},
    {48, "GSM 1900", QMI_NAS_BAND_GSM_1900},
    {80, "WCDMA 2100", QMI_NAS_BAND_WCDMA_2100},
    {81, "WCDMA PCS 1900", QMI_NAS_BAND_WCDMA_1900},
    {82, "WCDMA DCS 1800", QMI_NAS_BAND_WCDMA_1800},
    {83, "WCDMA 1700 (US)", QMI_NAS_BAND_AWS},
    {84, "WCDMA 850", QMI_NAS_BAND_WCDMA_850},
    {85, "WCDMA 800", QMI_NAS_BAND_WCDMA_800},
    {86, "WCDMA 2600", QMI_NAS_BAND_WCDMA_2600},
    {87, "WCDMA 900", QMI_NAS_BAND_WCDMA_900},
    {88, "WCDMA 1700 (Japan)", QMI_NAS_BAND_WCDMA_1700_JP},
    {90, "WCDMA 1500", QMI_NAS_BAND_WCDMA_1500},
    {91, "WCDMA 850 (Japan)", QMI_NAS_BAND_WCDMA_850_JP},
    {249, "GSM all", QMI_NAS_BAND_GSM_ALL},
    {250, "WCDMA all", QMI_NAS_BAND_WCDMA_ALL},
    {252, "GSM/WCDMA all", QMI_NAS_BAND_GSM_WCDMA_ALL}
};

const int band_info_qmi_len = __countof(band_info_qmi);

/* full list of LTE bands: hex, name, code */
const struct band_info_t lte_band_info_qmi[] = {
    { 120, "LTE Band 1 - 2100MHz",   QMI_NAS_LTE_BAND_1},
    { 121, "LTE Band 2 - 1900MHz",   QMI_NAS_LTE_BAND_2},
    { 122, "LTE Band 3 - 1800MHz",   QMI_NAS_LTE_BAND_3},
    { 123, "LTE Band 4 - 1700MHz",   QMI_NAS_LTE_BAND_4},
    { 124, "LTE Band 5 - 850MHz",    QMI_NAS_LTE_BAND_5},
    { 125, "LTE Band 6 - 850MHz",    QMI_NAS_LTE_BAND_6},
    { 126, "LTE Band 7 - 2600MHz",   QMI_NAS_LTE_BAND_7},
    { 127, "LTE Band 8 - 900MHz",    QMI_NAS_LTE_BAND_8},
    { 128, "LTE Band 9 - 1800MHz",   QMI_NAS_LTE_BAND_9},
    { 129, "LTE Band 10 - 1700MHz",  QMI_NAS_LTE_BAND_10},
    { 130, "LTE Band 11 - 1500MHz",  QMI_NAS_LTE_BAND_11},
    { 131, "LTE Band 12 - 700MHz",   QMI_NAS_LTE_BAND_12},
    { 132, "LTE Band 13 - 700MHz",   QMI_NAS_LTE_BAND_13},
    { 133, "LTE Band 14 - 700MHz",   QMI_NAS_LTE_BAND_14},
    { 134, "LTE Band 17 - 700MHz",   QMI_NAS_LTE_BAND_17},
    { 143, "LTE Band 18 - 850MHz",   QMI_NAS_LTE_BAND_18},
    { 144, "LTE Band 19 - 850MHz",   QMI_NAS_LTE_BAND_19},
    { 145, "LTE Band 20 - 800MHz",   QMI_NAS_LTE_BAND_20},
    { 146, "LTE Band 21 - 1500MHz",  QMI_NAS_LTE_BAND_21},
    { 152, "LTE Band 23 - 2000MHz",  QMI_NAS_LTE_BAND_23},
    { 147, "LTE Band 24 - 1600MHz",  QMI_NAS_LTE_BAND_24},
    { 148, "LTE Band 25 - 1900MHz",  QMI_NAS_LTE_BAND_25},
    { 153, "LTE Band 26 - 850MHz",   QMI_NAS_LTE_BAND_26},
    { 158, "LTE Band 28 - 700MHz",   QMI_NAS_LTE_BAND_28},
    { 159, "LTE Band 29 - 700MHz",   QMI_NAS_LTE_BAND_29},
    { 154, "LTE Band 32 - 1500MHz",  QMI_NAS_LTE_BAND_32},
    { 135, "LTE Band 33 - TDD 2100", QMI_NAS_LTE_BAND_33},
    { 136, "LTE Band 34 - TDD 2100", QMI_NAS_LTE_BAND_34},
    { 137, "LTE Band 35 - TDD 1900", QMI_NAS_LTE_BAND_35},
    { 138, "LTE Band 36 - TDD 1900", QMI_NAS_LTE_BAND_36},
    { 139, "LTE Band 37 - TDD 1900", QMI_NAS_LTE_BAND_37},
    { 140, "LTE Band 38 - TDD 2600", QMI_NAS_LTE_BAND_38},
    { 141, "LTE Band 39 - TDD 1900", QMI_NAS_LTE_BAND_39},
    { 142, "LTE Band 40 - TDD 2300", QMI_NAS_LTE_BAND_40},
    { 149, "LTE Band 41 - TDD 2500", QMI_NAS_LTE_BAND_41},
    { 150, "LTE Band 42 - TDD 3500", QMI_NAS_LTE_BAND_42},
    { 151, "LTE Band 43 - TDD 3700", QMI_NAS_LTE_BAND_43},
    { 155, "LTE Band 125",           QMI_NAS_LTE_BAND_125},
    { 156, "LTE Band 126",           QMI_NAS_LTE_BAND_126},
    { 157, "LTE Band 127",           QMI_NAS_LTE_BAND_127},
    { 251, "LTE all",                QMI_NAS_LTE_BAND_ALL},
};

const int lte_band_info_qmi_len = __countof(lte_band_info_qmi);

/* mixed band groups */
const struct band_group_info_t band_group_info_qmi[] = {
    {253, "WCDMA/LTE all", QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL},
    {254, "GSM/LTE all", QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_ALL},
    {255, "All bands", QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL}
};

const int band_group_info_qmi_len = __countof(band_group_info_qmi);

/* check if a band hex is a gsm band or band group */
static int _qmi_is_gsm_band_hex(int hex)
{
    return (hex >= 40 && hex < 80) ||
           (hex == 249 || hex == 252 || hex == 254 || hex == 255);
}

/* check if a band hex is a wcdma band or band group */
static int _qmi_is_wcdma_band_hex(int hex)
{
    return (hex >= 80 && hex < 120) ||
           (hex == 250 || hex == 252 || hex == 253 || hex == 255);
}

/* check if a band hex is an lte band or band group */
static int _qmi_is_lte_band_hex(int hex)
{
    return (hex >= 120 && hex < 200) ||
           (hex == 251 || hex == 253 || hex == 254 || hex == 255);
}

/*
 * Helper function to update band_list[] and current_band[] from band mask.
 * Params:
 *  mask: either a band mask for GSM/WCDMA or band mask for LTE.
 *  band_info: either band_info_qmi or lte_band_info_qmi.
 *  band_info_len: length of band_info[].
 *  band_list: band_list string buffer.
 *  p_bl: pointer to pointer to current position in band_list.
 *  cap_bl: pointer to remaining capacity of band_list.
 *  current_band: current_band string buffer.
 *  p_cb: pointer to pointer to current position in current_band.
 *  cap_cb: pointer to remaining capacity of current_band.
 * Return:
 *  0 on success, -1 on failure.
 * The band hex codes and names corresponding to mask will be appended to
 * band_list and current_band from position indicated by p_bl and p_cb.
 * cap_bl and cap_cb contain the remaining capacity (including terminating nil)
 * of the corresponding string buffers, respectively.
 * If capacity is not enough, this function returns failure.
 * p_bl, cap_bl, p_cb and cap_cb will all be updated after each band hex/name
 * is appended. If it fails at some stage, these pointers will be updated to
 * the end of the lastest successfully appended hex/name.
 * Note: Both individual bands and band groups in band_info[] will be included
 * as long as they match/cover the mask.
 */
int _qmi_update_band_list_helper(const struct qmi_band_bit_mask * mask,
                                 const struct band_info_t band_info[],
                                 int band_info_len,
                                 char band_list[],
                                 char **p_bl, int *cap_bl,
                                 char current_band[],
                                 char **p_cb, int *cap_cb)
{
    int i, n;
    const char * name;
    // no matter what happens, make sure nil terminates the buffers
    **p_bl = '\0';
    **p_cb = '\0';
    if(mask && mask->band) {
        for(i = 0; i < band_info_len; i++) {
            if(mask->band & band_info[i].code) {
                if(!(name = get_band_name_by_hex(band_info[i].hex))) {
                    continue;
                }
                n = snprintf(*p_bl, *cap_bl,
                             *p_bl == band_list ? "%02x,%s" : "&%02x,%s",
                             band_info[i].hex, name);
                if(n < 0 || n >= *cap_bl) {
                    **p_bl = '\0'; // clear partial entry
                    SYSLOG(LOG_ERROR, "Failed to update band_list");
                    return -1;
                }
                *p_bl += n;
                *cap_bl -= n;

                n = snprintf(*p_cb, *cap_cb,
                             *p_cb == current_band ? "%s" : ";%s", name);
                if(n < 0 || n >= *cap_cb) {
                    **p_cb = '\0'; // clear partial entry
                    SYSLOG(LOG_ERROR, "Failed to update current_band");
                    return -1;
                }
                *p_cb += n;
                *cap_cb -= n;
                if(band_info[i].hex >=  QMI_NAS_MIN_BAND_GROUP_HEX &&
                   (mask->band & band_info[i].code) == mask->band) {
                    /*
                     * Prevent including unnecessary big group.
                     * e.g. If mask contains only GSM bands,
                     * we should include 'GSM all' but not 'GSM/WCDMA all'.
                     */
                    break;
                }
            }
        }
    }
    return 0;
}

/*
 * Similar to _qmi_update_band_list_helper, this helper function deals with
 * mixed band groups from GSM/WCDMA and LTE.
 */
int _qmi_update_band_list_groups(const struct qmi_band_bit_mask * mask,
                             const struct qmi_band_bit_mask * lte_mask,
                             const struct band_group_info_t band_group_info[],
                             int band_group_info_len,
                             char band_list[],
                             char **p_bl, int *cap_bl,
                             char current_band[],
                             char **p_cb, int *cap_cb)
{
    int i, n;
    const char * name;
    // no matter what happens, make sure nil terminates the buffers
    **p_bl = '\0';
    **p_cb = '\0';
    if(mask && mask->band && lte_mask && lte_mask->band) {
        for(i = 0; i< band_group_info_len; i++) {
            if((mask->band & band_group_info[i].code) &&
               (lte_mask->band & band_group_info[i].lte_code)) {
                if(!(name = get_band_name_by_hex(band_group_info[i].hex))) {
                    continue;
                }
                n = snprintf(*p_bl, *cap_bl,
                             *p_bl == band_list ? "%02x,%s" : "&%02x,%s",
                             band_group_info[i].hex, name);
                if(n < 0 || n >= *cap_bl) {
                    **p_bl = '\0'; // clear partial entry
                    SYSLOG(LOG_ERROR, "Failed to update band_list (groups)");
                    return -1;
                }
                *p_bl += n;
                *cap_bl -= n;

                n = snprintf(*p_cb, *cap_cb,
                             *p_cb == current_band ? "%s" : ";%s", name);
                if(n < 0 || n >= *cap_cb) {
                    **p_cb = '\0'; // clear partial entry
                    SYSLOG(LOG_ERROR, "Failed to update current_band (groups)");
                    return -1;
                }
                *p_cb += n;
                *cap_cb -= n;
                if((mask->band & band_group_info[i].code) == mask->band &&
                   (lte_mask->band & band_group_info[i].lte_code) ==
                   lte_mask->band) {
                    /*
                     * Prevent including unnecessary big group.
                     * e.g. If mask contains only WCDMA and LTE bands,
                     * we should include 'WCDMA/LTE all' but not 'All bands'.
                     */
                    break;
                }
            }
        }
    }
    return 0;
}

/*
 * Build band_list and current_band based on band masks: mask and lte_mask.
 * Params:
 *  mask: band bit mask for GSM/WCDMA. Can be NULL.
 *  lte_mask: band bit mask for LTE. Can be NULL.
 *  band_list: module_band_list string buffer
 *  cap_bl: capacity of band_list
 *  current_band: currentband.current_band string buffer
 *  cap_cb: capacity of current_band
 * Return:
 *  0 for success; -1 for failure.
 * Note: Both string buffers will be always nil-terminated even in failure.
 */
int _qmi_build_band_list_from_masks(const struct qmi_band_bit_mask * mask,
                                    const struct qmi_band_bit_mask * lte_mask,
                                    char * band_list, int cap_bl,
                                    char * current_band, int cap_cb)
{
    /* band_list: HH,band_name&HH,band_name... */
    /* current_band: band_name;band_name... */

    char *p_bl = band_list, *p_cb = current_band;
    int ret;

    // add GSM/WCDMA bands/groups
    ret = _qmi_update_band_list_helper(mask,
                                       band_info_qmi,
                                       band_info_qmi_len,
                                       band_list, &p_bl, &cap_bl,
                                       current_band, &p_cb, &cap_cb);
    if(ret < 0) {
        SYSLOG(LOG_ERROR, "Failed to update band list (GSM/WCDMA)");
        goto err;
    }

    // add LTE bands/groups
    ret = _qmi_update_band_list_helper(lte_mask,
                                       lte_band_info_qmi,
                                       lte_band_info_qmi_len,
                                       band_list, &p_bl, &cap_bl,
                                       current_band, &p_cb, &cap_cb);
    if(ret < 0) {
        SYSLOG(LOG_ERROR, "Failed to update band list (LTE)");
        goto err;
    }

    // add mixed GSM/WCDMA/LTE band groups
    ret = _qmi_update_band_list_groups(mask, lte_mask,
                                       band_group_info_qmi,
                                       band_group_info_qmi_len,
                                       band_list, &p_bl, &cap_bl,
                                       current_band, &p_cb, &cap_cb);
    if(ret < 0) {
        SYSLOG(LOG_ERROR, "Failed to update band list (groups)");
        goto err;
    }

    return 0;

err:
    return -1;
}

#ifdef V_MULTI_SELBAND_y

/*
 * Helper function to update curband[] from band mask.
 * Params:
 *  band: either a band mask for GSM/WCDMA or a band mask for LTE.
 *  band_info: either band_info_qmi or lte_band_info_qmi.
 *  band_info_len: length of band_info[].
 *  curband: current_selband string buffer.
 *  p: pointer to pointer to current position in curband.
 *  cap: pointer to remaining capacity of curband.
 * Return:
 *  0 on success, -1 on failure.
 * The names corresponding to mask will be appended to curband
 * from position indicated by p.
 * cap contains the remaining capacity (including terminating nil)
 * of the current_selband string buffer.
 * If capacity is not enough, this function returns failure.
 * p and cap will both be updated after each band name is appended.
 * If it fails at some stage, these pointers will be updated to
 * the end of the lastest successfully appended band name.
 */
static int _qmi_band_match_helper(const struct qmi_band_bit_mask * band,
                                  struct band_info_t band_info[],
                                  int band_info_len,
                                  char curband[],
                                  char **p, int *cap)
{
    int i, n;
    const char * name;

    if(band && band->band) {
        for(i = 0; i < band_info_len; i++) {
            if(band_info[i].hex >= QMI_NAS_MIN_BAND_GROUP_HEX) {
                break;
            }
            if(band->band & band_info[i].code) {
                if(!(name = get_band_name_by_hex(band_info[i].hex))) {
                    continue;
                }
                n = snprintf(*p, *cap, *p == curband ? "%s" : ";%s", name);
                if(n < 0 || n >= *cap) {
                    SYSLOG(LOG_ERROR, "Failed to generate current_selband");
                    return -1;
                }
                *p += n;
                *cap -= n;
            }
        }
    }
    return 0;
}

#else // V_MULTI_SELBAND

/*
 * Return the index to band_info[] that matches the band bit mask: band.
 * If there is no exact match, return the index of the smallest band group
 * that covers the band bit mask.
 * If failed, return -1.
 */
static int _qmi_band_group_match_helper(const struct qmi_band_bit_mask * band,
                                        const struct band_info_t band_info[],
                                        int band_info_len)
{
    int i;
    if(!band || !band->band) {
        return -1;
    }
    /* assume band_info is sorted from individual bands to the smallest to
     * the biggest groups, so the first match is the one we want */
    for(i = 0; i < band_info_len; i++) {
        if((band->band & band_info[i].code) == band->band) {
            return i;
        }
    }
    return -1;
}

#endif // V_MULTI_SELBAND

/*
 * Build a band bit mask for all bands in both custom_band_qmi and band_info.
 */
static void
build_custom_band_mask(const struct band_info_t band_info[],
                       int band_info_len,
                       struct qmi_band_bit_mask * custom_band_mask)
{
    int i, j;
    custom_band_mask->band = 0;
    for(i = 0; i < custom_band_qmi_len; i++) {
        if(custom_band_qmi[i].hex >=  QMI_NAS_MIN_BAND_GROUP_HEX) {
            continue; // skip band groups
        }
        for(j = 0; j < band_info_len; j++) {
            if(custom_band_qmi[i].hex == band_info[j].hex) {
                custom_band_mask->band |= band_info[j].code;
                break;
            }
        }
    }
}

static struct qmi_band_bit_mask _custom_band_mask, _custom_lte_band_mask;
static int _custom_band_mask_initialized;

void build_custom_band_masks(void)
{
    _custom_band_mask_initialized = 1;
    build_custom_band_mask(band_info_qmi, band_info_qmi_len,
                           &_custom_band_mask);
    build_custom_band_mask(lte_band_info_qmi, lte_band_info_qmi_len,
                           &_custom_lte_band_mask);
    SYSLOG(LOG_DEBUG, "_custom_band_mask=%llx", _custom_band_mask.band);
    SYSLOG(LOG_DEBUG, "_custom_lte_band_mask=%llx", _custom_lte_band_mask.band);
}

/* mask out bands beyong custom band */
void custom_band_limit(struct qmi_band_bit_mask * band,
                       struct qmi_band_bit_mask * lte_band)
{
    if(!_custom_band_mask_initialized) {
        build_custom_band_masks();
    }
    band->band &= _custom_band_mask.band;
    lte_band->band &= _custom_lte_band_mask.band;
}

/*
 * Check if a given band mask contains any band that is in band_info
 * but not in custom_band.
 * Return 0 if all bands are allowed; 1 if any band is not allowed.
 */
static int check_if_bands_allowed(const struct qmi_band_bit_mask * band,
                                  const struct qmi_band_bit_mask * lte_band)
{
    if(!_custom_band_mask_initialized) {
        build_custom_band_masks();
    }
    if((band->band & _custom_band_mask.band) == band->band &&
       (lte_band->band & _custom_lte_band_mask.band) == lte_band->band) {
        return 0;
    }
    return 1;
}

/*
 * Convert mode_pref, band_pref and lte_band_pref to currentband.current_selband
 * All three pref parameters can be NULL.
 * Return 0 if success; 1 if there are some non-allowed bands; -1 for failure.
 */
int _qmi_mode_band_to_curband(const unsigned short * mode,
                              const struct qmi_band_bit_mask * band,
                              const struct qmi_band_bit_mask * lte_band,
                              char * curband, int len)
{
    struct qmi_band_bit_mask rband, rlte_band;
    int ret;

    if(!curband) {
        return -1;
    }
    if(!band && !lte_band) {
        *curband = '\0';
        return 0;
    }

    rband.band = band ? band->band : 0;
    rlte_band.band = lte_band ? lte_band->band : 0;

    rband.band &= QMI_NAS_BAND_GSM_WCDMA_ALL;
    rlte_band.band &= QMI_NAS_LTE_BAND_ALL;

    if(mode) { /* mode_pref takes precedence over band_pref */
        if(!(*mode & 0x0004)) { // there is no GSM band
            rband.band &= ~QMI_NAS_BAND_GSM_ALL;
        }
        if(!(*mode & 0x0008)) { // there is no WCDMA band
            rband.band &= ~QMI_NAS_BAND_WCDMA_ALL;
        }
        if(!(*mode & 0x0010)) { // there is no LTE band
            rlte_band.band = 0;
        }
    }
    /* now we can forget mode */

    ret = check_if_bands_allowed(&rband, &rlte_band);
    SYSLOG(LOG_DEBUG, "check_if_band_allowed=%d", ret);

#ifdef V_MULTI_SELBAND_y
/*
 * Multiple individual bands selection is enabled. In this mode,
 * curband will be updated with semicolon separated list of individual bands.
 * No band groups will be added to curband.
 */
    char * p = curband;
    if(_qmi_band_match_helper(&rband, band_info_qmi,
                              band_info_qmi_len,
                              curband, &p, &len)) {
        return ret ? ret : -1;
    }
    if(_qmi_band_match_helper(&rlte_band, lte_band_info_qmi,
                              lte_band_info_qmi_len,
                              curband, &p, &len)) {
        return ret ? ret : -1;
    }
    return ret;

#else // V_MULTI_SELBAND = 'none'
/*
 * Single band (band group) selection only. In this mode,
 * curband will contain at most one band or band group.
 * This is the default mode, until other parts of the system support multiple
 * individual band selection.
 */
    int i;
    int idx, idx_lte;
    const char * name;

    curband[--len] = '\0'; // make sure it is null-terminated

    idx = _qmi_band_group_match_helper(&rband, band_info_qmi,
                                       band_info_qmi_len);

    idx_lte = _qmi_band_group_match_helper(&rlte_band, lte_band_info_qmi,
                                           lte_band_info_qmi_len);

    if(idx < 0) {
        if(idx_lte < 0) {
            return ret; // no band selected
        }
        // LTE only
        SYSLOG(LOG_DEBUG, "lte_band_sel=%d", lte_band_info_qmi[idx_lte].hex);
        if((name = get_band_name_by_hex(lte_band_info_qmi[idx_lte].hex))) {
            strncpy(curband, name, len);
        } else {
            curband[0] = '\0'; // should not happen
        }
        return ret;
    }
    // GSM/WCDMA only
    if(idx_lte < 0) {
        SYSLOG(LOG_DEBUG, "band_sel=%d", band_info_qmi[idx].hex);
        if((name = get_band_name_by_hex(band_info_qmi[idx].hex))) {
            strncpy(curband, name, len);
        } else {
            curband[0] = '\0'; // should not happen
        }
        return ret;
    }
    // Mixed band group
    SYSLOG(LOG_DEBUG, "band_sel=%d, lte_band_sel=%d",
           band_info_qmi[idx].hex, lte_band_info_qmi[idx_lte].hex);
    for(i = 0; i < band_group_info_qmi_len; i++) {
        if((band_info_qmi[idx].code & band_group_info_qmi[i].code)
           == band_info_qmi[idx].code &&
           (lte_band_info_qmi[idx_lte].code & band_group_info_qmi[i].lte_code)
           == lte_band_info_qmi[idx_lte].code) {
            if((name = get_band_name_by_hex(band_group_info_qmi[i].hex))) {
                strncpy(curband, name, len);
            } else {
                curband[0] = '\0'; // should not happen
            }
            return ret;
        }
    }

    // something is wrong if we arrive here
    SYSLOG(LOG_ERROR, "something is wrong");
    return ret ? ret : -1;

#endif // V_MULTI_SELBAND
}

/*
 * Look up band_info[] (band_info_qmi[] or lte_band_info_qmi[]) for the given
 * band (name or hex).
 * If there is a match, both mode_pref and band_pref will be updated (OR'd).
 * Return 0 if there is no match; 1 otherwise.
 */
int _qmi_band_mask_lookup(const char * band,
                          const struct band_info_t band_info[],
                          int band_info_len,
                          unsigned short * mode_pref,
                          struct qmi_band_bit_mask * band_pref)
{
    int i;
    char * ep;

    long lhex = strtol(band, &ep, 16);
    // if ep does not point to terminating nil, this is not a valid hex
    if(lhex > 255 || *ep) {
        lhex = -1;
    }

    int hex = (int)lhex;

    if(hex >= 0) { // match by hex
        if(!(band = get_band_name_by_hex(hex))) {
            return 0;
        }
    } else { // match by name
        if((hex = get_band_hex_by_name(band)) < 0) {
            return 0;
        }
    }
    // both band and hex are valid now

    for(i = 0; i < band_info_len; i++) {
        if(hex != band_info[i].hex) {
            continue;
        }

        // we have a match if arriving here
        band_pref->band |= band_info[i].code;

        // mode_pref: 0x0004-GSM, 0x0008-UMTS, 0x0010-LTE
        if(_qmi_is_gsm_band_hex(hex)) {
            *mode_pref |= 0x0004;
        }
        if(_qmi_is_wcdma_band_hex(hex)) {
            *mode_pref |= 0x0008;
        }
        if(_qmi_is_lte_band_hex(hex)) {
            *mode_pref |= 0x0010;
        }
        return 1; // there should be one match at maximum
    }
    return 0; // no match found
}

/* similar to _qmi_band_mask_lookup, but look up in mixed band groups */
int _qmi_band_mask_group_lookup(const char * band,
                                const struct band_group_info_t group_info[],
                                int group_info_len,
                                unsigned short * mode_pref,
                                struct qmi_band_bit_mask * band_pref,
                                struct qmi_band_bit_mask * lte_band_pref)
{
    int i;
    char * ep;

    long lhex = strtol(band, &ep, 16);
    // if ep does not point to terminating nil, this is not a valid hex
    if(lhex > 255 || *ep) {
        lhex = -1;
    }

    int hex = (int)lhex;

    if(hex >= 0) { // match by hex
        if(!(band = get_band_name_by_hex(hex))) {
            return 0;
        }
    } else { // match by name
        if((hex = get_band_hex_by_name(band)) < 0) {
            return 0;
        }
    }
    // both band and hex are valid now

    for(i = 0; i < group_info_len; i++) {
        if(hex != group_info[i].hex) {
            continue;
        }

        // we have a match if arriving here
        band_pref->band |= group_info[i].code;
        lte_band_pref->band |= group_info[i].lte_code;

        // mode_pref: 0x0004-GSM, 0x0008-UMTS, 0x0010-LTE
        if(_qmi_is_gsm_band_hex(hex)) {
            *mode_pref |= 0x0004;
        }
        if(_qmi_is_wcdma_band_hex(hex)) {
            *mode_pref |= 0x0008;
        }
        if(_qmi_is_lte_band_hex(hex)) {
            *mode_pref |= 0x0010;
        }
        return 1; // there should be one match at maximum
    }
    return 0; // no match found
}

/*
 * Build masks: mode_pref, band_pref & lte_band_pref from band.
 * band is a semicolon separated list of either band names or hex codes,
 * each from band_info_qmi[], lte_band_info_qmi[] or band_group_info_qmi[].
 * Return 0 if success; -1 if failure or no band found
 */
int _qmi_build_masks_from_band(const char * band,
                               unsigned short * mode_pref,
                               struct qmi_band_bit_mask * band_pref,
                               struct qmi_band_bit_mask * lte_band_pref)
{
    char rband[QMIMGR_MAX_DB_BIGVALUE_LENGTH];
    char * p;
    int ret;

    *mode_pref = 0;
    band_pref->band = 0;
    lte_band_pref->band = 0;

    // make a copy for strtok
    strncpy(rband, band, __countof(rband));
    if(rband[__countof(rband) - 1]) {
        SYSLOG(LOG_ERROR, "param.band is too long: %d", strlen(band));
        return -1;
    }

    // get parameters
    p = strtok(rband, ";");
    while(p) {
        // look up in GSM/WCDMA bands/groups
        ret = _qmi_band_mask_lookup(p, band_info_qmi, band_info_qmi_len,
                                    mode_pref, band_pref);
        if(!ret) { // do not need to look up further if there is a match
            // look up in LTE bands/groups
            ret = _qmi_band_mask_lookup(p, lte_band_info_qmi,
                                        lte_band_info_qmi_len,
                                        mode_pref, lte_band_pref);
        }
        if(!ret) {
            // look up in mixed band groups
            ret = _qmi_band_mask_group_lookup(p, band_group_info_qmi,
                                              band_group_info_qmi_len,
                                              mode_pref, band_pref,
                                              lte_band_pref);
        }
        if(!ret) {
            SYSLOG(LOG_WARNING, "Unknown band %s in current_selband", p);
        }
        p = strtok(NULL, ";");
    }

    if(!*mode_pref || (!band_pref->band && !lte_band_pref->band)) { // not found
        SYSLOG(LOG_ERROR, "wrong band %s - not found, mode_pref=%04x",
               band, *mode_pref);
        return -1;
    }

    return 0;
}
