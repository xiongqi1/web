/*
 * Implementing of JSON data functions
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

#include "json_data.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

/*
 * See json_data.h
 */
json_value_t *new_json_value(json_value_type_t value_type)
{
    json_value_t *json_value = calloc(1, sizeof(json_value_t));
    if (json_value) {
        json_value->value_type = value_type;
        switch (value_type) {
            case OBJECT:
                json_value->value = new_json_object();
                break;
            case ARRAY:
                json_value->value = new_json_array();
                break;
            case NUMBER:
                json_value->value = new_json_number();
                break;
            case DOUBLE_NUMBER:
                json_value->value = new_json_double_number();
                break;
            case STRING:
                json_value->value = new_json_string();
                break;
            case BOOLEAN:
                json_value->value = new_json_boolean();
                break;
        }
    }
    if (!json_value || !json_value->value) {
        free(json_value);
        return NULL;
    }
    return json_value;
}

/*
 * See json_data.h
 */
json_object_t *new_json_object()
{
    json_object_t *json_obj = calloc(1, sizeof(json_object_t));
    return json_obj;
}

/*
 * See json_data.h
 */
json_array_t *new_json_array()
{
    json_array_t *json_arr = calloc(1, sizeof(json_array_t));
    return json_arr;
}

/*
 * See json_data.h
 */
json_string_t *new_json_string()
{
    json_string_t *json_string = calloc(1, sizeof(json_string_t));
    return json_string;
}

/*
 * See json_data.h
 */
json_number_t *new_json_number()
{
    json_number_t *json_number = calloc(1, sizeof(json_number_t));
    return json_number;
}

/*
 * See json_data.h
 */
json_double_number_t *new_json_double_number()
{
    json_double_number_t *json_double_number = calloc(1, sizeof(json_double_number_t));
    return json_double_number;
}

/*
 * See json_data.h
 */
json_boolean_t *new_json_boolean()
{
    json_boolean_t *json_bool = calloc(1, sizeof(json_boolean_t));
    return json_bool;
}

/*
 * See json_data.h
 */
void delete_json_object(json_object_t *json_obj)
{
    delete_json_object_properties(json_obj);
    free(json_obj);
}

/*
 * See json_data.h
 */
void delete_json_array(json_array_t *json_arr)
{
    delete_json_array_elements(json_arr);
    free(json_arr);
}

/*
 * See json_data.h
 */
void delete_json_string(json_string_t *json_str)
{
    free(json_str->string);
    free(json_str);
}

/*
 * See json_data.h
 */
json_object_property_t *new_json_object_property(const char* name, json_value_type_t value_type)
{
    if (!name) {
        return NULL;
    }
    json_object_property_t *json_prop = calloc(1, sizeof(json_object_property_t));
    if (json_prop) {
        json_prop->name = strdup(name);
        if (!json_prop->name) {
            free(json_prop);
            return NULL;
        }
        json_prop->value = new_json_value(value_type);
        if (!json_prop->value) {
            free(json_prop->name);
            free(json_prop);
            return NULL;
        }
    }
    return json_prop;
}

/*
 * See json_data.h
 */
void delete_json_object_property(json_object_property_t *json_prop)
{
    if (!json_prop) {
        return;
    }
    free(json_prop->name);
    delete_json_value(json_prop->value);
    free(json_prop);
}

/*
 * See json_data.h
 */
void delete_json_object_properties(json_object_t *json_obj)
{
    if (!json_obj || !json_obj->properties) {
        return;
    }
    json_object_property_t *next = json_obj->properties;
    json_object_property_t *current;
    while (next) {
        current = next;
        next = next->next;
        delete_json_object_property(current);
    }
    json_obj->properties = NULL;
    json_obj->last = NULL;
}

/*
 * See json_data.h
 */
json_array_element_t *new_json_array_element(json_value_type_t value_type)
{
    json_array_element_t *json_el = calloc(1, sizeof(json_array_element_t));
    if (json_el) {
        json_el->value = new_json_value(value_type);
        if (!json_el->value) {
            free(json_el);
            return NULL;
        }
    }
    return json_el;
}

/*
 * See json_data.h
 */
void delete_json_array_element(json_array_element_t *json_el)
{
    if (!json_el) {
        return;
    }
    delete_json_value(json_el->value);
    free(json_el);
}

/*
 * See json_data.h
 */
void delete_json_array_elements(json_array_t *json_array)
{
    if (!json_array || !json_array->elements) {
        return;
    }
    json_array_element_t *next = json_array->elements;
    json_array_element_t *current;
    while (next) {
        current = next;
        next = next->next;
        delete_json_array_element(current);
    }
    json_array->elements = NULL;
    json_array->last = NULL;
}

/*
 * See json_data.h
 */
void delete_json_value(json_value_t *json_value)
{
    if (!json_value) {
        return;
    }
    switch (json_value->value_type) {
        case OBJECT:
            delete_json_object_properties((json_object_t *)(json_value->value));
            break;
        case ARRAY:
            delete_json_array_elements((json_array_t *)(json_value->value));
            break;
        case STRING:
            free(((json_string_t *)(json_value->value))->string);
            break;
        default:
            break;
    }
    free(json_value->value);
    free(json_value);
}

/*
 * See json_data.h
 */
int set_json_value(json_value_t *json_val, void *value)
{
    int ret = -EINVAL;

    if (!json_val) {
        return ret;
    }

    switch (json_val->value_type) {
        case STRING:
            free(((json_string_t *)(json_val->value))->string);
            if (!value) {
                ((json_string_t *)(json_val->value))->string = NULL;
            } else {
                ((json_string_t *)(json_val->value))->string = strdup((const char *)value);
                if (!((json_string_t *)(json_val->value))->string) {
                    return -ENOMEM;
                }
            }
            break;
        case NUMBER:
            if (!value) {
                return ret;
            }
            ((json_number_t *)(json_val->value))->number = *((long long int *)value);
            break;
        case DOUBLE_NUMBER:
            if (!value) {
                return ret;
            }
            ((json_double_number_t *)(json_val->value))->number = *((double *)value);
            break;
        case BOOLEAN:
            if (!value) {
                return ret;
            }
            ((json_boolean_t *)(json_val->value))->bool_val = *((bool *)value);
            break;
        case OBJECT:
            delete_json_object_properties((json_object_t *)(json_val->value));
            json_val->value = value;
            break;
        case ARRAY:
            delete_json_array_elements((json_array_t *)(json_val->value));
            json_val->value = value;
            break;
        default:
            return ret;
    }

    return 0;
}

/*
 * See json_data.h
 */
int add_json_property(json_object_t *json_obj, json_object_property_t *json_prop)
{
    int ret = -EINVAL;

    if (!json_obj || !json_prop) {
        return ret;
    }
    if (json_obj->properties) {
        json_obj->last->next = json_prop;
    } else {
        json_obj->properties = json_prop;
    }
    json_obj->last = json_prop;

    return 0;
}

/*
 * See json_data.h
 */
int add_json_element(json_array_t *json_array, json_array_element_t *json_el)
{
    int ret = -EINVAL;

    if (!json_array || !json_el) {
        return ret;
    }
    if (json_array->elements) {
        json_array->last->next = json_el;
    } else {
        json_array->elements = json_el;
    }
    json_array->last = json_el;

    return 0;
}
