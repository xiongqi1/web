/*
 * Unit test for qmimgr.
 * This currently only tests bandsel.
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

#include "qmi_unit_test.h"
#include "../minilib.h"
#include "../bandsel.h"
#include <unit_test.h>

void test_update_band_list(unsigned long long band_mask,
                           const char * exp_band_list,
                           const char * exp_current_band)
{
    struct qmi_band_bit_mask mask;
    char band_list[BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[BUF_CAP];
    char *p_cb;
    int cap_cb;

    mask.band = band_mask;
    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_helper(&mask, band_info_qmi, band_info_qmi_len, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == 0);
    //printf("band_list=%s\ncurrent_band=%s\n", band_list, current_band);
    TEST_CHECK(strcmp(band_list, exp_band_list) == 0);
    TEST_CHECK(strcmp(current_band, exp_current_band) == 0);
    TEST_CHECK(p_bl - band_list == strlen(band_list));
    TEST_CHECK(strlen(band_list) + cap_bl == __countof(band_list));
    TEST_CHECK(p_cb - current_band == strlen(current_band));
    TEST_CHECK(strlen(current_band) + cap_cb == __countof(current_band));
}

void test_update_lte_band_list(unsigned long long band_mask,
                               const char * exp_band_list,
                               const char * exp_current_band)
{
    struct qmi_band_bit_mask mask;
    char band_list[BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[BUF_CAP];
    char *p_cb;
    int cap_cb;

    mask.band = band_mask;
    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_helper(&mask, lte_band_info_qmi, lte_band_info_qmi_len, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == 0);
    //printf("band_list=%s\ncurrent_band=%s\n", band_list, current_band);
    TEST_CHECK(strcmp(band_list, exp_band_list) == 0);
    TEST_CHECK(strcmp(current_band, exp_current_band) == 0);
    TEST_CHECK(p_bl - band_list == strlen(band_list));
    TEST_CHECK(strlen(band_list) + cap_bl == __countof(band_list));
    TEST_CHECK(p_cb - current_band == strlen(current_band));
    TEST_CHECK(strlen(current_band) + cap_cb == __countof(current_band));
}

// single GSM band
void TEST_AUTO(test_update_band_list1)
{
    test_update_band_list(QMI_NAS_BAND_GSM_450, "28,GSM 450&f9,GSM all", "GSM 450;GSM all");
}

// single GSM band
void TEST_AUTO(test_update_band_list2)
{
    test_update_band_list(QMI_NAS_BAND_GSM_1900, "30,GSM 1900&f9,GSM all", "GSM 1900;GSM all");
}

// two GSM bands
void TEST_AUTO(test_update_band_list3)
{
    test_update_band_list(QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_GSM_1900, "28,GSM 450&30,GSM 1900&f9,GSM all", "GSM 450;GSM 1900;GSM all");
}

// single WCDMA band
void TEST_AUTO(test_update_band_list4)
{
    test_update_band_list(QMI_NAS_BAND_WCDMA_2100, "50,WCDMA 2100&fa,WCDMA all", "WCDMA 2100;WCDMA all");
}

// two WCDMA bands
void TEST_AUTO(test_update_band_list5)
{
    test_update_band_list(QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_850_JP, "50,WCDMA 2100&5b,WCDMA 850 (Japan)&fa,WCDMA all", "WCDMA 2100;WCDMA 850 (Japan);WCDMA all");
}

// 1 GSM + 1 WCDMA bands
void TEST_AUTO(test_update_band_list6)
{
    test_update_band_list(QMI_NAS_BAND_GSM_900_RAIL|QMI_NAS_BAND_WCDMA_850_JP, "2e,GSM 900 (Railways)&5b,WCDMA 850 (Japan)&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all", "GSM 900 (Railways);WCDMA 850 (Japan);GSM all;WCDMA all;GSM/WCDMA all");
}

// 1 GSM + 2 WCDMA bands
void TEST_AUTO(test_update_band_list7)
{
    test_update_band_list(QMI_NAS_BAND_GSM_900_RAIL|QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_850_JP, "2e,GSM 900 (Railways)&50,WCDMA 2100&5b,WCDMA 850 (Japan)&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all", "GSM 900 (Railways);WCDMA 2100;WCDMA 850 (Japan);GSM all;WCDMA all;GSM/WCDMA all");
}

// all GSM bands
void TEST_AUTO(test_update_band_list8)
{
    test_update_band_list(QMI_NAS_BAND_GSM_ALL, "28,GSM 450&29,GSM 480&2a,GSM 750&2b,GSM 850&2c,GSM 900 (Extended)&2d,GSM 900 (Primary)&2e,GSM 900 (Railways)&2f,GSM 1800&30,GSM 1900&f9,GSM all", "GSM 450;GSM 480;GSM 750;GSM 850;GSM 900 (Extended);GSM 900 (Primary);GSM 900 (Railways);GSM 1800;GSM 1900;GSM all");
}

// all WCDMA bands
void TEST_AUTO(test_update_band_list9)
{
    test_update_band_list(QMI_NAS_BAND_WCDMA_ALL, "50,WCDMA 2100&51,WCDMA PCS 1900&52,WCDMA DCS 1800&53,WCDMA 1700 (US)&54,WCDMA 850&55,WCDMA 800&56,WCDMA 2600&57,WCDMA 900&58,WCDMA 1700 (Japan)&5a,WCDMA 1500&5b,WCDMA 850 (Japan)&fa,WCDMA all", "WCDMA 2100;WCDMA PCS 1900;WCDMA DCS 1800;WCDMA 1700 (US);WCDMA 850;WCDMA 800;WCDMA 2600;WCDMA 900;WCDMA 1700 (Japan);WCDMA 1500;WCDMA 850 (Japan);WCDMA all");
}

// all GSM & WCDMA bands
void TEST_AUTO(test_update_band_list10)
{
    test_update_band_list(QMI_NAS_BAND_GSM_WCDMA_ALL, "28,GSM 450&29,GSM 480&2a,GSM 750&2b,GSM 850&2c,GSM 900 (Extended)&2d,GSM 900 (Primary)&2e,GSM 900 (Railways)&2f,GSM 1800&30,GSM 1900&50,WCDMA 2100&51,WCDMA PCS 1900&52,WCDMA DCS 1800&53,WCDMA 1700 (US)&54,WCDMA 850&55,WCDMA 800&56,WCDMA 2600&57,WCDMA 900&58,WCDMA 1700 (Japan)&5a,WCDMA 1500&5b,WCDMA 850 (Japan)&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all", "GSM 450;GSM 480;GSM 750;GSM 850;GSM 900 (Extended);GSM 900 (Primary);GSM 900 (Railways);GSM 1800;GSM 1900;WCDMA 2100;WCDMA PCS 1900;WCDMA DCS 1800;WCDMA 1700 (US);WCDMA 850;WCDMA 800;WCDMA 2600;WCDMA 900;WCDMA 1700 (Japan);WCDMA 1500;WCDMA 850 (Japan);GSM all;WCDMA all;GSM/WCDMA all");
}

// no bands at all
void TEST_AUTO(test_update_band_list11)
{
    test_update_band_list(0, "", "");
}

// bands outside of gsm & wcdma
void TEST_AUTO(test_update_band_list12)
{
    test_update_band_list(~QMI_NAS_BAND_GSM_WCDMA_ALL, "", "");
}

// passed in a null pointer for band
void TEST_AUTO(test_update_band_list13)
{
    char band_list[BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[BUF_CAP];
    char *p_cb;
    int cap_cb;

    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_helper(NULL, NULL, 0, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == 0);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
    TEST_CHECK(p_bl == band_list);
    TEST_CHECK(cap_bl == __countof(band_list));
    TEST_CHECK(p_cb == current_band);
    TEST_CHECK(cap_cb == __countof(current_band));
}

// buffer too short
void TEST_AUTO(test_update_band_list14)
{
    struct qmi_band_bit_mask mask;
    char band_list[SHORT_BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[SHORT_BUF_CAP];
    char *p_cb;
    int cap_cb;

    mask.band = QMI_NAS_BAND_GSM_450;
    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_helper(&mask, band_info_qmi, band_info_qmi_len, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == -1);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
    TEST_CHECK(p_bl == band_list);
    TEST_CHECK(cap_bl == __countof(band_list));
    TEST_CHECK(p_cb == current_band);
    TEST_CHECK(cap_cb == __countof(current_band));
}

// single LTE band
void TEST_AUTO(test_update_lte_band_list1)
{
    test_update_lte_band_list(QMI_NAS_LTE_BAND_1, "78,LTE Band 1 - 2100MHz&fb,LTE all", "LTE Band 1 - 2100MHz;LTE all");
}

// two LTE bands
void TEST_AUTO(test_update_lte_band_list2)
{
    test_update_lte_band_list(QMI_NAS_LTE_BAND_1|QMI_NAS_LTE_BAND_127, "78,LTE Band 1 - 2100MHz&9d,LTE Band 127&fb,LTE all", "LTE Band 1 - 2100MHz;LTE Band 127;LTE all");
}

// no bands at all
void TEST_AUTO(test_update_lte_band_list3)
{
    test_update_lte_band_list(0, "", "");
}

// bands outside of LTE
void TEST_AUTO(test_update_lte_band_list4)
{
    test_update_band_list(0x8000000000000000ULL, "", "");
}

void test_update_band_list_groups(unsigned long long band_mask,
                                  unsigned long long lte_band_mask,
                                  const char * exp_band_list,
                                  const char * exp_current_band)
{
    struct qmi_band_bit_mask mask, lte_mask;
    char band_list[BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[BUF_CAP];
    char *p_cb;
    int cap_cb;

    mask.band = band_mask;
    lte_mask.band = lte_band_mask;
    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_groups(&mask, &lte_mask, band_group_info_qmi, band_group_info_qmi_len, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == 0);
    //printf("band_list=%s\ncurrent_band=%s\n", band_list, current_band);
    TEST_CHECK(strcmp(band_list, exp_band_list) == 0);
    TEST_CHECK(strcmp(current_band, exp_current_band) == 0);
    TEST_CHECK(p_bl - band_list == strlen(band_list));
    TEST_CHECK(strlen(band_list) + cap_bl == __countof(band_list));
    TEST_CHECK(p_cb - current_band == strlen(current_band));
    TEST_CHECK(strlen(current_band) + cap_cb == __countof(current_band));
}


// nothing at all
void TEST_AUTO(test_update_band_list_groups0)
{
    test_update_band_list_groups(0, 0, "", "");
}

// non-mixed group
void TEST_AUTO(test_update_band_list_groups1)
{
    test_update_band_list_groups(0, QMI_NAS_LTE_BAND_ALL, "", "");
}

// non-mixed group
void TEST_AUTO(test_update_band_list_groups2)
{
    test_update_band_list_groups(QMI_NAS_BAND_GSM_WCDMA_ALL, 0, "", "");
}

// WCDMA+LTE
void TEST_AUTO(test_update_band_list_groups3)
{
    test_update_band_list_groups(QMI_NAS_BAND_WCDMA_850, QMI_NAS_LTE_BAND_1, "fd,WCDMA/LTE all", "WCDMA/LTE all");
}

// GSM+LTE
void TEST_AUTO(test_update_band_list_groups4)
{
    test_update_band_list_groups(QMI_NAS_BAND_GSM_850, QMI_NAS_LTE_BAND_41, "fe,GSM/LTE all", "GSM/LTE all");
}

// GSM+WCDMA+LTE
void TEST_AUTO(test_update_band_list_groups5)
{
    test_update_band_list_groups(QMI_NAS_BAND_GSM_850|QMI_NAS_BAND_WCDMA_2100, QMI_NAS_LTE_BAND_41, "fd,WCDMA/LTE all&fe,GSM/LTE all&ff,All bands", "WCDMA/LTE all;GSM/LTE all;All bands");
}

// everything
void TEST_AUTO(test_update_band_list_groups6)
{
    test_update_band_list_groups(0xffffffffffffffffULL, 0xffffffffffffffffULL, "fd,WCDMA/LTE all&fe,GSM/LTE all&ff,All bands", "WCDMA/LTE all;GSM/LTE all;All bands"); 
}

// passed in a null pointer for band
void TEST_AUTO(test_update_band_list_groups7)
{
    char band_list[BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[BUF_CAP];
    char *p_cb;
    int cap_cb;

    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_groups(NULL, NULL, NULL, 0, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == 0);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
    TEST_CHECK(p_bl == band_list);
    TEST_CHECK(cap_bl == __countof(band_list));
    TEST_CHECK(p_cb == current_band);
    TEST_CHECK(cap_cb == __countof(current_band));
}

// buffer too short
void TEST_AUTO(test_update_band_list_groups8)
{
    struct qmi_band_bit_mask mask, lte_mask;
    char band_list[SHORT_BUF_CAP];
    char *p_bl;
    int cap_bl;
    char current_band[SHORT_BUF_CAP];
    char *p_cb;
    int cap_cb;

    mask.band = QMI_NAS_BAND_GSM_450;
    lte_mask.band = QMI_NAS_LTE_BAND_42;
    p_bl = band_list;
    cap_bl = __countof(band_list);
    p_cb = current_band;
    cap_cb = __countof(current_band);

    TEST_CHECK(_qmi_update_band_list_groups(&mask, &lte_mask, band_group_info_qmi, band_group_info_qmi_len, band_list, &p_bl, &cap_bl, current_band, &p_cb, &cap_cb) == -1);
    //printf("band_list=%s\ncurrent_band=%s\n", band_list, current_band);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
    TEST_CHECK(p_bl == band_list);
    TEST_CHECK(cap_bl == __countof(band_list));
    TEST_CHECK(p_cb == current_band);
    TEST_CHECK(cap_cb == __countof(current_band));
}

void test_build_band_list(unsigned long long band_mask,
                          unsigned long long lte_band_mask,
                          const char * exp_band_list,
                          const char * exp_current_band)
{
    struct qmi_band_bit_mask mask, lte_mask;
    char band_list[BUF_CAP];
    char current_band[BUF_CAP];
    mask.band = band_mask;
    lte_mask.band = lte_band_mask;
    TEST_CHECK(_qmi_build_band_list_from_masks(&mask, &lte_mask, band_list, __countof(band_list), current_band, __countof(current_band)) == 0);
    //printf("band_list=%s\ncurrent_band=%s\n", band_list, current_band);
    TEST_CHECK(strcmp(band_list, exp_band_list) == 0);
    TEST_CHECK(strcmp(current_band, exp_current_band) == 0);
}

// both NULL masks
void TEST_AUTO(test_build_band_list0)
{
    char band_list[BUF_CAP];
    char current_band[BUF_CAP];
    TEST_CHECK(_qmi_build_band_list_from_masks(NULL, NULL, band_list, __countof(band_list), current_band, __countof(current_band)) == 0);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
}

// lte_mask == NULL
void TEST_AUTO(test_build_band_list1)
{
    struct qmi_band_bit_mask mask;
    char band_list[BUF_CAP];
    char current_band[BUF_CAP];
    mask.band = QMI_NAS_BAND_GSM_900_PRI;
    TEST_CHECK(_qmi_build_band_list_from_masks(&mask, NULL, band_list, __countof(band_list), current_band, __countof(current_band)) == 0);
    TEST_CHECK(strcmp(band_list, "2d,GSM 900 (Primary)&f9,GSM all") == 0);
    TEST_CHECK(strcmp(current_band, "GSM 900 (Primary);GSM all") == 0);
}

// mask == NULL
void TEST_AUTO(test_build_band_list2)
{
    struct qmi_band_bit_mask lte_mask;
    char band_list[BUF_CAP];
    char current_band[BUF_CAP];
    lte_mask.band = QMI_NAS_LTE_BAND_7;
    TEST_CHECK(_qmi_build_band_list_from_masks(NULL, &lte_mask, band_list, __countof(band_list), current_band, __countof(current_band)) == 0);
    TEST_CHECK(strcmp(band_list, "7e,LTE Band 7 - 2600MHz&fb,LTE all") == 0);
    TEST_CHECK(strcmp(current_band, "LTE Band 7 - 2600MHz;LTE all") == 0);
}

// nothing
void TEST_AUTO(test_build_band_list3)
{
    test_build_band_list(0, 0, "","");
}

// single GSM
void TEST_AUTO(test_build_band_list4)
{
    test_build_band_list(QMI_NAS_BAND_GSM_480, 0, "29,GSM 480&f9,GSM all","GSM 480;GSM all");
}

// single WCDMA
void TEST_AUTO(test_build_band_list5)
{
    test_build_band_list(QMI_NAS_BAND_AWS, 0, "53,WCDMA 1700 (US)&fa,WCDMA all","WCDMA 1700 (US);WCDMA all");
}

// single LTE
void TEST_AUTO(test_build_band_list6)
{
    test_build_band_list(0, QMI_NAS_LTE_BAND_10, "81,LTE Band 10 - 1700MHz&fb,LTE all","LTE Band 10 - 1700MHz;LTE all");
}

// 1 GSM + 2 WCDMA bands
void TEST_AUTO(test_build_band_list7)
{
    test_build_band_list(QMI_NAS_BAND_GSM_900_RAIL|QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_850_JP, 0, "2e,GSM 900 (Railways)&50,WCDMA 2100&5b,WCDMA 850 (Japan)&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all", "GSM 900 (Railways);WCDMA 2100;WCDMA 850 (Japan);GSM all;WCDMA all;GSM/WCDMA all");
}

// multiple LTE bands
void TEST_AUTO(test_build_band_list8)
{
    test_build_band_list(0, QMI_NAS_LTE_BAND_1|QMI_NAS_LTE_BAND_17|QMI_NAS_LTE_BAND_127, "78,LTE Band 1 - 2100MHz&86,LTE Band 17 - 700MHz&9d,LTE Band 127&fb,LTE all", "LTE Band 1 - 2100MHz;LTE Band 17 - 700MHz;LTE Band 127;LTE all");
}

// 1 GSM + 1 WCDMA + 1 LTE
void TEST_AUTO(test_build_band_list9)
{
    test_build_band_list(QMI_NAS_BAND_GSM_900_EXT|QMI_NAS_BAND_WCDMA_800, QMI_NAS_LTE_BAND_29, "2c,GSM 900 (Extended)&55,WCDMA 800&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all&9f,LTE Band 29 - 700MHz&fb,LTE all&fd,WCDMA/LTE all&fe,GSM/LTE all&ff,All bands","GSM 900 (Extended);WCDMA 800;GSM all;WCDMA all;GSM/WCDMA all;LTE Band 29 - 700MHz;LTE all;WCDMA/LTE all;GSM/LTE all;All bands");
}

// multiple mixed bands
void TEST_AUTO(test_build_band_list10)
{
    test_build_band_list(QMI_NAS_BAND_GSM_900_RAIL|QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_850_JP, QMI_NAS_LTE_BAND_1|QMI_NAS_LTE_BAND_17|QMI_NAS_LTE_BAND_127, "2e,GSM 900 (Railways)&50,WCDMA 2100&5b,WCDMA 850 (Japan)&f9,GSM all&fa,WCDMA all&fc,GSM/WCDMA all&78,LTE Band 1 - 2100MHz&86,LTE Band 17 - 700MHz&9d,LTE Band 127&fb,LTE all&fd,WCDMA/LTE all&fe,GSM/LTE all&ff,All bands", "GSM 900 (Railways);WCDMA 2100;WCDMA 850 (Japan);GSM all;WCDMA all;GSM/WCDMA all;LTE Band 1 - 2100MHz;LTE Band 17 - 700MHz;LTE Band 127;LTE all;WCDMA/LTE all;GSM/LTE all;All bands");
}

// buffer is too short
void TEST_AUTO(test_build_band_list11)
{
    struct qmi_band_bit_mask mask, lte_mask;
    char band_list[SHORT_BUF_CAP];
    char current_band[SHORT_BUF_CAP];
    mask.band = QMI_NAS_BAND_GSM_850;
    lte_mask.band = QMI_NAS_LTE_BAND_7;
    TEST_CHECK(_qmi_build_band_list_from_masks(&mask, &lte_mask, band_list, __countof(band_list), current_band, __countof(current_band)) == -1);
    TEST_CHECK(strcmp(band_list, "") == 0);
    TEST_CHECK(strcmp(current_band, "") == 0);
}

void test_mode_band_to_curband(unsigned short mode_mask, unsigned long long band_mask, unsigned long long lte_band_mask, const char * exp_curband)
{
    struct qmi_band_bit_mask band, lte_band;
    char curband[BUF_CAP];
    band.band = band_mask;
    lte_band.band = lte_band_mask;
    TEST_CHECK(_qmi_mode_band_to_curband(&mode_mask, &band, &lte_band, curband, __countof(curband)) == 0);
    //printf("curband=%s\n", curband);
    TEST_CHECK(strcmp(curband, exp_curband) == 0);
}

// all NULL
void TEST_AUTO(test_mode_band_to_curband0)
{
    TEST_CHECK(_qmi_mode_band_to_curband(NULL, NULL, NULL, NULL, 0) == -1);
}

// both band and lte_band are NULL
void TEST_AUTO(test_mode_band_to_curband1)
{
    unsigned short mode;
    char curband[BUF_CAP];
    TEST_CHECK(_qmi_mode_band_to_curband(&mode, NULL, NULL, curband, __countof(curband)) == 0);
    TEST_CHECK(strcmp(curband, "") == 0);
}

// mode & band are NULL
void TEST_AUTO(test_mode_band_to_curband2)
{
    struct qmi_band_bit_mask lte_band;
    char curband[BUF_CAP];
    lte_band.band = QMI_NAS_LTE_BAND_10;
    TEST_CHECK(_qmi_mode_band_to_curband(NULL, NULL, &lte_band, curband, __countof(curband)) == 0);
    TEST_CHECK(strcmp(curband, "LTE Band 10 - 1700MHz") == 0);
}

// mode & lte_band are NULL
void TEST_AUTO(test_mode_band_to_curband3)
{
    struct qmi_band_bit_mask band;
    char curband[BUF_CAP];
    band.band = QMI_NAS_BAND_GSM_1800;
    TEST_CHECK(_qmi_mode_band_to_curband(NULL, &band, NULL, curband, __countof(curband)) == 0);
    TEST_CHECK(strcmp(curband, "GSM 1800") == 0);
}

// mode GSM
void TEST_AUTO(test_mode_band_to_curband4)
{
    test_mode_band_to_curband(0x0004, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_WCDMA_1900, QMI_NAS_LTE_BAND_1, "GSM 450");
}

// mode WCDMA
void TEST_AUTO(test_mode_band_to_curband5)
{
    test_mode_band_to_curband(0x0008, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_WCDMA_1900, QMI_NAS_LTE_BAND_1, "WCDMA PCS 1900");
}

// mode LTE
void TEST_AUTO(test_mode_band_to_curband6)
{
    test_mode_band_to_curband(0x0010, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_WCDMA_1900, QMI_NAS_LTE_BAND_1, "LTE Band 1 - 2100MHz");
}

// one from each band
void TEST_AUTO(test_mode_band_to_curband7)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_WCDMA_1900, QMI_NAS_LTE_BAND_1, "All bands");
}

// GSM all
void TEST_AUTO(test_mode_band_to_curband8)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_GSM_900, 0, "GSM all");
}

// WCDMA all
void TEST_AUTO(test_mode_band_to_curband9)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_WCDMA_2100|QMI_NAS_BAND_WCDMA_900, 0, "WCDMA all");
}

// LTE all
void TEST_AUTO(test_mode_band_to_curband10)
{
    test_mode_band_to_curband(0x001c, 0, QMI_NAS_LTE_BAND_1|QMI_NAS_LTE_BAND_3, "LTE all");
}

// GSM+WCDMA all
void TEST_AUTO(test_mode_band_to_curband11)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_GSM_450|QMI_NAS_BAND_WCDMA_850, 0, "GSM/WCDMA all");
}

// GSM+LTE all
void TEST_AUTO(test_mode_band_to_curband12)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_GSM_450, QMI_NAS_LTE_BAND_40, "GSM/LTE all");
}

// WCDMA+LTE all
void TEST_AUTO(test_mode_band_to_curband13)
{
    test_mode_band_to_curband(0x001c, QMI_NAS_BAND_WCDMA_850, QMI_NAS_LTE_BAND_40, "WCDMA/LTE all");
}

void test_band_mask_lookup(const char * band, unsigned short exp_mode, unsigned long long exp_band)
{
    unsigned short mode_pref=0;
    struct qmi_band_bit_mask band_pref = {0};
    TEST_CHECK(_qmi_band_mask_lookup(band, band_info_qmi, band_info_qmi_len, &mode_pref, &band_pref));
    //printf("mode_pref=%02x\nband_pref=%llx\n", mode_pref, band_pref.band);
    TEST_CHECK(mode_pref == exp_mode);
    TEST_CHECK(band_pref.band == exp_band);
}

void test_lte_band_mask_lookup(const char * band, unsigned short exp_mode, unsigned long long exp_band)
{
    unsigned short mode_pref = 0;
    struct qmi_band_bit_mask band_pref = {0};
    TEST_CHECK(_qmi_band_mask_lookup(band, lte_band_info_qmi, lte_band_info_qmi_len, &mode_pref, &band_pref));
    //printf("mode_pref=%02x\nband_pref=%llx\n", mode_pref, band_pref.band);
    TEST_CHECK(mode_pref == exp_mode);
    TEST_CHECK(band_pref.band == exp_band);
}

// single GSM
void TEST_AUTO(test_band_mask_lookup1)
{
    test_band_mask_lookup("GSM 450", 0x0004, QMI_NAS_BAND_GSM_450);
}

// single GSM
void TEST_AUTO(test_band_mask_lookup2)
{
    test_band_mask_lookup("30", 0x0004, QMI_NAS_BAND_GSM_1900);
}

// single WCDMA
void TEST_AUTO(test_band_mask_lookup3)
{
    test_band_mask_lookup("WCDMA 1700 (Japan)", 0x0008, QMI_NAS_BAND_WCDMA_1700_JP);
}

// single WCDMA
void TEST_AUTO(test_band_mask_lookup4)
{
    test_band_mask_lookup("5b", 0x0008, QMI_NAS_BAND_WCDMA_850_JP);
}

// GSM all
void TEST_AUTO(test_band_mask_lookup5)
{
    test_band_mask_lookup("GSM all", 0x0004, QMI_NAS_BAND_GSM_ALL);
}

// WCDMA all
void TEST_AUTO(test_band_mask_lookup6)
{
    test_band_mask_lookup("WCDMA all", 0x0008, QMI_NAS_BAND_WCDMA_ALL);
}

// GSM/WCDMA all
void TEST_AUTO(test_band_mask_lookup7)
{
    test_band_mask_lookup("GSM/WCDMA all", 0x000C, QMI_NAS_BAND_GSM_WCDMA_ALL);
}

// All bands
void TEST_AUTO(test_band_mask_lookup8)
{
    unsigned short mode_pref=0;
    struct qmi_band_bit_mask band_pref = {0};
    TEST_CHECK(_qmi_band_mask_lookup("All bands", band_info_qmi, band_info_qmi_len, &mode_pref, &band_pref) == 0);
}

// single LTE
void TEST_AUTO(test_lte_band_mask_lookup1)
{
    test_lte_band_mask_lookup("LTE Band 12 - 700MHz", 0x0010, QMI_NAS_LTE_BAND_12);
}

// single LTE
void TEST_AUTO(test_lte_band_mask_lookup2)
{
    test_lte_band_mask_lookup("92", 0x0010, QMI_NAS_LTE_BAND_21);
}

// LTE all
void TEST_AUTO(test_lte_band_mask_lookup3)
{
    test_lte_band_mask_lookup("LTE all", 0x0010, QMI_NAS_LTE_BAND_ALL);
}

// All bands
void TEST_AUTO(test_lte_band_mask_lookup4)
{
    unsigned short mode_pref=0;
    struct qmi_band_bit_mask band_pref = {0};
    TEST_CHECK(_qmi_band_mask_lookup("All bands", lte_band_info_qmi, lte_band_info_qmi_len, &mode_pref, &band_pref) == 0);
}

void test_band_mask_group_lookup(const char * band, unsigned short exp_mode, unsigned long long exp_band, unsigned long long exp_lte_band)
{
    unsigned short mode_pref = 0;
    struct qmi_band_bit_mask band_pref = {0}, lte_band_pref = {0};
    TEST_CHECK(_qmi_band_mask_group_lookup(band, band_group_info_qmi, band_group_info_qmi_len, &mode_pref, &band_pref, &lte_band_pref));
    //printf("mode_pref=%02x\nband_pref=%llx\nlte_band_pref=%llx\n", mode_pref, band_pref.band, lte_band_pref.band);
    TEST_CHECK(mode_pref == exp_mode);
    TEST_CHECK(band_pref.band == exp_band);
    TEST_CHECK(lte_band_pref.band == exp_lte_band);
}

// WCDMA/LTE all
void TEST_AUTO(test_band_mask_group_lookup1)
{
    test_band_mask_group_lookup("WCDMA/LTE all", 0x0018, QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}
// WCDMA/LTE all
void TEST_AUTO(test_band_mask_group_lookup2)
{
    test_band_mask_group_lookup("fd", 0x0018, QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// GSM/LTE all
void TEST_AUTO(test_band_mask_group_lookup3)
{
    test_band_mask_group_lookup("GSM/LTE all", 0x0014, QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_ALL);
}
// GSM/LTE all
void TEST_AUTO(test_band_mask_group_lookup4)
{
    test_band_mask_group_lookup("fe", 0x0014, QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_ALL);
}

// All bands
void TEST_AUTO(test_band_mask_group_lookup5)
{
    test_band_mask_group_lookup("All bands", 0x001c, QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}
// All bands
void TEST_AUTO(test_band_mask_group_lookup6)
{
    test_band_mask_group_lookup("ff", 0x001c, QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// Failed lookup
void TEST_AUTO(test_band_mask_group_lookup7)
{
    unsigned short mode_pref=0;
    struct qmi_band_bit_mask band_pref = {0}, lte_band_pref = {0};
    TEST_CHECK(_qmi_band_mask_group_lookup("all bands", band_group_info_qmi, band_group_info_qmi_len, &mode_pref, &band_pref, &lte_band_pref) == 0);
}

// Failed lookup
void TEST_AUTO(test_band_mask_group_lookup8)
{
    unsigned short mode_pref=0;
    struct qmi_band_bit_mask band_pref = {0}, lte_band_pref = {0};
    TEST_CHECK(_qmi_band_mask_group_lookup("f0", band_group_info_qmi, band_group_info_qmi_len, &mode_pref, &band_pref, &lte_band_pref) == 0);
}

void test_build_masks_from_band(const char * band, unsigned short exp_mode, unsigned long long exp_band, unsigned long long exp_lte_band)
{
    unsigned short mode_pref;
    struct qmi_band_bit_mask band_pref, lte_band_pref;
    TEST_CHECK(_qmi_build_masks_from_band(band, &mode_pref, &band_pref, &lte_band_pref) == 0);
    //printf("mode_pref=%02x\nband_pref=%llx\nlte_band_pref=%llx\n", mode_pref, band_pref.band, lte_band_pref.band);
    TEST_CHECK(mode_pref == exp_mode);
    TEST_CHECK(band_pref.band == exp_band);
    TEST_CHECK(lte_band_pref.band == exp_lte_band);
}

// single GSM
void TEST_AUTO(test_build_masks_from_band1)
{
    test_build_masks_from_band("GSM 450", 0x0004, QMI_NAS_BAND_GSM_450, 0);
}

// single GSM
void TEST_AUTO(test_build_masks_from_band2)
{
    test_build_masks_from_band("30", 0x0004, QMI_NAS_BAND_GSM_1900, 0);
}

// single WCDMA
void TEST_AUTO(test_build_masks_from_band3)
{
    test_build_masks_from_band("WCDMA 1700 (Japan)", 0x0008, QMI_NAS_BAND_WCDMA_1700_JP, 0);
}

// single WCDMA
void TEST_AUTO(test_build_masks_from_band4)
{
    test_build_masks_from_band("5b", 0x0008, QMI_NAS_BAND_WCDMA_850_JP, 0);
}

// GSM all
void TEST_AUTO(test_build_masks_from_band5)
{
    test_build_masks_from_band("GSM all", 0x0004, QMI_NAS_BAND_GSM_ALL, 0);
}

// WCDMA all
void TEST_AUTO(test_build_masks_from_band6)
{
    test_build_masks_from_band("WCDMA all", 0x0008, QMI_NAS_BAND_WCDMA_ALL, 0);
}

// GSM/WCDMA all
void TEST_AUTO(test_build_masks_from_band7)
{
    test_build_masks_from_band("GSM/WCDMA all", 0x000C, QMI_NAS_BAND_GSM_WCDMA_ALL, 0);
}

// single LTE
void TEST_AUTO(test_build_masks_from_band8)
{
    test_build_masks_from_band("LTE Band 12 - 700MHz", 0x0010, 0, QMI_NAS_LTE_BAND_12);
}

// single LTE
void TEST_AUTO(test_build_masks_from_band9)
{
    test_build_masks_from_band("92", 0x0010, 0, QMI_NAS_LTE_BAND_21);
}

// LTE all
void TEST_AUTO(test_build_masks_from_band10)
{
    test_build_masks_from_band("LTE all", 0x0010, 0, QMI_NAS_LTE_BAND_ALL);
}

// WCDMA/LTE all
void TEST_AUTO(test_build_masks_from_band11)
{
    test_build_masks_from_band("WCDMA/LTE all", 0x0018, QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// WCDMA/LTE all
void TEST_AUTO(test_build_masks_from_band12)
{
    test_build_masks_from_band("fd", 0x0018, QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// GSM/LTE all
void TEST_AUTO(test_build_masks_from_band13)
{
    test_build_masks_from_band("GSM/LTE all", 0x0014, QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_ALL);
}

// GSM/LTE all
void TEST_AUTO(test_build_masks_from_band14)
{
    test_build_masks_from_band("fe", 0x0014, QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_ALL);
}

// All bands
void TEST_AUTO(test_build_masks_from_band15)
{
    test_build_masks_from_band("All bands", 0x001c, QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// All bands
void TEST_AUTO(test_build_masks_from_band16)
{
    test_build_masks_from_band("ff", 0x001c, QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_ALL);
}

// 1 GSM + 1 WCDMA + 1 LTE
void TEST_AUTO(test_build_masks_from_band17)
{
    test_build_masks_from_band("GSM 850;WCDMA 2600;LTE Band 13 - 700MHz", 0x001c, QMI_NAS_BAND_GSM_850|QMI_NAS_BAND_WCDMA_2600, QMI_NAS_LTE_BAND_13);
}

// 1 GSM + 1 WCDMA + 1 LTE
void TEST_AUTO(test_build_masks_from_band18)
{
    test_build_masks_from_band("2B;56;84", 0x001c, QMI_NAS_BAND_GSM_850|QMI_NAS_BAND_WCDMA_2600, QMI_NAS_LTE_BAND_13);
}

// GSM all + 1 LTE
void TEST_AUTO(test_build_masks_from_band19)
{
    test_build_masks_from_band("GSM all;LTE Band 13 - 700MHz", 0x0014, QMI_NAS_BAND_GSM_ALL, QMI_NAS_LTE_BAND_13);
}

// WCDMA all + 1 LTE
void TEST_AUTO(test_build_masks_from_band20)
{
    test_build_masks_from_band("WCDMA all;LTE Band 13 - 700MHz", 0x0018, QMI_NAS_BAND_WCDMA_ALL, QMI_NAS_LTE_BAND_13);
}

// GSM/WCDMA all + 1 LTE
void TEST_AUTO(test_build_masks_from_band21)
{
    test_build_masks_from_band("GSM all;WCDMA all;LTE Band 13 - 700MHz", 0x001c, QMI_NAS_BAND_GSM_WCDMA_ALL, QMI_NAS_LTE_BAND_13);
}

// 1 GSM + LTE all
void TEST_AUTO(test_build_masks_from_band22)
{
    test_build_masks_from_band("GSM 850;LTE all", 0x0014, QMI_NAS_BAND_GSM_850, QMI_NAS_LTE_BAND_ALL);
}

// 1 GSM + unknown + LTE all
void TEST_AUTO(test_build_masks_from_band23)
{
    test_build_masks_from_band("GSM 850;abc;LTE all", 0x0014, QMI_NAS_BAND_GSM_850, QMI_NAS_LTE_BAND_ALL);
}

// no match
void TEST_AUTO(test_build_masks_from_band24)
{
    unsigned short mode_pref;
    struct qmi_band_bit_mask band_pref, lte_band_pref;
    TEST_CHECK(_qmi_build_masks_from_band("abc", &mode_pref, &band_pref, &lte_band_pref) == -1);
    TEST_CHECK(mode_pref == 0);
    TEST_CHECK(band_pref.band == 0);
    TEST_CHECK(lte_band_pref.band == 0);
}

TEST_AUTO_MAIN

