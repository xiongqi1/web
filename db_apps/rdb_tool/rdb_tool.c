/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
 * CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define __USE_GNU
#include <string.h>

#include <errno.h>
#include <sys/select.h>
#include <signal.h>
#include "rdb_ops.h"

typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

static struct rdb_session *rdb_s = NULL;

#define TRUE 1
#define FALSE 0

#define PRCHAR(x) (((x)>=0x20&&(x)<=0x7e)?(x):'.')

#define xtod(c) ((c>='0' && c<='9') ? c-'0' : ((c>='A' && c<='F') ? \
                c-'A'+10 : ((c>='a' && c<='f') ? c-'a'+10 : 0)))
int HextoDec(char *hex)
{
    if (*hex == 0) return 0;
    return  HextoDec(hex-1)*16 +  xtod(*hex) ;
}

int xstrtoi(char *hex)    // hex string to integer
{
    return HextoDec(hex+strlen(hex)-1);
}

typedef enum {
	GET = 0,
	SET,
	WAIT,
	DEL,
	SET_WAIT
} _mode ;

/* Verison Information */
#define VER_MJ		0
#define VER_MN		1
#define VER_BLD	1


const char shortopts[] = "splL?fPaAo:O";
#define	OPT_GET		0x0001
#define	OPT_SET		0x0002
#define	OPT_PERSIST	0x0004
#define	OPT_LIST 	0x0008
#define	OPT_VERBOSE	0x0010
#define	OPT_FLAGS 	0x0020
#define OPT_STAT	0x0040
#define OPT_PERM    0x0080
#define OPT_OWNER       0x0100
#define OPT_ALL     OPT_PERM | OPT_FLAGS | OPT_LIST | OPT_OWNER
#define OPT_ALL_V   OPT_PERM | OPT_FLAGS | OPT_VERBOSE | OPT_LIST | OPT_OWNER


