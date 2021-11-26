/**
 * @file main.c
 * @brief Implements entry point to start applications
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
#include "app_config.h"
#include "app_api.h"
#ifndef PC_SIMULATOR
#include "rdb_app.h"
#endif
#include "authentication.h"
#include "scomm_manager.h"
#include "util.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/*******************************************************************************
 * Define internal macros
 ******************************************************************************/

/* Default logging values */
#define DEFAULT_LOG_CHANNEL 0
#define DEFAULT_LOG_LEVEL LOG_ERR_PRIO

/* The maximum number of port entries for routing the ports */
#define MAX_PORT_ENTRY 5

/*******************************************************************************
 * Declare internal structures
 ******************************************************************************/

/**
 * @brief Represents the port mapping to route between source and target ports
 */
typedef struct port_route {
	unsigned short source;
	unsigned short target;
} port_route_t;

/**
 * @brief The main structure of the application context
 */
typedef struct main_context {
	const char *device_name;
	const char *hostname;
	const char *serial_name;
#ifdef HAVE_AUTHENTICATION
	const char *auth_keyfile;
#endif
	unsigned int initial_baud_rate;
	unsigned int optimal_baud_rate;
	int log_channel;
	int log_level;
	unsigned int service_mask;

	port_route_t route_ports[MAX_PORT_ENTRY];
	int num_ports;

	pthread_t scomm_manager_thread;
} main_context_t;

/*******************************************************************************
 * Declare private functions
 ******************************************************************************/

static int main_init(main_context_t *ctx);
static void main_set_options(main_context_t *ctx, int argc, char *argv[]);
static int main_init_app(main_context_t *ctx);
static int main_start_app(main_context_t *ctx);
static void main_run(main_context_t *ctx);
static void usage(const char *name);

/*******************************************************************************
 * Declare static variables
 ******************************************************************************/

static main_context_t context;

/*******************************************************************************
 * Implement public functions
 ******************************************************************************/

int main(int argc, char *argv[])
{
	main_init(&context);
	main_set_options(&context, argc, argv);

	LOG_INFO("start running\n");
	main_run(&context);

	LOG_INFO("done\n");

	return 0;
}

int get_source_ports(unsigned short *sources, int max_source_num)
{
	int i;
	int num_ports = MIN(context.num_ports, max_source_num);

	for (i = 0; i < num_ports; i++) {
		sources[i] = context.route_ports[i].source;
	}

	return num_ports;
}

unsigned short get_target_port(unsigned short source)
{
	int i;

	for (i = 0; i < context.num_ports; i++) {
		if (context.route_ports[i].source == source) {
			return context.route_ports[i].target;
		}
	}

	return source;
}

unsigned int get_optimal_baud_rate(void)
{
	return context.optimal_baud_rate;
}

/*******************************************************************************
 * Implement private functions
 ******************************************************************************/

/**
 * @brief Initalises internal variables in the main application
 *
 * @param ctx The instance of the main application
 * @return 0 on success, or a negative value on error
 */
static int main_init(main_context_t *ctx)
{
	ctx->device_name = DEFAULT_DEVICE_NAME;
	ctx->hostname = DEFAULT_SOCKET_HOSTNAME;
	ctx->serial_name = DEFAULT_SERIAL_DEVICE_NAME;
#ifdef HAVE_AUTHENTICATION
	ctx->auth_keyfile = NULL;
#endif
	ctx->initial_baud_rate = DEFAULT_SERIAL_BAUD_RATE;
	ctx->optimal_baud_rate = 0; /* Default is none */
	ctx->log_channel = DEFAULT_LOG_CHANNEL;
	ctx->log_level = DEFAULT_LOG_LEVEL;
	ctx->service_mask = SERVICE_ID_MANAGEMENT_MASK;
	ctx->num_ports = 0;

	logger_init("ia_relay", ctx->log_channel, ctx->log_level, NULL);
	return 0;
}

/**
 * @brief Parses port arguments to configure the routing options
 *
 * @param ctx The instance of the main application
 * @param port_str The string which holds arguments for routing options
 * @return 0 on success, or a negative value on error
 */
static int main_set_port_option(main_context_t *ctx, char *port_str)
{
	char *str;

	if (ctx->num_ports >= MAX_PORT_ENTRY) {
		LOG_ERR("Too many ports are open\r\n");
		return -1;
	}

	str = strtok(port_str, ",");
	if (str) {
		ctx->route_ports[ctx->num_ports].source = atoi(str);
		if (ctx->route_ports[ctx->num_ports].source) {
			str = strtok(NULL, ",");
			if (str) {
				ctx->route_ports[ctx->num_ports].target = atoi(str);
				if (ctx->route_ports[ctx->num_ports].target) {
					LOG_INFO("Port Map[%d] Source:%d, Target:%d\n", ctx->num_ports,
							ctx->route_ports[ctx->num_ports].source,
							ctx->route_ports[ctx->num_ports].target);
					ctx->num_ports++;
				} else {
					LOG_ERR("Invalid target port\r\n");
					return -1;
				}
			} else {
				LOG_ERR("Invalid target port\r\n");
				return -1;
			}
		} else {
			LOG_ERR("Invalid source port\r\n");
			return -1;
		}
	} else {
		LOG_ERR("Invalid source port\r\n");
		return -1;
	}

	return 0;
}

