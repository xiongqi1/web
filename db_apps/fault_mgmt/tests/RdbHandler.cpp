/*
 * NDD-0300 FaultMgmt Unit Test - RDB Simulator
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

#include "mockrdblib.h"
#include "RdbHandler.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <gtest_test_helpers.h>

#include <string>
#include <list>
#include <iterator>
#include <iostream>

extern "C"
{
#include "fm_errno.h"
#include "fm_shared.h"
}

using namespace ::std;
using namespace ::testing;
using ::testing::Sequence;

// fake rdb_session type for testing
struct rdb_session
{
    int fd;
};


/**************************************
 *
 * RdbSimulator
 *
 **************************************/
RdbSimulator::RdbSimulator(MockRdbLib *MockRdb)
{
    rdb = MockRdb;
    Reset();
}

RdbSimulator::~RdbSimulator()
{
    Reset();
}

void RdbSimulator::Reset()
{
    db.clear();

    ON_CALL(*rdb, rdb_getinfo(_,_,_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, int* pLen, int* pFlags,int* pPerm)->int{
        ADD_FAILURE() << "Undefined rdb_getinfo() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_get(_,_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char* pValue, int *nLen)->int{
        ADD_FAILURE() << "Undefined rdb_get() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_get_alloc(_,_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char** ppValue, int* pLen)->int{
        ADD_FAILURE() << "Undefined rdb_get_alloc() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_get_int(_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, int *pValue)->int{
        ADD_FAILURE() << "Undefined rdb_get_int() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_create(_,_,_,_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, const char* szValue, int nLen, int nFlags, int nPerm)->int{
        ADD_FAILURE() << "Undefined rdb_create() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_set(_,_,_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, const char *szValue, int len)->int{
        ADD_FAILURE() << "Undefined rdb_set() action to " << string(szName) << "\n";
    }));
    ON_CALL(*rdb, rdb_delete(_,_))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName)->int{
        ADD_FAILURE() << "Undefined rdb_delete() action to " << string(szName) << "\n";
    }));
}

/**
 * @brief      Dump all keys in RDB
 */
void RdbSimulator::Dump()
{
    map <string, RdbKey> :: iterator it;
    for(it = db.begin(); it != db.end(); ++it)
        cout << (it->second.PeekExist()? "[*]" : "[ ]")
             << "(R" << it->second.ReadCount() << ","
             << "W" << it->second.WriteCount() << ") "
             << it->second.Key() << ": " << it->second.Peek() << '\n';
}

/**
 * @brief      Create/update a RDB key with given value. This action is not recorded by
 *             the key and will not increase write counter.
 *
 * @param[in]  path   The RDB key
 * @param[in]  value  The value
 */
void RdbSimulator::PrepareRdbValue(const char *path, const char *value)
{
    TouchKey(string(path), string(value));
}

/**
 * @brief      Setup a RDB key to track. Add this key to database with a default value.
 *
 * @param[in]  path           The RDB key
 * @param[in]  default_value  The default value of the key
 */
void RdbSimulator::SetupRdbActionTracking(const char *path, const char *default_value)
{
    PrepareRdbValue(path, default_value);
    SetupRdbActionTracking(path);
}

/**
 * @brief      Setup a RDB key to track. Writings and readings against this key will be
 *             monitored but the key is seen as not existed in RDB yet.
 *
 * @param[in]  path  The RDB key
 */
