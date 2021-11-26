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

#ifndef __RDB_HANDLER_H_16161614022018__
#define __RDB_HANDLER_H_16161614022018__

#include "mockrdblib.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gtest_test_helpers.h>

#include <string>
#include <list>

using namespace ::std;
using namespace ::testing;

class RdbKey
{
public:
    RdbKey(const string &key, const string &value, bool exist)
    {
        ClearCounters();

        _exist = exist;
        _key.assign(key);
        _value.assign(value);
    }

    RdbKey(const string &key, const string &value)
    {
        RdbKey(key, value, 1);
        _write_count++;
    }

    virtual ~RdbKey() { }

    const string& Key() const { return _key; }
    bool Exist() { _read_count++; return _exist; }
    void SetExist() { _exist = true; _write_count++; }
    void ClearExist() { _exist = 0; _write_count++; }
    const string& Value() { _read_count++; return _value; }
    void SetValue(const string &new_value) { TouchValue(new_value); _write_count++; }

    bool PeekExist() { return _exist; }
    const string& Peek() const { return _value; }
    void TouchValue(const string &new_value) { _exist = true; _value.assign(new_value); }

    void ClearCounters() { _read_count = 0; _write_count = 0; }
    int ReadCount() const { return _read_count; }
    int WriteCount() const { return _write_count; }

private:
    string _key;
    string _value;
    bool _exist;
    int _read_count;
    int _write_count;
};

class RdbSimulator
{
public:
    RdbSimulator(MockRdbLib *MockRdb);
    ~RdbSimulator();

    void Reset();
    void Dump();

    void PrepareRdbValue(const char *path, const char *value);
    void SetupRdbActionTracking(const char *path);
    void SetupRdbActionTracking(const char *path, const char *default_value);
    void ValidateRdbValue(const char *path, const char *expect_value, int expected_read_count, int expected_write_count);
    void ValidateRdbValue(const char *path, int expect_value, int expected_read_count, int expected_write_count);
    void ValidateRdbValue(const char *path, int expected_read_count, int expected_write_count);
    void ValidateRdbValue(const char *path, const char *expect_value);
    void ValidateRdbValue(const char *path, int expect_value);
    void ValidateRdbValueNotExist(const char *path, int expected_read_count, int expected_write_count);
    void ValidateRdbValueNotExist(const char *path);
    int GetReadCount(const char *path);
    int GetWriteCount(const char *path);
    int CompareRdbKey(const char *path1, const char *path2);

private:
    MockRdbLib *rdb;
    map <string, RdbKey> db;

    RdbKey *SearchKey(const string key);
    void TouchKey(const string key, const string value);
    void SetKey(const string key, const string &value);
    int DeleteKey(const string key);
};

class RdbHandler
{
public:
    MockRdbLib *rdb;
    RdbSimulator *sim;

    RdbHandler();
    ~RdbHandler();

    void ExpectRdbLock(int ret);
    void ExpectRdbUnlock(int ret);
    void ExpectRdbLockUnlock();

    void ExpectRdbGetInfo(int ret, const char* path);
    void ExpectRdbGetInfo(Sequence seq, int ret, const char* path);

    void OnRdbGet(int ret, const char* path, const char* value);
    void ExpectRdbGet(int ret, const char* path, const char* value);
    void ExpectRdbGet(Sequence seq, int ret, const char* path, const char* value);

    void ExpectRdbGetInt(int ret, const char* path, int value);

    void OnRdbGetAlloc(int ret, const char* path, const char* value);
    void ExpectRdbGetAlloc(int ret, const char* path, const char* value);
    void ExpectRdbGetAlloc(Sequence seq, int ret, const char* path, const char* value);

    void ExpectRdbCreate(int ret, const char* path, const char* value);
    void ExpectRdbCreate(Sequence seq, int ret, const char* path, const char* value);

    void ExpectRdbSet(int ret, const char* path, const char* value);
    void ExpectRdbSet(Sequence seq, int ret, const char* path, const char* value);

    void ExpectRdbNoChange(const char* path);
};

#endif // __RDB_HANDLER_H_16161614022018__