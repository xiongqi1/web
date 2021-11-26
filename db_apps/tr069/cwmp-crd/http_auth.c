/*
 * This file contains the functions used for doing HTTP digest authorization
 * for the CR daemon.
 *
 * by Matthew Forgie, March 2012.
 * Copyright NetComm Wireless Limited.
 *
 */
#include "cwmp-crd.h"

//#define DEBUGS_ON

#ifdef DEBUGS_ON
	#define DEBUG(...) printf(__VA_ARGS__);fflush(stdout);
#else
	#define DEBUG(...)
#endif

static short count = 0xCA53;
static unsigned char nonce_init = 0;
static unsigned char mutex_init = 0;

static int session_count = 0;

/*
 * The below lock is to prevent races on reading/writing entries in the
 * session table.  There are multiple child threads receiving requests and
 * this is the only thing they share.
 */
static pthread_mutex_t session_lock = PTHREAD_MUTEX_INITIALIZER;

struct session {
	char nonce[NONCE_LENGTH + 1];
	char opaque[OPAQUE_LENGTH + 1];
	char realm[MAXFIELDSIZE + 1];
	int timestamp;
	unsigned char valid;
};

static struct session session_table[MAX_SESSIONS];

void print_session_table (void)
{
	int i;
	struct session *ptr;

	printf("Session Table Contents(%i sessions):\n");
	printf("=========================\n", session_count);
	for (i = 0; i < MAX_SESSIONS; i++) {
		ptr = &session_table[i];
		pthread_mutex_lock(&session_lock);
		if (ptr->valid) {
			printf("[%i]: nonce=%s\n", i, ptr->nonce);
			printf("      opaque=%s\n", ptr->opaque);
			printf("      realm=%s\n", ptr->realm);
			printf("      tstamp=%i\n", ptr->timestamp);
		}
		pthread_mutex_unlock(&session_lock);
	}
	printf("=========================\n");
}

/*
 * Returns a pointer to the next unused slot in the session table.  This
 * function MUST NOT be called UNLESS we are holding the session_lock mutex.
 */
struct session *get_next_free_session (void)
{
	struct session *ptr;
	int i = 0;

	if (session_count < MAX_SESSIONS) {
		for (i = 0; i < MAX_SESSIONS; i++) {
			ptr = &session_table[i];
			if (!ptr->valid) {
				memset(ptr, 0, sizeof(struct session));
				ptr->valid = 1;
				session_count++;
				return ptr;
			}
		}
	}
	return NULL;
}

/*
 * Checks whether there is an existing session which matches the given
 * details.  Also, if the entry has timed out it is marked as invalid.
 * Returns 0 if a valid session is found, but will also mark that session
 * as invalid so that it is only used once. If no valid session is found
 * the return value will be non-zero.
 */
int validate_session (char *nonce, char *opaque, char *realm)
{
	struct session *ptr;
	int i = 0;
	int time_now;

	if (nonce == NULL || opaque == NULL || realm == NULL) {
		return 1;
	}

	printf("validate: nonce = %s\n", nonce);
	printf("validate: opaque = %s\n", opaque);
	printf("validate: realm = %s\n", realm);

	for ( i = 0; i < MAX_SESSIONS; i++) {
		ptr = &session_table[i];
		pthread_mutex_lock(&session_lock);
		if (ptr->valid) {
			time_now = (int)time(NULL);
			// mark entry invalid if timed out
			if (ptr->timestamp + NONCE_TIMEOUT < time_now) {
				DEBUG("Validate: Timed out entry found.\n");
				ptr->valid = 0;
				session_count--;
				pthread_mutex_unlock(&session_lock);
				continue;
			}
			// if entry matches, mark invalid and return success
			if (!strcmp(ptr->nonce, nonce) &&
			    !strcmp(ptr->opaque, opaque) &&
			    !strcmp(ptr->realm, realm)) {
				DEBUG("Validate: matching entry found!\n");
				ptr->valid = 0;
				session_count--;
				pthread_mutex_unlock(&session_lock);
				return 0;
			}
		}
		pthread_mutex_unlock(&session_lock);
	}
	return 1;
}

