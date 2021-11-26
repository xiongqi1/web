#ifndef STRING_UTILS_H_09240026022018
#define STRING_UTILS_H_09240026022018

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

#include <stdlib.h>
#include <stdbool.h>

// Splits the given *string* into a list of null terminated strings and populates *elements* with
// pointers to each.  The int pointed to by *size* must be set to the length of the *elements*
// buffer before the call, and is set to the number of elements found on exit.  The components are
// split on the character *separator*.
// Leading and trailing separator characters are ignored (they don't yield empty strings) as are
// running sequences of them.
// *string* must be NUL terminated.
//
// If we are able to split out all the substrings successfully then return true.
//
// Note: modifies the original.
//
// Example:
//   char input[] = "one;two;three";
//   char *elems[5];
//   size_t s = 5;
//   split_string_into_list(input, elems, &s, ';');
// will set:
//   s == 3,
//   elems[0] == "one", elems[1] == "two" and elems[0] == "three"
// and replace the semicolons in input with NULs.
// elems and s would have the same value if input[] was set to ";one;two;;three;";
//
// If there are more substrings in *string* than space in *elements* then the remaining substrings
// are ignored and the function returns false.  *size* will remain the length of *elements*.
bool split_string_into_list(char *string, char **elements, size_t *size, char separator);

#endif        //  #ifndef STRING_UTILS_H_09240026022018
