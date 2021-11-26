#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <limits.h> 
#include "rdb_ops.h"

#define DEFAULT_PORT "7547"  // the port users will be connecting to
#define DEFAULT_REALM "Casa Systems. Inc."
#define WORKER_THREAD_STACKSIZE 1024*100
#define MAXREQUESTSIZE 4096
#define MAXFIELDSIZE 256 
#define MAX_SESSIONS 100 // the maximum number of concurrent sessions
#define NONCE_TIMEOUT 500
#define NONCE_LENGTH 21
#define OPAQUE_LENGTH 9
#define MAXHEADERLINES 100
#define BACKLOG 10	 // how many pending connections queue will hold
#define REQUEST_PERIOD_LENGTH 60  //measured in seconds
#define MAX_REQUESTS_PER_PERIOD 60 
#define INVALID_REQUEST_400 "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n"
#define INVALID_REQUEST_401 "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\nContent-Length: 0\r\nWWW-Authenticate: Digest realm=\"Casa Systems. Inc.\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"\r\n\r\n"
#define INVALID_REQUEST_404 "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"
#define INVALID_REQUEST_503 "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n"
#define VALID_REQUEST_200 "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
#define VALID_REQUEST_204 "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n"
#define DEFAULT_URI "kjhad917an"
#define DEFAULT_USER "user"
#define DEFAULT_PASS "pass"

struct auth_params {
	char *user;
	char *realm;
	char *nonce;
	char *uri;
	char *qop;
	char *nc;
	char *cnonce;
	char *response;
	char *opaque;
};
