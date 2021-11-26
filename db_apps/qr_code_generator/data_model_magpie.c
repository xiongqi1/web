/*
 * Implementing Magpie data model
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "data_model.h"
#include "print_msg.h"
#include "json_data.h"
#include "print_json.h"
#include "qr_code_image.h"
#include "rdb_helper.h"
#include "data_build_helper.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#ifdef V_TITAN_INSTALLATION_ASSISTANT_CONN_magpie_nitv2
#define RDB_SW_VERSION "owa.sw.version"
#define RDB_HW_VERSION "owa.system.product.hwver"
#else
#define RDB_SW_VERSION "sw.version"
#define RDB_HW_VERSION "system.product.hwver"
#endif

static const char *arg_date_time;

/*
 * Build ECGI filter property
 * @param json_obj JSON root object to add property
 * @return 0 on success, negative code otherwise
 */
static int build_ecgi_filter(json_object_t *json_obj)
{
    int ret = -ENOMEM;

    char rdb_name[64];
    // maximum 3 ECGI
#define ECGI_FILTER_BUF_LEN 192
    char *ecgi_filter = calloc(1, ECGI_FILTER_BUF_LEN);
    int ecgi_filter_buf_pos = 0;
    int printed_ret;

    const char *rdb_value;

    if (!ecgi_filter) {
        return ret;
    }
#define MAX_USER_CELLS 9
    for (int i = 0; i < MAX_USER_CELLS; i++) {
        sprintf(rdb_name, "installation.cell_filter.%d.ecgi", i);
        if ((rdb_value = get_rdb(rdb_name)) && rdb_value[0] != 0) {
            int free_bytes_num = ECGI_FILTER_BUF_LEN - ecgi_filter_buf_pos;
            printed_ret = snprintf(ecgi_filter + ecgi_filter_buf_pos, free_bytes_num,
                            "%s%s", ecgi_filter_buf_pos ? "," : "", rdb_value);
            if (printed_ret > 0 && printed_ret < free_bytes_num) {
                ecgi_filter_buf_pos += printed_ret;
            } else {
                free(ecgi_filter);
                return ret;
            }
        }
    }

    ret = build_json_string_prop(json_obj, "ECGIFilter", ecgi_filter);

    free(ecgi_filter);
    return ret;
}


/*
 * Build detected cell list property
 * @param json_container_obj JSON root object to add property
 * @return 0 on success, negative code otherwise
 */
static int build_detected_cell_list(json_object_t *json_container_obj)
{
    int ret = -ENOMEM;
    char *rdb_value;
    char prop_name[32];

    // format of installation.cell_rf_stats: ecgi1,max_rsrp,max_rsrq,max_rssi;ecgi2,max_rsrp,max_rsrq,max_rssinr
    if ((rdb_value = get_rdb("installation.cell_rf_stats")) && rdb_value[0] != 0) {
        char *pstring, *pstring_i;
        char *token, *token_i;
        int cell_index = 0;

        pstring = rdb_value;
        // QR_Code_Data_Fields_Draft_0.4.xlsx specifies ECGI00 to ECGI09 i.e maximum 10 cells
        while ((token = strsep(&pstring, ";")) != NULL && cell_index <= 9){
            int i = 0;

            json_object_t *json_obj_i = new_json_object();
            if (!json_obj_i) {
                return ret;
            }

            pstring_i = token;

            while ((token_i = strsep(&pstring_i, ",")) != NULL && i <= 3){
                json_object_property_t *json_prop_i = NULL;
                const char *prop_name_i;

                switch (i) {
                    case 0:
                        prop_name_i = "ECGI";
                        break;
                    case 1:
                        prop_name_i = "Rsrpmax";
                        break;
                    case 2:
                        prop_name_i = "Rsrqmax";
                        break;
                    case 3:
                        prop_name_i = "Rssinrmax";
                        break;
                }

                json_prop_i = new_json_object_property(prop_name_i, STRING);
                if (json_prop_i && !(ret = set_json_value(json_prop_i->value, token_i))) {
                    add_json_property(json_obj_i, json_prop_i);
                } else {
                    delete_json_object_property(json_prop_i);
                    delete_json_object(json_obj_i);
                    return ret;
                }

                i++;
            }

            json_object_property_t *json_prop = NULL;
            sprintf(prop_name, "ECGI%02d", cell_index);
            json_prop = new_json_object_property(prop_name, OBJECT);
            if (json_prop && !(ret = set_json_value(json_prop->value, json_obj_i))) {
                add_json_property(json_container_obj, json_prop);
            } else {
                delete_json_object(json_obj_i);
                delete_json_object_property(json_prop);
                return ret;
            }
            cell_index++;

        }

    }
    return 0;
}

/*
 * Build RLF failures property
 * @param json_container_obj JSON root object to add property
 * @return 0 on success, negative code otherwise
 */
