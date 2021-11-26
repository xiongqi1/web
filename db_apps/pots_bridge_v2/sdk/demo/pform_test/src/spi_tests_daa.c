/******************************************************************************
 * Copyright (c) 2015-2016 by Silicon Laboratories
 *
 * $Id: spi_tests_daa.c 6007 2016-10-03 16:26:33Z nizajerk $
 *
 * This file contains proprietary information.
 * No dissemination allowed without prior written permission from
 * Silicon Laboratories, Inc.
 *
 * File Description:
 *
 * This file implements some basic SPI communications test to the DAA (SI3050)
 *
 */

#include "spi_tests.h"

#define WRV_REG_ADDR      34
#define REV_REG_ADDR      11
#define LS_REG_ADDR       12
#define PCMMODE_REG_ADDR  33
#define DAACTRL2_REG_ADDR 6

#define FDT_BIT           0x40

/* The Si3050 has 1 bit less than the ProSLIC's, so if the
 * register Read/Write Control word (CW) was coded for
 * ProSLIC, we need to increment by 2, not one for the
 * channel address.
 */
#if defined(VMB1) || defined(VMB2)
#define CHANNEL_INC       2
#else
#define CHANNEL_INC       1
#endif

/**********************************************************
 * Read the revision ID and see if it might be valid.
 */
static int spiReadRevIDTest(controlInterfaceType *hwIf, uInt8 channel)
{
  return spiReadRevIDTestCommon(hwIf, channel, REV_REG_ADDR);
}

/**********************************************************
 * Write a test pattern to a register, read back to see if
 * the value changed.
 */

static int spiBasicWriteTest(controlInterfaceType *hwIf, uInt8 channel)
{
  return spiBasicWriteTestCommon(hwIf, channel, WRV_REG_ADDR);
}

/**********************************************************
 *  Check if reset pin is working by writing to a
 *  register and reseting the device and see if the value
 *  changed.
 */
static int spiResetTest(controlInterfaceType *hwIf, uInt8 channel)
{
  return spiResetTestCommon(hwIf, channel, WRV_REG_ADDR);
}

/**********************************************************
 * Read the Line-Side Device Status (register 12)
 * Not really a SPI test, but would help indicate the hw/clocks
 * are good...
 */
static int spiLineSideStatus(controlInterfaceType *hwIf, uInt8 channel)
{
  uInt8 data;
  int rc = 0;

  /* Does a very basic initialization based upon section 5.3 */
  hwIf->WriteRegister_fptr(hwIf->hCtrl, channel, PCMMODE_REG_ADDR, 0x20);
  hwIf->WriteRegister_fptr(hwIf->hCtrl, channel, DAACTRL2_REG_ADDR, 0x0);

  /* Put some delay for synchronization on the line side device */
  hwIf->Delay_fptr(hwIf->hCtrl, 50);

  data = hwIf->ReadRegister_fptr(hwIf->hCtrl, channel, LS_REG_ADDR);

  if( (data & FDT_BIT) == 0)
  {
    TEST_LOG("Expected: 0x40 bit mask, read: 0x%02X\n", data);
    rc = 1;
  }

  /* Power down the line side device */
  hwIf->WriteRegister_fptr(hwIf->hCtrl, channel, DAACTRL2_REG_ADDR, 0x10);

  REPORT_LOG(rc);
  return rc;
}

/**********************************************************
 *  Run through some basic tests. This code is NOT compatible
 *  with any of the ProSLIC chips.
 */

static silabs_test testList[] =
{
  spiReadRevIDTest, spiBasicWriteTest,
  spiResetTest, spiLineSideStatus,
  NULL
};

int spiTests(controlInterfaceType *hwIf, uInt8 channel)
{
  return runTests(hwIf, channel, testList);
}
/**********************************************************
 * Timed tests start here...
 *
 */

static int spiTimedReadRegTest(controlInterfaceType *hwIf, uInt8 channel)
{
  return spiReadTimedTestCommon(hwIf, channel, REV_REG_ADDR, LOOP_COUNT);

}

static int spiTimedWriteRegTest(controlInterfaceType *hwIf, uInt8 channel)
{
  return spiWriteTimedTestCommon(hwIf, channel, WRV_REG_ADDR, LOOP_COUNT);
}

/**********************************************************
 *  Run through some basic tests and see how long they took.
 *  NOT compatbile with Si321x or Si3050.
 */

static silabs_test spiTimedTestList[] =
{
  spiTimedReadRegTest,
  spiTimedWriteRegTest,
  NULL
};

int spiTimedTests(controlInterfaceType *hwIf, uInt8 channel)
{
  return runTests(hwIf, channel, spiTimedTestList);
}


/**********************************************************
 *  Check for register/RAM access across multiple channels.
 * 
 *
 */

int SpiMultiChanTest(controlInterfaceType *hwIf, uInt8 channel_count)
{
  int i;
  int rc = 0;
  uInt8   rData;

  /* Do a simple write to 1 register with different values for each
   * channel.  Verify the values when read back match what we wrote.
   */
  for(i = 0; i < channel_count; i++)
  {
    hwIf->WriteRegister_fptr(hwIf->hCtrl, (i*CHANNEL_INC), WRV_REG_ADDR, i);
  }

  for(i = 0; i < channel_count; i++)
  {
    rData = hwIf->ReadRegister_fptr(hwIf->hCtrl, (i*CHANNEL_INC), WRV_REG_ADDR);

    if(  rData != i)
    {
      TEST_LOG("Expected: %02X read: %02X\n", i, rData);
      rc = 1;     
    }
  } 
  
  REPORT_LOG(rc);
  return rc;
}

