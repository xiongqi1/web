/*
 * Casa-defined scan algorithm
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
#ifndef _SWSCAN_CASA_SCAN_CONTROLLER_
#define _SWSCAN_CASA_SCAN_CONTROLLER_

#include <chrono>

#include <erdb.hpp>
#include <asio.hpp>

#include "scan_controller.hpp"
#include "scan_serving.hpp"

namespace swscan
{
    class CasaScanController: public ScanController
    {
        public:
            // throw std::invalid_argument if posMeasureSec or measureInterval are invalid
            CasaScanController(asio::io_context &io,
                int posMeasureSec, int measureInterval = 1);
            virtual void initialiseScan(int maxStep) override;
            virtual void scanNextStep() override;
            virtual void stopScan() override;
            virtual bool isScanningForStep(int step) override;
            virtual void checkScanConditions() override;
        protected:
            asio::io_context &io_context;
            asio::steady_timer measureTimer;
            int posMeasureTime;
            int measureInterval;
            int maxStep;
            int currentStep;
            std::chrono::time_point<std::chrono::steady_clock> posMeasTimeout;
            virtual void measure(const asio::error_code& e);

            bool scanSessionStarted;
            // for 5G
            float bestRf;
            float currentStepRf;
            int bestPositionStep;
            // for LTE only
            float bestRfLte;
            float currentStepRfLte;
            int bestPositionStepLte;
            virtual void resetMeasurementResult();
            virtual void resetCurrentStepResult();
            virtual void scheduleMeasure();
            virtual void updateMeasurementStepData();
            virtual void generateDataTraffic();
        private:
            virtual void asyncWaitOnMeasureTimer();
            // for 5G
            float currentStepRfSum;
            int currentStepRfCount;
            // for LTE only
            float currentStepRfLteSum;
            int currentStepRfLteCount;
    };
}

#endif