int usage_default()
{
	fprintf(stderr, "\nUsage: function [options] [arguments]...\n");

	fprintf(stderr, "\nrdb_tool is a multi-call binary used for accessing\n");
	fprintf(stderr, "RDB variables, and hence controlling system behaviour.\n");
	fprintf(stderr, "rdb_tool is called indirectly through the rdb_* functions."
					"\n");
	fprintf(stderr, "\nCurrently defined functions:\n");
	fprintf(stderr, "\trdb_get: get the current value of an RDB variable\n");
	fprintf(stderr, "\trdb_set: set the current value of an RDB variable\n");
	fprintf(stderr, "\trdb_wait: block waiting for a variable to change\n");
	fprintf(stderr, "\trdb_wait_set: wait for a variable to change, then\n"
					"\t\t\twrite the supplied value to another\n"
					"\t\t\tvariable.\n\n");
	fprintf(stderr, "Usage>\n");
	fprintf(stderr, "\t# rdb_get or rdb_set [options] <database variable name>\n");
	fprintf(stderr, "\t# rdb_get -L or -l [search word]\n");
	fprintf(stderr, "\t# rdb_wait <database variable> [timeout seconds]\n");

	fprintf(stderr, "\t# rdb_wait_set <wait database variable> <timeout seconds> <write database variable> < write value> \n");

	fprintf(stderr, "\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "\t-a same as -lfOP\n");
	fprintf(stderr, "\t-A same as -LfOP\n");
	fprintf(stderr, "\t-f get flags of the variable\n");
	fprintf(stderr, "\t-l get list of variable names\n");
	fprintf(stderr, "\t-L get list of variable names and values\n");
	fprintf(stderr, "\t-p set persist flag\n");
	fprintf(stderr, "\t-o <uid:gid> set ownership\n");
	fprintf(stderr, "\t-P get variable permissions\n");
	fprintf(stderr, "\t-O get variable ownership\n");
	fprintf(stderr, "\t-s set statistics flag\n");
	fprintf(stderr, "\n");

	return 0;
}

void sigDummyHandler(int iSig)
{
	const char* szSig = strsignal(iSig);
	fprintf(stderr, "%s ignored\n", szSig);
}

static int print_flags(const char* name, int options)
{
	int flags, ret;
	if (!(options & OPT_FLAGS)) { return 0; }
	ret = rdb_getinfo(rdb_s, name, NULL, &flags, NULL);
	if (ret < 0 && ret != -EOVERFLOW) {
		fprintf(stderr,"failed to get flags '%s'; ret=%d (%s)\n", name, ret, strerror(errno));
		rdb_close(&rdb_s);
		exit(-1);
	}
	printf("0x%04x", flags);
	return 1;
}

static int print_perm(const char* name, int options)
{
	int perm, ret;
	if (!(options & OPT_PERM)) { return 0; }
	ret = rdb_getinfo(rdb_s, name, NULL, NULL, &perm);
	if (ret < 0 && ret != -EOVERFLOW) {
		fprintf(stderr, "failed to get permissions for '%s'; ret=%d (%s)\n", name, ret, strerror(errno));
		rdb_close(&rdb_s);
		exit(-1);
	}
	printf("0x%04x", perm);
	return 1;
}

static int print_owner(const char* name, int options)
{
	int uid, gid, ret;
	if (!(options & OPT_OWNER)) { return 0; }
	ret = rdb_getinfo_privilege(rdb_s, name, NULL, NULL, NULL, &uid, &gid);
	if (ret < 0 && ret != -EOVERFLOW) {
		fprintf(stderr,"failed to get permissions for '%s'; ret=%d (%s)\n", name, ret, strerror(errno));
		rdb_close(&rdb_s);
		exit(-1);
	}
	printf("UID=%d, GID=%d", uid, gid);
	return 1;
}

static int delete_variable(const char* name)
{
	int ret;
	if (rdb_getinfo( rdb_s, name, NULL, NULL, NULL ) == -ENOENT ) { fprintf( stderr, "%s does not exist\n", name ); return 0; }
	if ((ret = rdb_delete( rdb_s, name )) == 0) { printf( "%s deleted\n", name ); }
	else { fprintf(stderr, "failed to delete '%s' (%s)\n", name, strerror(errno)); }
	return ret;
}

#define DEF_LIST_SIZE	8192*8
#define DEF_VALUE_SIZE	4096
int main(int argc, char **argv)
{
	int	ret = 0;
	int	len;
	_mode	mode;
	int	options = 0;			// Bitmask of options
	char	*value = malloc(DEF_VALUE_SIZE);
	char	*list = malloc(DEF_LIST_SIZE);
	int 	rval = -1;
	char	*tok;
	char	*vptr;
	int nFlags = 0;
	char* szValue  ="";
	int nTimeout = 0;
	int fd = 0;
	fd_set fdR;
	struct timeval tv = {0,0};
	struct timeval* pTv = 0;
	int flags = 0;
	int uid = -1;
	int gid = -1;

	if (strcasestr(argv[0], "rdb_get"))
		mode = GET;
	else if (strcasestr(argv[0], "rdb_set_wait"))
		mode = SET_WAIT;
	else if (strcasestr(argv[0], "rdb_set"))
		mode = SET;
	else if (strcasestr(argv[0], "rdb_del"))
		mode = DEL;
	else if (strcasestr(argv[0], "rdb_wait"))
		mode = WAIT;
	else {
		usage_default(argv);
		goto exit;
	}

	if (!list || !value) {
		fprintf(stderr, "failed to allocated memory\n");
		goto exit;
	}

	while ((ret = getopt(argc, argv, shortopts)) != EOF) {
		switch (ret) {
			case 'p': options |= OPT_PERSIST; break;
			case 'o':
				if (sscanf(optarg, "%d:%d", &uid, &gid) != 2) {
					fprintf(stderr, "Wrong uid:gid format - %s\n", optarg);
					goto exit;
				}
				break;
			case 's': options |= OPT_STAT; break;
			case 'l': options |= OPT_LIST; break;
			case 'L': options |= (OPT_LIST | OPT_VERBOSE); break;
			case 'f': options |= OPT_FLAGS; break;
			case 'P': options |= OPT_PERM; break;
			case 'O': options |= OPT_OWNER; break;
			case 'a': options |= OPT_ALL; break;
			case 'A': options |= OPT_ALL_V; break;
			case '?':
			case 'h':
			default:
				usage_default(argv); return 2;
		}
	}
	/* Open rdb database */
	if (rdb_open(NULL, &rdb_s) < 0) {
		perror("Opening database");
		goto exit;
	}

	if ((mode == WAIT) || (mode == SET_WAIT)) {
		// get variable name
		char* szName = NULL;
		if (optind < argc)
			szName = argv[optind++];

		// get timeout
		if (optind < argc)
			nTimeout = atol(argv[optind++]);

		tv.tv_sec = nTimeout;

		// check arguments
		if (!szName) {
			fprintf(stderr, "variable name not specified\n");
			goto exit;
		}

		// ignore otherwise getting killed - platypus platform sends SIGHUP instread of SIGUSR1
		signal(SIGHUP, sigDummyHandler);

		// subscribe
		if (rdb_subscribe(rdb_s, szName) < 0) {
			fprintf(stderr, "failed to subscribe %s - %s\n", szName, strerror(errno));
			goto exit;
		}

		// build description
		fd = rdb_fd(rdb_s);
		FD_ZERO(&fdR);
		FD_SET(fd,&fdR);

		// build timeout
		pTv = NULL;
		if(nTimeout)
			pTv = &tv;
	}

	if (mode == GET) {
		if (options & OPT_LIST) {
			len = DEF_LIST_SIZE;
			if ((ret = rdb_getnames_alloc(rdb_s, argv[optind], &list, &len, 0))) {
				if (ret != -ENOENT) {
					fprintf(stderr, "rdb_getnames_alloc() failed - %s\n", strerror(errno));
				}
				goto exit;
			}
			vptr = list;
			while ((tok = strtok(vptr, "&"))) {
				vptr = NULL;

				if (print_flags(tok, options)) { printf( " " ); }
				if (print_perm(tok, options)) { printf( " " ); }
				if (print_owner(tok, options)) { printf( " "); }
				printf("%s", tok);
				if (options & OPT_VERBOSE) {
					len = DEF_VALUE_SIZE;
					if ((ret = rdb_get_alloc(rdb_s, tok, &value, &len))) {
						if (ret != -ENOENT) {
							fprintf(stderr, "rdb_get_alloc() failed - %s\n", strerror(errno));
						}
						goto exit;
					}
					printf(" %s", value);
				}
				printf( "\n" );
			}
		}
		else if (options & OPT_FLAGS) {
			print_flags(argv[optind], options);
			printf("\n");
		}
		else if (options & OPT_PERM) {
			print_perm(argv[optind], options);
			printf("\n");
		}
		else if (options & OPT_OWNER) {
			print_owner(argv[optind], options);
			printf("\n");
		}
		else {
			len = DEF_VALUE_SIZE;
			if ((ret = rdb_get_alloc(rdb_s, argv[optind], &value, &len))) {
				if (ret != -ENOENT) {
					fprintf(stderr, "rdb_get_alloc() failed - %s\n", strerror(errno));
				}
				goto exit;
			}
			printf("%s\n", value);
		}
	}

	if ((mode == SET) || (mode == SET_WAIT)) {
		if (options & OPT_PERSIST)
			nFlags |= PERSIST;
		if (options & OPT_STAT)
			nFlags |= STATISTICS;
		if(optind < argc)
			szValue = argv[optind+1];

		if(!szValue)
			szValue = "";

		ret = rdb_update_string(rdb_s, argv[optind], szValue, nFlags, DEFAULT_PERM);

		if (ret != 0) {
			fprintf(stderr, "failed to set '%s'; ret=%d (%s)\n", argv[optind], ret, strerror( errno ));
			goto exit;
		}

		/////////////////////////////////////////////////////////////////////
		// rdb_update_single does not bother to set the flags if the variable
		// already exists.
		// For example (some does not exist)
		// rdb_set "some" 1
		// rdb_set -p "some" 2,
		// will results in "some" having a value of 2 but not being persistent
		// This is a workaround
		if (nFlags && (rdb_getflags(rdb_s, argv[optind], &flags) == 0) && (flags != nFlags))
			rdb_setflags(rdb_s, argv[optind], nFlags|flags); // "or" with old flags
		// end of fix
		////////////////////////////////////////////////////////////////////////

		optind += 2;
	}

	if (mode == DEL) {
		if(options & OPT_LIST) {
			len = DEF_LIST_SIZE;
			if ((ret = rdb_getnames_alloc(rdb_s, argv[optind], &list, &len, 0))) {
				if (ret != -ENOENT) {
					fprintf(stderr, "rdb_getnames_alloc() failed - %s\n", strerror(errno));
				}
				goto exit;
			}
			vptr = list;
			while ((tok = strtok( vptr, "&" ))) {
				vptr = NULL;
				delete_variable(tok);
			}
		} else {
			if( delete_variable(argv[optind]) != 0) { goto exit; }
		}
	}

	if ((mode == WAIT) || (mode == SET_WAIT)) {
		// select
		if(select(fd+1, &fdR, NULL, NULL, pTv) <= 0)
			goto exit;
	}

	rval = 0;
exit:
	free(value);
	free(list);
	if (rdb_s) {
		rdb_close(&rdb_s);
	}
	return rval;
}



/*
* vim:ts=4:sw=4:
*/
