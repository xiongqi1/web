/*
 * main.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * fmctl tool used to control FaultMgmt system.
 *
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>

#include "fm_errno.h"
#include "fm_supported_alarm.h"
#include "fault_mgmt.h"

#define APP_NAME                    "fmctl"

#define SUPPORTED_ALARM_CONFIG_FILE "/etc/supported_alarm.conf"

/* DEBUG is controlled in Makefile */
#if DEBUG
#define error(...)      fprintf(stderr,  __VA_ARGS__)
#define warn(...)       error(__VA_ARGS__)
#define notice(...)     error(__VA_ARGS__)
#define info(...)       error(__VA_ARGS__)
#define debug(...)      error(__VA_ARGS__)
#else
#define error(...)      syslog(LOG_ERR, __VA_ARGS__)
#define warn(...)       syslog(LOG_WARNING, __VA_ARGS__)
#define notice(...)     syslog(LOG_NOTICE, __VA_ARGS__)
#define info(...)       syslog(LOG_INFO, __VA_ARGS__)
#define debug(...)      syslog(LOG_DEBUG, __VA_ARGS__)
#endif


/**********************************************************************
 *
 * Config Parsing Functions
 *
 **********************************************************************/
static int remove_comment(char *string)
{
    int pos = 0;

    if (!string)
        return -1;

    while (string[pos] != '#' && string[pos])
        pos++;

    string[pos] = '\0';
    return pos;
}

static int remove_white_space(char *string)
{
    int start_pos = 0, end_pos;
    int len = 0, new_len = 0;

    if (!string)
        return -1;

    if ((len = strlen(string)) == 0)
        return 0;

    while (isspace(string[start_pos]))
    {
        start_pos++;
        if (start_pos == len)
        {
            /* The whole string is white space */
            string[0] = '\0';
            return 0;
        }
    }

    end_pos = len - 1;
    while(isspace(string[end_pos]) && end_pos > 0)
        end_pos--;

    new_len = (end_pos - start_pos) + 1;
    memcpy(string, string + start_pos, new_len);
    string[new_len] = '\0';
    return new_len;
}

static int count_char(const char *string, const char c)
{
    int count = 0;

    if (!string)
        return 0;

    do
    {
        count += (*string == c);
    } while(*string++ != '\0');
    return count;
}

static int find_flag_option_in_string(const char *string, const char *flag)
{
    char *start_pos = strcasestr(string, flag);
    int flag_len;
    if (!start_pos)
    {
        return 0;
    }

    /* Make sure the flag is an independent word */
    if (start_pos == string || (*(start_pos - 1) == ',' || isspace(*(start_pos - 1))))
    {
        flag_len = strlen(flag);
        if (*(start_pos + flag_len) == ',' || *(start_pos  + flag_len) == '\0' ||
            isspace(*(start_pos  + flag_len)))
        {
            return 1;
        }
    }

    return 0;
}

static int load_supported_alarm_from_string(const char *config, int *id, char *parameters[])
{
    int str_pos = 0, para_pos = -1, para_str_pos = 0;
    int content_started = 0;
    char id_str[16];

    if (!config || !id || !parameters)
        return -1;

    /* Read ID first */
    while(config[str_pos] != '|' && str_pos < sizeof(id_str) - 1)
    {
        id_str[str_pos] = config[str_pos];
        str_pos++;
    }
    id_str[str_pos++] = '\0';
    *id = atoi(id_str);

    /* Start paring paras */
    while(1)
    {
        /* Limit max length read */
        if (para_str_pos >= 0 && para_str_pos == ALARM_PARA_MAX_STR_LEN[para_pos] - 1)
            while(config[str_pos] != '|' && config[str_pos] != '\0')
                str_pos++;

        if (config[str_pos] == '|' || para_pos == -1)
        {
            if (para_pos >= 0)
            {
                parameters[para_pos][para_str_pos] = '\0';
                remove_white_space(parameters[para_pos]);
            }

            if (++para_pos >= SupportedAlarmParameterCount)
                break;
            para_str_pos = 0;
            content_started = 0;
            if ((parameters[para_pos] = malloc(ALARM_PARA_MAX_STR_LEN[para_pos])) == NULL)
                return -1;
        }
        else if (config[str_pos] == '\0')
        {
            parameters[para_pos][para_str_pos] = '\0';
            remove_white_space(parameters[para_pos]);
            break;
        }
        else
        {
            /* avoid white spaces */
            if (content_started || !isspace(config[str_pos]))
            {
                content_started = 1;
                parameters[para_pos][para_str_pos++] = config[str_pos];
            }
        }

        str_pos++;
    }

    return 0;
}

