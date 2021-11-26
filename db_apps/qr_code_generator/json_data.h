/*
 * Definition of JSON data structure and functions
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

#ifndef JSON_DATA_H_00000003032019
#define JSON_DATA_H_00000003032019

#include <stdbool.h>

/*
 * Data type
 */
typedef enum {
    OBJECT,
    ARRAY,
    NUMBER,
    DOUBLE_NUMBER,
    STRING,
    BOOLEAN
} json_value_type_t;

/*
 * JSON generic data value
 */
typedef struct {
    // data type
    json_value_type_t value_type;
    // data value
    void *value;
} json_value_t;

/*
 * JSON object property
 */
typedef struct json_object_property_ {
    // property name
    char *name;
    // property value
    json_value_t *value;
    // next property in same object
    struct json_object_property_ *next;
} json_object_property_t;

/*
 * JSON array element
 */
typedef struct json_array_element_ {
    // element value
    json_value_t *value;
    // next element in same array
    struct json_array_element_ *next;
} json_array_element_t;

/*
 * JSON object
 */
typedef struct {
    // Properties
    json_object_property_t *properties;
    // Last property
    json_object_property_t *last;
} json_object_t;

/*
 * JSON array
 */
typedef struct {
    // Elements
    json_array_element_t *elements;
    // Last element
    json_array_element_t *last;
} json_array_t;

/*
 * JSON string
 */
typedef struct {
    char *string;
} json_string_t;

/*
 * JSON integer number
 */
typedef struct {
    long long int number;
} json_number_t;

/*
 * JSON float number
 */
typedef struct {
    double number;
} json_double_number_t;

/*
 * JSON boolean
 */
typedef struct {
    bool bool_val;
} json_boolean_t;

/*
 * Create new JSON value
 * @param value_type Value type
 * @return JSON value or NULL on error
 */
json_value_t *new_json_value(json_value_type_t value_type);

/*
 * Create JSON object
 * @return JSON object or NULL on error
 */
json_object_t *new_json_object();

/*
 * Create JSON array
 * @return JSON array or NULL on error
 */
json_array_t *new_json_array();

/*
 * Create JSON string
 * @return JSON string or NULL on error
 */
json_string_t *new_json_string();

/*
 * Create JSON integer number
 * @return JSON integer number or NULL on error
 */
json_number_t *new_json_number();

/*
 * Create JSON float number
 * @return JSON float number or NULL on error
 */
json_double_number_t *new_json_double_number();

/*
 * Create JSON boolean
 * @return JSON boolean or NULL on error
 */
json_boolean_t *new_json_boolean();

/*
 * Delete JSON object
 * @param json_obj JSON object to delete
 */
void delete_json_object(json_object_t *json_obj);

/*
 * Detete JSON array
 * @param json_arr JSON array to delete
 */
void delete_json_array(json_array_t *json_arr);

/*
 * Delete JSON string
 * @param json_str JSON string to delete
 */
void delete_json_string(json_string_t *json_str);

/*
 * Create JSON object property
 * @param name Property name
 * @param value_type Value type
 * @return JSON object property or NULL on error
 */
json_object_property_t *new_json_object_property(const char* name, json_value_type_t value_type);

/*
 * Delete JSON object property
 * @param json_prop JSON object property to delete
 */
void delete_json_object_property(json_object_property_t *json_prop);

/*
 * Delete all properties of an JSON object
 * @param json_obj JSON object
 */
void delete_json_object_properties(json_object_t *json_obj);

/*
 * Create JSON array element
 * @param value_type Value type
 * @return JSON array element or NULL on error
 */
json_array_element_t *new_json_array_element(json_value_type_t value_type);

/*
 * Delete JSON array element
 * @param json_el JSON array element
 */
void delete_json_array_element(json_array_element_t *json_el);

/*
 * Delete all array elements of a JSON array
 * @param json_array JSON array element
 */
void delete_json_array_elements(json_array_t *json_array);

/*
 * Delete common JSON value
 * @param json_value JSON value to delete
 */
void delete_json_value(json_value_t *json_value);

/*
 * Set value for a JSON value
 * @param json_val JSON value to set value to
 * @param value Pointer to particular value data to set
 * @return 0 on success; negative code on error
 */
int set_json_value(json_value_t *json_val, void *value);

/*
 * Add property to a JSON object
 * @param json_obj JSON object
 * @param json_prop JSON object property
 * @return 0 on success; negative code on error
 */
int add_json_property(json_object_t *json_obj, json_object_property_t *json_prop);

/*
 * Add element to a JSON array
 * @param json_array JSON array
 * @param json_el JSON element
 * @return 0 on success; negative code on error
 */
int add_json_element(json_array_t *json_array, json_array_element_t *json_el);

#endif

