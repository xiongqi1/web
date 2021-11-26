#ifndef AUTHENTICATION_H_15224519062018
#define AUTHENTICATION_H_15224519062018
/**
 * @file authentication.h
 * @brief Provides public functions and data structures to use authentication
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "app_type.h"

#include <stddef.h>

/**
 * @brief Initiatialises the authentication module
 *
 * @param key The full path where the authentication key is stored.
 * @return 0 on success, or a negative value on error
 */
int authentication_init(const char *key);

/**
 * @brief Terminates the authentication module
 */
void authentication_term(void);

/**
 * @brief Return a boolean indicating whether the authentication key exists
 *i       or not
 *
 * @return 1 if the key exists, or 0 otherwise.
 */
int authentication_has_key(void);

/**
 * @brief Verifies the signed message with the original message.
 *
 * @param signed_message The signed message
 * @param signed_len The length of the signed message
 * @param ori_message The original message
 * @param ori_len The length of the original message
 * @return 0 on success, or a negative value on error
 */
int authentication_verify(unsigned char *signed_message, size_t signed_len,
		unsigned char *ori_message, size_t ori_len);


/**
 * @brief Generates a pseudo random number
 *
 * @param output The location where random numbers are stored.
 * @param output_len The length of random numbers are expected to be generated
 * @return 0 on success, or a negative value on error
 */
int authentication_generate_challenge(unsigned char *output, size_t output_len);

#endif
