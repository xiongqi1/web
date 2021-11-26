/** Copyright (c) 2017 by Silicon Laboratories
**
** $Id: demo_config.h 6851 2017-10-02 21:56:49Z nizajerk $
**
** Distributed by:
** Silicon Laboratories, Inc
**
** This file contains proprietary information.
** No dissemination allowed without prior written permission from
** Silicon Laboratories, Inc.
**
** File Description:
** This header file configures various parameters for the demo application.
**
*/

#ifndef __APIDEMO_CONFIG_HDR__
#define __APIDEMO_CONFIG_HDR__

#define SI3217XB_NUMBER_OF_DEVICE 1 /* Same for 17x C */
#define SI3218X_NUMBER_OF_DEVICE 1
#define SI3219X_NUMBER_OF_DEVICE 1
#define SI3226X_NUMBER_OF_DEVICE 1
#define SI3228X_NUMBER_OF_DEVICE 1
#define SI3050_NUMBER_OF_DEVICE  1
#define DEMO_PORT_COUNT 1

/* 26x_3050 example
#undef DEMO_PORT_COUNT
#define DEMO_PORT_COUNT 2
#define SIVOICE_USE_CUSTOM_PORT_INFO 1
*/

#define MAX_NUMBER_OF_DEVICE 16
#define MAX_NUMBER_OF_CHAN 32

/* If manual ring cadence code is included, how may periods to support... */
#define PROSLIC_MAX_RING_PERIODS  2

#endif

