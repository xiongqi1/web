/*
 * Configuration parser/provider
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
#ifndef _SWSCAN_CONFIGURATION_
#define _SWSCAN_CONFIGURATION_

#include <vector>
#include <exception>
#include <string>

#include <erdb.hpp>

namespace swscan
{
    class invalid_configuration: public std::exception
    {
        public:
            invalid_configuration(const char *msg) : message{msg} { }
            invalid_configuration(const std::string &msg) : message{msg} { }
            invalid_configuration() = default;
            virtual const char* what() const noexcept override
            {
                return message.c_str();
            }
        private:
            std::string message;
    };

    class Configuration
    {
        public:
            // parse configuration
            // throw swscan::invalid_configuration for invalid configuration
            Configuration();
            virtual ~Configuration() = default;
            virtual int getMaxCoordinate() const { return maxCoordinate; }
            virtual int getPositionNumber() const { return positionNumber; }
            virtual int getCoordinateAtPositionIndex(int index) const;
            virtual int getIndexFromCoordinate(int coord) const;
            virtual int getHomePositionIndex() const { return homePositionIndex; }
            virtual int getMotorInitialDelayUs() const { return motorInitialDelayUs; }
            virtual int getMotorMaxStepsPerSec() const { return motorMaxStepsPerSec; }
            virtual int getMeasurementSecAtPosition() const { return measurementSecAtPosition; }
            virtual int getBestPositionIndex() const { return bestPositionIndex; }
            // throw swscan::invalid_configuration on invalid argument
            virtual void setBestPositionIndex(int index);
            virtual bool getNoSwivellingSafetyCheck() const { return noSwivellingSafetyCheck; }
        protected:
            int maxCoordinate;
            int positionNumber;
            std::vector<int> positions;
            int homePositionIndex;
            int motorInitialDelayUs;
            int motorMaxStepsPerSec;
            int measurementSecAtPosition;
            int bestPositionIndex;
            erdb::Rdb rdbBestPositionIndex{"swivelling_scan.best_position_index"};
            bool noSwivellingSafetyCheck = false;
    };
}


#endif
