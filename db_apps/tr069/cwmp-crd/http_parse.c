/*
 * This file contains the functions used for parsing HTTP requests
 * for the CR daemon.
 *
 * by Matthew Forgie, March 2012.
 * Copyright NetComm Wireless Limited.
 *
 */
#include "cwmp-crd.h"

//#define DEBUGS_ON

#ifdef DEBUGS_ON
	#define DEBUG(...) printf(__VA_ARGS__);fflush(stdout)
#else
	#define DEBUG(...)
#endif


/*
 * The first line of the HTTP header is the status line.  Check that this is
 * a GET request, and that a valid URI has been given.  At present, we don't
 * care what the HTTP version is, provided it is of the form "HTTP/x.x".
 * 
 * request = a string containing the first line of a HTTP header
 *
 * Returns 0 if a valid status line is received, else returns 1.
 */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int parse_status_line (char *request, char *reply)
{
	char cmd[MAXFIELDSIZE + 1];
	char uri[MAXFIELDSIZE + 1];
	char local_uri[MAXFIELDSIZE + 1];
	char version[MAXFIELDSIZE + 1];

	if (sscanf(request, "%"TOSTRING(MAXFIELDSIZE)"s /%"TOSTRING(MAXFIELDSIZE)"s %"TOSTRING(MAXFIELDSIZE)"s", &cmd, &uri, &version) < 3) {
		sprintf(reply, INVALID_REQUEST_400);
		printf("sccanf failed!!\n");
		return 1;
	}
	DEBUG("First token is %s\n", cmd);
	if (strcmp(cmd, "GET") != 0) {
		sprintf(reply, INVALID_REQUEST_400);
		return 1;
	}
	DEBUG("URI token is %s\n", uri);
	rdb_get_single("tr069.request.uri", local_uri, MAXFIELDSIZE);
	if (strncmp(uri, local_uri, strlen(uri)) != 0) {
		DEBUG("URI non-match: uri=%s, local_uri=%s\n", uri, local_uri);
		sprintf(reply, INVALID_REQUEST_404);
		return 1;
	}
	DEBUG("version is %s\n", version);
	return 0;	
}

/*
 * Parse option lines looking for the authentication header.  If we get it
 * Set the appropriate reply, otherwise return non-zero to indicate that we
 * should keep looking.
 *
 * buf = a string containing one option line from an HTTP header
 *
 * Returns 0 if we've got a valid authorization, else non-zero.
 */
int parse_option_line (char *buf)
{
	char *c = buf;
	int i, len = strlen(buf);
	char *key, *value;

	if (c == NULL || len == 0) {
		return -1;
	}
	//Skip past non-alphanumeric characters
	while (isalnum(*c) == 0) {
		c++;
	}
	key = c;
	//Scan the string for the end of the key
	for ( i = 0; i < len; i++) {
		c = key + i;
		if (*c == ':') {
			*c = '\0';
			c++;
			while (isspace(*c) != 0) {
				c++;
			}
			value = c;
			break;
		}
	}
	/* Next interpret the option line information.  We're only really
	 * interested in the Authorization header.
	 */
	DEBUG("Option line has key = %s, value = %s\n", key, value);
	if (!strcmp(key, "Authorization")) {
		//parse authorization fields
		return check_authorization(value);
	} else {
		//Currently we don't care about other fields
		DEBUG("Skipping option: %s\n", key);
	}
	return 1;
}

/*
 * Parse an incoming communication, which is expected to be some form of
 * HTTP request.  We only deal with the a limited subset of HTTP, specifically
 * GET requests, and message digest authentication.
 */
void parse_request (char *request, char *reply)
{
	int i, j = 0;
	char *c;
	char *request_line[MAXHEADERLINES];
	int len;
	char nonce[NONCE_LENGTH + 1];
	char opaque[OPAQUE_LENGTH + 1];
	char busy[MAXFIELDSIZE + 1];

	request_line[j++] = request;
	len = strlen(request);
	for (i = 0; i < len; i++) {
		c = request + i;
		if ( *c == '\r' && *(c + 1) == '\n') {
			DEBUG("detected break at %i and %i\n", i, i + 1);
			// Check if there are more header lines than the pointer buffer allowed.
			if(j >= MAXHEADERLINES) {
				sprintf(reply, INVALID_REQUEST_400);
				DEBUG("ERROR: too many lines in the request!!!\n");
				return;
			}

			//check if line is folded
			if ( *(c + 2) == ' ' || *(c + 2) == '\t') {
				// the line is folded so continue after break
				i += 2;
				DEBUG("detected folded line#####\n");
			} else {
				//otherwise put a break, and add a pointer
				*c = '\0';
				*(c + 1) = '\0';
				request_line[j++] = c + 2;
				DEBUG("add a pointer at index %i, total number of lines now is %i\n", i, j - 1);
			}
		}
	}
	// parse each line (now unfolded) that we have detected in the header
	if (parse_status_line(request_line[0], reply)) {
		DEBUG("skipping the option lines\n");
		return; // an invalid status line was received.
	}
	for (i = 1; i < j - 1; i++) {
		DEBUG("parsing option line %i\n", i);
		if (request_line[i] == NULL || strlen(request_line[i]) == 0) {
			break;
		}
		if (parse_option_line(request_line[i]) == 0) {
			DEBUG("found the line we wanted, so stopping\n");
			// check the busy lock
			rdb_get_single("tr069.request.busy", busy, MAXFIELDSIZE);
			if (strncmp(busy, "1", 1) == 0) {
				DEBUG("trigger locked out, returning 503\n");
				sprintf(reply, INVALID_REQUEST_503);
			} else { 
				DEBUG("triggering inform\n");
				rdb_set_single("tr069.request.trigger", "1");
				sprintf(reply, VALID_REQUEST_204);
			}
			return;
		}
	}
	// if we get here, then no authorization received, so 
        // generate an WWW-authenticate response.
	start_new_session(&nonce, &opaque);
	sprintf(reply, INVALID_REQUEST_401, &nonce, &opaque);
			
	return;
}
