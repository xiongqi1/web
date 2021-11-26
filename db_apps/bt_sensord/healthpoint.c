/*
 * healthpoint.c:
 *    Posts data to Healthpoint cloud service.
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#include <curl/curl.h>

#include "bt_sensord.h"

// Healthpoint defined post URL
#define HPOINT_POST_BASEURL "https://www.myglucohealth.net/PostTestNew.asp"
#define HPOINT_POST_URL_SN_FORMAT \
    HPOINT_POST_BASEURL"?no=%d&yr=%d&mo=%d&da=%d&hr=%d&min=%d&val=%s&act=%d&cs=%d&sn=%s&src=%s&all=y"

// NTC Vendor id
#define HPOINT_POST_DATA_SRC "NC7sf34bc"

// Communication source - Computer via Bluetooth
#define HPOINT_POST_DATA_CS 2

// Activity - fake data for now
#define HPOINT_POST_DATA_ACT 4

#define HPOINT_URL_MAX_LEN 256

/*
 * Post data to the Healthpoint cloud service
 *
 * Parameters:
 *    device     [in] The device that collected the data
 *    data       [in] Array of collected data to be posted
 *    num_data   [in] Number of data items in the 'data' array
 *
 * Returns:
 *    0 on success. Non-zero on error.
 */
int
hpoint_data_post (bt_device_t *device, bt_device_data_t *data, int num_data)
{
    int ix;
    CURL *curl;
    CURLcode res;
    char url[HPOINT_URL_MAX_LEN];
    int ret = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    for (ix = 0; ix < num_data; ix++) {

        snprintf(url, sizeof(url), HPOINT_POST_URL_SN_FORMAT, 1,
                 data[ix].timestamp.year, data[ix].timestamp.month,
                 data[ix].timestamp.day, data[ix].timestamp.hour,
                 data[ix].timestamp.minute, data[ix].value,
                 HPOINT_POST_DATA_ACT, HPOINT_POST_DATA_CS,
                 device[ix].serial_num, HPOINT_POST_DATA_SRC);

        curl_easy_setopt(curl, CURLOPT_URL, url);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            dbgp("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            ret = -1;
        } else {
            dbgp("healthpoint: Post data succeeded!\n");
        }
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return ret;
}
