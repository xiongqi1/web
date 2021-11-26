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
}

using namespace ::testing;

namespace
{

class FaultMgmtSupportedAlarmAPITest : public RdbHandler, public ::testing::Test
{
public:
    FaultMgmtSupportedAlarmAPITest() {}

    void SetUp()
    {
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
                (char *)"",                             // _flags
                (char *)SEVERITY_OPT_MINOR,             // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_EXPEDITED, // ReportingMechanism
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
                (char *)"",                             // _flags
                (char *)SEVERITY_OPT_CRITICAL,          // PerceivedSeverity
                (char *)REPORT_MECHANISM_OPT_LOGGED,    // ReportingMechanism
                (char *)"TestEventType4",               // EventType
                (char *)"Test Cause 004",               // ProbableCause
                (char *)"004 Test Specific Problem",    // SpecificProblem
            }
        };
    }

    void TearDown() {}

    SupportedAlarmInfo sample_alarm_info[5];

    void SetupRdbSupportedAlarmTrackingForSample(int sample_id)
    {
        char szname[256];
        int id = sample_alarm_info[sample_id].id;
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d._flags", id);
        sim->SetupRdbActionTracking(szname);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.PerceivedSeverity", id);
        sim->SetupRdbActionTracking(szname);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ReportingMechanism", id);
        sim->SetupRdbActionTracking(szname);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.EventType", id);
        sim->SetupRdbActionTracking(szname);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ProbableCause", id);
        sim->SetupRdbActionTracking(szname);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.SpecificProblem", id);
        sim->SetupRdbActionTracking(szname);
    }

    void ValidateRdbSupportedAlarmSampleNotExist(int sample_id, int expected_read_count, int expected_write_count)
    {
        char szname[256];
        int id = sample_alarm_info[sample_id].id;
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d._flags", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.PerceivedSeverity", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ReportingMechanism", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.EventType", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ProbableCause", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.SpecificProblem", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
    }

    void ValidateRdbSupportedAlarmSampleNotExist(int sample_id)
    {
        ValidateRdbSupportedAlarmSampleNotExist(sample_id, -1, -1);
    }

    void ValidateRdbSupportedAlarmWithSample(int sample_id, int expected_read_count, int expected_write_count)
    {
        char szname[256];
        int id = sample_alarm_info[sample_id].id;
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d._flags", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[0], expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.PerceivedSeverity", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[1], expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ReportingMechanism", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[2], expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.EventType", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[3], expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.ProbableCause", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[4], expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".%d.SpecificProblem", id);
        sim->ValidateRdbValue(szname, sample_alarm_info[sample_id].parameters[5], expected_read_count, expected_write_count);
    }
};

TEST_F(FaultMgmtSupportedAlarmAPITest, test_add_supported_alarm_new_alone)
{
    SCOPED_TRACE("");
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);

    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "0", -1, 1);
    ValidateRdbSupportedAlarmWithSample(0, -1, 1);

    // Add one with negative id should return error
    SupportedAlarmInfo alarm_info_negative_id;
    memcpy(&alarm_info_negative_id, &sample_alarm_info[0], sizeof(SupportedAlarmInfo));
    alarm_info_negative_id.id = -1;
    ASSERT_EQ(FAILED_CREATE_OBJECT, add_supported_alarm(&alarm_info_negative_id));
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_add_supported_alarm_new_with_others)
{
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "1,2,3");
    SetupRdbSupportedAlarmTrackingForSample(0);

    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "1,2,3,0", -1, 1);
    ValidateRdbSupportedAlarmWithSample(0, -1, 1);
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_add_supported_alarm_overwrite_existing_same)
{
    SCOPED_TRACE("");
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);

    // Added the existing one
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    // Overwite existing
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    // Do not need to update object index
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "0", -1, 1);
    ValidateRdbSupportedAlarmWithSample(0, -1, 1);
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_add_supported_alarm_overwrite_existing_different)
{
    SupportedAlarmInfo alarm_info_to_overwrite = {
        .id = sample_alarm_info[0].id,
        .parameters = {
            (char *)"",                             // _flags
            (char *)SEVERITY_OPT_MAJOR,             // PerceivedSeverity
            (char *)REPORT_MECHANISM_OPT_QUEUED,    // ReportingMechanism
            (char *)"TestEventTypeNew",             // EventType
            (char *)"Test Cause New",               // ProbableCause
            (char *)"Test Specific Problem New",    // SpecificProblem
        }
    };

    SCOPED_TRACE("");
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);

    // Added the existing one
    ASSERT_EQ(0, add_supported_alarm(&alarm_info_to_overwrite));

    // Overwite existing
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    // Do not need to update object index
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", "0", -1, 1);
    ValidateRdbSupportedAlarmWithSample(0, -1, 2);
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_remove_supported_alarm_only_one)
{
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);

    // Added the existing one
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));

    // Remove the only one
    ASSERT_EQ(0, remove_supported_alarm(sample_alarm_info[0].id));

    // The second remove should fail
    ASSERT_EQ(FAILED_DELETE_OBJECT, remove_supported_alarm(sample_alarm_info[0].id));
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_remove_supported_alarm_one_of_many)
{
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);
    SetupRdbSupportedAlarmTrackingForSample(1);
    SetupRdbSupportedAlarmTrackingForSample(2);

    // Added the existing one
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[1]));
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[2]));

    // Remove the only one
    ASSERT_EQ(0, remove_supported_alarm(sample_alarm_info[1].id));

    // The second remove should fail
    ASSERT_EQ(FAILED_DELETE_OBJECT, remove_supported_alarm(sample_alarm_info[1].id));

    // The deleted one should have been removed and there should be 2 writes (one for add, one for remove)
    ValidateRdbSupportedAlarmSampleNotExist(1, -1, 2);

    // Others should not be touched (except for creating them)
    ValidateRdbSupportedAlarmWithSample(0, 2, 1);
    ValidateRdbSupportedAlarmWithSample(2, 2, 1);
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_get_supported_alarms_count)
{
    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    SetupRdbSupportedAlarmTrackingForSample(0);
    SetupRdbSupportedAlarmTrackingForSample(1);
    SetupRdbSupportedAlarmTrackingForSample(2);

    ASSERT_EQ(0, get_supported_alarms_count());

    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[0]));
    ASSERT_EQ(1, get_supported_alarms_count());

    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[1]));
    ASSERT_EQ(2, get_supported_alarms_count());

    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[2]));
    ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[2]));
    ASSERT_EQ(3, get_supported_alarms_count());
}

