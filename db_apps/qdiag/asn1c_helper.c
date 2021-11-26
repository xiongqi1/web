/*!
 * Implementation of ASN1C helper
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/

#include "asn1c_helper.h"

/*
 convert MCC_T type of mcc or MNC_T type of mnc into C string value.

 Params:
  mncmcc : output buffer for string version of given MCC_MNC_Digit_t value.
  mncmcc_size : output buffer size of mncmcc.
  mcc : MCC_t type of mcc if mcc is not NULL
  mnc : MNC_t type of mnc if mnc is not NULL

 Return:
  Total number of letters that are written into mncmcc

 Note:
*/

int get_mccmnc_from_mccmnc_digits(char* mncmcc, int mncmcc_size, const MCC_t* mcc, const MNC_t* mnc)
{
    MCC_MNC_Digit_t* mccmnc_digit;

    int i;
    int j;

    j = 0;

    /* mcc */
    if (mcc) {
        for_each_asn1c_array(i, mcc, mccmnc_digit) {
            /* bypass if index is is out of size */
            if (j >= mncmcc_size)
                break;
            mncmcc[j++] = *mccmnc_digit + '0';
        }
    }

    /* mnc */
    if (mnc) {
        for_each_asn1c_array(i, mnc, mccmnc_digit) {
            /* bypass if index is is out of size */
            if (j >= mncmcc_size)
                break;
            mncmcc[j++] = *mccmnc_digit + '0';
        }
    }

    /* put NULL termination if space allows */
    if (j < mncmcc_size)
        mncmcc[j++] = 0;

    return j;
}

/*
 convert ASN1C BIT_STRING_t into unsigned int value.

 Params:
  bs : BIT_STRING_t type value.

 Return:
  unsigned int version of given BIT_STRING_T type value.
*/
unsigned int get_uint_from_bit_string(BIT_STRING_t* bs)
{
    int i;
    unsigned int res = 0;

    for (i = 0; i < bs->size; i++) {
        res = (res << 8) | bs->buf[i];
    }

    return res >> bs->bits_unused;
}