static int rlf_failures(json_object_t *json_container_obj)
{
    int ret = -ENOMEM;
    int i = 0;
    char rdb_name[64];
    char *rdb_value, *failure_cause;
    char *endptr;
    long long int count_val;

    json_array_t *json_arr = new_json_array();
    if (!json_arr) {
        print_err("Failed to create new JSON array.\n");
        return ret;
    }
    print_debug("Processing RLF failures RDB variables\n");
    while (sprintf(rdb_name, "wwan.0.servcell_info.rlf_failures.%d.failure_cause", i) > 0
        && (rdb_value = get_rdb(rdb_name))) {
        print_debug("Read %s = %s\n", rdb_name, rdb_value);
        if (!(failure_cause = strdup(rdb_value))) {
            print_err("Failed to copy rdb value %s.\n", rdb_value);
            return ret;
        }

        sprintf(rdb_name, "wwan.0.servcell_info.rlf_failures.%d.count", i);
        if ((rdb_value = get_rdb(rdb_name))) {
            print_debug("Read %s = %s\n", rdb_name, rdb_value);
            errno = 0;
            count_val = strtoll(rdb_value, &endptr, 10);
            if (errno == 0 && *endptr == '\0' && endptr != rdb_value && count_val > 0){
                print_debug("count = %ld\n", count_val);
                // count > 0, insert to array
                json_object_t *json_obj = new_json_object();
                if (json_obj) {
                     json_object_property_t *json_prop;
                     if ((json_prop = new_json_object_property("Count", NUMBER))
                             && !(ret = set_json_value(json_prop->value, &count_val))) {
                         add_json_property(json_obj, json_prop);
                     } else {
                         delete_json_object_property(json_prop);
                         delete_json_object(json_obj);
                         delete_json_array(json_arr);
                         print_err("Failed to set the property Count.\n");
                         return ret;
                     }

                     if ((json_prop = new_json_object_property("FailureCause", STRING))
                             && !(ret = set_json_value(json_prop->value, failure_cause))) {
                         add_json_property(json_obj, json_prop);
                     } else {
                         delete_json_object_property(json_prop);
                         delete_json_object(json_obj);
                         delete_json_array(json_arr);
                         print_err("Failed to set the property FailureCause.\n");
                         return ret;
                     }

                     json_array_element_t *json_array_el = new_json_array_element(OBJECT);
                     if (json_array_el && !(ret = set_json_value(json_array_el->value, json_obj))) {
                         add_json_element(json_arr, json_array_el);
                     } else {
                         delete_json_object(json_obj);
                         delete_json_array_element(json_array_el);
                         delete_json_array(json_arr);
                         print_err("Failed to add array element.\n");
                         return ret;
                     }

                 } else {
                    print_err("Unable to create new JSON object\n");
                    free(failure_cause);
                    delete_json_array(json_arr);
                    return ret;
                 }
            }
        }
        free(failure_cause);
        i++;
    }

    json_object_property_t *json_prop = new_json_object_property("RLFFailures", ARRAY);
    if (json_prop) {
        set_json_value(json_prop->value, json_arr);
        add_json_property(json_container_obj, json_prop);
    } else {
        delete_json_array(json_arr);
        print_err("Failed to add property RLFFailures.\n");
        return ret;
    }

    return 0;
}

/*
 * Build speed test property
 * @param json_container_obj JSON root object to add property
 * @return 0 on success, negative code otherwise
 */
static int speed_test_result(json_object_t *json_container_obj)
{
#define SPEED_TEST_PREFIX "service.ttest.ftp"
    int ret = 0;
    int test_type;
    char rdb_name[64];
    char *speed_result_str, *is_dload, *dl_speed = NULL, *ul_speed = NULL;
    bool is_dl_speed;

    for (test_type = 0; test_type < 2; test_type++) {
        sprintf(rdb_name, SPEED_TEST_PREFIX ".%d.is_dload", test_type);
        is_dload = get_rdb(rdb_name);
        if (is_dload && !strcmp(is_dload, "1")) {
            // download
            is_dl_speed = true;
        } else {
            // upload
            is_dl_speed = false;
        }
        sprintf(rdb_name, SPEED_TEST_PREFIX ".%d.avg_speed_mbps", test_type);
        if ((speed_result_str = get_rdb(rdb_name))) {
            if (is_dl_speed) {
                free(dl_speed);
                dl_speed = strdup(speed_result_str);
            } else {
                free(ul_speed);
                ul_speed = strdup(speed_result_str);
            }
        }
    }

    if (dl_speed || ul_speed) {
        rdb_data_build_t rdb_data_set[] = {
            {"SpeedECGI", "wwan.0.system_network_status.ECGI", false, NULL, STRING},
            {"SpeedAPN", "link.profile.1.apn", false, NULL, STRING}
        };

        if ((ret = process_rdb_data_build_set(json_container_obj, rdb_data_set,
                sizeof(rdb_data_set)/sizeof(rdb_data_build_t)))
                || (dl_speed && (ret = build_json_string_prop(json_container_obj,
                        "DLTpt",dl_speed)))
                || (ul_speed && (ret = build_json_string_prop(json_container_obj,
                        "UPTpt", ul_speed)))) {
            free(dl_speed);
            free(ul_speed);
            return ret;
        }

        free(dl_speed);
        free(ul_speed);
    }

    return ret;
}

