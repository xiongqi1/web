/**
 * @file manufacture.c
 * Provides Web URLs for the manufacture tool to read information to verify
 * products in the final production stage.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "rdb_util.h"

/* --------------- Private data structures --------------- */
/* Structure containing a query and associated handler */
typedef struct json_handler {
    const char *query;
    void (*run)(void);
} json_handler_t;

/* --------------- Private function declaration --------------- */
static void print_element(const char *name, const char *value, int isLast);
static void print_rdb_element(const char *name, char *rdb, int isLast);
static const char *get_network_registration_status(void);
static void handle_version(void);
static void handle_status(void);

/* --------------- Private data initialisation --------------- */
static const json_handler_t json_handlers[] = {
    {
        "version",
        handle_version,
    },
    {
        "status",
        handle_status,
    },
};

#define MAX_NETWORK_STATUS_REGISTRATION_CODE 9
static const char *network_status_registration_descriptions[MAX_NETWORK_STATUS_REGISTRATION_CODE+1] = {
    "Not registered, searching stopped",
    "Registered, home network",
    "Not registered, searching",
    "Registration denied",
    "Unknown",
    "Registered, roaming",
    "Registered for SMS(home network)",
    "Registered for SMS(roaming)",
    "Emergency",
    "N/A",
};

/* --------------- Private function implementation --------------- */
/**
 * Prints a Json element with the given name and associated value.
 *
 * @param name The name of the element
 * @param value The value of the element
 * @param isLast Boolean Indicating whether or not it is the last element in the object.
 */
static void print_element(const char *name, const char *value, int isLast)
{
    printf("\t\"%s\": \"%s\"%s\n", name, value, isLast ? "" : ",");
}

/**
 * Prints a Json element with the given name and associated rdb.
 *
 * @param name The name of the element
 * @param rdb The name of RDB variable to be read
 * @param isLast Boolean Indicating whether or not it is the last element in the object.
 */
static void print_rdb_element(const char *name, char *rdb, int isLast)
{
    print_element(name, get_single(rdb), isLast);
}

/**
 * Get network registration status
 *
 * @retval String representing the network registration status.
 */
static const char *get_network_registration_status(void)
{
    const char *str;
    char *end_ptr;

    str = get_single("wwan.0.system_network_status.reg_stat");

    errno = 0;
    int code = strtol(str, &end_ptr, 0);
    if (errno == ERANGE || *end_ptr != '\0' || str == end_ptr ||
        code > MAX_NETWORK_STATUS_REGISTRATION_CODE) {
        return "Error";
    }

    return network_status_registration_descriptions[code];
}

/**
 * Prints version information as json formatted strings
 */
static void handle_version(void)
{
    printf("{\n");
    print_rdb_element("board_hw_ver", "system.hwver.hostboard", 0);
    print_rdb_element("class", "system.product.class", 0);
    print_rdb_element("fw_ver", "wwan.0.firmware_version", 0);
    print_rdb_element("hw_ver", "wwan.0.hardware_version", 0);
    print_rdb_element("imei", "wwan.0.imei", 0);
    print_rdb_element("imsi", "wwan.0.imsi.msin", 0);
    print_rdb_element("mac", "system.product.mac", 0);
    print_rdb_element("model", "system.product.model", 0);
    print_rdb_element("modem_hw_ver", "system.hwver.module", 0);
    print_rdb_element("serial_number", "system.product.sn", 0);
    print_rdb_element("skin", "system.product.skin", 0);
    print_rdb_element("sw_ver", "sw.version", 0);
    print_rdb_element("title", "system.product.title", 1);
    printf("}\n");
}

/**
 * Prints status information as json formatted strings
 */
static void handle_status(void)
{
    printf("{\n");
    print_rdb_element("cell_id", "wwan.0.system_network_status.CellID", 0);
    print_rdb_element("earfcn", "wwan.0.system_network_status.channel", 0);
    print_rdb_element("band", "wwan.0.system_network_status.current_band", 0);
    print_element("network_registration_status", get_network_registration_status(), 0);
    /*
     * TODO: Should be measured RSRP on each port, but they are not available
     * since a customised QMI message is not ready to collect them from modem.
     * Until it's ready, use main RSRP for both ports.
     */
    print_rdb_element("rsrp.0", "wwan.0.signal.0.rsrp", 0);
    print_rdb_element("rsrp.1", "wwan.0.signal.0.rsrp", 0);

    print_rdb_element("rsrq", "wwan.0.signal.rsrq", 0);
    print_rdb_element("sim_iccid", "wwan.0.system_network_status.simICCID", 1);
    printf("}\n");
}

/* --------------- Public function implementation --------------- */
/**
 * Main function
 */
int main(int argc, const char* argv[])
{
    printf("Content-Type: application/json\r\n\r\n");
    if (rdb_start()) {
        goto rdb_error;
    }

    /* only support GET method */
    const char *method = getenv("REQUEST_METHOD");
    if (!method || strncmp(method, "GET", 3)) {
        goto parameter_error;
    }

    int i;
    const char *query = getenv("QUERY_STRING");
    if (!query) {
        goto parameter_error;
    }
    for (i = 0; i < sizeof(json_handlers)/sizeof(json_handlers[0]); i++) {
        if (strncmp(query, json_handlers[i].query, strlen(json_handlers[i].query)+1) == 0) {
            json_handlers[i].run();
            break;
        }
    }

parameter_error:
    rdb_end();

rdb_error:
    printf("\n");

    return 0;
}
