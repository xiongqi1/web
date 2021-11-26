#ifndef __SUPPL_H__
#define __SUPPL_H__

#include "../model/model.h"

typedef enum { at_cmd_enable = 1, at_cmd_disable = 0 } at_cmd_enable_disable_enum;

int handleSupplServ(const struct name_value_t* args);
int handle_clip(at_cmd_enable_disable_enum n);


int supplInit(void);
void supplFini(void);

typedef enum { ccfc_unconditional = 0, ccfc_busy = 1, ccfc_no_reply = 2, ccfc_not_reachable = 3, ccfc_none=4 } ccfc_reason_enum;
int handle_ccfc_query(ccfc_reason_enum reason);
int handle_ccwa_query(void);

#endif