static int load_supported_alarm(const char *config_file)
{
    FILE *fptr;
    char *line = NULL;
    int i, len = 0, line_number = 0, loaded_count = 0;;

    SupportedAlarmInfo alarm_info;

    if (!config_file || access(SUPPORTED_ALARM_CONFIG_FILE, R_OK) != 0)
    {
        error("Invalid config file to load.");
        return -1;
    }

    if ((fptr = fopen(config_file, "r")) == NULL)
    {
        error("Failed to open config file %s", config_file);
        return -1;
    }

    while (getline(&line, (size_t *)&len, fptr) != -1)
    {
        line_number++;
        if (remove_comment(line) <= 0)
            continue;

        if ((len = remove_white_space(line)) <= 0)
            continue;

        /* make sure para count is correct, plus 1 which is the ID */
        if (count_char(line, '|') < SupportedAlarmParameterCount)
        {
            info("Skipped invalid config in config file line %d\n", line_number);
            continue;
        }

        if (load_supported_alarm_from_string(line, &alarm_info.id, alarm_info.parameters) < 0)
        {
            error("Failed loading supported alarm config from line %d\n", line_number);
            continue;
        }

        debug("From config (line %d): %s\n", line_number, line);
        if (add_supported_alarm(&alarm_info) == 0)
            loaded_count++;

        /* Release the memory */
        for (i = 0; i < SupportedAlarmParameterCount; i++)
        {
            if (alarm_info.parameters[i])
            {
                free(alarm_info.parameters[i]);
            }
        }
    }

    if(line)
        free(line);
    fclose(fptr);

    return loaded_count;
}

/**********************************************************************
 *
 * FaultMgmt Internal Functions
 *
 **********************************************************************/
/**
 * @brief      fm_boot is designed to be called once during each boot up to setup
 *             FaultMgmt system environment. The system will only work when
 *             fm_boot has been run properly.
 *
 * @return     0 if succeeded. others if failed.
 */
static int fm_boot(void)
{
    int ret, i;
    extern SupportedAlarmInfo **alarm_info;
    extern int alarm_count;
    extern int create_db_default_objects(void);
    extern int create_db_default_settings(void);

    if ((ret = load_supported_alarm(SUPPORTED_ALARM_CONFIG_FILE)) >= 0)
    {
        notice("Loaded %d new supported alarm(s).\n", ret);

        /* Try init fault init to create */
        if ((ret = fault_mgmt_startup()) == SUCCESS)
        {
            if ((ret = create_db_default_objects()) != SUCCESS)
                error("Failed to create default objects for fault mgmt! (ret=%d)\n", ret);

            if ((ret = create_db_default_settings()) != SUCCESS)
                error("Failed to create default settings for fault mgmt! (ret=%d)\n", ret);

            /* Try to clear any set alarms that do not have Persist flag */
            for (i = 0; i < alarm_count; i++)
            {
                /* Do not care about alarms that are not set */
                if (fault_mgmt_check_status(alarm_info[i]->id) <= 0)
                    continue;

                if (find_flag_option_in_string(alarm_info[i]->parameters[_flags], "Persist"))
                {
                    notice("Alarm %d (%s) is currently set!\n",
                        alarm_info[i]->id, alarm_info[i]->parameters[EventType]);
                }
                else
                {
                    if (fault_mgmt_clear(alarm_info[i]->id, "Cleared by system reset.", NULL) == 0)
                    {
                        notice("Alarm %d (%s) has been cleared by reset!\n",
                            alarm_info[i]->id, alarm_info[i]->parameters[EventType]);
                    }
                    else
                    {
                        notice("Alarm %d (%s) failed to be cleared by reset!\n",
                            alarm_info[i]->id, alarm_info[i]->parameters[EventType]);
                    }
                }
            }

            fault_mgmt_shutdown();
            ret = 0;
        }
        else
        {
            error("Failed to start FaultMgmt. (ret=%d)\n", ret);
            ret = -1;
        }
    }
    else
    {
        error("Failed to load supported alarm. (ret=%d)\n", ret);
        ret = -1;
    }
    return ret;
}

