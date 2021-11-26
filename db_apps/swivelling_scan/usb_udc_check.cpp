/*
 * Implementing USB UDC check
 *
 * Copyright Notice:
 * Copyright (C) 2021 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms)
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


#include <filesystem>
#include <fstream>
#include <string>

#include "usb_udc_check.hpp"

using namespace std::filesystem;

namespace swscan
{

    //return true only if state "not attached" is confirmed
    bool UsbUdcCheck::isSafeForSwivelling()
    {
        path udcDirPath("/sys/class/udc");
        directory_entry udcDirEntry(udcDirPath);
        if (udcDirEntry.exists() && udcDirEntry.is_directory()) {
            auto begin = directory_iterator(udcDirPath);
            auto end = directory_iterator();
            // assuming that there is only one UDC
            // that should be true in most cases
            // consider first child entry only
            if (begin != end) {
                auto& childEntry = *begin;
                if (childEntry.is_symlink()) {
                    path statePath{childEntry.path()};
                    statePath.append("state");
                    directory_entry stateEntry(statePath);
                    if (stateEntry.exists() && stateEntry.is_regular_file()) {
                        std::ifstream stateFile(stateEntry.path());
                        std::string stateStr;
                        if (std::getline(stateFile, stateStr)) {
                            if (stateStr == "not attached") {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        return false;
    }
}
