#ifndef __QMISTRING_HPP__
#define __QMISTRING_HPP__

/*
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
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

#include <common_v01.h>
#include <qmi_client.h>
#include <qualcomm_mobile_access_point_msgr_v01.h>

#include <memory>

#include <estd.hpp>
#include <estring.hpp>

namespace eqmi
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class StrResClientError : public estd::StrRes<qmi_client_error_type>
{
  public:
    StrResClientError();
};

class StrResQmiError : public estd::StrRes<qmi_error_type_v01>
{
  public:
    StrResQmiError();
};

class StrResQcMapMsgId : public estd::StrRes<unsigned int>
{
  public:
    StrResQcMapMsgId();
};

class StrResWdsMsgId : public estd::StrRes<unsigned int>
{
  public:
    StrResWdsMsgId();
};

class StrResWdsVerboseError : public estd::StrRes<unsigned int>
{
  public:
    using StrRes::StrRes;
};

class StrResWdsVerboseErrorMip : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorMip();
};

class StrResWdsVerboseErrorInternal : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorInternal();
};

class StrResWdsVerboseErrorCm : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorCm();
};

class StrResWdsVerboseError3gpp : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseError3gpp();
};

class StrResWdsVerboseErrorPpp : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorPpp();
};

class StrResWdsVerboseErrorEhrpd : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorEhrpd();
};

class StrResWdsVerboseErrorIpv6 : public StrResWdsVerboseError
{
  public:
    StrResWdsVerboseErrorIpv6();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class StrRes : public estd::Singleton<StrRes>
{
  public:
    StrResClientError clientQmiErrors;
    StrResQmiError qmiErrors;
    StrRes():clientQmiErrors(),qmiErrors(), qmiWdsVerboseError {
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_MOBILE_IP_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorMip()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_INTERNAL_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorInternal()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_CALL_MANAGER_DEFINED_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorCm()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_3GPP_SPEC_DEFINED_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseError3gpp()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_PPP_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorPpp()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_EHRPD_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorEhrpd()) },
        { QCMAP_MSGR_WWAN_CALL_END_TYPE_IPV6_V01, std::shared_ptr<StrResWdsVerboseError>(new StrResWdsVerboseErrorIpv6()) },

    }
    {
    }

    std::map<qcmap_msgr_wwan_call_end_type_enum_v01, std::shared_ptr<StrResWdsVerboseError>>  qmiWdsVerboseError;
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STR_RES_QMI_ERR(qmiErr) StrRes::getInstance().qmiErrors[qmiErr].c_str()
#define STR_RES_CLIENT_ERR(clientErr) StrRes::getInstance().clientQmiErrors[clientErr].c_str()
#define STR_RES_ERR(err) ((err <= 0) ? STR_RES_CLIENT_ERR(err) : STR_RES_QMI_ERR(static_cast<qmi_error_type_v01>(err)))
} // namespace eqmi

#endif
