/*
 * NetComm OMA-DM Client
 *
 * utils.h
 * Timer utilities.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __OMADM_UTILS_H_20180402__
#define __OMADM_UTILS_H_20180402__

    #include <sys/types.h>
    #include <time.h>

    /* Get the current time. */
    void time_now(struct timespec* ts);

    /* Convert milliseconds to timespec. */
    void time_from_ms(struct timespec* ts, int ms);

    /* Add one time into another. */
    void time_add(struct timespec* tsA, const struct timespec* tsB);

    /* Add milliseconds into timespec. */
    void time_add_ms(struct timespec* ts, int ms);

    /* Return the difference between two times in milliseconds. */
    int time_diff_ms(const struct timespec* tsA, const struct timespec* tsB);

    /* Return the difference between now and the specified time in milliseconds. */
    int time_diff_now_ms(const struct timespec* ts);

    int base64_decode_length(int len);
    int base64_decode(unsigned char* bin, const char* b64, int len);

    int base64_encode_length(int len);
    int base64_encode(char* b64, const unsigned char* bin, int len);

    int hex_decode(char* bin, const char* hex, int len);
    void hex_encode(char* hex, const char* bin, int len);

#endif