/*
 * This function is used to generate a new nonce and opaque to send in an
 * WWW-Authenticate response.  The generated values are stored in the
 * session table so that they can be checked later.  The return value
 * is 0 on success, and non-zero for failure.
 *
 */
int start_new_session (char *nonce, char *opaque)
{
	struct session *new;

	if (!mutex_init) {
		pthread_mutex_init(&session_lock, NULL);
	}

	pthread_mutex_lock(&session_lock);
	new = get_next_free_session();
	if (new == NULL) {
		fprintf(stderr, "Failed to create new session\n");
		return 1;
	}
	// set the time
	new->timestamp = (int)time(NULL);
	sprintf(new->realm, DEFAULT_REALM);

	// generate a nonce
	if (!nonce_init) {
		srand(time(NULL));
	}
	DEBUG("generated values: time=%8.8x, count=%4.4x, random=%8.8x\n",
		new->timestamp,
		count, rand());
	sprintf(new->nonce, "%8.8x%4.4hx%8.8x", new->timestamp, count++,
		rand());
	//generate an opaque
	sprintf(new->opaque, "%8.8x", rand());
	//Assign the return values
	strncpy(nonce, new->nonce, NONCE_LENGTH);
	strncpy(opaque, new->opaque, OPAQUE_LENGTH);
	pthread_mutex_unlock(&session_lock);
	DEBUG("generated nonce is %s\n", nonce);
	DEBUG("generate opaque is %s\n", opaque);
	print_session_table();
	return 0;
}

static void hex2Ascii (const unsigned char *hex, char *ascii, int len)
{
	char *aptr = ascii;
	register int i;
	for (i = 0; i != len; i++)
	{
		sprintf (aptr, "%2.2x", hex[i]);
		aptr++;
                aptr++;
	}
}

/*
 * Generates the MD5sum hash that we need to compare against the response
 * sent by the client.  This is a 3 stage MD5 as per RF2617, first combining
 * MD5(username:realm:password), then MD5(cmd:uri), and then finally
 * combining both these with the nonce, qop, nc, cnonce. Note that auth-int
 * is not supported here.
 */

/*
    OpenSSL 1.1.1 or higher version do not allow an instance of EVP_MD_CTX to be statically defined.

    1. dynamically allocate EVP_MD_CTX - use EVP_DigestInit_ex() and EVP_DigestFinal_ex().
    2. use the previous static allocation code for those old versions of OpenSSL that do not have
       EVP_DigestInit_ex() and EVP_DigestFinal_ex() functions.
*/

