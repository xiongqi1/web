/*
 * NDD-0300 FaultMgmt Unit Test
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

#include <string.h>
#include <errno.h>

#include "mockrdblib.h"
#include "RdbHandler.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <gtest_test_helpers.h>

extern "C"
{
#include "fm_errno.h"
#include "fm_shared.h"
#include "fm_supported_alarm.h"
#include "fault_mgmt.h"
}

using namespace ::testing;

#define STRINGIFY(x) #x
#define CACLULATE(x) x
#define TOSTRING(x) STRINGIFY(x)

// Following default values should be the same as values set in code
#define DEFAULT_HISTORY_EVENT_NUMBER        100
#define DEFAULT_HISTORY_EVENT_ID            DEFAULT_HISTORY_EVENT_NUMBER

#define DEFAULT_EXPEDITED_EVENT_NUMBER      15
#define DEFAULT_EXPEDITED_EVENT_ID          DEFAULT_EXPEDITED_EVENT_NUMBER

#define DEFAULT_QUEUED_EVENT_NUMBER         15
#define DEFAULT_QUEUED_EVENT_ID             DEFAULT_QUEUED_EVENT_NUMBER

#define DEFAULT_SPARE_EVENT_NUMBER          10

#define DEFAULT_ALARM_COUNT                 8

// HISTORY_EVENT + EXPEDITED_EVENT + QUEUED_EVENT + SPARE_EVENT + ALARM_COUNT
#define DEFAULT_MAX_EVENT_NUMBER            148
#define DEFAULT_MAX_EVENT_ID                DEFAULT_MAX_EVENT_NUMBER

extern "C"
{
extern int alarm_count;

extern int create_db_default_objects(void);
extern int create_db_default_settings(void);
}

namespace
{

class FaultMgmtAPITest : public RdbHandler, public ::testing::Test
{
public:
    FaultMgmtAPITest() {}

    void SetUp()
    {
        alarm_count = DEFAULT_ALARM_COUNT;

        sample_alarm_info[0] = {
            .id = 0,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_CRITICAL,          // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_EXPEDITED, // ReportingMechanism
                (char *)"TestEventType0",               // EventType
                (char *)"Test Cause",                   // ProbableCause
                (char *)"000 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[1] = {
            .id = 1,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_MAJOR,             // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_QUEUED,    // ReportingMechanism
                (char *)"TestEventType1",               // EventType
                (char *)"Test Cause 001",               // ProbableCause
                (char *)"001 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[2] = {
            .id = 2,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_MINOR,             // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_LOGGED,    // ReportingMechanism
                (char *)"TestEventType2",               // EventType
                (char *)"Test Cause 002",               // ProbableCause
                (char *)"002 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[3] = {
            .id = 3,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_MAJOR,             // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_DISABLED,  // ReportingMechanism
                (char *)"TestEventType3",               // EventType
                (char *)"Test Cause 003",               // ProbableCause
                (char *)"003 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[4] = {
            .id = 4,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_CRITICAL,          // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_LOGGED,    // ReportingMechanism
                (char *)"TestEventType4",               // EventType
                (char *)"Test Cause 004",               // ProbableCause
                (char *)"004 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[5] = {
            .id = 5,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_CRITICAL,          // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_LOGGED,    // ReportingMechanism
                (char *)"TestEventType5",               // EventType
                (char *)"Test Cause 005",               // ProbableCause
                (char *)"004 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[6] = {
            .id = 6,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_MINOR,             // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_QUEUED,    // ReportingMechanism
                (char *)"TestEventType6",               // EventType
                (char *)"Test Cause 006",               // ProbableCause
                (char *)"006 Test Specific Problem",    // SpecificProblem
            }
        };
        sample_alarm_info[7] = {
            .id = 7,
            .parameters = {
                (char *)"Persist",                      // _flags
                (char *)SEVERITY_OPT_INDETERMINATE,     // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_LOGGED,    // ReportingMechanism
                (char *)"TestEventType7",               // EventType
                (char *)"Test Cause 007",               // ProbableCause
                (char *)"007 Test Specific Problem",    // SpecificProblem
            }
        };
    }

    void TearDown()
    {
        alarm_count = 0;
    }

    SupportedAlarmInfo sample_alarm_info[DEFAULT_ALARM_COUNT];

    void AddDefaultObjects();
    void AddSupportedAlarm(const SupportedAlarmInfo *sample_alarm);
    void AddSupportedAlarms();
    void AddDefaultSettings();
    void FaultMgmtBoot();
    void SetupRdbEventTracking(int id);
    void SetupRdbEventReferrerTracking(int id, const char *name);
    void SetupRdbCurrentAlarmTracking(int id);

    void SetupRdbHistoryEventTracking(int id);
    void SetupRdbExpeditedEventTracking(int id);
    void SetupRdbQueuedEventTracking(int id);

    void ValidateRdbEvent(int id, int event_type, int ref_count,
                          const char *notify_type, const char *text, const char *info);
    void ValidateRdbFaultMgmt(int curr_alarm_entry, int idx_event,
                              int idx_history, int idx_expedited, int idx_queued);
    void ValidateRdbEventReferrer(int id, const char *name, int ref_event_id);


};

TEST_F(FaultMgmtAPITest, test_create_db_default_objects_new)
{
    ExpectRdbLockUnlock();

    // With an empty db
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_QUEUED_EVENT "._index");

    ASSERT_EQ(0, create_db_default_objects());

    // Should create default values with only 1 write
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EVENT "._index", "", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT "._index", "", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_QUEUED_EVENT "._index", "", -1, 1);
}

TEST_F(FaultMgmtAPITest, test_create_db_default_objects_existing)
{
    ExpectRdbLockUnlock();

    // With keys exist
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT "._index", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_QUEUED_EVENT "._index", "");

    ASSERT_EQ(0, create_db_default_objects());

    // Should not touch the values any more
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EVENT "._index", "", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT "._index", "", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_QUEUED_EVENT "._index", "", -1, 0);
}

TEST_F(FaultMgmtAPITest, test_create_db_default_settings_new)
{
    ExpectRdbLockUnlock();

    // With an empty db
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent");

    ASSERT_EQ(0, create_db_default_settings());

    // Should create settings with default values
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID), -1, 1);
}

TEST_F(FaultMgmtAPITest, test_create_db_default_settings_existing_same_items)
{
    ExpectRdbLockUnlock();

    // With existing values
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent",  TOSTRING(DEFAULT_EXPEDITED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID));

    ASSERT_EQ(0, create_db_default_settings());

    // Should not over-write
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER), 1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID), 1, 0);

}

TEST_F(FaultMgmtAPITest, test_create_db_default_settings_increase_alarm_count_empty)
{
    char tmp_str[32];
    int old_alarm_count = alarm_count;

    ExpectRdbLockUnlock();

    // With an empty db
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent");

    // With more supported alarm count than default
    alarm_count = DEFAULT_ALARM_COUNT + 1;
    ASSERT_EQ(0, create_db_default_settings());
    alarm_count = old_alarm_count;

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_MAX_EVENT_NUMBER + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_MAX_EVENT_ID + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", tmp_str, -1, 1);
}

TEST_F(FaultMgmtAPITest, test_create_db_default_settings_increase_alarm_count_with_existing_settings)
{
    char tmp_str[32];
    int old_alarm_count = alarm_count;

    // *********************** when _IndexEvent pointing Event slot is used ***********************
    ExpectRdbLockUnlock();

    // With an empty db
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent",  TOSTRING(DEFAULT_EXPEDITED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID));

    // Other used keys
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".EventTime", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AlarmIdentifier", "TEST_ID");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".NotificationType", "0 Expedited");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".ManagedObjectInstance", "");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".PerceivedSeverity", "Critical");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AdditionalText", "Test Text");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AdditionalInformation", "Test Info");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) "._ref_count", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) "._event_type_id", "100");

    // With more supported alarm count than default
    alarm_count = DEFAULT_ALARM_COUNT + 1;
    ASSERT_EQ(0, create_db_default_settings());
    alarm_count = old_alarm_count;

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), -1, 0);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_MAX_EVENT_NUMBER + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", tmp_str, -1, 1);
    // Event index should be unchanged
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID), -1, 0);


    // *********************** When _IndexEvent pointing Event slot is not used ***********************
    ExpectRdbLockUnlock();

    // With an empty db
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent",  TOSTRING(DEFAULT_EXPEDITED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID));

    // Other used keys
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".EventTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AlarmIdentifier");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".NotificationType");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".ManagedObjectInstance");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".PerceivedSeverity");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AdditionalText");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) ".AdditionalInformation");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) "._ref_count");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "." TOSTRING(DEFAULT_MAX_EVENT_NUMBER) "._event_type_id");

    // With more supported alarm count than default
    alarm_count = DEFAULT_ALARM_COUNT + 1;
    ASSERT_EQ(0, create_db_default_settings());
    alarm_count = old_alarm_count;

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), -1, 0);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_ALARM_COUNT + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_MAX_EVENT_NUMBER + 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", tmp_str, -1, 1);
    sprintf(tmp_str, "%d", DEFAULT_MAX_EVENT_ID + 1);
    // Event index should increase and point to the new last index
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", tmp_str, -1, 1);
}

TEST_F(FaultMgmtAPITest, test_create_db_default_settings_increase_other_settings)
{
    // *********************** when _Index pointing Event slots are empty ***********************
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent",  "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries", "24");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent", "24");

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".2._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT ".2._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_QUEUED_EVENT ".2._event_id");

    // Other used keys
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.EventTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AlarmIdentifier");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.NotificationType");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.ManagedObjectInstance");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.PerceivedSeverity");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AdditionalText");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AdditionalInformation");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24._ref_count");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24._event_type_id");

    ASSERT_EQ(0, create_db_default_settings());

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", TOSTRING(DEFAULT_HISTORY_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", TOSTRING(DEFAULT_EXPEDITED_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", TOSTRING(DEFAULT_QUEUED_EVENT_ID), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", TOSTRING(DEFAULT_MAX_EVENT_ID), -1, 1);

    // *********************** when _Index pointing Event slots are not empty ***********************
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries", "0");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent",  "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", "2");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT));
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries", "24");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent", "24");

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".2._event_id", "11");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT ".2._event_id", "22");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_QUEUED_EVENT ".2._event_id", "33");

    // Other used keys
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.EventTime", "000");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AlarmIdentifier", "111");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.NotificationType", "222");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.ManagedObjectInstance", "333");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.PerceivedSeverity", "444");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AdditionalText", "555");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24.AdditionalInformation", "666");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24._ref_count", "777");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT ".24._event_type_id", "888");

    ASSERT_EQ(0, create_db_default_settings());

    // The indexes should not be changed
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  "0", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries", TOSTRING(DEFAULT_HISTORY_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries", TOSTRING(DEFAULT_EXPEDITED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries", TOSTRING(DEFAULT_QUEUED_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", "2", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", "2", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", "2", -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries", TOSTRING(DEFAULT_ALARM_COUNT), -1, 0);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._MaxEventEntries", TOSTRING(DEFAULT_MAX_EVENT_NUMBER), -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", "24", -1, 0);
}

/*********************************************************

 Test of Exposed Functions

**********************************************************/
void FaultMgmtAPITest::AddDefaultObjects()
{
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EXPEDITED_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_QUEUED_EVENT "._index");
    ASSERT_EQ(0, create_db_default_objects());
}