void RdbSimulator::SetupRdbActionTracking(const char *path)
{
    string key = string(path);
    if (!SearchKey(key))
    {
        /* Not found */
        RdbKey newKey = RdbKey(key, "", false);
        //db.push_back(newKey);
        db.insert(pair <string, RdbKey> (key, newKey));
    }

    // rdb_getinfo
    ON_CALL(*rdb, rdb_getinfo(NotNull(), StrEq(path), _, _, _))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, int* pLen, int* pFlags,int* pPerm)->int{
        RdbKey *rdbKey = SearchKey(string(szName));
        /* Actual check */
        return (rdbKey && rdbKey->Exist()) ? -EOVERFLOW : -ENOENT;
    }));

    // rdb_get
    ON_CALL(*rdb, rdb_get(NotNull(), StrEq(path), _, _))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char* pValue, int *nLen)->int{
        int len = 0;
        RdbKey *rdbKey = SearchKey(string(szName));
        if (!rdbKey)
        {
            return -ENOENT;
        }
        else if (!rdbKey->PeekExist())
        {
            /* Actual check */
            rdbKey->Exist();
            return -ENOENT;
        }
        else
        {
            /* Actual read */
            len = rdbKey->Value().length() + 1;
            if (pValue)
            {
               strcpy(pValue, rdbKey->Peek().c_str());
            }
            if (nLen)
            {
                *nLen = len;
            }
            return 0;
        }
    }));

    // rdb_get_int
    ON_CALL(*rdb, rdb_get_int(NotNull(), StrEq(path), _))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, int *pValue)->int{
        int len = 0;
        RdbKey *rdbKey = SearchKey(string(szName));
        if (!rdbKey)
        {
            return -ENOENT;
        }
        else if (!rdbKey->PeekExist())
        {
            /* Actual check */
            rdbKey->Exist();
            return -ENOENT;
        }
        else
        {
            /* Actual read */
            if (pValue)
            {
                *pValue = atoi(rdbKey->Value().c_str());
            }
            return 0;
        }
    }));

    // rdb_get_alloc
    ON_CALL(*rdb, rdb_get_alloc(NotNull(), StrEq(path), _, _))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char** ppValue, int* pLen)->int{
        int len = 0, ret = 0;
        RdbKey *rdbKey = SearchKey(string(szName));
        if (!rdbKey)
        {
            return -ENOENT;
        }
        else if (!rdbKey->PeekExist())
        {
            /* Actual check */
            rdbKey->Exist();
            return -ENOENT;
        }
        else
        {
            /* Actual read */
            len = rdbKey->Value().length() + 1;
            *ppValue = (char *)realloc(*ppValue, len);
            if (*ppValue)
            {
                memcpy(*ppValue, rdbKey->Peek().c_str(), len);
            }
            else
            {
                len = 0;
                ret = -ENOMEM;
            }
            if (pLen)
            {
                *pLen = len;
            }
            return ret;
        }
    }));

    // rdb_create
    ON_CALL(*rdb, rdb_create(NotNull(), StrEq(path), NotNull(), Gt(0), _, _))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, const char* szValue, int nLen, int nFlags, int nPerm)->int{
        string key = string(szName);
        RdbKey *rdbKey = SearchKey(key);
        if (!rdbKey || !rdbKey->PeekExist())
        {
            /* Actual write */
            SetKey(key, string(szValue));
            return 0;
        }
        else
        {
            return -EOVERFLOW;
        }
    }));

    // rdb_set
    ON_CALL(*rdb, rdb_set(NotNull(), StrEq(path), NotNull(), Gt(0)))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, const char *szValue, int len)->int{
        string key = string(szName);
        RdbKey *rdbKey = SearchKey(key);
        if (rdbKey && rdbKey->PeekExist())
        {
            /* Actual write */
            SetKey(key, string(szValue));
            return 0;
        }
        else
        {
            return -ENOENT;
        }
    }));

    // rdb_delete
    ON_CALL(*rdb, rdb_delete(NotNull(), StrEq(path)))
        .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName)->int{
        string key = string(szName);
        RdbKey *rdbKey = SearchKey(key);
        if (rdbKey && rdbKey->PeekExist())
        {
            rdbKey->TouchValue("");
            rdbKey->ClearExist();
            return 0;
        }
        else
        {
            return -ENOENT;
        }
    }));
}

/**
 * @brief      Validate if the value of a RDB key matches the expected value. Also check if read
 *             count and write count match.
 *
 * @param[in]  path                     The RDB key
 * @param[in]  expect_value             The expected value
 * @param[in]  expected_read_count      The expected read count, do not validate if < 0
 * @param[in]  expected_write_count     The expected write count, do not validate if < 0
 */
