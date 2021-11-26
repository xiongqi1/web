/*
 * RDB bridge daemon
 *
 * Borrowed some codes from cdcs_apps/padd, db_apps/timedaemon
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

#include "options.h"
#include "rdb_bridge.h"
#include "debug.h"
#include "rbtreehash-dlist.h"

#include <daemon.h>
#include <rdb_ops.h>
#include <json/json.h>

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#if !defined (_PATH_DEVNULL)
#define _PATH_DEVNULL    "/dev/null"
#endif

#include <setjmp.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <poll.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdarg.h>

/* default config file */
#define DEF_CONFIG_FILE "/etc/rdb_bridge.conf"

/* this RDB will trigger config reload and perform initial sync */
#define CONFIG_FILE_RDB "service.rdb_bridge.configuration_file"

#define STATUS_RDB "service.rdb_bridge.connection_status"

/* Global data */
struct global G;

struct options options;

static struct rdb_session *rdb = NULL;
static struct strbt_t *rdb_rbt = NULL;
static struct timeval current_time;
static int current_time_valid = 1;

/* record active config file name so that we can check if config file changes */
static char *active_config_file = NULL;

const char *version = "1.0";

/**
 * Signal handler
 *
 * SIGUSR1 toggles debug mode
 *
 * @param   sig     Signal to handle
 *
 * @retval  none
 */
static void signal_handler(int sig)
{
    switch (sig)
    {
        case SIGUSR1:
            debug_mode = !debug_mode;
            MSG("Received SIGUSR1. toggle debug mode to %d", debug_mode);
            break;
        case SIGHUP:
            MSG("Received SIGHUP. Exiting...");
            G.ExitSig = sig;
            break;
        case SIGTERM:
            MSG("Received SIGTERM. Exiting...");
            G.ExitSig = sig;
            break;
        case SIGINT:
            MSG("Received SIGINT. Exiting...");
            G.ExitSig = sig;
            break;
        default:
            MSG("Received signal %d", sig);
            break;
    }
}

/**
 * Get current system up time
 *
 * @param   void
 *
 * @retval  secs     Current up time in second
 */
static long long up_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec;
}

/**
 * Get current monotonic clock time
 *
 * @param   curr     A pointer to time structure
 *
 * @retval  void
 */
static void get_current_time(struct timeval *curr)
{
    struct timespec timespec_result;

    if (clock_gettime(CLOCK_MONOTONIC, &timespec_result)) {
        ERR("failed to get current time, rate limit may not work");
        current_time_valid = 0;
        return;
    }
    curr->tv_sec = timespec_result.tv_sec;
    curr->tv_usec = timespec_result.tv_nsec/1000;
    current_time_valid = 1;
}

/**
 * Calculate elpased time
 *
 * @param   last     Last time structure
 *
 * @retval  Elapsed time in millisecond
 */
static long int elapsed_time(struct timeval last)
{
    struct timeval elapsed;
    elapsed.tv_sec = current_time.tv_sec - last.tv_sec;
    elapsed.tv_usec = current_time.tv_usec - last.tv_usec;
    return (elapsed.tv_sec*1000 + elapsed.tv_usec/1000);
}

/**
 * Check if the line is blank
 *
 * @param   str     line string
 *
 * @retval  0       not blank line
 * @retval  1       blank line
 */