/**********************************************************************
 *
 * User Functions
 *
 **********************************************************************/
/**
 * @brief      Shows the list of supported alarms.
 *
 * @return     0 if succeeded. others if failed.
 */
static int show_supported_alarm(void)
{
    int i, j;
    extern SupportedAlarmInfo **alarm_info;
    extern int alarm_count;

    if (fault_mgmt_startup() == 0)
    {
        for (i = 0; i < alarm_count; i++)
        {
            fprintf(stdout, "[%d] %s\n",
                    alarm_info[i]->id, alarm_info[i]->parameters[EventType]);

            for (j = 0; j < SupportedAlarmParameterCount; j++)
            {
                if (j != EventType)
                    fprintf(stdout, "    - %-20s : %s\n", ALARM_PARA_NAME[j], alarm_info[i]->parameters[j]);
            }
            fprintf(stdout, "\n");
        }
        notice("A total of %d alarms are supported.\n", alarm_count);
        fault_mgmt_shutdown();
        return 0;
    }
    else
    {
        error("Failed to retrieve supported alarm info!\n");
        return -1;
    }
}

/**
 * @brief      Show status of an alarm or all alarms.
 *
 * @param      alarm_id  The supported alarm ID to be checked. When alarm_id
 *                       is smaller than 0, status of all supported alarms will
 *                       be displayed.
 *
 * @return     when >= 0, return value is the number of alarms that has been set.
 *             when  < 0, error has occurred.
 */
static int show_alarm(int alarm_id)
{
    int i, alarm_status, set_alarm_count = 0;
    char str_dot[64];
    extern SupportedAlarmInfo **alarm_info;
    extern int alarm_count;

    memset(str_dot, '.', sizeof(str_dot));

    if (fault_mgmt_startup() == 0)
    {
        for (i = 0; i < alarm_count; i++)
        {
            if (alarm_id < 0 || alarm_info[i]->id == alarm_id)
            {
                if ((alarm_status = fault_mgmt_check_status(alarm_info[i]->id)) > 0)
                    set_alarm_count++;

                fprintf(stdout, "[ %5d ] %s %.*s [%s]\n",
                        alarm_info[i]->id, alarm_info[i]->parameters[EventType],
                        (sizeof(str_dot) - strlen(alarm_info[i]->parameters[EventType])), str_dot,
                        alarm_status > 0 ? "X" : " ");

                if (alarm_info[i]->id == alarm_id)
                    break;
            }
        }
        fault_mgmt_shutdown();

        if (alarm_id < 0)
        {
            if (set_alarm_count == 0)
                notice("No alarm has been set.\n");
            else
                notice("%d %s been set.\n", set_alarm_count, set_alarm_count > 1 ? "alarms have" : "alarm has");
        }
        return set_alarm_count;
    }
    else
    {
        error("Failed to init fault mgmt!\n");
        return -1;
    }
}

/**
 * @brief      Logs or clears an alarm.
 *
 * @param      action           The command. When it is non-zero, log a new alarm.
 *                              Otherwise clear an alarm.
 * @param      alarm_type_id    The alarm type identifier
 * @param      additional_text  The additional text
 * @param      additional_info  The additional information
 *
 * @return     when  = 0, the action was successful.
 *             when != 0, error has occurred.
 */
static int log_alarm(int action, int id,
                     const char *additional_text, const char *additional_info)
{
    int ret;
    if (fault_mgmt_startup() == 0)
    {
        if (id >= 0)
        {
            if (action)
            {
                if ((ret = fault_mgmt_log(id, additional_text, additional_info)) == 0)
                    notice("Logged new alarm.\n");
                else
                    notice("Failed to log. ret = %d\n", ret);
            }
            else
            {
                if ((ret = fault_mgmt_clear(id, additional_text, additional_info)) == 0)
                    notice("Cleared alarm.\n");
                else
                    notice("Failed to clear alarm. ret = %d\n", ret);
            }
            show_alarm(id);
        }
        else
        {
            error("Input alarm type ID %d is invalid!\n", id);
            return -1;
        }
        fault_mgmt_shutdown();
        return 0;
    }
    else
    {
        error("Failed to init fault mgmt!\n");
        return -1;
    }
}

#if DEBUG
/**
 * @brief      Run a loop test to write logs
 *
 * @param[in]  id     The alarm type identifier
 * @param[in]  loops  The loops to run
 *
 * @return     when  = 0, the action was successful.
 *             when != 0, error has occurred.
 */
