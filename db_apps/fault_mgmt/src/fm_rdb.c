/*
 * fm_supported_util.c
 *
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * FaultMgmt system RDB-related utility functions.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fm_errno.h"
#include "fm_shared.h"
#include "rdb_ops.h"

#include "fm_shared.h"

#define RDB_RETRY_COUNT     3

int get_rdb_lock(void)
{
    assert(session != NULL);
    return (rdb_lock(session, 0) == 0) ? SUCCESS : FAILED_LOCK_RDB;
}

int put_rdb_lock(void)
{
    assert(session != NULL);
    rdb_unlock(session);
    return SUCCESS;
}

static int add_or_update_with_retry(int retry, const char *szname, const char *value, int len, int flag)
{
    int ret = SUCCESS, old_len = 0;
    char *old_buf = NULL;

    assert(session != NULL && szname != NULL && value != NULL);

    if ((ret = rdb_get_alloc(session, szname, &old_buf, &old_len)) == -ENOENT)
    {
        if (retry <= 0)
        {
            ret = FAILED_CREATE_PARA;
            goto exit;
        }
        else
        {
            /* Need to create key */
            rdb_create(session, szname, value, len, flag, 0);
        }
    }
    else if (ret == 0)
    {
        if (old_len != len || memcmp(old_buf, value, len) != 0)
        {
            if (retry <= 0)
            {
                ret = FAILED_SET_PARA;
                goto exit;
            }
            else
            {
                /* Need to update value */
                rdb_set(session, szname, value, len);
            }
        }
        else
        {
            ret = SUCCESS;
            goto exit;
        }
    }
    else
    {
        if (retry <= 0)
        {
            ret = FAILED_GET_PARA;
            goto exit;
        }
    }

    safe_free(old_buf);
    return add_or_update_with_retry(retry - 1, szname, value, len, flag);
exit:
    safe_free(old_buf);
    return ret;
}

int add_or_update(const char *szname, const char *value, int len, int flag)
{
    return add_or_update_with_retry(RDB_RETRY_COUNT, szname, value, len, flag);
}

int get_number(const char *path)
{
    int tmp_val = FAILED_GET_PARA;
    int ret = FAILED_GET_PARA, retry = RDB_RETRY_COUNT;

    assert(session != NULL && path != NULL);

    /* Need to retry in case of failures */
    while (retry-- && (ret = rdb_get_int(session, path, &tmp_val)) != 0 && ret != -ENOENT);

    if (ret == -ENOENT)
        return FAILED_PARA_NOT_EXIST;
    else if (ret != 0)
        return FAILED_GET_PARA;
    else
        return tmp_val;
}

int set_number(const char *path, int number)
{
    char tmp_val[16];
    assert(session != NULL && path != NULL);
    sprintf(tmp_val, "%d", number);
    if (add_or_update(path, tmp_val, strlen(tmp_val)+1, PERSIST_FLAG_EVENT) == 0)
        return SUCCESS;
    else
        return FAILED_SET_PARA;
}

void add_object_id_to_index(const char *path, int id, int persist)
{
    char *str_index = NULL;
    int len = 0;
    assert(session != NULL && path != NULL);
    rdb_get_alloc(session, path, &str_index, &len);
    len = add_index(&str_index, id);
    add_or_update(path, str_index, len + 1, persist);
    free(str_index);
}

void remove_object_id_from_index(const char *path, int id, int persist)
{
    char *str_index = NULL;
    int len = 0;
    assert(session != NULL && path != NULL);
    rdb_get_alloc(session, path, &str_index, &len);
    len = remove_index(&str_index, id);
    add_or_update(path, str_index, len + 1, persist);
    free(str_index);
}

int count_object_id_in_index(const char *path)
{
    char *str_index = NULL;
    int len = 0, ret = 0;
    assert(session != NULL && path != NULL);
    if ((ret = rdb_get_alloc(session, path, &str_index, &len)) == 0 && len > 0)
    {
        ret = count_index(str_index);
    }
    else if(ret == -ENOENT)
    {
        ret = 0;
    }

    free(str_index);
    return ret >= 0 ? ret : FAILED_GET_PARA;
}
