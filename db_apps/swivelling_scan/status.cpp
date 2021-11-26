/*
 * Status definition
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
#include <sstream>
#include <jsoncons/json.hpp>
#include "status.hpp"

namespace swscan
{
    void Status::report()
    {
        std::ostringstream ss;
        jsoncons::encode_json(*this, ss);
        rdb.set(ss.str());
    }

    void Status::setStatus(Status::Mode st)
    {
        status = st;
        if (st != Status::Mode::Failed && st != Status::Mode::MovingDueToFailedScan) {
            // because there should be new error message to be set later
            setErrorStr("");
        }
        if (st == Status::Mode::Initialising
                || st == Status::Mode::SuccessFinal
                || st == Status::Mode::Failed
                || st == Status::Mode::None
                || st == Status::Mode::Stopped) {
            // do not call setMovingToPositionIndex(-1) here as user code may need
            // to getMovingToPositionIndex() to setCurrentPositionIndex;
            // user code needs to setMovingToPositionIndex(-1) if needed.
        }
        if (st != Status::Mode::SwivellingScan
                && st != Status::Mode::SwivellingScanMoving
                && st != Status::Mode::SwivellingScanAtPosition
                && st != Status::Mode::MovingToBestPosition) {
            setCurrentSwivellingScanStep(0);
            setMaxSwivellingScanStep(0);
        }
    }
}