static int fm_loop(int id, int loops)
{
    char tmp_text[128], tmp_info[128];
    int total_loop = loops;

    if (id < 0)
    {
        error("Input alarm type ID %d is invalid!\n", id);
        return -1;
    }

    info("Starting %d loop test...\n", total_loop);
    while (loops)
    {
        sprintf(tmp_text, "Text Loop %d", loops);
        sprintf(tmp_info, "Info Loop %d", loops);
        if (fault_mgmt_startup() == 0)
        {
            if (fault_mgmt_log(id, tmp_text, tmp_info) != 0)
            {
                error("fault_mgmt_log() returned error on loop %d!\n", total_loop - loops + 1);
                break;
            }

            if (fault_mgmt_clear(id, tmp_text, tmp_info) != 0)
            {
                error("fault_mgmt_clear() returned error on loop %d!\n", total_loop - loops + 1);
                break;
            }

            fault_mgmt_shutdown();
            loops--;
        }
        else
        {
            error("Failed to init fault mgmt on loop %d!\n", total_loop - loops + 1);
            break;
        }
    }

    if (loops == 0)
    {
        info("Passed %d loop test!\n", total_loop);
        return 0;
    }
    else
    {
        error("Failed %d loop test!\n", total_loop);
        return -1;
    }
}

/**
 * @brief      Run the test function of FaultMgmt library
 *
 * @return     The returned result from fault_mgmt_test function
 */
extern int fault_mgmt_test(void);
static int fm_test()
{
    return fault_mgmt_test();
}
#endif

/**********************************************************************
 *
 * Main & Help
 *
 **********************************************************************/
void usage(const char *app_name)
{
    fprintf(stderr, "Usage:   %s <command>\n", app_name);
    fprintf(stderr, "\nCommand List:\n");
    fprintf(stderr, "  supported                        Show loaded supported alarm.\n");
    fprintf(stderr, "  log <alarm type ID> <additional text> [additional info]\n");
    fprintf(stderr, "                                   Log an alarm.\n");
    fprintf(stderr, "  clear <alarm type ID> <additional text> [additional info]\n");
    fprintf(stderr, "                                   Clear an alarm\n");
    fprintf(stderr, "  show|check|status [alarm type ID]\n");
    fprintf(stderr, "                                   Show status of alarm(s).\n");
#if DEBUG
    fprintf(stderr, "  loop [alarm type ID (default = 100)] [loop count (default = 10000)]\n");
    fprintf(stderr, "                                   Run alarm set/clear loop test.\n");
    fprintf(stderr, "  test                             Run test function in libfm.so\n");
#endif
}

int main(int argc, char *argv[])
{
    int ret = 0;
    openlog(APP_NAME, 0, 0);

    if (argc >= 2)
    {
        if (strcmp(argv[1], "boot") == 0)
            ret = fm_boot();
#if DEBUG
        else if (strcmp(argv[1], "loop") == 0)
            ret = fm_loop(argc >= 3 ? atoi(argv[2]) : 100 , argc >= 4 ? atoi(argv[3]) : 10000);
        else if (strcmp(argv[1], "test") == 0)
            ret = fm_test();
#endif
        else if (strcmp(argv[1], "supported") == 0)
            ret = show_supported_alarm();
        else if (strcmp(argv[1], "show") == 0 ||
                 strcmp(argv[1], "check") == 0 ||
                 strcmp(argv[1], "status") == 0)
        {
            if (argc == 2)
                ret = show_alarm(-1);
            else if (argc == 3)
            {
                ret = show_alarm(atoi(argv[2]));
            }
            else
            {
                usage(argv[0]);
                ret = -1;
            }
        }
        else if (strcmp(argv[1], "log") == 0 || strcmp(argv[1], "clear") == 0)
        {
            if (argc == 5 || argc == 4)
            {
                ret = log_alarm(strcmp(argv[1], "log") == 0 ? 1 : 0,
                                atoi(argv[2]), argv[3], argc == 5? argv[4] : "");
            }
            else
            {
                usage(argv[0]);
                ret = -1;
            }
        }
        else if (strcmp(argv[1], "help") == 0)
            usage(argv[0]);
        else
        {
            usage(argv[0]);
            ret = -1;
        }
    }
    else
    {
        usage(argv[0]);
        ret = -1;
    }

    closelog();
    return ret;
}
