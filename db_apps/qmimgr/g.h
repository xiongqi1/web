#ifndef __G_H__
#define __G_H__

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/select.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <alloca.h>
#include <dirent.h>
#include <stdarg.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"


#include "funcschedule.h"
#include "generictree.h"
#include "dbenum.h"
#include "dbhash.h"
#include "resourcetree.h"
#include "rdb_ops.h"
#include "port.h"

#include "featurehash.h"

#include "qmidef.h"
#include "qmiservtran.h"
#include "qmimsg.h"
#include "qmiuniclient.h"
#include "qmiioctl.h"

#include "ezqmi.h"

#include "qmi_cell_info.h"

#ifdef V_SMS_QMI_MODE_y
#include <iconv.h>
#include "ezqmi_sms.h"
#include "gsm.h"
#endif // V_SMS_QMI_MODE_y

#ifdef QMI_VOICE_y
#include "rwpipe.h"
#include "ezqmi_voice.h"
#endif

#ifdef __uClinux__
#define fork vfork
#endif

#include "ezqmi_loc.h"

#endif
