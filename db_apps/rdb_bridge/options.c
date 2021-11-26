/*
 * Option handler for RDB bridge daemon
 *
 * Original codes copied from cdcs_apps/padd
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

#include "debug.h"
#include "options.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

int daemonized=0; /* if true, send output to syslog. */
int debug_mode=0; /* if false than debug messages get suppressed */

void DumpOptions(struct options *opt)
{
	MSG("debug_mode=%d",opt->debug_mode);
	MSG("daemonize=%d",opt->daemonize);

	if (opt->server) {
		MSG("Server %s:%s",opt->server,opt->tcp_port);
	} else {
		MSG("tcp_port=%s",opt->tcp_port);
	}
	MSG("retry_time=%d[sec]",opt->retry_time);
	MSG("keepalive=%d",opt->keepalive);
	MSG("keepalive_cnt=%d",opt->keepalive_cnt);
	MSG("client mode=%s",opt->client_mode ? "YES" : "NO");
}

/**
 * Initialize option structure
 *
 * @param   opt     Option structure pointer
 *
 * @retval  none
 */
static void InitOptions(struct options *opt)
{
	debug_mode      = DEFAULT_DEBUG;
	opt->debug_mode = DEFAULT_DEBUG;
	daemonized      = DEFAULT_DAEMON;
	opt->daemonize  = DEFAULT_DAEMON;
	opt->tcp_port   = DEFAULT_PORT;
	opt->server     = 0;
	opt->retry_time = DEFAULT_RTIME;
	opt->keepalive  = DEFAULT_KEEPALIVE;
	opt->keepalive_cnt = DEFAULT_KEEPALIVE_CNT;
	opt->client_mode = DEFAULT_MODE;
}

extern const char *version;

/**
 * Print out usage strings
 *
 * @param   message     Help string for invalid option error
 *
 * @retval  none        Exit the program
 */
static void Usage(const char *message, ...)
{
	fprintf(stderr,"\n");
	if (message) {
		va_list ap;
		va_start(ap, message);
		vfprintf(stderr,message,ap);
		va_end(ap);
		fprintf(stderr,"\n\n");
	}
	fprintf(stderr,
"Usage: rdb_bridge [options]\n"
"\n"
"    -h             = This stuff\n"
"    -d             = Debug mode\n"
"    -D             = Daemonise, debug goes to syslog\n"
"\n"
"    -p port        = TCP port for server mode(default=%s\n"
"    -c server:port = Server:port for client mode\n"
"    -s seconds     = Seconds between retries (default=%d)\n"
"    -k keepalive   = Seconds between TCP keepalive packets, 0 for off. (default=%d)\n"
"    -n count       = Number of failed keepalive probes before reconnecting. (default=%d)\n"
"\n"
"    Should run server mode or client mode exclusively.\n"
"\n"
"Version %s, Built %s %s\n"
		,DEFAULT_PORT,DEFAULT_RTIME,DEFAULT_KEEPALIVE,
		DEFAULT_KEEPALIVE_CNT,version,__DATE__,__TIME__);
	fprintf(stderr,"\n");
	exit(-1);
}

int HandleOptions(int argc, char *argv[], struct options *opt)
{
	int optc;
	int temp;
	char *cp;
	regex_t ip_host_regexp;

	/* Regular expression to match valid hostnames. '_' is not valid, but sometimes used. */
	if ((temp=regcomp(&ip_host_regexp,"^[-A-Za-z0-9._]+$",REG_EXTENDED|REG_NOSUB))) {
		ERR("regcomp() error %d",temp);
		return -1;
	}

	DBG("Scanning options");

	InitOptions(opt);
	while (((optc=getopt(argc,argv,"-hdDp:c:s:k:n:"))) != -1) {
		switch (optc) {
		case 'h' :
			Usage(NULL);
			break;
		case 'd' :
			opt->debug_mode = debug_mode = 1;
			break;
		case 'D' :
			opt->daemonize = daemonized = 1;
			break;

		case 'p' :
			temp = atoi(optarg);
			if (temp < 1 || temp > 65535) {
				Usage("\"%d\" is an invalid TCP port",temp);
			}
			opt->tcp_port = optarg;
			break;

		case 'c' :
			cp=strrchr(optarg,':');
			if (!cp) Usage("\"%s\" is not a valid IP address:port",optarg);
			*cp++ = '\0'; /* cp now points at the port number, optarg at the address */
			temp = regexec(&ip_host_regexp,optarg,0,NULL,0);
			if (temp!=0) {
				Usage("\"%s\" is not a valid host name or IP address",optarg);
			}
			temp = atoi(cp);
			if (temp < 1 || temp > 65535) {
				Usage("\"%d\" is an invalid TCP port",temp);
			}
			opt->tcp_port = cp;
			opt->server = optarg;
			/* It will overide to client mode even though -p option is given */
			opt->client_mode = 1;
			break;

		case 's' :
			opt->retry_time = atoi(optarg);
			break;

		case 'k' :
			opt->keepalive = atoi(optarg);
			break;

		case 'n' :
			opt->keepalive_cnt = atoi(optarg);
			break;

		default :
			Usage("Unknown option: %c\n",optc);
			break;
		}
	}

	regfree(&ip_host_regexp);

	return 0;
}
