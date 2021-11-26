/*
 * Implementing Configuration parser/provider
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

#include <string>

#include <erdb.hpp>
#include "configuration.hpp"

namespace swscan
{
    Configuration::Configuration()
    {
        const std::string prefix("swivelling_scan.conf.");
        maxCoordinate = erdb::Rdb{prefix + "coordinate.max"}.get();
        if (maxCoordinate <= 0) {
            throw swscan::invalid_configuration("Invalid max coordinate");
        }
        positionNumber = erdb::Rdb{prefix + "position.number"}.get();
        if (positionNumber <= 0) {
            throw swscan::invalid_configuration("Invalid position number");
        }
        for (int i = 0; i < positionNumber; i++) {
            erdb::Rdb rdb{ prefix + "position." + std::to_string(i) + ".coordinate"};
            auto val = rdb.get();
            if (val.isSet()) {
                int coordinate = val;
                if (coordinate < 0 || coordinate > maxCoordinate) {
                    throw swscan::invalid_configuration("Invalid coordinate");
                }
                positions.push_back(coordinate);
            } else {
                throw swscan::invalid_configuration("Insufficient positions");
            }
        }
        homePositionIndex = erdb::Rdb{prefix + ".home_position_index"}.get();
        if (homePositionIndex < 0 || homePositionIndex >= positionNumber) {
            throw swscan::invalid_configuration("Invalid home position index");
        }
        motorInitialDelayUs = erdb::Rdb{prefix + "motor.initial_delay_us"}.get();
        if (motorInitialDelayUs <= 0) {
            throw swscan::invalid_configuration("Invalid initial_delay_us");
        }
        motorMaxStepsPerSec = erdb::Rdb{prefix + "motor.max_steps_per_second"}.get();
        if (motorMaxStepsPerSec <= 0) {
            throw swscan::invalid_configuration("Invalid max_steps_per_second");
        }
        measurementSecAtPosition = erdb::Rdb{prefix + "measurement_time_at_position_s"}.get();
        if (measurementSecAtPosition <= 0) {
            throw swscan::invalid_configuration("Invalid measurement_time_at_position_s");
        }
        std::string bestPosStr = rdbBestPositionIndex.get();
        if (bestPosStr.length() > 0) {
            try {
                bestPositionIndex = std::stoi(bestPosStr);
                if (bestPositionIndex < 0 || bestPositionIndex > positionNumber) {
                    throw swscan::invalid_configuration("Invalid best position index " + bestPosStr);
                }
            } catch (const std::invalid_argument &e) {
                throw swscan::invalid_configuration("Invalid best position index " + bestPosStr);
            }
        } else {
            bestPositionIndex = -1;
        }
        noSwivellingSafetyCheck = erdb::Rdb{prefix + "no_swivelling_safety_check"}.get();
    }

    int Configuration::getCoordinateAtPositionIndex(int index) const
    {
        return index >= 0 ? positions.at(index) : -1;
    }

    int Configuration::getIndexFromCoordinate(int coord) const
    {
        if (coord < 0) {
            return -1;
        }
        int bestIndex = 0;
        int bestDelta = std::abs(positions[bestIndex] - coord);
        for (unsigned int index = 1; index < positions.size(); index++) {
            int delta = std::abs(positions[index] - coord);
            if (delta < bestDelta) {
                bestIndex = index;
                bestDelta = delta;
            }
        }

        return bestIndex;
    }

    void Configuration::setBestPositionIndex(int index)
    {
        if (index < 0 || index > positionNumber) {
            throw swscan::invalid_configuration("Invalid best position index: " + std::to_string(index));
        }
        bestPositionIndex = index;
        rdbBestPositionIndex.set(index);
    }
}
