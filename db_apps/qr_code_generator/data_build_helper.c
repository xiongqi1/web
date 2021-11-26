/*
 * Implementing helper functions to build data
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

#include "data_build_helper.h"
#include "json_data.h"
#include "rdb_helper.h"
#include "print_msg.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * See data_build_helper.h
 */
int build_json_prop_from_rdb(json_object_t *json_obj, const char *prop_name,
        const char *rdb_name, bool must_exist, const char *default_value,
        json_value_type_t data_type)
{
    int ret = -EINVAL;
    const char *rdb_value;

    if (!json_obj || !prop_name || !rdb_name) {
        return ret;
    }

    rdb_value = get_rdb(rdb_name);
    if (!rdb_value && must_exist) {
        print_err("RDB %s must exist\n", rdb_name);
        return ret;
    }
    if (!rdb_value && default_value) {
        rdb_value = default_value;
    }
    if (rdb_value) {
        void *value = NULL;
        char *endptr;
        long long int val;
        double value_double;
        bool value_bool;

        switch (data_type) {
        case STRING:
            value = (void *)rdb_value;
            break;
        case NUMBER:
            if (rdb_value[0] != '\0') {
                errno = 0;
                val = strtoll(rdb_value, &endptr, 10);
                if (errno == 0 && endptr != rdb_value && *endptr == '\0') {
                    value = &val;
                } else {
                    print_err("RDB %s. Unable to parse number: %s - Error: %d - %s\n", rdb_name, rdb_value,
                        errno, strerror(errno));
                    return ret;
                }
            } else if (must_exist) {
                print_err("RDB %s is empty, unable to parse as number.\n", rdb_name);
                return ret;
            }
            break;
        case DOUBLE_NUMBER:
            if (rdb_value[0] != '\0') {
                errno = 0;
                value_double = strtod(rdb_value, &endptr);
                if (errno == 0 && endptr != rdb_value && *endptr == '\0') {
                    value = &value_double;
                } else {
                    print_err("RDB %s. Unable to parse double number: %s - Error: %d - %s\n", rdb_name, rdb_value,
                        errno, strerror(errno));
                    return ret;
                }
            } else if (must_exist) {
                print_err("RDB %s is empty, unable to parse as double number.\n", rdb_name);
                return ret;
            }
            break;
        case BOOLEAN:
            if (!strcmp(rdb_value, "1")) {
                value_bool = true;
                value = &value_bool;
            } else if (!strcmp(rdb_value, "0")) {
                value_bool = false;
                value = &value_bool;
            }
            break;
        default:
            break;
        }
        if (value) {
            json_object_property_t *json_prop = new_json_object_property(prop_name, data_type);
            if (!json_prop || (ret = set_json_value(json_prop->value, value))
                    || (ret = add_json_property(json_obj, json_prop))) {
                delete_json_object_property(json_prop);
                print_err("Failed in setting JSON property value\n");
                return ret;
            }
        }
    }
    return 0;
}

/*
 * See data_build_helper.h
 */
int build_json_string_prop(json_object_t *json_obj, const char *prop_name,
        const char *prop_value)
{
    int ret = -EINVAL;

    if (!json_obj || !prop_name || !prop_value) {
        return ret;
    }

    print_debug("Setting property %s = %s\n", prop_name, prop_value);

    json_object_property_t *json_prop = new_json_object_property(prop_name, STRING);
    if (!json_prop || (ret = set_json_value(json_prop->value, (void *)prop_value))
            || (ret = add_json_property(json_obj, json_prop))) {
        delete_json_object_property(json_prop);
        print_err("Failed in setting property %s = %s\n", prop_name, prop_value);
        return ret;
    }
    return 0;
}

/*
 * See data_build_helper.h
 */
int process_rdb_data_build_set(json_object_t *json_container_obj,
        rdb_data_build_t *rdb_data_set, int len)
{
    int ret = -EINVAL;
    int i;
    if (!json_container_obj || !rdb_data_set) {
        return ret;
    }

    for (i = 0; i < len; i++) {
        rdb_data_build_t *rdb_data = rdb_data_set + i;
        print_debug("Processing RDB %s for property %s\n", rdb_data->rdb_name, rdb_data->prop_name);
        ret = build_json_prop_from_rdb(json_container_obj, rdb_data->prop_name,
                rdb_data->rdb_name, rdb_data->must_exist, rdb_data->default_value,
                rdb_data->data_type);
        if (ret) {
            print_err("Failed in processing RDB %s for property %s\n", rdb_data->rdb_name,
                    rdb_data->prop_name);
            return ret;
        }
    }

    return 0;
}
