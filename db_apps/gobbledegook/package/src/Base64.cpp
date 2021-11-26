/*
 * Class to convert hexadecimal string to Base64 or vice versa
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Base64.h"
#include <iostream>
#include <regex>

namespace ggk {

static const std::string base64_chars =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";

static const std::string hex_chars = "0123456789ABCDEF";

// get hex value of hexadecimal character.
static unsigned char chhex(char ch) {
    if(isdigit(ch))
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if(ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    throw std::invalid_argument("Invalid hex input");
}

// Convert HexString to unsigned char vector.
Base64::uCharVec cvtHexStrToVchar(const std::string &source, std::size_t min) {
    int i = 0;
    std::regex hexPrefix("^0[xX]");
    Base64::uCharVec retVal;
    std::string paddedSource, sStr;
    std::size_t reserve_bytes, source_size;

    sStr = std::regex_replace(source, hexPrefix, "");

    source_size = sStr.size();

    if ((source_size/2) < min) {
        reserve_bytes = min*2;
    } else {
        reserve_bytes = source_size + (source_size % 2);
    }

    paddedSource.reserve(reserve_bytes);
    paddedSource.append((reserve_bytes - source_size), '0'); // add padding
    paddedSource.append(sStr);
    retVal.reserve(paddedSource.size() * 2);
    unsigned char completedCh = 0;
    for (auto ch : paddedSource) {
        i++;
        if ((i & 0x01) == 1) {
            completedCh = chhex(ch) << 4;
        } else {
            completedCh |= chhex(ch);
            retVal.push_back(completedCh);
        }
    }
    return retVal;
}

std::string Base64::cvtHexStrToBase64(const uCharVec &bytes_to_encode) {
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3], char_array_4[4];

    ret.reserve((bytes_to_encode.size()/3 + 1) * 4);
    for (auto elem : bytes_to_encode) {
        char_array_3[i++] = elem;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++) {
                ret.push_back(base64_chars[char_array_4[i]]);
            }
            i = 0;
        }
    }

    if (i) {
        for(int j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = ( char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; (j < i + 1); j++) {
            ret.push_back(base64_chars[char_array_4[j]]);
        }

        while((i++ < 3)) {
            ret.push_back('=');
        }
    }

    return ret;
}

std::string Base64::cvtHexStrToBase64(const std::string &hexStr, size_t min_bytes) {
    uCharVec bytes_to_encode = cvtHexStrToVchar(hexStr, min_bytes);
    return cvtHexStrToBase64(bytes_to_encode);
}

std::string Base64::cvtBase64ToHexStr(const std::string &encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    if ((in_len %4) != 0 ||
        std::regex_match (encoded_string, std::regex(".*[^" + base64_chars +"=].*"))) {
        throw std::invalid_argument("Invalid base64 input");
    }

    ret.reserve((encoded_string.size()/4) * 3 * 2);
    while (in_len-- && (encoded_string[in_] != '=')) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++) {
                char_array_4[i] = base64_chars.find(char_array_4[i]) & 0xff;
            }

            char_array_3[0] = ( char_array_4[0] << 2       ) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) +   char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret.push_back(hex_chars[ ( char_array_3[i] & 0xF0 ) >> 4 ]);
                ret.push_back(hex_chars[ ( char_array_3[i] & 0x0F ) >> 0 ]);
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = 0; j < i; j++) {
            char_array_4[j] = base64_chars.find(char_array_4[j]) & 0xff;
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (int j = 0; (j < i - 1); j++) {
            ret.push_back(hex_chars[ ( char_array_3[j] & 0xF0 ) >> 4 ]);
            ret.push_back(hex_chars[ ( char_array_3[j] & 0x0F ) >> 0 ]);
        }
    }

    return ret;
}

}
