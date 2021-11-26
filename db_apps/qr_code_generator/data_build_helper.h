/*
 * Definition of helper functions to build data
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

#ifndef DATA_BUILD_HELPER_H_00000003032019
#define DATA_BUILD_HELPER_00000003032019

#include "json_data.h"
#include <stdbool.h>

/*
 * Meta data to add properties from RDB variable data to a JSON object
 */
typedef struct {
    const char *prop_name;
    const char *rdb_name;
    bool must_exist;
    const char *default_value;
    json_value_type_t data_type;
} rdb_data_build_t;

/*
 * Build JSON object property for given JSON object with data from RDB variable
 * If RDB variable does not exist while must_exist is not set, no property will be created.
 * @param json_obj JSON object to add property
 * @param prop_name Property name
 * @param rdb_name RDB variable name to read for data
 * @param must_exist whether RDB variable must exist
 * @param default_value Default value to take in case of non-existing RDB variable
 * @param data_type Data type
 * @return 0: success; negative code on error
 */
int build_json_prop_from_rdb(json_object_t *json_obj, const char *prop_name,
        const char *rdb_name, bool must_exist, const char *default_value,
        json_value_type_t data_type);

/*
 * Build JSON object property with string value for given JSON object
 * @param json_obj JSON object to add property
 * @param prop_name Property name
 * @param prop_value Property value
 * @return 0: success; negative code on error
 */
int build_json_string_prop(json_object_t *json_obj, const char *prop_name, const char *prop_value);

/*
 * Process given array of rdb_data_build_t to add properties for given JSON object
 * @param json_container_obj JSON object to add property
 * @param rdb_data_set array of rdb_data_build_t
 * @param len Number of elements of array rdb_data_set
 * @return 0: success; negative code on error
 */
int process_rdb_data_build_set(json_object_t *json_container_obj,
        rdb_data_build_t *rdb_data_set, int len);

#endif