void FaultMgmtAPITest::AddSupportedAlarm(const SupportedAlarmInfo *alarm_info)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d._flags", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.PerceivedSeverity", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ReportingMechanism", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.EventType", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ProbableCause", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.SpecificProblem", alarm_info->id);
    sim->SetupRdbActionTracking(szname);
    ASSERT_EQ(0, add_supported_alarm(alarm_info));
}

void FaultMgmtAPITest::AddSupportedAlarms()
{
    for (int i = 0; i < 8; i++)
    {
        AddSupportedAlarm(&sample_alarm_info[i]);
    }
}

void FaultMgmtAPITest::AddDefaultSettings()
{
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".HistoryEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".ExpeditedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".QueuedEventNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexHistoryEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexQueuedEvent");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".SupportedAlarmNumberOfEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH ".MaxCurrentAlarmEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._MaxEventEntries");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH "._IndexEvent");
    ASSERT_EQ(0, create_db_default_settings());
}

void FaultMgmtAPITest::FaultMgmtBoot()
{
    AddDefaultObjects();
    AddSupportedAlarms();
    AddDefaultSettings();
}

void FaultMgmtAPITest::SetupRdbEventTracking(int id)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.EventTime", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AlarmIdentifier", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.NotificationType", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.ManagedObjectInstance", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.PerceivedSeverity", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalText", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalInformation", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._ref_count", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._event_type_id", id);
    sim->SetupRdbActionTracking(szname);
}

