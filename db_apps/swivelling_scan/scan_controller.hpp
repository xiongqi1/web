/*
 * Scan controller interface
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
#ifndef _SWSCAN_SCAN_CONTROLLER_
#define _SWSCAN_SCAN_CONTROLLER_

#include <vector>
#include <functional>
#include <exception>

#include "scan_serving.hpp"

namespace swscan
{
    class ScanController
    {
        public:
            virtual void setScanServing(ScanServing& ss);
            virtual ~ScanController() = default;
            // throw std::invalid_argument on invalid arguments
            virtual void initialiseScan(int maxStep) = 0;
            virtual void scanNextStep() = 0;
            virtual void stopScan() = 0;
            virtual bool isScanningForStep(int step) = 0;

            class invalid_scan_conditions: public std::exception
            {
                public:
                    invalid_scan_conditions(const char *msg) : message{msg} { }
                    invalid_scan_conditions(const std::string &msg) : message{msg} { }
                    invalid_scan_conditions() = default;
                    virtual const char* what() const noexcept override
                    {
                        return message.c_str();
                    }
                private:
                    std::string message;
            };
            // throws invalid_scan_conditions for invalid conditions for doing scanning
            virtual void checkScanConditions() = 0;
        protected:
            //throw exception std::out_of_range if no ScanServing has been set
            virtual ScanServing& getScanServing();
        private:
            std::vector<std::reference_wrapper<ScanServing>> scanServing;
    };
}

#endif
