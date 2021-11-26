/**
 * @file gridvar.h
 * @brief Grid variation calculation for lark sensors.
 * Detailed information about WMM lib: https://www.ngdc.noaa.gov/geomag/WMM/soft.shtml
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

#ifndef GRIDVAR_H_10480910032019
#define GRIDVAR_H_10480910032019

extern "C" {
#include "GeomagnetismHeader.h"
}

#include <string>

/*
 * GridVariation singleton class
 *
 * This classs uses underlying WMM lib to perform calculation
 */
class GridVariation {
public:
   /*
    * public interface to get the singleton instance.
    *
    * @return a pointer to the singleton instance.
    */
    static GridVariation* GetInstance();

   /*
    * GridVariation destructor.
    *
    */
    ~GridVariation();

   /*
    * perform a grid variation calculation
    *
    * @param lon longitude
    * @param lat latitude
    * @param alt altitude
    * @param date date string in ddmmyy format
    *
    * @return grid variation
    */
    double Calc(double lon, double lat, double alt, const char* date);

   /*
    * get wwm lib version date
    *
    * @retrun date string
    */
    std::string VersionDate(void) const;

protected:

   /*
    * initialize the underlying lib prior accepting any calculation.
    *
    */
    void Initialize();

   /*
    * convert date string to decimal date
    * decimal dates are expected by WMM lib, such as 4 Jan 2019 is represented as: (2019.0+4.0/365.0)
    * this allows easy calculation of magentic field shifting over time, such as distributing the 0.1
    * degrees per year change to a certain date.
    *
    * @param data date string in ddmmyy format
    *
    * @return decimal date
    */
    double DecimalDate(const char* date) const;

    /// whether an instance is initiailized properly.
    bool initialized;

    /// version date of wwm lib
    std::string versionDate;

    /// please refer to wwm lib document for detail
    MAGtype_MagneticModel* magneticModels[1];

    /// please refer to wwm lib document for detail
    MAGtype_MagneticModel* timedMagneticModel;

    /// please refer to wwm lib document for detail
    MAGtype_Ellipsoid ellip;

    /// please refer to wwm lib document for detail
    MAGtype_Geoid geoid;

private:
   /*
    * GridVariation constructor.
    *
    */
    GridVariation();

    /// global var holding the singleton instance.
    static GridVariation* instance;
};

/// a macro definition to make calculation easier.
#define GridVarCal (GridVariation::GetInstance())->Calc

/// a macro definition to get WMM lib version
#define WMMVersion() ((GridVariation::GetInstance())->VersionDate())

#endif // GRIDVAR_H_10480910032019