void FaultMgmtAPITest::SetupRdbEventReferrerTracking(int id, const char *name)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH ".%s.%d._event_id", name, id);
    sim->SetupRdbActionTracking(szname);
}

void FaultMgmtAPITest::SetupRdbCurrentAlarmTracking(int id)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d.AlarmRaisedTime", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d._event_id", id);
    sim->SetupRdbActionTracking(szname);
    sprintf(szname, FAULT_MGMT_DB_PATH_CURRENT_ALARM ".%d._event_type_id", id);
    sim->SetupRdbActionTracking(szname);
}

void FaultMgmtAPITest::ValidateRdbEvent(int id, int event_type, int ref_count,
                                        const char *notify_type, const char *text, const char *info)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.NotificationType", id);
    sim->ValidateRdbValue(szname, notify_type);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.PerceivedSeverity", id);
    sim->ValidateRdbValue(szname, sample_alarm_info[event_type].parameters[1]);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalText", id);
    sim->ValidateRdbValue(szname, text);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalInformation", id);
    sim->ValidateRdbValue(szname, info);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._ref_count", id);
    sim->ValidateRdbValue(szname, ref_count);

    sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._event_type_id", id);
    sim->ValidateRdbValue(szname, event_type);
}

void FaultMgmtAPITest::ValidateRdbFaultMgmt(int curr_alarm_entry, int idx_event,
                                            int idx_history, int idx_expedited, int idx_queued)
{
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH ".CurrentAlarmNumberOfEntries",  curr_alarm_entry);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexHistoryEvent", idx_history);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexExpeditedEvent", idx_expedited);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexQueuedEvent", idx_queued);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH "._IndexEvent", idx_event);
}

