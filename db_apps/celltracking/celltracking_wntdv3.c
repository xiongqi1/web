/**
 * cell track log generator for wnrdv3
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
*/
/* local headers */
#include "log.h"
#include "rdb.h"
#include "celltracking.h"

/* standard headers */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/*
Detect any pci and earfcn changes
Parameters:
report_pci -- output, current PCI
report_earfcn -- output, current EARFCN
Return
TRUE -- PCI or EARFCN is changed
FALSE -- PCI and EARTCH has no change
*/
int detect_cell_change(int *report_pci, int *report_earfcn)
{
    char val[CELLTRACKING_LINE_MAX];
    // value format: "E,42000,2, ..."
    rdb_get_str(val, sizeof(val), "wwan.0.cell_measurement.0");
    if (val[0] != '\0') {
        char *token;
        int earfcn;
        int pci;
        // ignore first field
        token = strtok(val, ",");
        if (token == NULL) return FALSE;

        // second field: earfcn
        token = strtok(NULL, ",");
        if (token == NULL) return FALSE;
        earfcn = atoi(token);

        // third field: pci
        token = strtok(NULL, ",");
        if (token == NULL) return FALSE;
        pci = atoi(token);

        if (earfcn != *report_earfcn || pci != *report_pci) {
            *report_earfcn = earfcn;
            *report_pci = pci;
            DEBUG("cell_changed earfcn=%d, pci=%d\n", earfcn, pci);
            return TRUE;
        }
    }
    return FALSE;
}

/*
Get signal information
There can be some delay for the signal info rdbs to be
populated and they get populated seperately. This function
will not return success unless all the rdbs are currently available.

Parameters:
syslog_line -- message line for syslog
celllog_line -- message line for servingcell log
line_max -- max buffer size of above parameters
Return:
TRUE -- success
FALSE -- invalid
*/
int get_signal_info(char *syslog_line, char *celllog_line, int line_max)
{
    char rsrp[RSRP_BUF_MAX];
    char rssinr[RSRP_BUF_MAX];
    char rsrq[RSRP_BUF_MAX];
    rdb_get_str(rsrp, sizeof(rsrp), "wwan.0.signal.0.rsrp");
    rdb_get_str(rsrq, sizeof(rsrq), "wwan.0.signal.rsrq");
    rdb_get_str(rssinr, sizeof(rssinr), "wwan.0.signal.rssinr");

    // please make sure the rsrp, rsrq, rssinr are valid
    if ((rsrp[0] == '\0') || (rsrq[0] == '\0') || (rssinr[0] == '\0')) {
        return FALSE;
    }

    /* syslog:<timestamp> new serving cell earfcn=<earfcn>, pci=<pci>, rsrp=<rsrp>, rsrq=<rsrq>, rssinr=<rssinr> */
    snprintf(syslog_line, line_max, "rsrp=%s, rsrq=%s, rssinr=%s", rsrp, rsrq, rssinr);
    /* servicecell.{0,1}:<timestamp> earfcn=<earfcn>, pci=<pci>, rsrp=<rsrp0,rsrp1>, rsrq=<rsrq>, rssinr=<rssinr0,rssinr1> */
    snprintf(celllog_line, line_max, "rsrp=%s,%s, rsrq=%s, rssinr=%s,%s", rsrp, rsrp, rsrq, rssinr, rssinr);
    return TRUE;
}
