/*
 * General purpose string utilities for us in simple_at_manager.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
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

#include "string_utils.h"

// Refer to string_utils.h for documentation of this function.
bool split_string_into_list(char *string, char **elements, size_t *size, char separator)
{

    char *pos = string;
    int count = 0;

    while (*pos != '\0') {
        if (*pos != separator) {
            if (count == *size) {
                // We've run out of pointer to the substrings; Report a failure (and leave *size*
                // at the buffer capacity).  The last pointer in *elements* will equal the remainder
                // of the string.
                return false;
            }
            // point to the start of the next substring and advance.
            elements[count] = pos;
            count++;
        }
        while ((*pos != '\0') && (*pos != separator)) {
            // read through the substring.
            pos++;
        }
        if (*pos == separator) {
            // Got to the end of the substring; replace the separator and start looking for the
            // next substring.
            *pos = '\0';
            pos++;
        }
    }
    // Got to the end of the string safely; set the number of items found and report success.
    *size = count;
    return true;
}