void RdbSimulator::ValidateRdbValue(const char *path, const char *expect_value, int expected_read_count, int expected_write_count)
{
    string key = string(path);
    RdbKey *rdbKey = SearchKey(key);
    if (rdbKey)
    {
        if (expect_value)
        {
            string value = string(expect_value);
            ASSERT_STREQ(expect_value, rdbKey->Peek().c_str()) << "While validating path " << key << "\n";
        }
        if (expected_read_count >= 0)
        {
            ASSERT_EQ(rdbKey->ReadCount(), expected_read_count) << "Expected read count is " << expected_read_count
                                                                << ", but actual is " << rdbKey->ReadCount()
                                                                << " (key " << key << ")";
        }
        if (expected_write_count >= 0)
        {
            ASSERT_EQ(rdbKey->WriteCount(), expected_write_count)  << "Expected write count is " << expected_write_count
                                                                   << ", but actual is " << rdbKey->WriteCount()
                                                                   << " (key " << key << ")";
        }
    }
    else
    {
        ADD_FAILURE() << "Did not find path " << key;
    }
}

/**
 * @brief       Validate if the value of a RDB key matches the expected value. Also check if read
 *              count and write count match.
 *
 * @param[in]  path                     The RDB key
 * @param[in]  expect_value             The expected value in integer
 * @param[in]  expected_read_count      The expected read count, do not validate if < 0
 * @param[in]  expected_write_count     The expected write count, do not validate if < 0
 */
void RdbSimulator::ValidateRdbValue(const char *path, int expect_value, int expected_read_count, int expected_write_count)
{
    char tmp_val[32];
    sprintf(tmp_val, "%d", expect_value);
    ValidateRdbValue(path, tmp_val, expected_read_count, expected_write_count);
}

/**
 * @brief      Validate if the read count and write count to a RDB key is the same as expected.
 *
 * @param[in]  path                  The RDB key
 * @param[in]  expected_read_count   The expected read count, do not validate if < 0
 * @param[in]  expected_write_count  The expected write count, do not validate if < 0
 */
void RdbSimulator::ValidateRdbValue(const char *path, int expected_read_count, int expected_write_count)
{
    ValidateRdbValue(path, (const char *)NULL, expected_read_count, expected_write_count);
}

/**
 * @brief      Validate if the value of a RDB key matches the expected value.
 *
 * @param[in]  path          The RDB key
 * @param[in]  expect_value  The expected value
 */
void RdbSimulator::ValidateRdbValue(const char *path, const char *expect_value)
{
    ValidateRdbValue(path, expect_value, -1, -1);
}

/**
 * @brief      Validate if the value of a RDB key matches the expected value.
 *
 * @param[in]  path          The RDB key
 * @param[in]  expect_value  The expected value in integer
 */
void RdbSimulator::ValidateRdbValue(const char *path, int expect_value)
{
    ValidateRdbValue(path, expect_value, -1, -1);
}

/**
 * @brief      Validate if a RDB key that should not exist.
 *
 * @param[in]  path                  The path
 * @param[in]  expected_read_count   The expected read count
 * @param[in]  expected_write_count  The expected write count
 */
void RdbSimulator::ValidateRdbValueNotExist(const char *path, int expected_read_count, int expected_write_count)
{
    string key = string(path);
    RdbKey *rdbKey = SearchKey(key);
    if (rdbKey)
    {
        ASSERT_EQ(0, rdbKey->PeekExist()) << "Data should not exist at " << key << "\n";

        if (expected_read_count >= 0)
        {
            ASSERT_EQ(rdbKey->ReadCount(), expected_read_count) << "Expected read count is " << expected_read_count
                                                                << ", but actual is " << rdbKey->ReadCount()
                                                                << " (key " << key << ")";
        }
        if (expected_write_count >= 0)
        {
            ASSERT_EQ(rdbKey->WriteCount(), expected_write_count)  << "Expected write count is " << expected_write_count
                                                                   << ", but actual is " << rdbKey->WriteCount()
                                                                   << " (key " << key << ")";
        }
    }
    else
    {
        ADD_FAILURE() << "Path is not tracked: " << key;
    }
}

/**
 * @brief      Validate if a RDB key that should not exist.
 *
 * @param[in]  path                  The path
 */
void RdbSimulator::ValidateRdbValueNotExist(const char *path)
{
    ValidateRdbValueNotExist(path, -1, -1);
}

/**
 * @brief      Gets the read count.
 *
 * @param[in]  path  The path
 *
 * @return     The read count.
 */
int RdbSimulator::GetReadCount(const char *path)
{
    RdbKey *rdbKey = SearchKey(string(path));
    return rdbKey ? rdbKey->ReadCount() : -1;
}