/*
 * Build SAS data property
 * @param json_container_obj JSON root object to add property
 * @return 0 on success, negative code otherwise
 */
static int sas_data(json_object_t *json_container_obj)
{
    int ret;
    char *rdb_value, *reg_state, *grant_state;

    rdb_value = get_rdb("sas.cbsdid");
    if (rdb_value) {
        if ((ret = build_json_string_prop(json_container_obj, "CBSDID", rdb_value))) {
            return ret;
        }

        if ((ret = build_json_prop_from_rdb(json_container_obj, "GrantState", "sas.grant.0.state",
                false, NULL, STRING))) {
            return ret;
        }

        if ((ret = build_json_string_prop(json_container_obj, "GrantRequiredFor", "ServingCell"))) {
            return ret;
        }

        rdb_data_build_t rdb_data_set[] = {
            {"FreqRangeLow", "sas.grant.0.freq_range_low", false, NULL, NUMBER},
            {"FreqRangeHigh", "sas.grant.0.freq_range_high", false, NULL, NUMBER},
            {"MaxEIRP", "sas.grant.0.max_eirp", false, NULL, NUMBER},
            {"CellList", "sas.grant.0.ecgi_list", false, NULL, STRING}
        };

        if ((ret = process_rdb_data_build_set(json_container_obj, rdb_data_set, sizeof(rdb_data_set)/sizeof(rdb_data_build_t)))) {
            return ret;
        }
    }
    // SAS error code field
    // If not registered, look at sas.registration.response_code
    // If Registered, but not Grant Authorized, look at sas.grant.0.reason
    // If Registered and Grant Authorized, do not include this field to QR code.
    reg_state = get_rdb("sas.registration.state");
    if (reg_state && strcmp(reg_state, "Registered")) {
        if ((ret = build_json_prop_from_rdb(json_container_obj, "RegRespCode",
                        "sas.registration.response_code", false, NULL, NUMBER))) {
            return ret;
        }
    } else {
        grant_state = get_rdb("sas.grant.0.state");
        if (!grant_state || strcmp(grant_state, "AUTHORIZED")) {
            if ((ret = build_json_prop_from_rdb(json_container_obj, "RegRespCode",
                            "sas.grant.0.reason", false, NULL, NUMBER))) {
                return ret;
            }
        }
    }
    return 0;
}

/*
 * Implementing init function of data model
 */
static int init(int argc, char *argv[])
{
    int rval = -EINVAL;

    if (argc < 1) {
        print_err("Need date time argument");
        return rval;
    }

    arg_date_time = argv[0];

    rval = init_rdb();
    if (rval) {
        deinit_rdb();
        print_err("Unable to open rdb");
        return rval;
    }

    rval = init_json_print(MAX_QR_TEXT_LENGTH);
    if (rval) {
        print_err("Failed to initialise JSON buffer.");
        deinit_rdb();
    }

    return  0;
}

/*
 * Implementing deinit function of data model
 */
static void deinit()
{
    deinit_rdb();
    deinit_json_print();
}

/*
 * Implementing get_data function of data model
 */
