#ifndef __QMICSI_HPP__
#define __QMICSI_HPP__

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

#include <syslog.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <QCMAP_Client.h>
#pragma GCC diagnostic pop

#include <elogger.hpp>
#include <estdexcept.hpp>
#include <functional>
#include <estd.hpp>

#include "qmistrres.hpp"
#include "qmistruct.hpp"

namespace eqmi
{
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief runtime_error exception for eqmi
 *
 */
class runtime_error : public estd::runtime_error
{
  public:
    using estd::runtime_error::runtime_error;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// forward declare

static constexpr int defaultTimeOut = 60000;

template <typename TRequest, typename TResponse>
class QmiMessage;

class QmiClientInterface;

template <typename T>
class QmiClientBase;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class QmiClientInterface
{
  public:
    explicit QmiClientInterface(qmi_client_type clientHandle_) : clientHandle(clientHandle_), qmiOsParams() {}

    virtual std::string getMsgIdStr(unsigned int msgId) const = 0;

    QmiClientInterface(const QmiClientInterface &o) = default;
    QmiClientInterface &operator=(const QmiClientInterface &o) = default;

    virtual ~QmiClientInterface() = default;

  protected:
    QmiClientInterface() : clientHandle(nullptr), qmiOsParams() {}

    qmi_client_type clientHandle;
    qmi_cci_os_signal_type qmiOsParams;

    template <typename TRequest, typename TResponse>
    friend class QmiMessage;

    template <typename TRequest, typename TResponse>
    qmi_client_error_type sendMsg(unsigned int msgId, const TRequest &req, TResponse &resp, int timeOut) const
    {
        qmi_client_error_type retVal;
        retVal = qmi_client_send_msg_sync(clientHandle, msgId, const_cast<void *>(static_cast<const void *>(&req)), sizeof(req), &resp, sizeof(resp),
                                          timeOut);

#ifdef DEBUG_DUMP_QMI
        log(LOG_DEBUG, "resp (msgId=%s)\n%s", getMsgIdStr(msgId).c_str(), estd::dumpHex(reinterpret_cast<const char *>(&resp), sizeof(resp)).c_str());
#endif

        return retVal;
    }

    // TODO: implement sendMsgAsync
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
class QmiClientBase : public QmiClientInterface
{
  protected:
    QmiClientBase() : QmiClientInterface() {}

  public:
    template <int msgId>
    using reqType = typename RawQmiMessageType<T, msgId>::reqType;

    template <int msgId>
    using respType = typename RawQmiMessageType<T, msgId>::respType;

    template <int msgId>
    auto makeMsg(int msgId_ = msgId)
    {
        return QmiMessage<typename RawQmiMessageType<T, msgId>::reqType, typename RawQmiMessageType<T, msgId>::respType>(*this, msgId_);
    }

    explicit QmiClientBase(qmi_client_type clientHandle_) : QmiClientInterface(clientHandle_) {}

