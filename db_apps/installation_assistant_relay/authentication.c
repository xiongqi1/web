/**
 * @file authentication.c
 * @brief Implements methods to support authentication
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
 *
 */
#include "logger.h"

#ifndef PC_SIMULATOR
#include <mbedtls/rsa.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

#define AUTHENITCATION_HASH_SIZE 32 /* SHA256 */

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

typedef struct authentication {
	char *keyfile;
} authentication_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

static authentication_t context;

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int authentication_init(const char *keyfile)
{
	if (keyfile) {
		context.keyfile = strdup(keyfile);
	} else {
		context.keyfile = NULL;
	}

	LOG_INFO("Authentication keyfile: %s\n", context.keyfile ? context.keyfile : "null");
	return 0;
}

void authentication_term(void)
{
	if (context.keyfile) {
		free(context.keyfile);
		context.keyfile = NULL;
	}
}

int authentication_has_key(void)
{
	if (context.keyfile) {
		return access(context.keyfile, R_OK) == 0;
	}

	return 0;
}

int authentication_verify(const unsigned char *signed_message, size_t signed_len,
		const unsigned char *ori_message, size_t ori_len)
{
#ifndef PC_SIMULATOR
	int result;
	size_t expected_signed_len;
	unsigned char hash[AUTHENITCATION_HASH_SIZE];
	mbedtls_pk_context pk;
	mbedtls_rsa_context *rsa;

	mbedtls_pk_init(&pk);
	if ((result = mbedtls_pk_parse_public_keyfile(&pk, context.keyfile)) != 0) {
		LOG_ERR("Failed to parse the public key: %d\n", result);
		result = -1;
		goto out;
	}

	if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
		LOG_ERR("Invalid RSA public key\n");
		result = -1;
		goto out;
	}

	rsa = mbedtls_pk_rsa(pk);

	expected_signed_len = (mbedtls_mpi_bitlen(&rsa->N) + 7) >> 3;
	if (expected_signed_len != signed_len) {
		LOG_ERR("Invalid encryption length\n");
		result = -1;
		goto out;
	}

	mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);

	mbedtls_sha256(ori_message, ori_len, hash, 0);
	result = mbedtls_rsa_pkcs1_verify(rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC,
				MBEDTLS_MD_SHA256, 0, hash, signed_message);
	if (result) {
		LOG_ERR("Failed to verify the message: %d\n", result);
		result = -1;
		goto out;
	} else {
		LOG_NOTICE("Verify the message successfully\n");
	}

	result = 0;
out:
	mbedtls_pk_free(&pk);
	return result;
#else
	return -1;
#endif
}

int authentication_generate_challenge(unsigned char *output, size_t output_len)
{
	FILE *file;
	int result = 0;
	unsigned int read_len;

	file = fopen("/dev/urandom", "rb");
	if (!file) {
		return -1;
	}

	read_len = fread(output, 1, output_len, file);
	if (read_len != output_len) {
		result = -1;
		goto out;
	}

out:
	fclose(file);
	return result;
}
