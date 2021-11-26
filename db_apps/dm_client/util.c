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

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#include "util.h"

void time_now(struct timespec* ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

void time_from_ms(struct timespec* ts, int ms)
{
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1e6;
}

void time_add(struct timespec* tsA, const struct timespec* tsB)
{
    tsA->tv_sec += tsB->tv_sec;
    tsA->tv_nsec += tsB->tv_nsec;
    if (tsA->tv_nsec >= 1e9) {
        tsA->tv_nsec -= 1e9;
        tsA->tv_sec++;
    }
}

void time_add_ms(struct timespec* ts, int ms)
{
    struct timespec tsB;
    time_from_ms(&tsB, ms);
    time_add(ts, &tsB);
}

int time_diff_ms(const struct timespec* tsA, const struct timespec* tsB)
{
    struct timespec ts = {
        .tv_sec = tsB->tv_sec - tsA->tv_sec,
        .tv_nsec = tsB->tv_nsec - tsA->tv_nsec
    };
    return ts.tv_sec * 1000 + ts.tv_nsec / 1e6;
}

int time_diff_now_ms(const struct timespec* ts)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return time_diff_ms(&now, ts);
}

int base64_decode_length(int len)
{
    int num = len / 4;
    int rem = len % 4;
    return (rem > 0 ? num + 1 : num) * 3;
}

static char base64_decode_char(char b64)
{
    if (b64 >= '0' && b64 <= '9') { return b64 + (52 - '0'); }
    if (b64 >= 'a' && b64 <= 'z') { return b64 + (26 - 'a'); }
    if (b64 >= 'A' && b64 <= 'Z') { return b64 - 'A'; }
    if (b64 == '+') { return 62; }
    if (b64 == '/') { return 63; }
    if (b64 == '=') { return 0; }
    return -1;
}

int base64_decode(unsigned char* bin, const char* b64, int len)
{
    unsigned char* start = bin;
    for (; len > 0; len -= 4, b64 += 4)
    {
        int a = base64_decode_char(b64[0]);
        int b = base64_decode_char(b64[1]);
        if (a < 0 || b < 0) {
            return -1;
        }
        *bin++ = (a << 2) | ((b & 0x30) >> 4);
        if (len > 2 && b64[2] != '=') {
            int c = base64_decode_char(b64[2]);
            if (c < 0) {
                return -1;
            }
            *bin++ = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
            if (len > 3 && b64[3] != '=') {
                int d = base64_decode_char(b64[3]);
                if (d < 0) {
                    return -1;
                }
                *bin++ = ((c & 0x03) << 6) | d;
            }
        }
    }
    return bin - start;
}

int base64_encode_length(int len)
{
    int num = len / 3;
    int rem = len % 3;
    return (rem > 0 ? num + 1 : num) * 4;
}

int base64_encode(char* b64, const unsigned char* bin, int len)
{
    const char* map = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char* start = b64;
    for (; len > 0; len -= 3, bin += 3)
    {
        *b64++ = map[(bin[0] & 0xFC) >> 2];
        if (len > 1) {
            *b64++ = map[((bin[0] & 0x3) << 4) | ((bin[1] & 0xF0) >> 4)];
            if (len > 2) {
                *b64++ = map[((bin[1] & 0x0F) << 2) | ((bin[2] & 0xC0) >> 6)];
                *b64++ = map[bin[2] & 0x3F];
            } else {
                *b64++ = map[(bin[1] & 0x0F) << 2];
                *b64++ = '=';
            }
        } else {
            *b64++ = map[(bin[0] & 0x3) << 4];
            *b64++ = '=';
            *b64++ = '=';
        }
    }
    return b64 - start;
}

static int hex_decode_char(char hex)
{
    if (hex >= '0' && hex <= '9') { return hex - '0'; }
    if (hex >= 'a' && hex <= 'f') { return (hex - 'a') + 10; }
    if (hex >= 'A' && hex <= 'F') { return (hex - 'A') + 10; }
    return -1;
}

int hex_decode(char* bin, const char* hex, int len)
{
    if (len % 2) {
        return false;
    }
    for (; len > 0; len -= 2) {
        int l = hex_decode_char(*hex++);
        int r = hex_decode_char(*hex++);
        if (l < 0 || r < 0) {
            return false;
        }
        *bin++ = (l << 4) + r;
    }
    return true;
}

void hex_encode(char* hex, const char* bin, int len)
{
    const char* map = "0123456789ABCDEF";
    for (; len > 0; len--) {
        *hex++ = map[(*bin & 0xF0) >> 4];
        *hex++ = map[*bin++ & 0x0F];
    }
}
