/*
 * Motor driver interface
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
#ifndef _SWSCAN_MOTOR_DRIVER_
#define _SWSCAN_MOTOR_DRIVER_

namespace swscan
{
    class MotorDriver
    {
        public:
            virtual ~MotorDriver() = default;
            // @return 0 or error code
            // throw std::invalid_argument on invalid arguments
            virtual int init(int initialDelayUs, int maxStepPerSecond) = 0;
            // @return 0 or error code
            virtual int goToCoordinate(int coordinate) = 0;

            struct CheckMotorResult
            {
                // 0 or error code
                int returnCode;
                // current coordinate if success
                int currCoord;
            };
            virtual CheckMotorResult checkMotor() = 0;
    };
}
#endif
