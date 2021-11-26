/*!
 * Copyright Notice:
 * Copyright (C) 2013 NetComm Wireless limited.
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

#ifndef HEADER_GUARD_LOGMASK_20130917_
#define HEADER_GUARD_LOGMASK_20130917_

typedef enum {
	LOGMASK_AT = 0,
	LOGMASK_SIM,
	LOGMASK_SMS,
	LOGMASK_PDU,
#ifdef USSD_SUPPORT
	LOGMASK_USSD,
#endif	
	LOGMASK_MAX
} logmask_enum_type;

#define MAX_LOG_TYPE	10
typedef struct {
	char *logname;
	int loglevel;
} logmask_t;

extern logmask_t log_db[MAX_LOG_TYPE];

#endif // HEADER_GUARD_LOGMASK_20130917_

/*
* vim:ts=4:sw=4:
*/
