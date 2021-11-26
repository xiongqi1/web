/**
 * cell track log generator
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

#ifndef __CELLTRACKING_H_15031812042018
#define __CELLTRACKING_H_15031812042018

/* Variables for supporting of cell tracking feature
--Please refer to  https://pdgwiki.netcommwireless.com/mediawiki/index.php/WNTD_RF_cell_tracking
*/
#define CELLTRACKING_EVENT_MAX 6000
#define CELLTRACKING_LINE_MAX 255
#define RSRP_BUF_MAX 32

#ifdef V_CELLTRACKING_wntdv3
#define LOG_STORAGE_PATH "/opt"
#endif
#ifdef V_CELLTRACKING_wntdv2
#define LOG_STORAGE_PATH "/NAND"
#endif

#define TRUE 1
#define FALSE 0

/*
Detect any pci and earfcn changes
Parameters:
report_pci -- output, current PCI
report_earfcn -- output, current EARFCN
Return
TRUE -- PCI or EARFCN is changed
FALSE -- PCI and EARTCH has no change
*/
int detect_cell_change(int *report_pci, int *report_earfcn);

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
int get_signal_info(char *syslog_line, char *celllog_line, int line_max);

#endif