static int is_blank_line(const char *str)
{
    while(*str) {
        if(!isspace(*str++)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Clean up local memory
 *
 * @param   void
 *
 * @retval  void
 */
static void fini_locals(void)
{
    struct rdb_var *rdb_var_p;
    const char *rdb_rbt_var;
    int i;

    if(rdb_rbt) {
        rdb_var_p = strbt_get_first(rdb_rbt, &rdb_rbt_var);
        while(rdb_var_p) {
            free(rdb_var_p->name);
            free(rdb_var_p->alias_name);
            for (i = 0; rdb_var_p->child[i]; i++) {
                free(rdb_var_p->child[i]);
            }
            rdb_var_p = strbt_get_next(rdb_rbt, &rdb_rbt_var);
        }
        strbt_destroy(rdb_rbt);
    }

    if(rdb) {
        rdb_close(&rdb);
    }
}

/**
 * Free JSON object if allocated
 *
 * @param   jobj     A pointer to the JSON object
 *
 * @retval  void
 */
static void free_json_obj(struct json_object *jobj)
{
    if (jobj != NULL) {
        json_object_put(jobj);
    }
}

/**
 * Update RDB variable with given value, create if not exist before updating
 *
 * @param   var     A pointer to RDB variable name
 * @param   val     A pointer to RDB value
 *
 * @retval  rc      result of RDB update/create
 */
static int update_rdb_str(const char *var, const char *val)
{
    int rc;

    if( (rc = rdb_set_string(rdb, var, val)) < 0 ) {
        if(rc == -ENOENT) {
            rc = rdb_create_string(rdb, var, val, 0, 0);
        }
    }

    if(rc < 0) {
        ERR("failed to update rdb(%s) - %s", val, strerror(errno));
        return rc;
    }
    DBG("updated rdb '%s' to '%s'", var, val);
    return rc;
}

static int update_rdb(const char *var, const char *val, int len)
{
    int rc;

    if( (rc = rdb_set(rdb, var, val, len)) < 0 ) {
        if(rc == -ENOENT) {
            rc = rdb_create(rdb, var, val, len, 0, 0);
        }
    }

    if(rc < 0) {
        ERR("failed to update rdb '%s' - %s", var, strerror(errno));
        return rc;
    }
    DBG("updated rdb '%s' to '%.*s'", var, len, val);
    return rc;
}

/**
 * Delete RDB variable
 *
 * @param   var     A pointer to RDB variable name
 *
 * @retval  rc      result of RDB delete
 */
static int delete_rdb(const char *var)
{
    int rc;

    if( (rc = rdb_delete(rdb, var)) < 0 ) {
        ERR("failed to delete rdb(%s) - %s", var, strerror(errno));
    } else {
        DBG("deleted rdb '%s'", var);
    }
    return rc;
}

/**
 * Delete non-existing dynamic RDB variables
 *
 * @param   format  A pointer to dynamic RDB variable format
 *                  ex) service.ttest.ftp.1.res.[x].duration
 * @param   max_index  Maximum index count
 *                  This max_index field is used to clean up the out-of-range
 *                  leftover variables in remote side. For example, if previous
 *                  service.ttest.ftp.0.repeats was 5 then the remote side will still have
 *                  service.ttest.ftp.0.res.3.ABC and service.ttest.ftp.0.res.4.ABC
 *                  after service.ttest.ftp.0.repeats changes to 3. By informing current
 *                  last index count the remote side can delete service.ttest.ftp.0.res.[x].ABC
 *                  where [x] is greater than 2. As a result, RDB variables with its indices
 *                  0 to max_index-1 are left.
 *
 * @retval  rc      result of RDB delete
 */
static int delete_obsolete_dynamic_rdbs(const char *format, int max_index)
{
    int rc, i, len, nFlags;
    char *token, *token2, *formatP;
    char var[RDB_NAME_MAX_LEN];

    formatP = strdup(format);
    if (!formatP) {
        ERR("failed to allocate memory for format string '%s'", format);
        return -1;
    }
    token = strtok_r(formatP, "[x]", &token2);
    token2 += 2;
    for (i = max_index; ; i++) {
        sprintf(var, "%s%d%s", token, i, token2);
        rc = rdb_getflags(rdb, var, &nFlags);
        if (rc == -ENOENT) {
            break;
        } else if (rc < 0) {
            ERR("failed to get rdb(%s) - %s but try to delete", var, strerror(errno));
        }
        delete_rdb(var);
    }
    free(formatP);
    return rc;
}

/**
 * Create RDB RBT and subscribe all necessary RDB variables
 *
 * @param   void
 *
 * @retval  0       success
 * @retval  -1      error
 */
static int create_rdb_rbt_n_subscribe(void)
{
    FILE *fp = NULL;
    char *line_buf = NULL, *rdb_name = NULL, *alias_name = NULL;
    char *rdb_val = NULL;
    const char *config_file;
    int len;
    int rate_limit, subset_num, force_sync;
    int c, i, line_no, group_flag, sub_idx;
    struct rdb_var temp_rdb_var;
    struct rdb_var *rdb_var_p;
    int ret = -1;
    const char *rdb_rbt_var;

    memset(&temp_rdb_var, 0, sizeof(struct rdb_var));

    /* Cleanup for previous config before loading new one */
    if (rdb_rbt) {
        MSG("cleanup for active config %s", active_config_file);
        rdb_var_p = strbt_get_first(rdb_rbt, &rdb_rbt_var);
        while (rdb_var_p) {
            rdb_unsubscribe(rdb, rdb_rbt_var);
            free(rdb_var_p->name);
            free(rdb_var_p->alias_name);
            for (i = 0; rdb_var_p->child[i]; i++) {
                free(rdb_var_p->child[i]);
            }
            rdb_var_p = strbt_get_next(rdb_rbt, &rdb_rbt_var);
        }
        strbt_destroy(rdb_rbt);
        rdb_rbt = NULL;
    }
    if (active_config_file) {
        free(active_config_file);
        active_config_file = NULL;
    }

    /* Create rdb_rbt */
    if(!(rdb_rbt = strbt_create(sizeof(struct rdb_var)))) {
        ERR("failed to create rdb_rbt");
        goto _fini;
    }

    /* Allocate buffers */
    line_buf = malloc(CONFIG_MAX_LINE_LEN);
    rdb_name = malloc(RDB_NAME_MAX_LEN);
    alias_name = malloc(RDB_NAME_MAX_LEN);
    rdb_val = malloc(RDB_VALUE_MAX_LEN);

    if(!line_buf || !rdb_name || !alias_name || !rdb_val) {
        ERR("memory allocation failure - %s", strerror(errno));
        goto _fini;
    }

    len = RDB_VALUE_MAX_LEN;
    ret = rdb_get_alloc(rdb, CONFIG_FILE_RDB, &rdb_val, &len);
    if (ret || rdb_val[0] == '\0') {
        MSG("RDB %s is invalid - use default %s", CONFIG_FILE_RDB, DEF_CONFIG_FILE);
        config_file = DEF_CONFIG_FILE;
    } else {
        MSG("Use RDB config %s", rdb_val);
        config_file = rdb_val;
    }
    /* Open configuration file */
    fp = fopen(config_file, "r");
    if(!fp) {
        ERR("failed to open configuration file (%s) - %s\n", config_file, strerror(errno));
        goto _fini;
    }

    /* Read configuration file */
    line_no = group_flag = sub_idx = 0;
    while( fgets(line_buf, CONFIG_MAX_LINE_LEN, fp) ) {
        line_no++;

        /* Cut off comment */
        i = 0;
        while(line_buf[i] && (line_buf[i] != '#')) {
            i++;
        }
        line_buf[i] = 0;

        /* Bypass blank line */
        if(is_blank_line(line_buf)) {
            continue;
        }

        /*
        * RDB bridge config file format : refer to rdb_bridge_xxxx.conf
        *
        * rdb name    rate limit      subset number     alias name
        *
        * Alias name can be used to synchronise different RDB name with triggered variable name.
        * For example, if daemon-A is triggered by sw.version and its alias name is owa.sw.version
        * then daemon-A sends owa.sw.version and its value to daemon-B for synchronisation. This is
        * only effective for parent variables.
        *
        * '[x]' can be used as wild character for multiple index variables and it should not
        * be used in single variable or first (parent) variable of group variables. That means
        * it should be used with subset number 0 only. A content of the variable just one before the
        * first multiple index variable is used as maximum index count.
        * Example:
        *     service.ttest.ftp.0.current_repeat                               0      8        1
        *     service.ttest.ftp.0.repeats                                      0      0        1
        *     service.ttest.ftp.0.res.[x].duration                             0      0        1
        * In above example, if service.ttest.ftp.0.repeats is 2 then it indicates there are
        * service.ttest.ftp.0.res.0.duration and service.ttest.ftp.0.res.1.duration existing.
        *
        * Rate limit option can be set to any value between 0~3600000(ms).
        * If the rate is 1000 ms and the variable is changed again within same 1000 ms time period
        * then the synchronisation will be delayed until the next 1000 ms time slot.
        *
        * Group option can be used to reduce the number of triggering variables.
        * For example, to synchronise manual cell measurement data,
        * subscribe key variable, wwan.0.manual_cell_meas.qty only.
        * If this variable changes then read wwan.0.manual_cell_meas.[x]
        * and synchronise them all together.
        *
        * To identify the group and single variable, the subset number is used as below;
        *    subset number   description
        *        1           single variable, there is no subset variable
        *        > 1         parent variable of the subset variables
        *                    also indicate total number of subset variable including parent one
        *        0           a child variable in a subset, which marked by nearest parent variable
        *
        * The rate limit of child is set same as its parent internally.
        *
        * Force Sync option is to indicate that the variable should be synchronised after connecting to
        * remote. If this flag is not set then the variable is exlcuded from initial synchronisation
        * unless the variable is triggered before the connection is established.
        */

        memset(alias_name, 0, RDB_NAME_MAX_LEN);
        c = sscanf(line_buf, "%"STR(RDB_NAME_MAX_LEN)"s %d %d %d %"STR(RDB_NAME_MAX_LEN)"s", rdb_name, &rate_limit, &subset_num, &force_sync, alias_name);
        if(c < 4) {
            ERR("incorrect format found in line %d (%s), c=%d", line_no, config_file, c);
            goto _fini;
        }

        /* No fixed rate limit but check reasonable max limit, 1 hour */
        if (rate_limit < 0 || rate_limit > 60*60*1000) {
            ERR("incorrect rate limit %d, skip this line", rate_limit);
            goto _fini;
        }

        if (subset_num == 1) {
            group_flag = sub_idx = 0;
        } else if (subset_num > 1) {
            group_flag = 1;
            sub_idx = 0;
        } else if (subset_num != 0 ) {
            ERR("incorrect subset number %d", subset_num);
            goto _fini;
        }

        DBG("name=%s,alias_name=%s,rate limit=%d,subset num=%d,force sync=%d,group_flag=%d,sub_idx %d",
            rdb_name, alias_name, rate_limit, subset_num, force_sync, group_flag, sub_idx);

        /* Do not initialize variable struct for sub variable */
        if (subset_num > 0) {
            memset(&temp_rdb_var, 0, sizeof(struct rdb_var));
            temp_rdb_var.name = strdup(rdb_name);
            if (!temp_rdb_var.name) {
                ERR("failed to allocate %d bytes for name buffer", RDB_NAME_MAX_LEN);
                goto _fini;
            }
            if (alias_name[0]) {
                temp_rdb_var.alias_name = strdup(alias_name);
                if (!temp_rdb_var.alias_name) {
                    ERR("failed to allocate %d bytes for name buffer", RDB_NAME_MAX_LEN);
                    goto _fini;
                }
            }
            temp_rdb_var.rate_limit = rate_limit;
            temp_rdb_var.force_sync = force_sync;
            temp_rdb_var.last_time.tv_sec = 0;
            temp_rdb_var.last_time.tv_usec = 0;
            temp_rdb_var.var_num = subset_num;
        } else if (sub_idx < MAX_CHILD_VARS) {
            temp_rdb_var.child[sub_idx] = strdup(rdb_name);
            if (!temp_rdb_var.child[sub_idx]) {
                ERR("failed to allocate %d bytes for name buffer", RDB_NAME_MAX_LEN);
                goto _fini;
            }
            sub_idx++;
        } else {
            ERR("subset index %d exceed limit", sub_idx);
            goto _fini;
        }

        /* Add to rdb_rbt if single variable or after filling all subset variables */
        if (subset_num == 1 || sub_idx == temp_rdb_var.var_num - 1) {
            DBG("adding '%s' to rdb rbt", temp_rdb_var.name);
            if( strbt_add(rdb_rbt, temp_rdb_var.name, &temp_rdb_var ) < 0 ) {
                ERR("failed to add a RDB variable in line %d (%s)", line_no, config_file);
                goto _fini;
            }
        }
    } /* while */

    /* Get first RDB variable from table */
    if(!(rdb_var_p = strbt_get_first(rdb_rbt, &rdb_rbt_var))) {
        ERR("No RDB variable found in %s", config_file);
        goto _fini;
    }

    /* Subscribe RDB variables */
    while(rdb_var_p) {
        DBG("subscribing %s", rdb_var_p->name);
        ret = rdb_subscribe(rdb, rdb_rbt_var);
        if( ret == -ENOENT ) {
            DBG("rdb variable(%s) does not exist yet", rdb_rbt_var);
            /* set to 0 to keep going through */
            ret = 0;
        } else if (ret < 0) {
            ERR("failed to subscribe rdb variable(%s) - %s", rdb_rbt_var, strerror(errno));
            goto _fini;
        }
        rdb_var_p = strbt_get_next(rdb_rbt, &rdb_rbt_var);
    }
    ret = 0;

    /* record the name of current config file */
    active_config_file = strdup(config_file);
    if (!active_config_file) {
        ERR("failed to allocate memory for active_config_file");
        ret = -ENOMEM;
    }

_fini:
    if (fp) {
        fclose(fp);
    }
    free(line_buf);
    free(rdb_name);
    free(alias_name);
    free(rdb_val);
    if (ret < 0) {
        free(temp_rdb_var.name);
        free(temp_rdb_var.alias_name);
        for (i = 0; temp_rdb_var.child[i]; i++) {
            free(temp_rdb_var.child[i]);
        }
    }
    return ret;
}

/**
 * Parse received JSON format packet and update RDB variables
 *
 * @param   jobj     A pointer to the JSON object
 *
 * @retval  void
 */
static void handle_received_packet(struct json_object *jobj)
{
    enum json_type type;
    struct array_list *subgroup_patterns;
    struct rdb_var *rdb_var_p;
    const char *value, *action, *max_index, *format;
    int max_index_cnt = -1;
    int rdb_locked;
    int i, len, error = 0;

    /* Lock RDB during received packet process for atomic update */
    rdb_locked = (rdb_lock(rdb, 0) == 0);

    /* Parsing JSON packet */
    json_object_object_foreach(jobj, key, val) {
        type = json_object_get_type(val);
        switch (type) {
            /* Value field is object type */
            case json_type_object:
                value = action = max_index = format = subgroup_patterns = NULL;
                max_index_cnt = -1;
                json_object_object_foreach(val, child_key, child_val) {
                    type = json_object_get_type(child_val);
                    switch (type) {
                        case json_type_string:
                            if (!strcmp(child_key, "value")) {
                                len = json_object_get_string_len(child_val);
                                value = json_object_get_string(child_val);
                            } else if (!strcmp(child_key, "action")) {
                                action = json_object_get_string(child_val);
                            } else if (!strcmp(child_key, "max_index")) {
                                max_index = json_object_get_string(child_val);
                                max_index_cnt = atoi(max_index);
                            }
                            break;
                        case json_type_array:
                            if (!strcmp(child_key, "subgroup_patterns")) {
                                subgroup_patterns = json_object_get_array(child_val);
                                if (!subgroup_patterns) {
                                    ERR("failed to get array");
                                    error = 1;
                                }
                            }
                            break;
                        default:
                            ERR("not supporting JSON type %d", type);
                            error = 1;
                            break;
                    }
                }
                DBG("key = '%s', val = '%s', action = '%s', maxindex = '%s', error = %d", key,
                    value? value:"", action?action:"",  max_index?max_index:"", error);
                if (error) {
                    break;
                }

                /* Delete non-existing dynamic variables */
                if (subgroup_patterns && max_index_cnt >= 0) {
                    for (i = 0; i < subgroup_patterns->length; i++) {
                        format = json_object_get_string(array_list_get_idx(subgroup_patterns, i));
                        DBG("subgroup_patterns[%d] = '%s'", i, format);
                        delete_obsolete_dynamic_rdbs(format, max_index_cnt);
                    }
                }
                if (action && !strcmp(action, "delete")) {
                    delete_rdb(key);
                } else if (value) {
                    if((rdb_var_p = strbt_find(rdb_rbt, key)) && !rdb_var_p->alias_name) {
                        ERR("'%s' is subscribed by this daemon already so can't be synchronised by remote", key);
                        error = 1;
                        break;
                    }
                    update_rdb(key, value, len);
                }
                break;
            default:
                ERR("not supporting JSON type %d", type);
                error = 1;
                break;
        }
        if (error) {
            break;
        }
    }

    if (rdb_locked) {
        rdb_unlock(rdb);
    }

}

/**
 * Read a parent RDB variable and its child variables then add to JSON object
 *
 * @param   rdb_var_p   A pointer to rdb_var struct
 * @param   jobj        A pointer to json_object struct
 *                      It is Null in offline mode where only force sync flag is set
 *                      without filling JSON object.
 *
 * @param[out] jobj     Filled with RDB variables and its values
 * @retval  void
 */
static void read_variable(struct rdb_var *rdb_var_p, json_object *jobj)
{
    char *buf;
    char buf2[RDB_NAME_MAX_LEN];
    char *valueP = malloc(RDB_VALUE_MAX_LEN);
    int i, j, len, ret, delete = 0, max_index = -1;
    char *token, *token2, *max_index_cp;
    long int elapsed;
    json_object *child_jobj, *index_jobj, *index_jobj_val, *array_jobj;

    child_jobj = index_jobj = index_jobj_val = array_jobj = NULL;

    if (!valueP) {
        ERR("failed to allocate %d bytes", RDB_VALUE_MAX_LEN+1);
        goto error_ret;
    }

    len = RDB_VALUE_MAX_LEN;
    ret = rdb_get_alloc(rdb, rdb_var_p->name, &valueP, &len);
    if (ret == -ENOENT) {
        DBG("'%s' does not exist", rdb_var_p->name);
        len = 0;
        delete = 1;
    } else if (ret){
        ERR("Failed to read '%s', error = %d", rdb_var_p->name, ret);
        goto error_ret;
    }
    DBG("read parent rdb '%s' = '%.*s', len = %d, alias '%s', var_num = %d, force sync %d",
        rdb_var_p->name, len, len? valueP:"", len, rdb_var_p->alias_name, rdb_var_p->var_num, rdb_var_p->force_sync);

    /* Json parameter is NULL when read_variable() is called in offline mode.
     * Set force sync flag for the parent variable and return immediately here.
     * All the variables with this force sync flag set will be synchronised
     * in do_first_sync() once the connection is established to remote later.
     */
    if (!jobj) {
        if (rdb_var_p->force_sync == 0) {
            DBG("'%s' is triggered in offline mode, set force sync to 2 for later initial sync",
                rdb_var_p->name);
            rdb_var_p->force_sync = 2;
        }
        free(valueP);
        return;
    }

    if (rdb_var_p->last_time.tv_sec != 0 || rdb_var_p->last_time.tv_usec != 0) {
        elapsed = elapsed_time(rdb_var_p->last_time);
        DBG("elapsed time = %ld > rate_limit = %d ? current_time_valid = %d",
            elapsed, rdb_var_p->rate_limit, current_time_valid);
        /* If current time is invalid then ignore rate limit */
        if (elapsed < rdb_var_p->rate_limit && current_time_valid) {
            if (rdb_var_p->triggered) {
                DBG("triggered flag was set already, %d delayed trigger variables",
                    G.delayed_trigger);
            } else {
                rdb_var_p->triggered = 1;
                G.delayed_trigger++;
                DBG("increase delayed trigger variable count, %d delayed",
                    G.delayed_trigger);
            }
            goto error_ret;
        }
    }
    rdb_var_p->last_time.tv_sec = current_time.tv_sec;
    rdb_var_p->last_time.tv_usec = current_time.tv_usec;

    child_jobj = json_object_new_object();
    if (!child_jobj) {
        ERR("failed to get new JSON object");
        goto error_ret;
    }

    if (delete) {
        json_object_object_add(child_jobj, "action", json_object_new_string("delete"));
    } else {
        json_object_object_add(child_jobj, "value", json_object_new_string_len(valueP, len));
    }

    /* Add first variable which can be single or first of group
     * If alias name exists then send the alias name instead of original name.
     */
    if (rdb_var_p->alias_name) {
        json_object_object_add(jobj, rdb_var_p->alias_name, child_jobj);
    } else {
        json_object_object_add(jobj, rdb_var_p->name, child_jobj);
    }

    /* Read out all child variables add to JSON object */
    for (i = 0; i < rdb_var_p->var_num - 1; i++) {
        /* Find out maximum index count if the variable has wild index number [x] */
        if (max_index < 0 && strstr(rdb_var_p->child[i], "[x]")) {
            if (i == 0) {
                /* If first child is dynamic variable then parent var has index count */
                if (rdb_var_p->alias_name) {
                    ret = json_object_object_get_ex(jobj, rdb_var_p->alias_name, &index_jobj);
                } else {
                    ret = json_object_object_get_ex(jobj, rdb_var_p->name, &index_jobj);
                }
            } else {
                /* Previous child variable has index count */
                ret = json_object_object_get_ex(jobj, rdb_var_p->child[i-1], &index_jobj);
            }
            if (!ret) {
                ERR("failed to get JSON object with index count");
                goto error_ret;
            }
            ret = json_object_object_get_ex(index_jobj, "action", &index_jobj_val);
            if (ret && !strcmp(json_object_get_string(index_jobj_val), "delete")) {
                /* If index variable is deleted then set max_index_cp to 0 */
                max_index_cp = "0";
            } else {
                ret = json_object_object_get_ex(index_jobj, "value", &index_jobj_val);
                if (!ret) {
                    ERR("failed to get value of JSON object with index count");
                    goto error_ret;
                }
                max_index_cp = json_object_get_string(index_jobj_val);
                /* If index variable is empty string or 0 then set max_index_cp to 0 */
                if (!strcmp(max_index_cp, "")) {
                    max_index_cp = "0";
                }
            }
            /* Add max_index field to index object because the value field could be empty
             * or not existing if the variable is deleted.
             * This max_index field is used to clean up the out-of-range
             * leftover variables in remote side. For example, if previous
             * service.ttest.ftp.0.repeats was 5 then the remote side will still have
             * service.ttest.ftp.0.res.3.ABC and service.ttest.ftp.0.res.4.ABC
             * after service.ttest.ftp.0.repeats changes to 3. By informing current
             * last index count the remote side can delete service.ttest.ftp.0.res.[x].ABC
             * where x starts from 3 and until the last existing variable */
            max_index = atoi(max_index_cp);
            DBG("found max index count %d", max_index);
            json_object_object_add(index_jobj, "max_index", json_object_new_string(max_index_cp));

            /* Get new JSON array object for subgroup patterns */
            array_jobj = json_object_new_array();
            if (!array_jobj) {
                ERR("failed to get new JSON array object");
                goto error_ret;
            }
        }
        delete = 0;
        if (strstr(rdb_var_p->child[i], "[x]")) {
            /* Add dynamic child variable patter to JSON array object */
            ret = json_object_array_add(array_jobj, json_object_new_string(rdb_var_p->child[i]));
            if (ret < 0) {
                ERR("failed to add child pattern to JSON array");
                goto error_ret;
            }

            buf = strdup(rdb_var_p->child[i]);
            if (!buf) {
                ERR("failed to allocate memory");
                goto error_ret;
            }
            token = strtok_r(buf, "[x]", &token2);
            token2 += 2;
            for (j = 0; j < max_index; j++) {
                ret = snprintf(buf2, RDB_NAME_MAX_LEN, "%s%d%s", token, j, token2);
                if (ret >= RDB_NAME_MAX_LEN) {
                    ERR("buffer size %d is not enough, variable name may be truncated", RDB_NAME_MAX_LEN);
                    free(buf);
                    goto error_ret;
                } else if (ret < 0) {
                    ERR("failed to copy variable name");
                    free(buf);
                    goto error_ret;
                }
                ret = rdb_get_alloc(rdb, buf2, &valueP, &len);
                /* Set deletion flag if the variable does not exist in order to
                 * synchronise with remote RDB bridge daemon */
                if (ret == -ENOENT) {
                    DBG("child rdb '%s' does not exist", buf2);
                    len = 0;
                    delete = 1;
                } else if (ret){
                    DBG("failed to read child rdb '%s'", buf2);
                    continue;
                }
                DBG("read child rdb '%s' = '%.*s', len = %d", buf2, len, len? valueP:"", len);
                child_jobj = json_object_new_object();
                if (!child_jobj) {
                    ERR("failed to get new JSON object");
                    free(buf);
                    goto error_ret;
                }
                if (delete) {
                    json_object_object_add(child_jobj, "action", json_object_new_string("delete"));
                } else {
                    json_object_object_add(child_jobj, "value", json_object_new_string_len(valueP, len));
                }
                json_object_object_add(jobj, buf2, child_jobj);
            }
            free(buf);
        } else {
            ret = rdb_get_alloc(rdb, rdb_var_p->child[i], &valueP, &len);
            if (ret == -ENOENT) {
                DBG("child rdb '%s' does not exist", rdb_var_p->child[i]);
                len = 0;
                delete = 1;
            } else if (ret){
                DBG("failed to read child rdb '%s'", rdb_var_p->child[i]);
                continue;
            }
            DBG("read child rdb '%s' = '%.*s', len = %d", rdb_var_p->child[i], len, len? valueP:"", len);
            child_jobj = json_object_new_object();
            if (!child_jobj) {
                ERR("failed to get new JSON object");
                goto error_ret;
            }
            if (delete) {
                json_object_object_add(child_jobj, "action", json_object_new_string("delete"));
            } else {
                json_object_object_add(child_jobj, "value", json_object_new_string_len(valueP, len));
            }
            json_object_object_add(jobj, rdb_var_p->child[i], child_jobj);
        }
    }

    /* Add JSON array object to index object */
    if (index_jobj && array_jobj) {
        json_object_object_add(index_jobj, "subgroup_patterns", array_jobj);
    }

    /* Decrease reserved trigger variable count */
    if (rdb_var_p->triggered) {
        G.delayed_trigger--;
        rdb_var_p->triggered = 0;
        DBG("decrease delayed trigger variable count, %d delayed", G.delayed_trigger);
    }
    goto normal_ret;
error_ret:
    /* Increase reserved trigger variable count and set triggered flag
     * if error occured to process triggered RDB variable in order to
     * process this variable in next polling */
    if (!rdb_var_p->triggered) {
        G.delayed_trigger++;
        rdb_var_p->triggered = 1;
        DBG("increase delayed trigger variable count, %d delayed", G.delayed_trigger);
    }
    free_json_obj(child_jobj);
    free_json_obj(array_jobj);
normal_ret:
    free(valueP);
    /* Restore to original force_sync value for next triggering */
    if (rdb_var_p->force_sync == 2) {
        rdb_var_p->force_sync = 0;
    }
}

/**
 * Convert JSON object to JSON string and send to remote daemon
 *
 * @param   jobj    A pointer to JSON object to send
 *
 * @retval  void
 */
static void send_json_string(json_object *jobj)
{
    const char *jstr;
    int len;

    len = json_object_object_length(jobj);
    if (len == 0) {
        DBG("ignore empty JSON obj");
        return;
    }

    /* Pretty format is better for debugging but occupy few more bytes */
    if (debug_mode) {
        jstr = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
    } else {
        jstr = json_object_to_json_string(jobj);
    }
    len = strlen(jstr);
    DBG("encoded JSON string = '%s', len = %d", jstr, len);

    /* Send JSON string to remote daemon for synchronisation */
    if (G.remotefd >= 0) {
        int rc;
        rc = send(G.remotefd, jstr, len, NULL);
        if (rc > 0) {
            DBG("sent %d bytes", rc);
        } else {
            DBG("failed to send : %s", strerror(errno));
        }
    }
}

static void do_first_sync_or_poll(int poll);

/**
 * Get triggered RDB variables, encode to JSON packet and send to remote
 *
 * @param   online  True if connected to remote. synchronise triggered variable
 *                  False if not connected to remote. Set triggered flag in order
 *                       to synchronise after connected to remote
 * @retval  void
 */
static void handle_triggered_rdb(int online)
{
    char *namesP = malloc(RDB_NAME_MAX_LEN);
    int names_len;
    const char *blink_str = "";
    char *token, *token2;
    char *saveptr;
    json_object *jobj = NULL;
    struct rdb_var *rdb_var_p;

    if (!namesP) {
        ERR("failed to allocate %d bytes", RDB_NAME_MAX_LEN);
        goto _fini;
    }
    if (online) {
        jobj = json_object_new_object();
        if (!jobj) {
            ERR("failed to allocate JSON object");
            goto _fini;
        }
    }

    /* Get triggered rdb names */
    names_len = RDB_NAME_MAX_LEN - 1;
    int rc = rdb_getnames_alloc(rdb, blink_str, &namesP, &names_len, TRIGGERED);
    if(rc) {
        ERR("rdb_getnames_alloc() failed - %s", strerror(errno));
        /* Ignore incorrect behaviour of RDB driver */
        names_len = 0;
        goto _fini;
    }

    namesP[names_len] = 0;
    DBG("rdb notify list - '%s'", namesP);

    get_current_time(&current_time);

    /* Get the first variable */
    token2 = strtok_r(namesP, "&", &saveptr);

    /* Read all rdb variables */
    while( (token = token2) != NULL ) {
        DBG("triggered rdb '%s'", token);
        token2 = strtok_r(NULL, "&", &saveptr);

        if (!strcmp(token, CONFIG_FILE_RDB)) {
            MSG("config file rdb %s triggered", CONFIG_FILE_RDB);
            int ret;
            int len = RDB_VALUE_MAX_LEN;
            char * rdb_val = malloc(RDB_VALUE_MAX_LEN);
            if (!rdb_val) {
                ERR("memory allocation failed - %s", strerror(errno));
                goto _fini;
            }
            ret = rdb_get_alloc(rdb, CONFIG_FILE_RDB, &rdb_val, &len);
            if (ret || rdb_val[0] == '\0') {
                MSG("config rdb %s is invalid - ignore trigger", CONFIG_FILE_RDB);
                free(rdb_val);
                continue;
            }
            if (!strcmp(rdb_val, active_config_file)) {
                MSG("config file '%s' is not changed - ignore", rdb_val);
                free(rdb_val);
                continue;
            }
            MSG("reloading config %s", rdb_val);
            ret = create_rdb_rbt_n_subscribe();
            if (ret) {
                ERR("failed to reload config %s", rdb_val);
            } else {
                MSG("config %s reloaded", rdb_val);
                if (online) {
                    /* do an initial sync immediately */
                    do_first_sync_or_poll(0);
                } else {
                    /* clear flag so that upon connected, an initial sync will be done */
                    G.init_sync = 0;
                }
            }
            free(rdb_val);
            /* once config is reloaded, discard all other RDB triggers */
            goto _fini;
        }

        /* Find from RDB RBT */
        if( !(rdb_var_p = strbt_find(rdb_rbt,token)) ) {
            ERR("internal struct error - cannot find rdb(%s) in rbt", token);
            continue;
        }
        read_variable(rdb_var_p, jobj);
    }

    /* Send JSON string to remote */
    if (jobj) {
        send_json_string(jobj);
    }

_fini:
    free_json_obj(jobj);
    free(namesP);
}

/**
 * Do first synchronisation for all variables after connection or
 * poll periodically in order to fire reserved trigger event which is
 * delayed due to rate limit.
 *
 * @param   poll    1 for periodic polling, 0 for first sync
 *
 * @retval  void
 */
static void do_first_sync_or_poll(int poll)
{
    struct rdb_var *rdb_var_p;
    const char *rdb_rbt_var;
    json_object *jobj;

    if (poll) {
        if (G.delayed_trigger > 0) {
            DBG("periodic polling, %d delayed trigger variables", G.delayed_trigger);
        } else  {
            DBG("periodic polling, no delayed trigger variables");
            return;
        }
    } else {
        DBG("first sync...");
    }
    /* Get first RDB variable from table */
    if(!(rdb_var_p = strbt_get_first(rdb_rbt, &rdb_rbt_var))) {
        ERR("internal error, failed to get first RDB variable");
        return;
    }

    jobj = json_object_new_object();
    if (!jobj) {
        ERR("failed to allocate JSON object");
        return;
    }

    /* Walk through all variables */
    get_current_time(&current_time);
    while(rdb_var_p) {
        if ((poll && rdb_var_p->triggered ) ||
            (!poll && rdb_var_p->force_sync)) {
            read_variable(rdb_var_p, jobj);
            /* Reset delayed trigger flag for initial sync */
            if (!poll) {
                rdb_var_p->triggered = 0;
            }
        } else {
            DBG("skip sync or poll : name %s, force_sync %d, poll %d",
                rdb_var_p->name, rdb_var_p->force_sync, poll);
        }
        rdb_var_p = strbt_get_next(rdb_rbt, &rdb_rbt_var);
    }

    /* Send JSON string to remote */
    send_json_string(jobj);
    free_json_obj(jobj);

    /* Reset delayed trigger variable count for initial sync */
    if (!poll) {
        G.delayed_trigger = 0;
    }
}

/**
 * Set keep-alive options
 *
 * @param   sockfd        socket file descriptor number
 * @param   opt           option structure pointer
 *
 * @retval  void
 */
static void SetKeepalive(int sockfd, struct options *opt)
{
    int val = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &opt->keepalive, sizeof(opt->keepalive));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &opt->keepalive, sizeof(opt->keepalive));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &opt->keepalive_cnt, sizeof(opt->keepalive_cnt));
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

/**
 * Open server/client socket
 *
 * @param   opt         A pointer to option structure
 *
 * @retval  0           Success
 * @retval  < 0         Failed to open socket
 */
static int socket_open(struct options *opt)
{
    int sockfd, port_number;
    int one = 1;
    int retval, flags;
    struct addrinfo hints;
    struct addrinfo *addr;
    struct sockaddr_in *addr_in;
    struct sockaddr_in addrin = {0};

    if (opt->client_mode) {
        memset(&hints, 0, sizeof hints);
        hints.ai_flags = 0;
        hints.ai_family = AF_INET;
        hints.ai_protocol = 0;
        hints.ai_socktype = SOCK_STREAM;
        /* Create socket */
        sockfd = socket(PF_INET, hints.ai_socktype, IPPROTO_TCP);
        if (sockfd < 0) {
            ERR("Can't create socket for %s:%s, %s", opt->server, opt->tcp_port, strerror(sockfd));
            return sockfd;
        }
        DBG("Got socket %d", sockfd);

        /* Look up DNS */
        retval = getaddrinfo(opt->server, opt->tcp_port, &hints, &addr);
        if (retval != 0){
            ERR("getaddrinfo(%s:%s) failed: %s", opt->server, opt->tcp_port, gai_strerror(retval));
            close(sockfd);
            return retval;
        }

        addr_in = (struct sockaddr_in *)addr->ai_addr;
        MSG("Connecting to server at %s:%s (%s:%d)...",
                opt->server, opt->tcp_port,
                inet_ntoa(addr_in->sin_addr),ntohs(addr_in->sin_port));

        /* Connect to server */
        retval = connect(sockfd, addr->ai_addr, addr->ai_addrlen);
        if (retval != 0) {
            ERR("... connect failed: %s", strerror(errno));
            freeaddrinfo(addr);
            close(sockfd);
            return retval;
        }
        else {
           MSG("... Connected.");
        }
        SetKeepalive(sockfd, opt);
        freeaddrinfo(addr);
        G.remotefd = sockfd;
    } else {
        sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd < 0) {
            ERR("Can't create socket");
            return -1;
        }

        /* After program termination the socket is still busy for a while.
         * This allows an immediate restart of the daemon after termination. */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0) {
            ERR("Can't set SO_REUSEADDR on socket");
            return -1;
        }
        flags = fcntl(sockfd, F_GETFL);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        port_number = atoi(opt->tcp_port);
        addrin.sin_family = AF_INET;     /* host byte order */
        addrin.sin_port = htons(port_number);
        addrin.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(sockfd, (struct sockaddr *)&addrin, sizeof(struct sockaddr)) < 0) {
            ERR("Can't bind to port %d", port_number);
            return -1;
        }

        #define QLEN     5 /* Maximum requests queued in socket */
        if (listen(sockfd, QLEN)<0) {
            ERR("Listen() error");
            return -1;
        }

        G.masterfd = sockfd;
        MSG("Server attached to port %d, fd %d", port_number, G.masterfd);
    }
    return 0;
}

