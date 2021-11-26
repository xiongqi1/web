/* qualcomm */
#include "qmi_client.h"
#include "qmi_platform.h"

#ifdef V_PROCESSOR_sdx20
#include "device_management_service_v01.h"
#include "network_access_service_v01.h"
#include "user_identity_module_v01.h"
#include "wireless_data_service_v01.h"
#include "wireless_messaging_service_v01.h"

#elif defined(V_PROCESSOR_sdx55)

/* SDx55 is identical to MDM9x50/MDM9x40 except SDx55 does not have netcomm service, yet */
#include "qmi_cci_target.h"
#include "qmi_idl_lib.h"
#include "qmi_cci_common.h"

#include "qmi_port_defs.h"
#include "card_application_toolkit_v02.h"
#include "control_service_v01.h"
#include "data_filter_service_v01.h"
#include "data_port_mapper_v01.h"
#include "data_system_determination_v01.h"
#include "device_management_service_v01.h"
#include "lowi_service_v01.h"
#include "network_access_service_v01.h"
#include "persistent_device_configuration_v01.h"

#ifndef V_QMI_PBM_none
#include "phonebook_manager_service_v01.h"
#endif

#include "radio_frequency_radiated_performance_enhancement_v01.h"
#include "specific_absorption_rate_v01.h"
#include "user_identity_module_remote_v01.h"
#include "user_identity_module_v01.h"

#ifndef V_QMI_VOICE_none
#include "voice_service_v02.h"
#endif

#include "wireless_data_administrative_service_v01.h"
#include "wireless_data_service_v01.h"

#ifndef V_QMI_WMS_none
#include "wireless_messaging_service_v01.h"
#endif

#include "thermal_mitigation_device_service_v01.h"
#include "coexistence_service_v01.h"

#ifndef V_QMI_CSD_none
#include "core_sound_driver_v01.h"
#endif

#include "location_service_v02.h"

#ifndef V_QMI_IMS_none
#include "ip_multimedia_subsystem_application_v01.h"
#include "ip_multimedia_subsystem_settings_v01.h"
#include "ip_multimedia_subsystem_presence_v01.h"
#endif

#include "subsystem_control_v01.h"

#ifdef V_EXTRA_NAND_FLASH_SCRUB_qualcomm
#include "flash_driver_service_v01.h"
#endif

#include "mmgsdilib_common.h"

/* defines for MDM9x40 and MDM9x50 */
#else
#include "qmi_cci_target.h"
#include "qmi_idl_lib.h"
#include "qmi_cci_common.h"

#include "qmi_port_defs.h"
/* from mdm9640-le-2-0_amss_oem_standard/apps_proc/oe-core/build/tmp-eglibc/sysroots/mdm9640/usr/include */
#include "card_application_toolkit_v02.h"
#include "control_service_v01.h"
#include "data_filter_service_v01.h"
#include "data_port_mapper_v01.h"
#include "data_system_determination_v01.h"
#include "device_management_service_v01.h"
#include "lowi_service_v01.h"
#include "network_access_service_v01.h"
#include "persistent_device_configuration_v01.h"

#ifndef V_QMI_PBM_none
#include "phonebook_manager_service_v01.h"
#endif

#include "radio_frequency_radiated_performance_enhancement_v01.h"
#include "specific_absorption_rate_v01.h"
#include "user_identity_module_remote_v01.h"
#include "user_identity_module_v01.h"

#ifndef V_QMI_VOICE_none
#include "voice_service_v02.h"
#endif

#include "wireless_data_administrative_service_v01.h"
#include "wireless_data_service_v01.h"
#include "wireless_messaging_service_v01.h"
#include "thermal_mitigation_device_service_v01.h"

#include "coexistence_service_v01.h"

#ifndef V_QMI_CSD_none
#include "core_sound_driver_v01.h"
#endif

#include "location_service_v02.h"

#ifndef V_QMI_IMS_none
#include "ip_multimedia_subsystem_application_v01.h"
#include "ip_multimedia_subsystem_settings_v01.h"
#include "ip_multimedia_subsystem_presence_v01.h"
#endif

#include "subsystem_control_v01.h"

/* include flash driver header if Qualcomm scrub is used */
#ifdef V_EXTRA_NAND_FLASH_SCRUB_qualcomm
#include "flash_driver_service_v01.h"
#endif

#include "mmgsdilib_common.h"

