/*
 * Class to provide manipulations on hexadecimal string.
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

#include "HexMask.h"
#include <regex>

namespace ggk {
HexMask::HexMask(): hexmask("0x00")
{
}

HexMask::HexMask(std::string value): hexmask(value)
{
}

std::string HexMask::operator&(const std::string &value) const
{
    std::regex hexPrefix("^0[xX]");
    std::string a = this->hexmask;
    std::string b = value;

    std::string ret, aAdj, bAdj;

    std::string aStr = std::regex_replace(a, hexPrefix, "");
    std::string bStr = std::regex_replace(b, hexPrefix, "");

    int aLen = aStr.size();
    int bLen = bStr.size();
    int retLen = aLen > bLen ? aLen : bLen;

    ret.reserve(retLen);
    aAdj.reserve(retLen);
    bAdj.reserve(retLen);

    aAdj.append(std::string(retLen - aLen, '0') + aStr);
    bAdj.append(std::string(retLen - bLen, '0') + bStr);

    for (int i = 0; i < retLen; i++)
    {
        ret.push_back(hex_chars[hexChToNum(aAdj[i]) & hexChToNum(bAdj[i])]);
    }
    return ret;
}

std::string HexMask::operator&(const HexMask &value) const
{
    return operator&((std::string &)(value.hexmask));
}

unsigned char HexMask::hexChToNum(const char &ch) const
{
    if(isdigit(ch))
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if(ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    throw std::invalid_argument("Invalid hex input");
}
} /* namespace ggk */