    // TODO: create a generic indication handler that replaces QcMapClient's
    // indication handler
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class QmiClientWds : public QmiClientBase<WdsRawQmiMessageType>
{
  public:
    QmiClientWds();
    ~QmiClientWds();

    std::string getMsgIdStr(unsigned int msgId) const override
    {
        return msgIds[msgId];
    }

  private:
    StrResWdsMsgId msgIds;
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class QmiClientQcMap : public QmiClientBase<QcMapRawQmiMessageType>
{
  public:
    explicit QmiClientQcMap(qmi_client_type clientHandle_) : QmiClientBase(clientHandle_), msgIds() {}

    std::string getMsgIdStr(unsigned int msgId) const override
    {
        return msgIds[msgId];
    }

  private:
    StrResQcMapMsgId msgIds;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class QcMapClient : public QCMAP_Client, public QmiClientQcMap
{
  public:
    typedef void client_status_ind_func_t(qmi_client_type, unsigned int, void *, unsigned int);
    typedef std::function<client_status_ind_func_t> client_status_ind_functor_t;

    // LE 1.0 (r00009.1a or older)
    template <typename T>
    using DetTraitQcMapClientCtorLe10 = decltype(T(std::declval<client_status_ind_t>(), std::declval<void*>()));
    template <typename T = QCMAP_Client, std::enable_if_t<estd::is_detected_v<DetTraitQcMapClientCtorLe10, T>, int> = 0>
    explicit QcMapClient(client_status_ind_functor_t indicationCallback_)
        : T(&invokeIndicationCallback, this), QmiClientQcMap(T::qmi_qcmap_msgr_handle), indicationCallback(std::move(indicationCallback_))
    {
      log(LOG_DEBUG, "QCMapClient is created for r00009.1a (qcMapClient=%p, indicationCallback=%p)", this, &indicationCallback);
    }

    // LE 1.2 (r00011.1 or newer)
    template <typename T>
    using DetTraitQcMapClientCtorLe12 = decltype(T(std::declval<client_status_ind_t>(), std::declval<qcmap_msgr_arch_type_enum_v01>(), std::declval<void*>()));
    template <typename T = QCMAP_Client, std::enable_if_t<estd::is_detected_v<DetTraitQcMapClientCtorLe12, T>, int> = 0>
    explicit QcMapClient(client_status_ind_functor_t indicationCallback_)
        : T(&invokeIndicationCallback, QCMAP_LEGACY_ARCH_V01, this), QmiClientQcMap(T::qmi_qcmap_msgr_handle), indicationCallback(std::move(indicationCallback_))
    {
      log(LOG_DEBUG, "QCMapClient is created for r00011.1 (qcMapClient=%p, indicationCallback=%p)", this, &indicationCallback);
    }


    template <typename TFunc, typename... TArgs>
    void call(TFunc func, TArgs... args)
    {
        qmi_error_type_v01 qmiError = QMI_ERR_NONE_V01;

        std::function<boolean(QcMapClient &, TArgs..., qmi_error_type_v01 *)> f(func);
        bool succ = f(*this, args..., &qmiError);
        log(LOG_DEBUG, "call '%s' (succ=%d,error=%s)", estd::demangleType(func).c_str(), succ, STR_RES_QMI_ERR(qmiError));

        if (!succ && (qmiError != QMI_ERR_NO_EFFECT_V01)) {
            throw runtime_error(estd::format("failed in %s (error=%s)", estd::demangleType(func).c_str(), STR_RES_QMI_ERR(qmiError)));
        }
    }

    template <typename TFunc, typename... TArgs>
    bool callSafe(TFunc func, TArgs... args) noexcept
    {
        qmi_error_type_v01 qmiError = QMI_ERR_NONE_V01;

        std::function<boolean(QcMapClient &, TArgs..., qmi_error_type_v01 *)> f(func);
        bool succ = f(*this, args..., &qmiError);
        log(LOG_DEBUG, "call '%s' (succ=%d,error=%s)", estd::demangleType(func).c_str(), succ, STR_RES_QMI_ERR(qmiError));

        if (!succ && (qmiError != QMI_ERR_NO_EFFECT_V01)) {
            log(LOG_INFO, "failed in %s (error=%s)", estd::demangleType(func).c_str(), STR_RES_QMI_ERR(qmiError));
        }

        return succ;
    }

    void EnableMobileAP();
    void RegisterForIndications(uint64_t indRegMask);

    boolean SetVLANConfig(const qcmap_msgr_vlan_config_v01 &vlan_config, bool *is_ipa_offloaded, qmi_error_type_v01 *qmi_err_num)
    {
        return QCMAP_Client::SetVLANConfig(vlan_config, qmi_err_num, is_ipa_offloaded);
    }

    void logLanConfig(estd::StringView desc, int vlanId, const qcmap_msgr_lan_config_v01 &lanConf);
    qcmap_msgr_lan_config_v01 getLanConfig(int vlanId);
    void setLanConfig(int vlanId, const qcmap_msgr_lan_config_v01 &lanConf);

  protected:
    static void invokeIndicationCallback(qmi_client_type user_handle, unsigned int msg_id, void *ind_buf, unsigned int ind_buf_len,
                                         void *ind_cb_data);


  private:
    client_status_ind_functor_t indicationCallback;
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TRequest, typename TResponse>
class QmiMessage
{
  public:
    QmiClientInterface &qmiClient;

    int msgId;
    TRequest req;
    TResponse resp;
    qmi_client_error_type error;

    // TODO: error may need to be an error code rather than ()
    QmiMessage(QmiClientInterface &qmiClient_, int msgId_) : qmiClient(qmiClient_), msgId(msgId_), req(), resp(), error() {}

    bool sendSafe(int timeOut = defaultTimeOut) noexcept
    {
        error = qmiClient.sendMsg<TRequest, TResponse>(msgId, req, resp, timeOut);

        log(LOG_DEBUG, "sendSafe %s (error=%s)", getMsgIdStr().c_str(), getErrorStr().c_str());

        return error == QMI_NO_ERR;
    }

    void send(int timeOut = defaultTimeOut)
    {
        error = qmiClient.sendMsg<TRequest, TResponse>(msgId, req, resp, timeOut);

        log(LOG_DEBUG, "send %s (error=%s)", getMsgIdStr().c_str(), getErrorStr().c_str());

        if (error != QMI_NO_ERR) {
            throw runtime_error(estd::format("failed to send (msgId=%s,error=%s)", getMsgIdStr().c_str(), getErrorStr().c_str()));
        }
    }

    std::string getMsgIdStr()
    {
        return qmiClient.getMsgIdStr(msgId);
    }

    std::string getErrorStr()
    {
        return STR_RES_ERR(error);
    }
};
} // namespace eqmi
#endif