#include "netcomm_v01.h"

#endif

#ifdef V_PROCESSOR_sdx20

#else
/* QMI LOC fixup */
typedef struct {
	qmi_response_type_v01 resp;
} qmiLocRespMsgT_v02;

typedef qmiLocRespMsgT_v02 qmiLocStartRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocRegEventsRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocGetOperationModeRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocGetEngineLockRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocSetOperationModeRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocSetProtocolConfigParametersRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocGetProtocolConfigParametersRespMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocInjectWifiApDataRespMsgT_v02;

/* QMI loc fixup for SUPL NI */
typedef qmiLocNiUserRespReqMsgT_v02 qmiLocNiUserResponseReqMsgT_v02;
typedef qmiLocRespMsgT_v02 qmiLocNiUserResponseRespMsgT_v02;
typedef qmiLocNiUserRespIndMsgT_v02 qmiLocNiUserResponseIndMsgT_v02;

/* QMI IMS fixup */
typedef struct {
	char __placeholder;
} ims_req_msg_v01;

#ifndef V_QMI_IMS_none
typedef ims_req_msg_v01 imsp_get_enabler_state_req_msg_v01;
typedef imsa_ind_reg_rsp_msg_v01 imsa_ind_reg_resp_msg_v01;
#endif

typedef struct {
} tmd_get_mitigation_device_list_req_msg_v01;

#ifndef V_PROCESSOR_mdm9x50
typedef struct {
} qmi_ssctl_shutdown_req_msg_v01;
#endif

typedef struct {
} qmi_ssctl_shutdown_req_msg_v02;

typedef struct {
  qmi_response_type_v01 resp;
} qmi_ssctl_shutdown_resp_msg_v02;

#include "qmi_extra.h"

#endif

/*

	* qmi_idl_service_object_type - pointer as a qmi service handle for QMI service list

		typedef struct qmi_idl_service_object *qmi_idl_service_object_type;

		struct qmi_idl_service_object {
		uint32_t library_version;
		uint32_t idl_version;
		uint32_t service_id;
		uint32_t max_msg_len;
		uint16_t n_msgs[QMI_IDL_NUM_MSG_TYPES];
		const qmi_idl_service_message_table_entry *msgid_to_msg[QMI_IDL_NUM_MSG_TYPES];
		const qmi_idl_type_table_object *p_type_table;
		uint32_t idl_minor_version;
		struct qmi_idl_service_object *parent_service_obj;
		};

	* qmi_client_type - pointer as a qmi client handle for QMI send or recv

		typedef struct qmi_client_struct *qmi_client_type;

		struct qmi_client_struct {
		int service_user_handle;
		qmi_idl_service_object_type p_service;
		}qmi_client_struct_type;

	* qmi_client_os_params - qmi signal structure

		typedef struct {
		uint32_t sig_set;
		uint32_t timed_out;
		pthread_cond_t cond;
		pthread_mutex_t mutex;
		} qmi_cci_os_signal_type;

		typedef qmi_cci_os_signal_type qmi_client_os_params;

	* error type - error code

		typedef int            qmi_client_error_type;

	* qmi_txn_handle - transaction handle

		typedef void           *qmi_txn_handle;

*/

struct luaqmi_t;

struct luaqmi_t* luaqmi_new(int serv_id, qmi_idl_service_object_type shndl, int timeout);
void luaqmi_delete(struct luaqmi_t* lq);

int luaqmi_get_async_fd(struct luaqmi_t* lq);
void luaqmi_update_flag(struct luaqmi_t* lq);

qmi_client_error_type luaqmi_send_msg(struct luaqmi_t* lq, unsigned int msg_id, void *req, unsigned int req_len, void *resp, unsigned int resp_len, unsigned int timeout_msecs);
qmi_client_error_type luaqmi_recv_msg_async(struct luaqmi_t* lq, unsigned int* msg_id, void *resp, unsigned int* resp_len, qmi_client_error_type* transp_err, int* ind, int* ref);
int luaqmi_remove_msg_async(struct luaqmi_t* lq);
qmi_client_error_type luaqmi_cancel_msg_async(struct luaqmi_t* lq, void** te_app_ptr);
qmi_client_error_type luaqmi_send_msg_async(struct luaqmi_t* lq, unsigned int msg_id, void *req, unsigned int req_len, unsigned int resp_len, void** te_app_ptr, int ref);
