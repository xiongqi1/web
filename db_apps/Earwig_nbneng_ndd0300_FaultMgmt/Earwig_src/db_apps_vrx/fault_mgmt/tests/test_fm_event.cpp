/*
 * NDD-0300 FaultMgmt Unit test
 *
 * Copyright Notice:
 * Copyright (C) 2017 NetComm Wireless limited.
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
#include "fm_event.h"
}

using namespace ::testing;

namespace
{

class FaultMgmtEventAPITest : public RdbHandler, public ::testing::Test
{
public:
    FaultMgmtEventAPITest() {}

    void SetUp()
    {
        sample_event[0] = {
            .EventTime = (char *)"TIME000",
            .AlarmIdentifier = (char *)"TEST_EVENT_000",
            .NotificationType = (char *)REPORT_MECHANISM_OPT_EXPEDITED,
            .ManagedObjectInstance = (char *)"TEST_INSTANCE_000",
            .PerceivedSeverity = (char *)SEVERITY_OPT_MAJOR,
            .AdditionalText = (char *)"TEST_TEXT_000",
            .AdditionalInformation = (char *)"TEST_INFOMATION_000",
            ._ref_count = 3,
            ._event_type_id = 0,
        };
        sample_event[1] = {
            .EventTime = (char *)"TIME001",
            .AlarmIdentifier = (char *)"TEST_EVENT_001",
            .NotificationType = (char *)REPORT_MECHANISM_OPT_LOGGED,
            .ManagedObjectInstance = (char *)"TEST_INSTANCE_001",
            .PerceivedSeverity = (char *)SEVERITY_OPT_CRITICAL,
            .AdditionalText = (char *)"TEST_TEXT_001",
            .AdditionalInformation = (char *)"TEST_INFOMATION_001",
            ._ref_count = 2,
            ._event_type_id = 1,
        };
        sample_event[2] = {
            .EventTime = (char *)"TIME002",
            .AlarmIdentifier = (char *)"TEST_EVENT_002",
            .NotificationType = (char *)REPORT_MECHANISM_OPT_QUEUED,
            .ManagedObjectInstance = (char *)"TEST_INSTANCE_002",
            .PerceivedSeverity = (char *)SEVERITY_OPT_WARNING,
            .AdditionalText = (char *)"TEST_TEXT_002",
            .AdditionalInformation = (char *)"TEST_INFOMATION_002",
            ._ref_count = 1,
            ._event_type_id = 2,
        };

        sample_event_referrer[0] = { ._event_id = 100, };
        sample_event_referrer[1] = { ._event_id = 101, };
    }

    void TearDown() {}

    Event sample_event[3];
    EventReferrer sample_event_referrer[2];

    void SetupRdbEventTracking(int id)
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

    void ValidateRdbEventNotExist(int id, int expected_read_count, int expected_write_count)
    {
        char szname[256];
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.EventTime", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AlarmIdentifier", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.NotificationType", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.ManagedObjectInstance", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.PerceivedSeverity", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalText", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalInformation", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._ref_count", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._event_type_id", id);
        sim->ValidateRdbValueNotExist(szname, expected_read_count, expected_write_count);
    }

    void ValidateRdbEventMatchWithGivenObject(int id, Event *obj, int expected_read_count, int expected_write_count)
    {
        char szname[256];
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.EventTime", id);
        sim->ValidateRdbValue(szname, obj->EventTime,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AlarmIdentifier", id);
        sim->ValidateRdbValue(szname, obj->AlarmIdentifier,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.NotificationType", id);
        sim->ValidateRdbValue(szname, obj->NotificationType,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.ManagedObjectInstance", id);
        sim->ValidateRdbValue(szname, obj->ManagedObjectInstance,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.PerceivedSeverity", id);
        sim->ValidateRdbValue(szname, obj->PerceivedSeverity,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalText", id);
        sim->ValidateRdbValue(szname, obj->AdditionalText,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d.AdditionalInformation", id);
        sim->ValidateRdbValue(szname, obj->AdditionalInformation,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._ref_count", id);
        sim->ValidateRdbValue(szname, obj->_ref_count,
                              expected_read_count, expected_write_count);
        sprintf(szname, FAULT_MGMT_DB_PATH_EVENT ".%d._event_type_id", id);
        sim->ValidateRdbValue(szname, obj->_event_type_id,
                              expected_read_count, expected_write_count);
    }

    void ValidateRdbEventMatchWithGivenObject(int id, Event *obj)
    {
        ValidateRdbEventMatchWithGivenObject(id, obj, -1, -1);
    }

    void ValidateRdbEventReferrerMatchWithGivenObject(int id, const char *name, EventReferrer *obj,
                                                      int expected_read_count, int expected_write_count)
    {
        char szname[256];
        sprintf(szname, FAULT_MGMT_DB_PATH ".%s.%d._event_id", name, id);
        // The saved value should have +1 shift
        sim->ValidateRdbValue(szname, obj->_event_id + 1, expected_read_count, expected_write_count);
    }

    void ValidateRdbEventReferrerMatchWithGivenObject(int id, const char *name, EventReferrer *obj)
    {
        ValidateRdbEventReferrerMatchWithGivenObject(id, name, obj, -1, -1);
    }
};

TEST_F(FaultMgmtEventAPITest, test_save_event_new)
{
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);
    SetupRdbEventTracking(2);

    // Write first
    save_event(0, &sample_event[0]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EVENT "._index", "1");
    ValidateRdbEventMatchWithGivenObject(1, &sample_event[0], -1, 1);

    // Write second
    save_event(1, &sample_event[1]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EVENT "._index", "1,2");
    ValidateRdbEventMatchWithGivenObject(1, &sample_event[0], -1, 1);
    ValidateRdbEventMatchWithGivenObject(2, &sample_event[1], -1, 1);

    // Save to negative ID should not cause any problem nor write to RDB
    save_event(-1, &sample_event[0]);

    // Null input should not cause any problem
    save_event(0, NULL);
}

TEST_F(FaultMgmtEventAPITest, test_save_event_existing)
{
    Event sample_event_update;
    memcpy((void *)&sample_event_update, (void *)&sample_event[1], sizeof(Event));

    /***** Write same twice but should only save once *****/
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);

    save_event(0, &sample_event[0]);
    save_event(0, &sample_event[0]);

    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_EVENT "._index", "1");
    ValidateRdbEventMatchWithGivenObject(1, &sample_event[0], -1, 1);

    /***** Write two different and value should be updated *****/
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);

    save_event(0, &sample_event[0]);
    save_event(0, &sample_event_update);

    // Event on slot 0 should be equal to sample_event_update
    ValidateRdbEventMatchWithGivenObject(1, &sample_event_update, -1, 2);
}