static void update_status(status_t status) {
    if (G.status == status) {
        return;
    }
    G.status = status;
    switch(status) {
        case CONNECTED:
            update_rdb_str(STATUS_RDB, "connected");
            break;
        case DISCONNECTED:
            update_rdb_str(STATUS_RDB, "disconnected");
            break;
        case SYNCHRONISED:
            update_rdb_str(STATUS_RDB, "synchronised");
            break;
        default:
            update_rdb_str(STATUS_RDB, "");
    }
}

/**
 * Select loop for incoming/outgoing packet, RDB trigger event
 *
 * @param   opt         A pointer to option structure
 *
 * @retval  void
 */
static void select_loop(struct options *opt)
{
    int rc;
    int newsock;
    int rdbfd;
    long long curr_time, last_time;

    update_status(DISCONNECTED);

    rdbfd = rdb_fd(rdb);
    while (!G.ExitSig) {
        struct json_tokener* tok = json_tokener_new();
        struct pollfd fds[] = {{
            .fd     = G.remotefd,
            .events = POLLIN,
        },{
            .fd     = rdbfd,
            .events = POLLIN,
        }};

        /* Client mode : reconnect to server if disconnected
         * Server mode : accept incoming client connection if master socket opened
         *               if master socket closed then try to open
         */
        curr_time = up_time();
        last_time = G.last_net + options.retry_time;
        if (curr_time > last_time) {
            if (opt->client_mode && G.remotefd < 0) {
                newsock = socket_open(opt);
                if (newsock < 0) {
                    ERR("Failed, err=%d", newsock);
                    G.remotefd = -1;
                } else {
                    DBG("Connected to %s:%s", opt->server, opt->tcp_port);
                    G.init_sync = 0;
                    update_status(CONNECTED);
                }
            } else if (opt->client_mode == 0 && G.masterfd < 0) {
                newsock = socket_open(opt);
                if (newsock < 0) {
                    ERR("Failed, err=%d", newsock);
                    G.masterfd = G.remotefd = -1;
                } else {
                    DBG("Opened Server master socket");
                    G.init_sync = 0;
                }
            }
            G.last_net = curr_time;
            DBG("last_net %lld, curr_time %lld", G.last_net, curr_time);
        }
        if (opt->client_mode == 0 && G.masterfd >= 0) {
            socklen_t sin_size = sizeof(struct sockaddr_in);
            struct sockaddr_in remote_addr;
            newsock = accept(G.masterfd, (struct sockaddr *)&remote_addr, &sin_size);
            if (newsock < 0) {
                /* Go through down to offline mode triggering check */
                DBG("New connection failed with %s.", strerror(errno));
            } else {
                MSG("New connection from %s:%d on fd=%d",
                    inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port), newsock);
                SetKeepalive(newsock, opt);
                /* Drop current connection if new connection incomes */
                if (G.remotefd >= 0) {
                    MSG("Dropping previous incoming (fd=%d)", G.remotefd);
                    close(G.remotefd);
                    G.init_sync = 0;
                }
                G.remotefd = newsock;
                update_status(CONNECTED);
            }
        }
        /* Update remote socket */
        if (G.remotefd >= 0) {
            fds[0].fd = G.remotefd;
        }

        /* Do first synchronisation */
        if (G.remotefd >= 0 && G.init_sync == 0) {
            do_first_sync_or_poll(0);
            G.init_sync = 1;
        }

        /* Exit if caught signal */
        if (G.ExitSig) {
            break;
        }

        /* Event polling loop */
        if (G.remotefd >= 0) {
            /* Process triggered RDB & incoming packet in online status */
            while (G.remotefd >= 0 && (rc = poll(fds, 2, 1000)) >= 0) {
                if (fds[0].revents & POLLIN) {
                    char buf[NETBUF_SIZE];
                    int len = recv(G.remotefd, buf, sizeof(buf), MSG_DONTWAIT);
                    if (len < 0 && errno != EAGAIN) {
                        ERR("recv error %d, (%s)", rc, strerror(errno));
                        break;
                    }
                    if (len == 0) {
                        /* disconnected */
                        DBG("remote disconnected");
                        break;
                    }
                    char *ptr = buf;
                    while (len > 0) {
                        DBG("TCP Rx from remote : %d bytes", len);
                        json_object* jobj = json_tokener_parse_ex(tok, ptr, len);
                        enum json_tokener_error jerr;
                        if (jobj) {
                            /* handle received json object */
                            handle_received_packet(jobj);
                            free_json_obj(jobj);
                        } else {
                            jerr = json_tokener_get_error(tok);
                            if(jerr != json_tokener_continue) {
                                /* error */
                                ERR("json tokener error: %s", json_tokener_error_desc(jerr));
                                break;
                            }
                        }
                        len -= tok->char_offset;
                        ptr += tok->char_offset;
                    }
                    update_status(SYNCHRONISED);
                }
                if (fds[1].revents & POLLIN) {
                    /* handle rdb notifications */
                    handle_triggered_rdb(1);
                }
                /* periodically poll for reserved trigger event due to rate limit */
                do_first_sync_or_poll(1);
            }
        } else {
            /* Process triggered RDB in offline status */
            while ((rc = poll(fds, 2, 1000)) >= 0) {
                if (fds[1].revents & POLLIN) {
                    /* handle rdb notifications */
                    handle_triggered_rdb(0);
                }
                if (rc == 0) {
                    break;
                }
            }
        }
        json_tokener_free(tok);
        close(G.remotefd);
        G.remotefd = -1;
        update_status(DISCONNECTED);
    }
    close(G.remotefd);
    close(G.masterfd);
}