TEST_F(FaultMgmtSupportedAlarmAPITest, test_get_and_free_supported_alarms)
{
    int i, j;
    SupportedAlarm **alarms = NULL;
    SupportedAlarmInfo **alarm_info = NULL;
    int test_alarm_count = -1;

    SCOPED_TRACE("");
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index");
    for (i = 0; i < 5; i++)
    {
        SetupRdbSupportedAlarmTrackingForSample(i);
    }

    /*********** Test load - Empty ******************/
    ASSERT_EQ(0, get_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 0);

    ASSERT_EQ(0, free_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 0);

    for (i = 0; i < 5; i++)
    {
        ASSERT_EQ(0, add_supported_alarm(&sample_alarm_info[i]));
    }

    /*********** Test load - With data ******************/
    ASSERT_EQ(0, get_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 5);
    for (i = 0; i < 5; i++)
    {
        // PerceivedSeverity
        // ReportingMechanism
        // EventType
        // ProbableCause
        // SpecificProblem

        ASSERT_EQ(alarms[i]->_event_type_id, sample_alarm_info[i].id);
        ASSERT_STREQ(alarms[i]->_flags, sample_alarm_info[i].parameters[0]);
        ASSERT_STREQ(alarms[i]->PerceivedSeverity, sample_alarm_info[i].parameters[1]);
        ASSERT_STREQ(alarms[i]->ReportingMechanism, sample_alarm_info[i].parameters[2]);
        ASSERT_STREQ(alarms[i]->EventType, sample_alarm_info[i].parameters[3]);
        ASSERT_STREQ(alarms[i]->ProbableCause, sample_alarm_info[i].parameters[4]);
        ASSERT_STREQ(alarms[i]->SpecificProblem, sample_alarm_info[i].parameters[5]);

        ASSERT_EQ(alarm_info[i]->id, sample_alarm_info[i].id);
        for (j = 0; j < 5; j++)
        {
            ASSERT_STREQ(alarm_info[i]->parameters[j], sample_alarm_info[i].parameters[j]) << "--> Parameter " << j << endl;
        }
    }

    // Test free
    ASSERT_EQ(0, free_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 0);
    ASSERT_TRUE(alarms == NULL);
    ASSERT_TRUE(alarm_info == NULL);

    /********** Test with negative ID ********************/
    // Add one more item with negative ID
    add_object_id_to_index(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM "._index", -1, 0);
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1._flags", "TEST__flags");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1.PerceivedSeverity", "TEST_PerceivedSeverity");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1.ReportingMechanism", "TEST_ReportingMechanism");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1.EventType", "TEST_EventType");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1.ProbableCause", "TEST_ProbableCause");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_SUPPORTED_ALARM ".-1.SpecificProblem", "TEST_SpecificProblem");

    ASSERT_EQ(0, get_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 5);
    for (i = 0; i < 5; i++)
    {
        // PerceivedSeverity
        // ReportingMechanism
        // EventType
        // ProbableCause
        // SpecificProblem

        ASSERT_EQ(alarms[i]->_event_type_id, sample_alarm_info[i].id);
        ASSERT_STREQ(alarms[i]->_flags, sample_alarm_info[i].parameters[0]);
        ASSERT_STREQ(alarms[i]->PerceivedSeverity, sample_alarm_info[i].parameters[1]);
        ASSERT_STREQ(alarms[i]->ReportingMechanism, sample_alarm_info[i].parameters[2]);
        ASSERT_STREQ(alarms[i]->EventType, sample_alarm_info[i].parameters[3]);
        ASSERT_STREQ(alarms[i]->ProbableCause, sample_alarm_info[i].parameters[4]);
        ASSERT_STREQ(alarms[i]->SpecificProblem, sample_alarm_info[i].parameters[5]);

        ASSERT_EQ(alarm_info[i]->id, sample_alarm_info[i].id);
        for (j = 0; j < 5; j++)
        {
            ASSERT_STREQ(alarm_info[i]->parameters[j], sample_alarm_info[i].parameters[j]) << "--> Parameter " << j << endl;
        }
    }

    // Test free
    ASSERT_EQ(0, free_supported_alarms(&alarms, &alarm_info, &test_alarm_count));
    ASSERT_EQ(test_alarm_count, 0);
    ASSERT_TRUE(alarms == NULL);
    ASSERT_TRUE(alarm_info == NULL);
}

};