#if (OPENSSL_VERSION_NUMBER >= 0x10101000L)
int generate_hash (char *hash, struct auth_params *params)
{
	EVP_MD_CTX* ctx  = EVP_MD_CTX_new();
	unsigned int size;
	unsigned char HA1[EVP_MAX_MD_SIZE];
	unsigned char HA2[EVP_MAX_MD_SIZE];
	char a1[MAXFIELDSIZE * 3 + 3];
	char a2[MAXFIELDSIZE + 5];
	char a3[(MAXFIELDSIZE * 3) + NONCE_LENGTH +
			 (EVP_MAX_MD_SIZE * 2) + 5];
	char username[MAXFIELDSIZE + 1] = DEFAULT_USER;
	char password[MAXFIELDSIZE + 1] = DEFAULT_PASS;
	char uri[MAXFIELDSIZE + 1] = DEFAULT_URI;

	rdb_get_single("tr069.request.username", username, MAXFIELDSIZE);
	rdb_get_single("tr069.request.password", password, MAXFIELDSIZE);
	rdb_get_single("tr069.request.uri", uri, MAXFIELDSIZE);

	if (username == NULL || password == NULL) {
		DEBUG("no username/password specified\n");
		return 1;
	}

	// Generate the HA1 hash
        EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
        EVP_DigestUpdate(ctx, username, strlen(username));
        EVP_DigestUpdate(ctx, ":", 1);
        EVP_DigestUpdate(ctx, params->realm, strlen(params->realm));
        EVP_DigestUpdate(ctx, ":", 1);
        EVP_DigestUpdate(ctx, password, strlen(password));
        EVP_DigestFinal_ex(ctx, HA1, &size);
        hex2Ascii(HA1, a1, size);

	// Generate the HA2 hash
        EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
        EVP_DigestUpdate(ctx, "GET:/", 5);
        EVP_DigestUpdate(ctx, uri, strlen(uri));
        EVP_DigestFinal_ex(ctx, HA2, &size);
        hex2Ascii(HA2, a2, size);

	// Generate the final response hash
        EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
        EVP_DigestUpdate(ctx, a1, strlen(a1));
        EVP_DigestUpdate(ctx, ":", 1);
        EVP_DigestUpdate(ctx, params->nonce, strlen(params->nonce));
        if ( params->qop && *params->qop ) {
            EVP_DigestUpdate(ctx, ":", 1);
            EVP_DigestUpdate(ctx, params->nc, strlen(params->nc));
            EVP_DigestUpdate(ctx, ":", 1);
            EVP_DigestUpdate(ctx, params->cnonce, strlen(params->cnonce));
            EVP_DigestUpdate(ctx, ":", 1);
            EVP_DigestUpdate(ctx, params->qop, strlen(params->qop));
        }
        EVP_DigestUpdate(ctx, ":", 1);
        EVP_DigestUpdate(ctx, a2, strlen(a2));
        EVP_DigestFinal_ex(ctx, hash, &size);
        hex2Ascii(hash, a3, size);
        strcpy(hash, a3);

	EVP_MD_CTX_destroy(ctx);
	return 0;
}
#else
int generate_hash (char *hash, struct auth_params *params)
{
	EVP_MD_CTX ctx;
	unsigned int size;
	unsigned char HA1[EVP_MAX_MD_SIZE];
	unsigned char HA2[EVP_MAX_MD_SIZE];
	unsigned char a1[MAXFIELDSIZE * 3 + 3];
	unsigned char a2[MAXFIELDSIZE + 5];
	unsigned char a3[(MAXFIELDSIZE * 3) + NONCE_LENGTH +
			 (EVP_MAX_MD_SIZE * 2) + 5];
	char username[MAXFIELDSIZE + 1] = DEFAULT_USER;
	char password[MAXFIELDSIZE + 1] = DEFAULT_PASS;
	char uri[MAXFIELDSIZE + 1] = DEFAULT_URI;

	rdb_get_single("tr069.request.username", username, MAXFIELDSIZE);
	rdb_get_single("tr069.request.password", password, MAXFIELDSIZE);
	rdb_get_single("tr069.request.uri", uri, MAXFIELDSIZE);

	if (username == NULL || password == NULL) {
		DEBUG("no username/password specified\n");
		return 1;
	}

	// Generate the HA1 input string
	strncpy(a1, username, MAXFIELDSIZE);
        strcat((char *)a1, ":");
        strcat((char *)a1, params->realm);
        strcat((char *)a1, ":");
        strncat((char *)a1, password, MAXFIELDSIZE);

	// Generate the HA2 input string
        strcpy((char *)a2, "GET");
        strcat((char *)a2, ":/");
        strncat((char *)a2, uri, MAXFIELDSIZE);

	// Generate the HA1 hash
        EVP_MD_CTX_init((EVP_MD_CTX*)&ctx);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate (&ctx, a1, strlen((char *)a1));
        EVP_DigestFinal(&ctx, HA1, &size);

	// Generate the HA2 hash
        hex2Ascii (HA1, (char *)a1, size);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate (&ctx, a2, strlen((char *)a2));
        EVP_DigestFinal(&ctx, HA2, &size);

	// Generate the final response hash
        hex2Ascii (HA2, (char *)a2, size);
        strcpy((char *)a3, (char *)a1);
        strcat((char *)a3, ":");
        strcat((char *)a3, params->nonce);
        if ( params->qop && *params->qop ) {
                strcat((char *)a3, ":");
                strcat((char *)a3, params->nc);
                strcat((char *)a3, ":");
                strcat((char *)a3, params->cnonce);
                strcat((char *)a3, ":");
                strcat((char *)a3, params->qop);
        }
        strcat ((char*)a3, ":");
        strcat ((char*)a3, (char*)a2);
        EVP_DigestInit(&ctx, EVP_md5());
        EVP_DigestUpdate(&ctx, (unsigned char *)a3, strlen((char *)a3));
        EVP_DigestFinal(&ctx, hash, &size);
	//translate into ASCII then put back in the return buffer
        hex2Ascii (hash, (char*)a3, size);
	strcpy(hash, (char *)a3);
	return 0;
}
#endif