int main(int argc, char *argv[])
{
    int ret;

    /* Handling option */
    if (HandleOptions(argc, argv, &options) < 0) {
        goto _fini;
    }
    if (options.daemonize) {
        daemon_init("rdb_bridge", NULL, 0, LOG_NOTICE);
        syslog(LOG_INFO, "daemonized");
    }
    MSG("rdb_bridge launching (V=%s, build %s %s)", version, __DATE__, __TIME__);
    DumpOptions(&options);
    GlobalInit(&G);

    /* Signals to handle. */
    signal (SIGINT,  signal_handler);
    signal (SIGHUP,  signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGQUIT, signal_handler);
    signal (SIGUSR1, signal_handler);

    /* open rdb database */
    ret = rdb_open(NULL, &rdb);
    if (ret < 0) {
        ERR("failed to open rdb driver - %s", strerror(errno));
        goto _fini;
    }

    if (create_rdb_rbt_n_subscribe() < 0) {
        ERR("failed to create rdb rbt");
        goto _fini;
    }

    /* subscribe to config file rdb so config reload can be triggered */
    ret = rdb_subscribe(rdb, CONFIG_FILE_RDB);
    if (ret < 0 && ret != -ENOENT) {
        goto _fini;
    }

    /* Socket open, keep going over error case for later retry chance */
    if (socket_open(&options) < 0) {
        ERR("Failed to open initial socket but keep going");
    }

    MSG("Start main Loop");
    select_loop(&options);

    MSG("rdb bridge daemon exits.");

    if (options.daemonize) {
        daemon_fini();
    }

_fini:
    fini_locals();
    return 0;
}
