/*!
 * C header file of ASN1C helper
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

/*
 loop over each of elements in ASN1C A_SEQUENCE_OF() array

 Params:
  i : loop integer iterator
  array_var : A_SEQUENCE_OF() variable
  element_var : each element pointer of A_SEQUENCE_OF() variable

 Return:
  N/A
*/

/* asn header */
#include <asn_application.h>
#include <asn_internal.h>	/* for _ASN_DEFAULT_STACK_MAX */
#include <BCCH-DL-SCH-Message.h>


#define for_each_asn1c_array(i,array_var,element_var) \
    for((i)=0,(element_var)=(array_var)->list.array[i];(i)<(array_var)->list.count;i++,(element_var)=(array_var)->list.array[i])

int get_mccmnc_from_mccmnc_digits(char* mncmcc, int mncmcc_size, const MCC_t* mcc, const MNC_t* mnc);
unsigned int get_uint_from_bit_string(BIT_STRING_t* bs);
