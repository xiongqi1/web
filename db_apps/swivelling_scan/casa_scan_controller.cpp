/*
 * Implementing Casa-defined scan algorithm
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

#include <stdexcept>
#include <exception>
#include <limits>
#include <string>

#include "elogger.hpp"
#include "casa_scan_controller.hpp"
#include "udp_client.hpp"

namespace swscan
{
    CasaScanController::CasaScanController(asio::io_context &io,
        int posMeasSec, int measInterval)
        :
        io_context(io),
        measureTimer{io},
        posMeasureTime(posMeasSec), measureInterval(measInterval)
    {
        if (posMeasSec <= 0 || measInterval <= 0 || posMeasSec < measInterval) {
            throw std::invalid_argument("Invalid time arguments");
        }
        resetMeasurementResult();
    }

    void CasaScanController::checkScanConditions()
    {
        if (erdb::Rdb{"wwan.0.sim.status.status"}.get().toStdString() != "SIM OK") {
            throw swscan::ScanController::invalid_scan_conditions("SIM card is invalid.");
        }
    }

    void CasaScanController::initialiseScan(int maxStepParam)
    {
        swscan_log(LOG_NOTICE, "maxStepParam = %d", maxStepParam);
        if (maxStepParam <= 0) {
            throw std::invalid_argument("Invalid maxStepParam");
        }
        maxStep = maxStepParam;
        resetMeasurementResult();
        scanSessionStarted = true;
    }

    void CasaScanController::scanNextStep()
    {
        swscan_log(LOG_NOTICE, "Being asked to scan for next step.");
        resetCurrentStepResult();

        currentStep++;
        posMeasTimeout = std::chrono::steady_clock::now() + std::chrono::seconds(posMeasureTime);
        measureTimer.expires_after(std::chrono::seconds(0));
        asyncWaitOnMeasureTimer();
    }

    bool CasaScanController::isScanningForStep(int step)
    {
        return scanSessionStarted && step == currentStep;
    }

    void CasaScanController::stopScan()
    {
        swscan_log(LOG_NOTICE, "Being asked to stop the scan.");
        if (scanSessionStarted) {
            scanSessionStarted = false;
            // try cancel timer
            measureTimer.cancel();
        }
        resetMeasurementResult();
        resetCurrentStepResult();
    }

    void CasaScanController::asyncWaitOnMeasureTimer()
    {
        measureTimer.async_wait(std::bind(&CasaScanController::measure, this,
            std::placeholders::_1));
    }

    void CasaScanController::resetMeasurementResult()
    {
        currentStep = 0;
        bestPositionStep = 0;
        bestRf = std::numeric_limits<float>::min();
        bestPositionStepLte = 0;
        bestRfLte = std::numeric_limits<float>::min();
        resetCurrentStepResult();
    }

    void CasaScanController::resetCurrentStepResult()
    {
        currentStepRf = std::numeric_limits<float>::min();
        currentStepRfSum = 0;
        currentStepRfCount = 0;
        currentStepRfLte = std::numeric_limits<float>::min();
        currentStepRfLteSum = 0;
        currentStepRfLteCount = 0;
    }

    void CasaScanController::updateMeasurementStepData()
    {
        if (currentStepRfCount > 0) {
            currentStepRf = currentStepRfSum/currentStepRfCount;
            if (currentStepRf > bestRf) {
                bestPositionStep = currentStep;
                bestRf = currentStepRf;
            }
        }

        if (currentStepRfLteCount > 0) {
            currentStepRfLte = currentStepRfLteSum/currentStepRfLteCount;
            if (currentStepRfLte > bestRfLte) {
                bestPositionStepLte = currentStep;
                bestRfLte = currentStepRfLte;
            }
        }
    }

    void CasaScanController::scheduleMeasure()
    {
        if (!scanSessionStarted) {
            swscan_log(LOG_NOTICE, "scanSessionStarted == false. Ignore.");
            return;
        }

        if (posMeasTimeout > std::chrono::steady_clock::now()) {
            swscan_log(LOG_NOTICE, "Scheduling next measurement for current step.");
            measureTimer.expires_after(std::chrono::seconds(measureInterval));
            asyncWaitOnMeasureTimer();
        } else {
            // current step is done, update best result
            updateMeasurementStepData();

            // either swivelling to next step or report best position
            if (currentStep < maxStep) {
                swscan_log(LOG_NOTICE, "Asking scan-serving to swivel next step");
                getScanServing().swivelNextStep(currentStep);
            } else {
                swscan_log(LOG_NOTICE, "Done, reporting best position.");
                scanSessionStarted = false;
                getScanServing().swivellingScanDone(bestPositionStep > 0 ? bestPositionStep : bestPositionStepLte);
            }
        }
    }

    void CasaScanController::measure(const asio::error_code& e)
    {
        if (e) {
            if (e != asio::error::operation_aborted) {
                if (scanSessionStarted) {
                    scanSessionStarted = false;
                    getScanServing().swivellingScanStopped(currentStep,
                        "Measurement timer error: " + e.message());
                }
            }
            return;
        }

        if (!scanSessionStarted) {
            return;
        }

        bool attached = erdb::Rdb{"wwan.0.system_network_status.attached"}.get();
        if (!attached) {
            swscan_log(LOG_NOTICE, "Not attached to network.");
            scheduleMeasure();
            return;
        }

        generateDataTraffic();

        std::string snrLteStr = erdb::Rdb{"wwan.0.signal.snr"}.get();
        swscan_log(LOG_NOTICE, "LTE SNR=%s", snrLteStr.c_str());
        if (snrLteStr.length() > 0) {
            try {
                float val = std::stof(snrLteStr);
                currentStepRfLteSum += val;
                currentStepRfLteCount++;
            } catch (const std::exception &e) {
                // ignore
                swscan_log(LOG_NOTICE, "Exception in converting LTE SNR %s: %s", snrLteStr.c_str(), e.what());
            }
        }

        std::string systemSoStr = erdb::Rdb{"wwan.0.system_network_status.current_system_so"}.get();
        if (systemSoStr.find("5G") != std::string::npos) {
            std::string snrStr = erdb::Rdb{"wwan.0.radio_stack.nr5g.snr"}.get();
            swscan_log(LOG_NOTICE, "SNR=%s", snrStr.c_str());
            if (snrStr.length() > 0) {
                try {
                    float val = std::stof(snrStr);
                    currentStepRfSum += val;
                    currentStepRfCount++;
                } catch (const std::exception &e) {
                    // ignore
                    swscan_log(LOG_NOTICE, "Exception in converting SNR %s: %s", snrStr.c_str(), e.what());
                }
            }
        } else {
            swscan_log(LOG_NOTICE, "5G is not active");
        }

        scheduleMeasure();
    }

    void CasaScanController::generateDataTraffic()
    {
        // make UE generate some uplink traffic
        // how remote site processes traffic does not matter
        std::string destIpAddr = erdb::Rdb{"link.policy.1.dns1"}.get();
        if (destIpAddr.length() == 0) {
            destIpAddr = "8.8.8.8";
        }
        unsigned short destPortNum = 53;
        try {
            UdpClient udp(io_context, destIpAddr, destPortNum);
            for (int i = 0; i < 20; i++) {
                udp.send(".............................................................");
            }
        } catch (const std::exception &e) {
            // as this is an attempt to generate uplink traffic,
            // failure does not matter, just print log
            swscan_log(LOG_NOTICE, "Exception in trying UDP: %s", e.what());
        }
    }
}
