/*
 * ookla speed test calculation for bellca.
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

#include "Calculation.h"

namespace speedtest
{
    inline SpeedTestExperience CalculateExperience(double bandwidthMbps)
    {
        if (bandwidthMbps >= 200)
            return SpeedTestExperience::Excellent;
        else if (bandwidthMbps >= 100)
            return SpeedTestExperience::VeryGood;
        else if (bandwidthMbps >= 80)
            return SpeedTestExperience::Good;
        else if (bandwidthMbps >= 50)
            return SpeedTestExperience::Fair;
        else
            return SpeedTestExperience::Poor;
    }

    SpeedTestExperience CalculateDownloadExperience(double bandwidthMbps) {
        return CalculateExperience(bandwidthMbps);
    }

    SpeedTestExperience CalculateUploadExperience(double bandwidthMbps) {
        return CalculateExperience(bandwidthMbps);
    }

} // namespace speedtest
