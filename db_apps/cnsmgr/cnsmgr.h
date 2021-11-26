#ifndef __CNSMSG_H__
#define __CNSMSG_H__


#include "base.h"

#define CNSMGR_MAX_VALUE_LENGTH				256

#define CNSMGR_TIMEOUT_NOTIFICATION		(10*1000)

#define CNSGMR_DB_SUBKEY_CMD				0
#define CNSMGR_DB_SUBKEY_STAT				1

#define CNSMGR_DB_KEY_VOICECALL			(-1)
#define CNSMGR_DB_KEY_CURRENTBAND		(-2)
#define CNSMGR_DB_KEY_SIMCARDPIN		(-3)
#define CNSMGR_DB_KEY_CNS_VERSION		(-4)
#define CNSMGR_DB_KEY_SMS						(-5)
#define CNSMGR_DB_FIFO_SMS					(-6)

#define CNSMGR_DB_PROFILE						(-7)
#define CNSMGR_DB_PLMN							(-8)

#define CNSMGR_DB_KEY_PREFIX				"wwan.%d."
#define CNSMGR_DB_AGPS_PREFIX				"sensors.gps.%d."

#define CNSMGR_ENCODE_GSM_7					0
#define CNSMGR_ENCODE_GSM_8					1
#define CNSMGR_ENCODE_UCS_2					2
#define CNSMGR_ENCODE_UNKNOWN				3

BOOL cnsmgr_init(void);
void cnsmgr_fini(void);

#endif
