#ifndef __HAUWEI_EM820U_H__
#define __HAUWEI_EM820U_H__

#include "../abstractcmd.h"

int huawei_em820u_info(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);
int huawei_em820u_detect(struct abstractcmd_t* abstract,struct abstractcmd_param_t* param);

#endif