static const char* get_data()
{

    int ret = -1;

    json_value_t *json_root_v_obj = new_json_value(OBJECT);
    if (!json_root_v_obj) {
        return NULL;
    }
    json_object_t *json_root_obj = (json_object_t *)(json_root_v_obj->value);

    if ((ret = build_json_string_prop(json_root_obj, "Ver", "1.0"))) {
        return NULL;
    }

    rdb_data_build_t rdb_data_set_1[] = {
        {"BAN", "installation.customer_ban", true, NULL, STRING},
        {"IMEI", "wwan.0.imei", false, "", STRING},
        {"HwVer", RDB_HW_VERSION, false, "", STRING},
        {"FwVer", RDB_SW_VERSION, false, "", STRING}
    };

    if ((ret = process_rdb_data_build_set(json_root_obj, rdb_data_set_1, sizeof(rdb_data_set_1)/sizeof(rdb_data_build_t)))) {
        return NULL;
    }

    if ((ret = build_json_string_prop(json_root_obj, "Origin", "GPS"))) {
        return NULL;
    }

    const char *gps_status = get_rdb("sensors.gps.0.common.status");
    if (gps_status && !strcmp(gps_status, "success")) {
        rdb_data_build_t rdb_data_set_gps[] = {
            {"Latitude", "sensors.gps.0.common.latitude_degrees", false, "", STRING},
            {"Longitude", "sensors.gps.0.common.longitude_degrees", false, "", STRING},
            {"Altitude", "sensors.gps.0.common.height_of_geoid", false, "", STRING},
            {"Accuracy", "sensors.gps.0.common.pdop", false, "", STRING}
        };
        if ((ret = process_rdb_data_build_set(json_root_obj, rdb_data_set_gps,
                sizeof(rdb_data_set_gps)/sizeof(rdb_data_build_t)))) {
            return NULL;
        }
    }

    const char *orientation_status = get_rdb("owa.orien.status");
    if (orientation_status && !strcmp(orientation_status, "0")) {
        rdb_data_build_t rdb_data_set_orientation[] = {
            {"AntennaAzimuth", "owa.orien.azimuth", false, "", STRING},
            {"AntennaDowntilt", "owa.orien.tilt", false, "", STRING}
        };
        if ((ret = process_rdb_data_build_set(json_root_obj, rdb_data_set_orientation,
                sizeof(rdb_data_set_orientation)/sizeof(rdb_data_build_t)))) {
            return NULL;
        }
    } else if (orientation_status && !strcmp(orientation_status, "2")) {
        rdb_data_build_t rdb_data_set_orientation[] = {
            {"AntennaDowntilt", "owa.orien.tilt", false, "", STRING}
        };
        if ((ret = process_rdb_data_build_set(json_root_obj, rdb_data_set_orientation,
                sizeof(rdb_data_set_orientation)/sizeof(rdb_data_build_t)))) {
            return NULL;
        }
    }

    if ((ret = build_json_string_prop(json_root_obj, "DateTime", arg_date_time))) {
        return NULL;
    }

    if ((ret = build_ecgi_filter(json_root_obj))) {
        return NULL;
    }

    if ((ret = build_detected_cell_list(json_root_obj))) {
        return NULL;
    }

    rdb_data_build_t rdb_data_set_3[] = {
        {"PLMNID", "wwan.0.system_network_status.PLMN", false, "", STRING},
        {"CellID", "wwan.0.system_network_status.CellID", false, NULL, STRING},
        {"FreqBandIndicator", "wwan.0.radio_stack.e_utra_measurement_report.freqbandind", false, NULL, STRING},
        {"DLBandwidth", "wwan.0.radio_stack.e_utra_measurement_report.dl_bandwidth", false, NULL, STRING},
        {"ULBandwidth", "wwan.0.radio_stack.e_utra_measurement_report.ul_bandwidth", false, NULL, STRING},
        {"RSRP", "wwan.0.signal.0.rsrp", false, NULL, STRING},
        {"RSRQ", "wwan.0.signal.rsrq", false, NULL, STRING},
        {"RSSI", "wwan.0.signal.rssi", false, NULL, STRING},
        {"RSSINR", "wwan.0.signal.snr", false, NULL, STRING},
        {"RSTxPower", "wwan.0.system_network_status.RSTxPower", false, NULL, STRING},
        {"PUSCHTx", "wwan.0.radio_stack.e_utra_pusch_transmission_status.total_pusch_txpower", false, NULL, STRING},
        {"AttachAttempts", "wwan.0.pdpcontext.attach_attempts", false, "", STRING},
        {"AttachFailures", "wwan.0.pdpcontext.attach_failures", false, "", STRING}
    };

    if ((ret = process_rdb_data_build_set(json_root_obj, rdb_data_set_3, sizeof(rdb_data_set_3)/sizeof(rdb_data_build_t)))) {
        return NULL;
    }

    if ((ret = rlf_failures(json_root_obj))) {
        print_err("Failed to get data for RLF failures\n");
        return NULL;
    }

    if ((ret = speed_test_result(json_root_obj))) {
        print_err("Failed to get data for speed test\n");
        return NULL;
    }

    if ((ret = sas_data(json_root_obj))) {
        print_err("Failed to get SAS data\n");
        return NULL;
    }

    if ((ret = print_json_value(json_root_v_obj))) {
        print_err("Failed to print JSON\n");
        return NULL;
    }

    return get_json_printed_text();
}

/*
 * Implementing new_data_model function
 */
data_model_t *new_data_model()
{
    data_model_t *data_model = calloc(1, sizeof(data_model_t));
    if (data_model) {
        data_model->arg_number = 1;
        data_model->guide = "\tDateTime\t\tGiven date time";
        data_model->init = init;
        data_model->deinit = deinit;
        data_model->get_data = get_data;
    }
    return data_model;
}

/*
 * Implementing delete_data_model function
 */
void delete_data_model(data_model_t *data_model)
{
    free(data_model);
}
