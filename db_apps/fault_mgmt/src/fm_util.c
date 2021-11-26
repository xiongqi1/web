/*
 * fm_supported_util.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system utility functions.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "fm_shared.h"

int count_index(const char *str_index)
{
    int not_empty = 0, ret = 0;
    const char *str_ptr = str_index;

    while (*str_ptr != '\0')
    {
        if (isdigit(*str_ptr))
        {
            not_empty = 1;
        }
        else if (*str_ptr == ',')
        {
            ret += not_empty;
            not_empty = 0;
        }
        str_ptr++;
    }
    ret += not_empty;
    return ret;
}

int find_index(const char *str_index, int id, int *start, int *end)
{
    int pos_start = -1, pos_end = -1;
    const char *str_s_pos, *str_e_pos, *id_pos;
    char str_id[16];

    if (!str_index)
    {
        goto exit;
    }

    sprintf(str_id, "%d", id);
    str_e_pos = str_index;
    do
    {
        str_s_pos = str_e_pos;
        id_pos = &str_id[0];
        while(*str_e_pos == *id_pos && *id_pos != '\0')
        {
            str_e_pos++;
            id_pos++;
        }

        if (*id_pos == '\0')
        {
            /* _index are seperated by ',' without any whitespace */
            if (*str_e_pos == '\0' || *str_e_pos == ',')
            {
                /* found */
                pos_start = (int)(str_s_pos - str_index);
                pos_end = (int)(str_e_pos - str_index);
                break;
            }
        }

        /* Does not match, move to next section */
        while (*str_e_pos != '\0' && *str_e_pos != ',')
        {
            str_e_pos++;
        }

        if (*str_e_pos == '\0')
        {
            break;
        }
        else
        {
            /* skip ',' and goto next value */
            str_e_pos++;
        }
    } while(1);

exit:
    if (start)
    {
        *start = pos_start;
    }
    if (end)
    {
        *end = pos_end;
    }
    return pos_start;
}

int add_index(char **str_index, int id)
{
    int len = 0, tmp_len = 0, i = 0;
    char tmp_str[16] = {'\0'};
    char *str = NULL;

    sprintf(tmp_str, "%d", id);
    /* Add space for '\0' */
    tmp_len = strlen(tmp_str) + 1;

    if (*str_index == NULL)
    {
        *str_index = malloc(tmp_len);
        sprintf(*str_index, "%d", id);
    }
    else
    {
        str = *str_index;
        len = strlen(str);

        /* Remove comma that starts a line */
        if (str[0] == ',')
        {
            while(i < len)
            {
                str[i] = str[i+1];
                i++;
            }
            len--;
        }

        if (find_index(str, id, NULL, NULL) < 0)
        {
            /* Add the index if it does not exist */
            if (len == 0 || (len > 0 && *(*str_index + len - 1) == ','))
            {
                *str_index = realloc(*str_index, len + tmp_len);
                sprintf(*str_index + len, "%s", tmp_str);
            }
            else
            {
                *str_index = realloc(*str_index, len + tmp_len + 1);
                sprintf(*str_index + len, ",%s", tmp_str);
            }
        }
    }

    len = strlen(*str_index);
    return len;
}

int remove_index(char **str_index, int id)
{
    int len = 0, start_pos, end_pos;
    char *str = NULL;
    if (*str_index == NULL)
    {
        *str_index = malloc(1);
        (*str_index)[0] = '\0';
        return 0;
    }

    str = *str_index;
    /* Remove comma that starts a line */
    if (str[0] == ',')
    {
        len = strlen(str);
        end_pos = 0;
        while(end_pos < len)
        {
            str[end_pos] = str[end_pos+1];
            end_pos++;
        }
        len--;
    }

    if (find_index(str, id, &start_pos, &end_pos) >= 0)
    {
        if (str[end_pos] != '\0')
        {
            end_pos++;

            while(str[end_pos] != '\0')
            {
                str[start_pos++] = str[end_pos++];
            }

            /* Remove the last comma */
            if (str[start_pos - 1] == ',')
            {
                str[start_pos - 1] = '\0';
            }
        }
        else if (start_pos > 0)
        {
            str[start_pos - 1] = '\0';
        }

        str[start_pos] = '\0';

        len = strlen(str);
        *str_index = realloc(*str_index, len + 1);
    }
    else
    {
        len = strlen(str);
    }
    return len;
}