TEST_F(FaultMgmtEventAPITest, test_get_put_event)
{
    Event event;
    memset((void *)&event, 0, sizeof(Event));

    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);
    SetupRdbEventTracking(2);

    // Get event should fail if it does not exist
    ASSERT_EQ(FAILED_GET_PARA, get_event(0, &event));

    // Add events now
    save_event(0, &sample_event[0]);
    save_event(1, &sample_event[1]);

    // Get event should succeed
    ASSERT_EQ(0, get_event(0, &event));

    // Event on slot 0 should be equal to event
    ValidateRdbEventMatchWithGivenObject(1, &event);

    // Put back and should not cause any problem
    put_event(&event);
    ASSERT_TRUE(event.EventTime == NULL);
    ASSERT_TRUE(event.AlarmIdentifier == NULL);
    ASSERT_TRUE(event.NotificationType == NULL);
    ASSERT_TRUE(event.ManagedObjectInstance == NULL);
    ASSERT_TRUE(event.PerceivedSeverity == NULL);
    ASSERT_TRUE(event.AdditionalText == NULL);
    ASSERT_TRUE(event.AdditionalInformation == NULL);

    // Null input should not cause any problem
    ASSERT_EQ(0, get_event(0, NULL));
    ASSERT_EQ(FAILED_GET_PARA, get_event(-1, NULL));

    // Get the other event
    ASSERT_EQ(0, get_event(1, &event));

    // Event on slot 1 should be equal to event
    ValidateRdbEventMatchWithGivenObject(2, &event);

    // Put back and should not cause any problem
    put_event(&event);
    ASSERT_TRUE(event.EventTime == NULL);
    ASSERT_TRUE(event.AlarmIdentifier == NULL);
    ASSERT_TRUE(event.NotificationType == NULL);
    ASSERT_TRUE(event.ManagedObjectInstance == NULL);
    ASSERT_TRUE(event.PerceivedSeverity == NULL);
    ASSERT_TRUE(event.AdditionalText == NULL);
    ASSERT_TRUE(event.AdditionalInformation == NULL);

    // Get negative ID should return error
    ASSERT_EQ(FAILED_GET_PARA, get_event(-1, &event));
    ASSERT_TRUE(event.EventTime == NULL);
    ASSERT_TRUE(event.AlarmIdentifier == NULL);
    ASSERT_TRUE(event.NotificationType == NULL);
    ASSERT_TRUE(event.ManagedObjectInstance == NULL);
    ASSERT_TRUE(event.PerceivedSeverity == NULL);
    ASSERT_TRUE(event.AdditionalText == NULL);
    ASSERT_TRUE(event.AdditionalInformation == NULL);
}

TEST_F(FaultMgmtEventAPITest, test_get_event_ref_count)
{
    sim->Reset();

    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);
    SetupRdbEventTracking(2);

    // Should return -1 if failed
    ASSERT_EQ(-1, get_event_ref_count(0));
    // Get negative ID should return error
    ASSERT_EQ(-1, get_event_ref_count(-1));

    // Add events now
    save_event(0, &sample_event[0]);
    save_event(1, &sample_event[1]);

    // Should be able to read correct value
    ASSERT_EQ(sample_event[0]._ref_count, get_event_ref_count(0));
    ASSERT_EQ(sample_event[1]._ref_count, get_event_ref_count(1));

    // Should only read _ref_count so the number of read count should be more than others
    ASSERT_GT(sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".1._ref_count"),
              sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".1.AdditionalInformation"));
    ASSERT_GT(sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".2._ref_count"),
              sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".2._event_type_id"));
}