/**
 * @brief      Gets the write count.
 *
 * @param[in]  path  The path
 *
 * @return     The write count.
 */
int RdbSimulator::GetWriteCount(const char *path)
{
    RdbKey *rdbKey = SearchKey(string(path));
    return rdbKey ? rdbKey->WriteCount() : -1;
}

/**
 * @brief      Compare the value of two RDB keys
 *
 * @param[in]  path1  The RDB key1
 * @param[in]  path2  The RDB key2
 *
 * @return     0 if equal, !0 if not or any of the given is not tracked.
 *             If both are tracked but not exist, return 0.
 */
int RdbSimulator::CompareRdbKey(const char *path1, const char *path2)
{
    RdbKey *rdbKey1 = SearchKey(string(path1));
    RdbKey *rdbKey2 = SearchKey(string(path2));
    if (!rdbKey1 || !rdbKey2 || (rdbKey1->PeekExist() != rdbKey2->PeekExist()))
    {
        return -1;
    }

    if (!rdbKey1->PeekExist() && !rdbKey2->PeekExist())
    {
        return 0;
    }

    return rdbKey1->Peek().compare(rdbKey2->Peek());
}


RdbKey *RdbSimulator::SearchKey(const string key)
{
    map <string, RdbKey> :: iterator it;
    it = db.find(key);
    if (it != db.end())
    {
        return &it->second;
    }
    else
    {
        return nullptr;
    }

    return nullptr;
}

void RdbSimulator::TouchKey(const string key, const string value)
{
    RdbKey *rdbKey = SearchKey(key);
    if (rdbKey)
    {
        rdbKey->TouchValue(value);
    }
    else
    {
        /* Not found */
        //RdbKey newKey = RdbKey(key, value, true);
        //db.push_back(newKey);
        db.insert(pair <string, RdbKey> (key, RdbKey(key, value, true)));
    }
}

void RdbSimulator::SetKey(const string key, const string &value)
{
    RdbKey *rdbKey = SearchKey(key);
    if (rdbKey)
    {
        rdbKey->SetValue(value);
    }
    else
    {
        /* Not found */
        //db.push_back(RdbKey(key, value));
        db.insert(pair <string, RdbKey> (key, RdbKey(key, value)));
    }
}

/**************************************
 *
 * RdbHandler
 *
 **************************************/
RdbHandler::RdbHandler()
{
    /* setup RDB */
    rdb = new MockRdbLib();
    sim = new RdbSimulator(rdb);

    session = (struct rdb_session *)calloc(sizeof(struct rdb_session), 1);
    session->fd = 1;
}

RdbHandler::~RdbHandler()
{
    if (session)
    {
        free(session);
        session = nullptr;
    }

    if (sim)
    {
        free(sim);
        sim = nullptr;
    }

    if (rdb)
    {
        delete(rdb);
        rdb = nullptr;
    }
}

void RdbHandler::ExpectRdbGetInt(int ret, const char* path, int value)
{
    EXPECT_CALL(*rdb, rdb_get_int(NotNull(), StrEq(path), NotNull()))
            .WillOnce(Invoke([=](struct rdb_session *s, const char* szName, int *pValue)->int{
            *pValue = value;
            return ret;
    }));
}
void RdbHandler::ExpectRdbLock(int ret)
{
    EXPECT_CALL(*rdb, rdb_lock(NotNull(), _))
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbUnlock(int ret)
{
    EXPECT_CALL(*rdb, rdb_unlock(NotNull()))
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbLockUnlock()
{
    ExpectRdbLock(0);
    ExpectRdbUnlock(0);
}

void RdbHandler::ExpectRdbGetInfo(int ret, const char* path)
{
    EXPECT_CALL(*rdb, rdb_getinfo(NotNull(), StrEq(path), _, _, _))
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbGetInfo(Sequence seq, int ret, const char* path)
{
    EXPECT_CALL(*rdb, rdb_getinfo(NotNull(), StrEq(path), _, _, _))
            .InSequence(seq)
            .WillOnce(Return(ret));
}

void RdbHandler::OnRdbGet(int ret, const char* path, const char* value)
{
    ON_CALL(*rdb, rdb_get(NotNull(), StrEq(path), _, _))
            .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char* pValue, int *nLen)->int{
            int len = 1;
            if (pValue && value)
            {
                len = strlen(value) + 1;
                memcpy(pValue, value, len);
            }
            if (nLen)
            {
                *nLen = len;
            }
            return ret;
        }));
}

