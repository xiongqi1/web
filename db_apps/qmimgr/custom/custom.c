/*
 * Custom band settings.
 * This module implements custom band list.
 * It enables band name customization and band limiting.
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

#include "custom.h"
#include "../minilib.h"

/*
 * Conditionally include appropriate custom band file.
 * Each product can have its own custom band file by adding to the following.
 * By default, all bands are enabled.
 */
#if defined(UNIT_TEST)
    #include "_default.c"
#elif defined(V_PRODUCT_ntc_145w)
    #include "_ntc_145w.c"
#elif defined(V_PRODUCT_ntc_nrb307)
    #include "_ntc_nrb307.c"
#else
    #include "_default.c"
#endif

const int custom_band_qmi_len = __countof(custom_band_qmi);

/* get custom band name by hex. return NULL if not found */
const char * get_band_name_by_hex(int hex)
{
    int i;
    for(i = 0; i < custom_band_qmi_len; i++) {
        if(hex == custom_band_qmi[i].hex) {
            return custom_band_qmi[i].name;
        }
    }
    return NULL;
}

/* get custom band hex by name. return -1 if not found */
int get_band_hex_by_name(const char * name)
{
    int i;
    for(i = 0; i < custom_band_qmi_len; i++) {
        if(!strcmp(name, custom_band_qmi[i].name)) {
            return custom_band_qmi[i].hex;
        }
    }
    return -1;
}
