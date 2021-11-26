/*
 * Implementing JSON printing functions
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

#include "print_json.h"
#include "json_data.h"
#include "print_msg.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static char *json_print_buf = NULL;
static int json_print_buf_len;
static int json_print_buf_pos;

/*
 * See print_json.h
 */
int init_json_print(int buffer_len)
{
    int ret = -ENOMEM;

    if (buffer_len <= 0) {
        return ret;
    }

    json_print_buf = calloc(1, buffer_len);
    if (json_print_buf) {
        json_print_buf_len = buffer_len;
        json_print_buf_pos = 0;
    } else {
        return ret;
    }
    return 0;
}

/*
 * See print_json.h
 */
void deinit_json_print()
{
    free(json_print_buf);
    json_print_buf = NULL;
    json_print_buf_len = 0;
    json_print_buf_pos = 0;
}

/*
 * Print a string to printing buffer
 * @param data data string to print
 * @return 0 on success, negative code on error
 */
static int print_to_json_buf(const char *data)
{
    int printed_num = snprintf(json_print_buf + json_print_buf_pos, json_print_buf_len, "%s", data);
    if (printed_num >= 0 && printed_num < json_print_buf_len) {
        json_print_buf_len -= printed_num;
        json_print_buf_pos += printed_num;
    } else {
        print_err("Need %d. Failed to print %s\n", printed_num, data);
        return -ENOMEM;
    }
    return 0;
}

/*
 * Print JSON string value to printing buffer
 * Do necessary escape and quote before printing to buffer
 * @param text JSON string value
 * @return 0 on success, negative code on error
 */
static int print_json_string_to_buf(const char *text)
{
    int ret = -ENOMEM;
    unsigned int i, text_len;
    char escaped_chars[8];

    if (!text) {
        return -EINVAL;
    }

    text_len = strlen(text);

    if ((ret = print_to_json_buf("\""))) {
        return ret;
    }

    for (i = 0; i < text_len; i++) {
        switch (text[i]) {
            case '"':
                strcpy(escaped_chars, "\\\"");
                break;
            case '\\':
                strcpy(escaped_chars, "\\\\");
                break;
            case '/':
                strcpy(escaped_chars, "\\/");
                break;
            case '\b':
                strcpy(escaped_chars, "\\b");
                break;
            case '\f':
                strcpy(escaped_chars, "\\f");
                break;
            case '\n':
                strcpy(escaped_chars, "\\n");
                break;
            case '\r':
                strcpy(escaped_chars, "\\r");
                break;
            case '\t':
                strcpy(escaped_chars, "\\t");
                break;
            default:
                // escape control characters 0x0 - 0x1f as \u00XX as required by JSON standards
                // though NULL is not possible to be a character of a pure C string
                if (0x0 <= text[i] && text[i] <= 0x1f) {
                    sprintf(escaped_chars, "\\u%4X", text[i]);
                } else {
                    escaped_chars[0] = text[i];
                    escaped_chars[1] = 0;
                }
        }
        if ((ret = print_to_json_buf(escaped_chars))) {
            return ret;
        }

    }
    if ((ret = print_to_json_buf("\""))) {
        return ret;
    }

    return 0;
}

/*
 * See print_json.h
 */
int print_json_value(json_value_t *json_val)
{
    int ret = -ENOMEM;
    char number_str[32];
    json_object_t *json_obj;
    json_object_property_t *next_prop;
    json_array_t *json_arr;
    json_array_element_t *next_el;
    bool first;

    if (!json_print_buf) {
        print_err("JSON buffer has not been initialised.\n");
        return ret;
    }

    if (!json_val || !json_val->value) {
        return print_to_json_buf("null");
    }

    switch (json_val->value_type) {
        case STRING:
            ret = print_json_string_to_buf(((json_string_t *)(json_val->value))->string);
            break;
        case NUMBER:
            sprintf(number_str, "%lld", ((json_number_t *)(json_val->value))->number);
            ret = print_to_json_buf(number_str);
            break;
         case DOUBLE_NUMBER:
            sprintf(number_str, "%f", ((json_double_number_t *)(json_val->value))->number);
            ret = print_to_json_buf(number_str);
            break;
         case BOOLEAN:
            ret = print_to_json_buf(((json_boolean_t *)(json_val->value))->bool_val ?
                    "true" : "false");
            break;
         case OBJECT:
            if ((ret = print_to_json_buf("{"))) {
                return ret;
            }

            json_obj = (json_object_t *)(json_val->value);
            next_prop = json_obj->properties;
            first = true;
            while (next_prop) {
                if (next_prop->name) {
                    if (!first) {
                        if ((ret = print_to_json_buf(","))) {
                            return ret;
                        }
                    }
                    if (!(ret = print_to_json_buf("\""))
                            && !(ret = print_to_json_buf(next_prop->name))
                            && !(ret = print_to_json_buf("\":"))) {
                        ret = print_json_value(next_prop->value);
                        if (ret) {
                            return ret;
                        }
                    } else {
                        return ret;
                    }
                    first = false;
                }
                next_prop = next_prop->next;
            }

            if ((ret = print_to_json_buf("}"))) {
                return ret;
            }
            break;
         case ARRAY:
            if ((ret = print_to_json_buf("["))) {
                return ret;
            }

            json_arr = (json_array_t *)(json_val->value);
            next_el = json_arr->elements;
            first = true;
            while (next_el) {
                if (!first) {
                    if ((ret = print_to_json_buf(","))) {
                        return ret;
                    }
                }
                ret = print_json_value(next_el->value);
                if (ret) {
                    return ret;
                }
                first = false;
                next_el = next_el->next;
            }

            if ((ret = print_to_json_buf("]"))) {
                return ret;
            }
            break;
    }

    return ret;
}

/*
 * See print_json.h
 */
const char *get_json_printed_text()
{
    return json_print_buf;
}