void RdbHandler::ExpectRdbGet(int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_get(NotNull(), StrEq(path), _, _))
            .WillOnce(Invoke([=](struct rdb_session *s, const char* szName, char* pValue, int *nLen)->int{
            int len = 1;
            if (pValue && value)
            {
                len = strlen(value) + 1;
                memcpy(pValue, value, len);
            }
            if (nLen)
            {
                *nLen = len;
            }
            return ret;
        }));
}

void RdbHandler::ExpectRdbGet(Sequence seq, int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_get(NotNull(), StrEq(path), _, _))
            .InSequence(seq)
            .WillOnce(Invoke([=](struct rdb_session *s, const char* szName, char* pValue, int *nLen)->int{
            int len = 1;
            if (pValue && value)
            {
                len = strlen(value) + 1;
                memcpy(pValue, value, len);
            }
            if (nLen)
            {
                *nLen = len;
            }
            return ret;
        }));
}

void RdbHandler::OnRdbGetAlloc(int ret, const char* path, const char* value)
{
    ON_CALL(*rdb, rdb_get_alloc(NotNull(), StrEq(path), _, NotNull()))
            .WillByDefault(Invoke([=](struct rdb_session *s, const char* szName, char** ppValue, int* pLen)->int{
            int len = 0;
            if (value)
            {
                len = strlen(value) + 1;
                if (!*ppValue)
                {
                    *ppValue = (char *)malloc(len);
                }
                else
                {
                    *ppValue = (char *)realloc(*ppValue, len);
                }
                strcpy(*ppValue, value);
            }
            if (pLen)
            {
                *pLen = len;
            }
            return ret;
        }));
}

void RdbHandler::ExpectRdbGetAlloc(int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_get_alloc(NotNull(), StrEq(path), _, NotNull()))
            .WillOnce(Invoke([=](struct rdb_session *s, const char* szName, char** ppValue, int* pLen)->int{
            int len = 0;
            if (value)
            {
                len = strlen(value) + 1;
                if (!*ppValue)
                {
                    *ppValue = (char *)malloc(len);
                }
                else
                {
                    *ppValue = (char *)realloc(*ppValue, len);
                }
                strcpy(*ppValue, value);
            }
            if (pLen)
            {
                *pLen = len;
            }
            return ret;
        }));
}

void RdbHandler::ExpectRdbGetAlloc(Sequence seq, int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_get_alloc(NotNull(), StrEq(path), _, NotNull()))
            .InSequence(seq)
            .WillOnce(Invoke([=](struct rdb_session *s, const char* szName, char** ppValue, int* pLen)->int{
            int len = 0;
            if (value)
            {
                len = strlen(value) + 1;
                if (!*ppValue)
                {
                    *ppValue = (char *)malloc(len);
                }
                else
                {
                    *ppValue = (char *)realloc(*ppValue, len);
                }
                strcpy(*ppValue, value);
                if (pLen)
                {
                    *pLen = len;
                }
            }
            if (pLen)
            {
                *pLen = len;
            }
            return ret;
    }));
}

void RdbHandler::ExpectRdbCreate(int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_create(NotNull(), StrEq(path), StrEq(value), _, _, _))
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbCreate(Sequence seq, int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_create(NotNull(), StrEq(path), StrEq(value), _, _, _))
            .InSequence(seq)
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbSet(int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_set(NotNull(), StrEq(path), StrEq(value), Ge(0)))
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbSet(Sequence seq, int ret, const char* path, const char* value)
{
    EXPECT_CALL(*rdb, rdb_set(NotNull(), StrEq(path), StrEq(value), Ge(0)))
            .InSequence(seq)
            .WillOnce(Return(ret));
}

void RdbHandler::ExpectRdbNoChange(const char* path)
{
    EXPECT_CALL(*rdb, rdb_create(NotNull(), StrEq(path),_,_,_,_)).Times(0);
    EXPECT_CALL(*rdb, rdb_set(NotNull(), StrEq(path),_,_)).Times(0);
}