TEST_F(FaultMgmtEventAPITest, test_up_down_event_ref_count)
{
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_EVENT "._index");
    SetupRdbEventTracking(1);

    /***** Should return failure if event does not exist *****/
    ASSERT_EQ(-1, up_event_ref_count(0));
    ASSERT_EQ(-1, down_event_ref_count(0));
    ASSERT_EQ(-1, up_event_ref_count(-1));
    ASSERT_EQ(-1, down_event_ref_count(-1));

    /***** Add event and it should work fine *****/
    save_event(0, &sample_event[0]);

    // Going up
    ASSERT_EQ(sample_event[0]._ref_count + 1, up_event_ref_count(0));
    ASSERT_EQ(sample_event[0]._ref_count + 1, get_event_ref_count(0));

    ASSERT_EQ(sample_event[0]._ref_count + 2, up_event_ref_count(0));
    ASSERT_EQ(sample_event[0]._ref_count + 2, get_event_ref_count(0));

    // Going down
    ASSERT_EQ(sample_event[0]._ref_count + 1, down_event_ref_count(0));
    ASSERT_EQ(sample_event[0]._ref_count + 1, get_event_ref_count(0));

    ASSERT_EQ(sample_event[0]._ref_count, down_event_ref_count(0));
    ASSERT_EQ(sample_event[0]._ref_count, get_event_ref_count(0));

    ASSERT_EQ(sample_event[0]._ref_count - 1, down_event_ref_count(0));
    ASSERT_EQ(sample_event[0]._ref_count - 1, get_event_ref_count(0));

    // Should only read _ref_count so the number of read count should be more than others
    ASSERT_GT(sim->GetWriteCount(FAULT_MGMT_DB_PATH_EVENT ".1._ref_count"),
              sim->GetWriteCount(FAULT_MGMT_DB_PATH_EVENT ".1.AdditionalInformation"));
    ASSERT_GT(sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".1._ref_count"),
              sim->GetReadCount(FAULT_MGMT_DB_PATH_EVENT ".1._event_type_id"));
}

TEST_F(FaultMgmtEventAPITest, test_save_event_referrer_new)
{
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".1._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".2._event_id");

    // Save to negative ID should not have any problem
    save_event_referrer(-1, "HistoryEvent", &sample_event_referrer[0]);

    // Normal save should work
    save_event_referrer(0, "HistoryEvent", &sample_event_referrer[0]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "1");

    save_event_referrer(1, "HistoryEvent", &sample_event_referrer[1]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "1,2");

    ValidateRdbEventReferrerMatchWithGivenObject(1, "HistoryEvent", &sample_event_referrer[0], -1, 1);
    ValidateRdbEventReferrerMatchWithGivenObject(2, "HistoryEvent", &sample_event_referrer[1], -1, 1);
}

TEST_F(FaultMgmtEventAPITest, test_save_event_referrer_with_existing)
{
    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".1._event_id");

    // The second save should have no effect
    save_event_referrer(0, "HistoryEvent", &sample_event_referrer[0]);
    save_event_referrer(0, "HistoryEvent", &sample_event_referrer[0]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index", "1");
    ValidateRdbEventReferrerMatchWithGivenObject(1, "HistoryEvent", &sample_event_referrer[0], -1, 1);

    // Writing a different value should cause update
    save_event_referrer(0, "HistoryEvent", &sample_event_referrer[1]);
    ValidateRdbEventReferrerMatchWithGivenObject(1, "HistoryEvent", &sample_event_referrer[1], -1, 2);

    // Should not cause any problem
    save_event_referrer(0, "HistoryEvent", NULL);
}

TEST_F(FaultMgmtEventAPITest, test_get_event_referrer)
{
    EventReferrer event_referrer;
    memset((void *)&event_referrer, 0, sizeof(EventReferrer));

    sim->Reset();
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_HISTORY_EVENT ".1._event_id");

    // Cannot retrieve if it does not exist
    ASSERT_EQ(FAILED_GET_PARA, get_event_referrer(-1, "HistoryEvent", &event_referrer));
    ASSERT_EQ(FAILED_GET_PARA, get_event_referrer(0, "HistoryEvent", &event_referrer));

    /***** Add event and it should work fine *****/
    save_event_referrer(0, "HistoryEvent", &sample_event_referrer[0]);
    ASSERT_EQ(0, get_event_referrer(0, "HistoryEvent", &event_referrer));
    ValidateRdbEventReferrerMatchWithGivenObject(1, "HistoryEvent", &sample_event_referrer[0]);

    // Should work as well
    ASSERT_EQ(0, get_event_referrer(0, "HistoryEvent", NULL));
}

} // namespace