void FaultMgmtAPITest::ValidateRdbEventReferrer(int id, const char *name, int ref_event_id)
{
    char szname[256];
    sprintf(szname, FAULT_MGMT_DB_PATH ".%s.%d._event_id", name, id);
    sim->ValidateRdbValue(szname, ref_event_id);
}

TEST_F(FaultMgmtAPITest, test_fault_mgmt_log_clear_alarm)
{
    SCOPED_TRACE("");
    sim->Reset();

    // Init (boot)
    FaultMgmtBoot();
    ASSERT_EQ(0, fault_mgmt_startup());

    /********** Log EventType 0 (Expedited) **********/
    SetupRdbEventTracking(1);
    SetupRdbCurrentAlarmTracking(1);

    SetupRdbEventReferrerTracking(1, "HistoryEvent");
    SetupRdbEventReferrerTracking(1, "ExpeditedEvent");

    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(0, "FAULT__TEXT__00_CLEAR", "FAULT ADD INFO 00 CLEAR"));

    // Check status return 0
    ASSERT_EQ(0, fault_mgmt_check_status(0));

    ASSERT_EQ(0, fault_mgmt_log(0, "FAULT__TEXT__00", "FAULT ADD INFO 00"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(1, 1, 1, 1, DEFAULT_QUEUED_EVENT_NUMBER);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(1, 0, 3, "NewAlarm", "FAULT__TEXT__00", "FAULT ADD INFO 00");
    // id,ref_event_id
    ValidateRdbEventReferrer(1, "HistoryEvent", 1);
    // id,ref_event_id
    ValidateRdbEventReferrer(1, "ExpeditedEvent", 1);

    // Check status should return 1 now
    ASSERT_EQ(1, fault_mgmt_check_status(0));

    /********** Log EventType 1 (Queued) **********/
    SetupRdbEventTracking(2);
    SetupRdbCurrentAlarmTracking(2);
    SetupRdbEventReferrerTracking(2, "HistoryEvent");
    SetupRdbEventReferrerTracking(1, "QueuedEvent");

    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(1, "FAULT__TEXT__01_CLEAR", "FAULT ADD INFO 01 CLEAR"));

    // Check status return 0
    ASSERT_EQ(0, fault_mgmt_check_status(1));

    ASSERT_EQ(0, fault_mgmt_log(1, "FAULT__TEXT__01", "FAULT ADD INFO 01"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(2, 2, 2, 1, 1);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(2, 1, 3, "NewAlarm", "FAULT__TEXT__01", "FAULT ADD INFO 01");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(2, "HistoryEvent", 2);
    // id,name,ref_event_id
    ValidateRdbEventReferrer(1, "QueuedEvent", 2);

    // Check status should return 1 now
    ASSERT_EQ(1, fault_mgmt_check_status(1));

    /********** Log EventType 2 (Logged) **********/
    SetupRdbEventTracking(3);
    SetupRdbCurrentAlarmTracking(3);
    SetupRdbEventReferrerTracking(3, "HistoryEvent");

    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(2, "FAULT__TEXT__02_CLEAR", "FAULT ADD INFO 02 CLEAR"));

    // Check status returns 0
    ASSERT_EQ(0, fault_mgmt_check_status(2));

    ASSERT_EQ(0, fault_mgmt_log(2, "FAULT__TEXT__02", "FAULT ADD INFO 02"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(3, 3, 3, 1, 1);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(3, 2, 2, "NewAlarm", "FAULT__TEXT__02", "FAULT ADD INFO 02");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(3, "HistoryEvent", 3);

    // Check status should return 1 now
    ASSERT_EQ(1, fault_mgmt_check_status(2));

    //********** Log EventType 3 (Disabled) - should not be logged **********/
    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(3, "FAULT__TEXT__03_CLEAR", "FAULT ADD INFO 03 CLEAR"));

    // Check status should return 0
    ASSERT_EQ(0, fault_mgmt_check_status(3));

    ASSERT_EQ(0, fault_mgmt_log(3, "FAULT__TEXT__03", "FAULT ADD INFO 03"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(3, 3, 3, 1, 1);

    // Check status should return 0
    ASSERT_EQ(0, fault_mgmt_check_status(3));

    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(3, "FAULT__TEXT__03_CLEAR", "FAULT ADD INFO 03 CLEAR"));

    /********** Log EventType 4 (Logged) **********/
    SetupRdbEventTracking(4);
    SetupRdbCurrentAlarmTracking(4);
    SetupRdbEventReferrerTracking(4, "HistoryEvent");

    // Clear the alarm should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(4, "FAULT__TEXT__04_CLEAR", "FAULT ADD INFO 04 CLEAR"));

    // Check status would return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(4));

    ASSERT_EQ(0, fault_mgmt_log(4, "FAULT__TEXT__04", "FAULT ADD INFO 04"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(4, 4, 4, 1, 1);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(4, 4, 2, "NewAlarm", "FAULT__TEXT__04", "FAULT ADD INFO 04");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(4, "HistoryEvent", 4);

    // Check status will return 1 now
    ASSERT_EQ(1, fault_mgmt_check_status(4));

    /********** Log EventType0 again (Expedited) **********/
    SetupRdbEventTracking(5);
    SetupRdbEventReferrerTracking(5, "HistoryEvent");
    SetupRdbEventReferrerTracking(2, "ExpeditedEvent");

    // Check status returns 1 now
    ASSERT_EQ(1, fault_mgmt_check_status(1));

    ASSERT_EQ(0, fault_mgmt_log(0, "FAULT__TEXT__00_2", "FAULT ADD INFO 00_2"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(4, 5, 5, 2, 1);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(5, 0, 3, "ChangedAlarm", "FAULT__TEXT__00_2", "FAULT ADD INFO 00_2");
    // ref count of old event should decrease
    ValidateRdbEvent(1, 0, 2, "NewAlarm", "FAULT__TEXT__00", "FAULT ADD INFO 00");
    // Current alarm AlarmRaisedTime should not change (still the same as event one)
    ASSERT_EQ(0, sim->CompareRdbKey(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime",
                                    FAULT_MGMT_DB_PATH_EVENT ".1.EventTime"));
    // id,name,ref_event_id
    ValidateRdbEventReferrer(5, "HistoryEvent", 5);
    // id,name,ref_event_id
    ValidateRdbEventReferrer(2, "ExpeditedEvent", 5);

    // Check status still returns 1
    ASSERT_EQ(1, fault_mgmt_check_status(1));

    /********** Clear EventType4 (Logged) **********/
    SetupRdbEventTracking(6);
    SetupRdbEventReferrerTracking(6, "HistoryEvent");

    ASSERT_EQ(0, fault_mgmt_clear(4, "FAULT__TEXT__04_CLEAR", "FAULT ADD INFO 04 CLEAR"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(3, 6, 6, 2, 1);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(6, 4, 1, "ClearedAlarm", "FAULT__TEXT__04_CLEAR", "FAULT ADD INFO 04 CLEAR");
    // Old events ref count should reduce to 1
    ValidateRdbEvent(4, 4, 1, "NewAlarm", "FAULT__TEXT__04", "FAULT ADD INFO 04");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(6, "HistoryEvent", 6);

    // Clear again should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(4, "FAULT__TEXT__04_CLEAR2", "FAULT ADD INFO 04 CLEAR2"));

    // Check status will return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(4));

    //********** Clear EventType 3 (Disabled) - should not be logged **********/
    ASSERT_EQ(0, fault_mgmt_clear(3, "FAULT__TEXT__03_CLEAR", "FAULT ADD INFO 03 CLEAR"));
    // Check status still return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(3));

    /********** Clear EventType 1 (Queued) **********/
    SetupRdbEventTracking(7);
    SetupRdbEventReferrerTracking(7, "HistoryEvent");
    SetupRdbEventReferrerTracking(2, "QueuedEvent");

    ASSERT_EQ(0, fault_mgmt_clear(1, "FAULT__TEXT__01_CLEAR", "FAULT ADD INFO 01 CLEAR"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(2, 7, 7, 2, 2);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(7, 1, 2, "ClearedAlarm", "FAULT__TEXT__01_CLEAR", "FAULT ADD INFO 01 CLEAR");
    // Old events ref count should reduce to 2
    ValidateRdbEvent(2, 1, 2, "NewAlarm", "FAULT__TEXT__01", "FAULT ADD INFO 01");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(7, "HistoryEvent", 7);
    // id,name,ref_event_id
    ValidateRdbEventReferrer(2, "QueuedEvent", 7);

    // Check status should return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(1));

    // Clear again should have no effect
    ASSERT_EQ(0, fault_mgmt_clear(1, "FAULT__TEXT__01_CLEAR2", "FAULT ADD INFO 01 CLEAR2"));

    /********** Clear EventType 0 (Expedited) **********/
    SetupRdbEventTracking(8);
    SetupRdbEventReferrerTracking(8, "HistoryEvent");
    SetupRdbEventReferrerTracking(3, "ExpeditedEvent");

    ASSERT_EQ(0, fault_mgmt_clear(0, "FAULT__TEXT__00_CLEAR", "FAULT ADD INFO 00 CLEAR"));
    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(1, 8, 8, 3, 2);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(8, 0, 2, "ClearedAlarm", "FAULT__TEXT__00_CLEAR", "FAULT ADD INFO 00 CLEAR");
    // Old events ref count should reduce to 2
    ValidateRdbEvent(1, 0, 2, "NewAlarm", "FAULT__TEXT__00", "FAULT ADD INFO 00");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(8, "HistoryEvent", 8);
    // id,name,ref_event_id
    ValidateRdbEventReferrer(3, "ExpeditedEvent", 8);

    // Check status should return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(0));

    /********** Clear EventType 2 (Logged) **********/
    SetupRdbEventTracking(9);
    SetupRdbEventReferrerTracking(9, "HistoryEvent");

    ASSERT_EQ(0, fault_mgmt_clear(2, "FAULT__TEXT__02_CLEAR", "FAULT ADD INFO 02 CLEAR"));

    // curr_alarm_entry,idx_event,idx_history,idx_expedited,idx_queued
    ValidateRdbFaultMgmt(0, 9, 9, 3, 2);
    // id,event_type,ref_count,notify_type,text,info
    ValidateRdbEvent(9, 2, 1, "ClearedAlarm", "FAULT__TEXT__02_CLEAR", "FAULT ADD INFO 02 CLEAR");
    // Old events ref count should reduce to 1
    ValidateRdbEvent(3, 2, 1, "NewAlarm", "FAULT__TEXT__02", "FAULT ADD INFO 02");
    // id,name,ref_event_id
    ValidateRdbEventReferrer(9, "HistoryEvent", 9);

    // Check status should return 0 now
    ASSERT_EQ(0, fault_mgmt_check_status(2));

    /********** Keep recording to overwrite old events **********/
    char tmp_text[64], tmp_info[128];
    for (int i = 5; i <= DEFAULT_ALARM_COUNT; i++)
    {
        SetupRdbCurrentAlarmTracking(i);
    }
    for (int i = 10; i <= DEFAULT_MAX_EVENT_ID; i++)
    {
        SetupRdbEventTracking(i);
    }
    for (int i = 10; i <= DEFAULT_HISTORY_EVENT_NUMBER; i++)
    {
        SetupRdbEventReferrerTracking(i, "HistoryEvent");
    }
    for (int i = 4; i <= DEFAULT_EXPEDITED_EVENT_ID; i++)
    {
        SetupRdbEventReferrerTracking(i, "ExpeditedEvent");
    }
    for (int i = 3; i <= DEFAULT_QUEUED_EVENT_NUMBER; i++)
    {
        SetupRdbEventReferrerTracking(i, "QueuedEvent");
    }

    /* Do a loop test of 100 times */
    for (int i = 0; i < 200; i++)
    {
        sprintf(tmp_text, "LOOP TEST %.4d", i);
        sprintf(tmp_info, "LOOP ADDITIONAL INFO %.4d", i);
        ASSERT_EQ(0, fault_mgmt_log(i % DEFAULT_ALARM_COUNT, tmp_text, tmp_info));
    }
    /* Clear all alarms */
    for (int i = 0; i < DEFAULT_ALARM_COUNT; i++)
    {
        sprintf(tmp_text, "LOOP TEST CLEAR %.4d", i);
        sprintf(tmp_info, "LOOP ADDITIONAL INFO CLEAR %.4d", i);
        ASSERT_EQ(0, fault_mgmt_clear(i, tmp_text, tmp_info));
    }
    /* Do a loop test of 100 times again */
    for (int i = 100; i > 0; i--)
    {
        sprintf(tmp_text, "LOOP TEST %.4d", i);
        sprintf(tmp_info, "LOOP ADDITIONAL INFO %.4d", i);
        ASSERT_EQ(0, fault_mgmt_log(i % DEFAULT_ALARM_COUNT, tmp_text, tmp_info));
    }

    /********** Test END **********/
    rdb_session* old_session = session;
    ASSERT_EQ(0, fault_mgmt_shutdown());
    session = old_session;
}



} // namespace
