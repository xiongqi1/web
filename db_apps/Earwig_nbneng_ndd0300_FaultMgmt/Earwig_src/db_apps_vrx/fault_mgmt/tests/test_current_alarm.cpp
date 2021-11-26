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
#include "fm_current_alarm.h"
}

using namespace ::testing;

namespace
{

class CurrentAlarmAPITest : public RdbHandler, public ::testing::Test
{
public:
    CurrentAlarmAPITest() {}

    void SetUp()
    {
        sample_current_alarm[0] = {
            .AlarmRaisedTime = (char *)"2018-01-10 16:32:11Z",
            ._event_id = 100,
            ._event_type_id = 99,
        };
        sample_current_alarm[1] = {
            .AlarmRaisedTime = (char *)"2022-11-33 44:10:19Z",
            ._event_id = 7,
            ._event_type_id = 11,
        };
    }

    void TearDown() {}

    CurrentAlarm sample_current_alarm[2];
};

TEST_F(CurrentAlarmAPITest, test_save_current_alarm_new)
{
    sim->Reset();

    // Current Alarm 0 is pointing to index 1
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_type_id");

    save_current_alarm(0, &sample_current_alarm[0]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "1");
    // New values set
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime", sample_current_alarm[0].AlarmRaisedTime, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id", sample_current_alarm[0]._event_id, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id", sample_current_alarm[0]._event_type_id, -1, 1);

    save_current_alarm(1, &sample_current_alarm[1]);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "1,2");
    // Old not changed
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime", sample_current_alarm[0].AlarmRaisedTime, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id", sample_current_alarm[0]._event_id, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id", sample_current_alarm[0]._event_type_id, -1, 1);
    // New values set
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2.AlarmRaisedTime", sample_current_alarm[1].AlarmRaisedTime, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_id", sample_current_alarm[1]._event_id, -1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_type_id", sample_current_alarm[1]._event_type_id, -1, 1);

    // Save to negative ID should not write to RDB or cause any problem
    save_current_alarm(-1, &sample_current_alarm[0]);
}

TEST_F(CurrentAlarmAPITest, test_get_put_current_alarm)
{
    CurrentAlarm current_alarm;
    memset((void *)&current_alarm, 0, sizeof(CurrentAlarm));

    sim->Reset();

    // Current Alarm 0 is pointing to index 1
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id");

    // Cannot get when it is not in RDB
    ASSERT_EQ(FAILED_GET_PARA, get_current_alarm(0, &current_alarm));

    // Save one
    save_current_alarm(0, &sample_current_alarm[0]);

    // Now read back should succeed
    ASSERT_EQ(0, get_current_alarm(0, &current_alarm));
    ASSERT_STREQ(sample_current_alarm[0].AlarmRaisedTime, current_alarm.AlarmRaisedTime);
    ASSERT_EQ(sample_current_alarm[0]._event_id, current_alarm._event_id);
    ASSERT_EQ(sample_current_alarm[0]._event_type_id, current_alarm._event_type_id);

    // free
    put_current_alarm(&current_alarm);
    ASSERT_TRUE(current_alarm.AlarmRaisedTime == NULL);
    put_current_alarm(&current_alarm);

    // Get negative ID should return error
    ASSERT_EQ(FAILED_GET_PARA, get_current_alarm(-1, &current_alarm));
}

TEST_F(CurrentAlarmAPITest, test_remove_current_alarm)
{
    CurrentAlarm current_alarm;
    memset((void *)&current_alarm, 0, sizeof(CurrentAlarm));

    sim->Reset();

    // Current Alarm 0 is pointing to index 1
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_type_id");

    // Nothing should happen
    remove_current_alarm(0);

    save_current_alarm(0, &sample_current_alarm[0]);
    save_current_alarm(1, &sample_current_alarm[0]);

    remove_current_alarm(0);
    ASSERT_EQ(FAILED_GET_PARA, get_current_alarm(0, &current_alarm));
    ASSERT_EQ(0, get_current_alarm(1, &current_alarm));
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "2");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id");

    remove_current_alarm(1);
    ASSERT_EQ(FAILED_GET_PARA, get_current_alarm(0, &current_alarm));
    ASSERT_EQ(FAILED_GET_PARA, get_current_alarm(0, &current_alarm));
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index", "");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2.AlarmRaisedTime");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_id");
    sim->ValidateRdbValueNotExist(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".2._event_type_id");

    // Remove negative ID should not write to RDB or cause any problem
    remove_current_alarm(-1);
}

TEST_F(CurrentAlarmAPITest, test_get_current_alarm_event_type_id)
{
    sim->Reset();

    // Current Alarm 0 is pointing to index 1
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM "._index");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id");
    sim->SetupRdbActionTracking(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id");

    // Return correct value
    save_current_alarm(0, &sample_current_alarm[0]);
    ASSERT_EQ(sample_current_alarm[0]._event_type_id, get_current_alarm_event_type_id(0));
    // By using this function, only _event_type_id is read
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1.AlarmRaisedTime", 1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_id", 1, 1);
    sim->ValidateRdbValue(FAULT_MGMT_DB_PATH_CURRENT_ALARM ".1._event_type_id", 2, 1);

    // If it does not exist, should return < 0
    remove_current_alarm(0);
    ASSERT_GT(0, get_current_alarm_event_type_id(0));

    // Get negative ID should not cause any problem
    get_current_alarm_event_type_id(-1);
}

} // namespace