/*
 * Once the digest has been divided into sections based on ',', each token
 * can be parsed by this function into a key and a value, then assigned to
 * the appropriate part of the auth_params structure, which maintains the
 * details of the digest.
 */
void parse_auth_params (struct auth_params *params, char *buf)
{
	char *key, *value;

	key = buf;
	if (key == NULL) {
		return;
	}
	// strip leading whitespace from key
	while (*key && isspace(*key)) {
		key++;
	}
	// scan through to start of value
	value = buf;
	while (*value && *value != '=') {
		value++;
	}
	if (value == NULL) {
		return;
	}
	// split the string, so key ends here
	*value = '\0';
	value++;

	//remove leading whitespace from value
	while (*value && isspace(*value)) {
		value++;
	}
	if (*value == '"') {
		value ++;
		value[strlen(value) - 1] = '\0';
	}
	DEBUG("Authorisation parameter: key = %s, value = %s\n", key, value);
	if (!strncmp(key, "user", strlen("user"))) {
		params->user = value;
	} else if  (!strncmp(key, "realm", strlen("user"))) {
		params->realm = value;
	} else if  (!strncmp(key, "nonce", strlen("nonce"))) {
		params->nonce = value;
	} else if  (!strncmp(key, "uri", strlen("uri"))) {
		params->uri = value;
	} else if  (!strncmp(key, "qop", strlen("qop"))) {
		params->qop = value;
	} else if  (!strncmp(key, "nc", strlen("nc"))) {
		params->nc = value;
	} else if  (!strncmp(key, "cnonce", strlen("cnonce"))) {
		params->cnonce = value;
	} else if  (!strncmp(key, "response", strlen("response"))) {
		params->response = value;
	} else if  (!strncmp(key, "opaque", strlen("opaque"))) {
		params->opaque = value;
	}
}

/*
 * check_authorization()
 *
 * Takes a string argument which should be the contents of the 'Authorization'
 * option from the HTTP header.  Returns 0 if the authorization is valid,
 * otherwise non-zero is returned.
 */
int check_authorization (char *buf)
{
	char *digest_challenge;
	char *token;
	struct auth_params incoming;
	char hash[EVP_MAX_MD_SIZE];

	if (strncmp(buf, "Digest", 6)) {
		DEBUG("Only Digest Authentication handled at present\n");
		return 1;
	}
	DEBUG("buf is %s\n", buf);
	digest_challenge = buf + 7;
	DEBUG("digest_challenge is %s\n", digest_challenge);

	DEBUG("Checking Authorization....\n");
	token = strtok(digest_challenge, ",");
	parse_auth_params(&incoming, token);
	while (token != NULL) {
		token = strtok(NULL, ",");
		parse_auth_params(&incoming, token);
	}
	// check whether nonce,opaque is on the valid list
	if (validate_session(incoming.nonce, incoming.opaque,
	    incoming.realm)) {
		DEBUG("Invalid authorization details received\n");
		return 1;
	}

	// generate a hash from local username password plus the nonce
	generate_hash(hash, &incoming);

	// compare to response sent from user
	if (strcmp(hash, incoming.response) == 0) {
		DEBUG("Authorization success!\n")
		return 0;
	}
	DEBUG("Authorization failed:\ngiven response = %s\ncalculated response = %s\n", incoming.response, hash);
	return 1;
}
