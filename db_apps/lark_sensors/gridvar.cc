/**
 * @file gridvar.cc
 * @brief Implementation of grid variation calculation for lark sensors.
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gridvar.h"
#include "EGM9615.h"
#include <cstring>
#include "log.h"

GridVariation* GridVariation::instance = nullptr;

GridVariation* GridVariation::GetInstance()
{
    if(!instance) {
        return instance = new GridVariation();
    }
    return instance;
}

GridVariation::GridVariation()
    :initialized(false)
{
    Initialize();
}

GridVariation::~GridVariation()
{
    MAG_FreeMagneticModelMemory(timedMagneticModel);
    MAG_FreeMagneticModelMemory(magneticModels[0]);
}

void GridVariation::Initialize()
{
    // get lib release date
    char buffer[16];
    strncpy(buffer, VERSIONDATE_LARGE + 39, 11);
    buffer[11] = '\0';
    versionDate = buffer;

    sprintf(buffer, "%s", "/tmp/WMM.COF");

    // load coefficient file
    if(!MAG_robustReadMagModels(buffer, &magneticModels, 1)) {
        LS_ERROR("Can't load coefficient file - %s", buffer);
        return;
    }

    int max = 0;
    if(max < magneticModels[0]->nMax) {
        max = magneticModels[0]->nMax;
    }

    // For storing the time modified WMM Model parameters
    int terms = ((max + 1) * (max + 2) / 2);
    timedMagneticModel = MAG_AllocateModelMemory(terms);
    if(!timedMagneticModel) {
        LS_ERROR("%s", "Can't allocate memory for WMM model parameters");
        return;
    }

    MAG_SetDefaults(&ellip, &geoid);

    /* Set EGM96 Geoid parameters */
    geoid.GeoidHeightBuffer = GeoidHeightBuffer;
    geoid.Geoid_Initialized = 1;

    initialized = true;
}

double GridVariation::Calc(double lon, double lat, double alt, const char* date)
{
    if(!initialized) {
        LS_ERROR("%s", "WMM lib not initialized");
        return 0.0;
    }

    MAGtype_CoordSpherical coordSpherical;
    MAGtype_CoordGeodetic coordGeodetic;
    MAGtype_Date userDate;
    MAGtype_GeoMagneticElements geoMagneticElements, errors;

    memset(&coordSpherical, 0, sizeof(coordSpherical));
    memset(&coordGeodetic, 0, sizeof(coordGeodetic));
    memset(&userDate, 0, sizeof(userDate));
    memset(&geoMagneticElements, 0, sizeof(geoMagneticElements));
    memset(&errors, 0, sizeof(errors));

    coordGeodetic.lambda = lon;
    coordGeodetic.phi = lat;
    coordGeodetic.HeightAboveGeoid = alt;
    userDate.DecimalYear = DecimalDate(date);

    // Convert from geodetic to Spherical Equations: 17-18, WMM Technical report
    MAG_GeodeticToSpherical(ellip, coordGeodetic, &coordSpherical);

    // Time adjust the coefficients, Equation 19, WMM Technical report
    MAG_TimelyModifyMagneticModel(userDate, magneticModels[0], timedMagneticModel);

    // Computes the geoMagnetic field elements and their time change
    MAG_Geomag(ellip, coordSpherical, coordGeodetic, timedMagneticModel, &geoMagneticElements);

    MAG_CalculateGridVariation(coordGeodetic, &geoMagneticElements);

    MAG_WMMErrorCalc(geoMagneticElements.H, &errors);

    return geoMagneticElements.Decl;
}

std::string GridVariation::VersionDate(void) const
{
    if(initialized) {
        return versionDate;
    }

    return "unknown(uninitialized)";
}

double GridVariation::DecimalDate(const char* date) const
{
    if(6 != strlen(date)) {
        LS_ERROR("Wrong date format: %s", date);
        return 2019.0;
    }

    int nday = 10 * (date[0] - '0') + date[1] - '0';
    int nmonth = 10 * (date[2] - '0') + date[3] - '0';
    int nyear = 2000 + 10 * (date[4] - '0') + date[5] - '0';

    // first day of month
    int fdom_normal[] = {1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
    int fdom_leap[] =   {1, 32, 61, 92, 122, 153, 183, 214, 245, 275, 306, 336};

    bool is_leap = (0 == nyear%400) || (0 == nyear%4 && 0 != nyear%100);
    int *pfdom = is_leap?fdom_leap:fdom_normal;

    // day of year
    int doy = pfdom[nmonth - 1] + nday -1;

    return nyear + doy/(is_leap?366.0:365.0);
}