/**
 * @brief Parses arguments to configure the applications before starting
 *
 * @param ctx The instance of the main application
 * @param argc The number of arguments
 * @param argv The string array to the arguments
 * @return 0 on success, or a negative value on error
 */
static void main_set_options(main_context_t *ctx, int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "STcdhli:s:b:o:p:n:k:")) != -1) {
		switch (ch) {
		case 'i':
			ctx->hostname = optarg;
			break;

		case 's':
			ctx->serial_name = optarg;
			break;

		case 'b':
			ctx->initial_baud_rate = atoi(optarg);
			break;

		case 'o':
			ctx->optimal_baud_rate = atoi(optarg);
			break;

		case 'c':
			ctx->log_channel |= LOGGER_STDIO;
			break;

		case 'l':
			ctx->log_channel |= LOGGER_SYSLOG;
			break;

		case 'd':
			if (ctx->log_level < LOG_DEBUG_PRIO) {
				ctx->log_level++;
			}
			break;

		case 'p':
			if (main_set_port_option(ctx, optarg)) {
				usage(argv[0]);
			}
			break;

		case 'n':
			ctx->device_name = optarg;
			break;

		case 'S':
			ctx->service_mask |= SERVICE_ID_SOCKET_MASK;
			break;

		case 'T':
			ctx->service_mask |= SERVICE_ID_TEST_MASK;
			break;

#ifdef HAVE_AUTHENTICATION
		case 'k':
			ctx->auth_keyfile = optarg;
			break;
#endif

		case 'h':
		default:
			usage(argv[0]);
			break;
		}
	}
}

/**
 * @brief Shows the help messages
 *
 * @param name The name of the application
 * @return Void
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n", name);
	fprintf(stderr, "\t -n Device name (default NRB-51)\n");
	fprintf(stderr, "\t -i Host name (default 127.0.0.1)\n");
	fprintf(stderr, "\t -b Initial baud rate (default 115200)\n");
	fprintf(stderr, "\t -o Optimal Baud rate\n");
	fprintf(stderr, "\t -s Serial device name (default /dev/ttyHSL0)\n");
	fprintf(stderr, "\t -p Port mapping (e.g. 80,8888 forward port 80 to 8888)\n");
	fprintf(stderr, "\t -S Activate socket service\n");
	fprintf(stderr, "\t -T Activate test service\n");
	fprintf(stderr, "\t -d More logging messages\n");
	fprintf(stderr, "\t -c Enable logging messages on console\n");
	fprintf(stderr, "\t -l Enable logging messages on syslog\n");
#ifdef HAVE_AUTHENTICATION
	fprintf(stderr, "\t -k The full path to the location of an RSA public key\n");
#endif
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "%s -n NRB-XXX -i 192.168.0.1 -o 921600 -s /dev/ttyUSB0 -p 80,8888"
					" -p 8080,8888 -d\n", name);
	exit(1);
}

/**
 * @brief Initialises the main application
 *
 * @param ctx The instance of the main application
 * @return 0 on success, or a negative value on error
 */
static int main_init_app(main_context_t *ctx)
{
	if ((ctx->log_level != DEFAULT_LOG_LEVEL) || (ctx->log_channel != DEFAULT_LOG_CHANNEL)) {
		logger_term();
		logger_init("ia_relay", ctx->log_channel, ctx->log_level, NULL);
	}

#ifndef PC_SIMULATOR
	if (rdb_app_init()) {
		LOG_ERR("RDB initialization failed\n");
		return -1;
	}
#endif

#ifdef HAVE_AUTHENTICATION
	if (authentication_init(ctx->auth_keyfile)) {
		LOG_ERR("Authentication initialization failed\n");
		return -1;
	}
#endif

	if (scomm_manager_init()) {
		return -1;
	}

	LOG_INFO("main_init_app done\n");
	return 0;
}

/**
 * @brief Starts the main application
 *
 * @param ctx The instance of the main application
 * @return 0 on success, or a negative value on error
 */
static int main_start_app(main_context_t *ctx)
{
	scomm_manager_option_t options;

	scomm_manager_init_options(&options);

	options.device_name = ctx->device_name;
	options.hostname = ctx->hostname;
	options.serial_name = ctx->serial_name;
	options.baud_rate = ctx->initial_baud_rate;
	options.service_mask = ctx->service_mask;
	scomm_manager_set_options(&options);

	if (scomm_manager_start()) {
		return -1;
	}

	LOG_INFO("main_start_app done\n");
	return 0;
}

/**
 * @brief Thread routine to run the scomm_manager_task thread
 *
 * @param param The instance of the main application
 * @return Void
 */
static void *scomm_manager_thread_routine(void *param)
{
	scomm_manager_main_run();
	return param;
}

/**
 * @brief Runs the threads and applications
 *
 * @param ctx The instance of the main application
 * @return Void
 */
static void main_run(main_context_t *ctx)
{
	if (main_init_app(ctx)) {
		fprintf(stderr, "Error initialising main\n");
		exit(1);
	}

	if (main_start_app(ctx)) {
		fprintf(stderr, "Error starting main\n");
		exit(1);
	}

	if (pthread_create(&ctx->scomm_manager_thread, NULL, scomm_manager_thread_routine, ctx)) {
		fprintf(stderr, "Error creating scomm_manager thread\n");
		exit(1);
	}

	if (pthread_join(ctx->scomm_manager_thread, NULL)) {
		fprintf(stderr, "Error joining scomm_manager thread\n");
		exit(1);
	}
}
