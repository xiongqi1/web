
#ifndef __RDB_HANDLER_H__
#define __RDB_HANDLER_H__

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

    void ExpectRdbAddOrUpdate_Add(const char *path, const char *value);
    void ExpectRdbAddOrUpdate_Skip(const char *path, const char *value);
    void ExpectRdbAddOrUpdate_Overwrite(const char *path, const char *old_value, const char *new_value);
    void ExpectRdbAddOrUpdate_FailGet(const char *path);
    void ExpectRdbAddOrUpdate_FailCreate(const char *path, const char *value);
    void ExpectRdbAddOrUpdate_FailSet(const char *path, const char *old_value, const char *new_value);
};

#endif // __RDB_HANDLER_H__