/*!
 * Copyright Notice:
 * Copyright (C) 2012 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Pty. Ltd
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
 * SUCH DAMAGE.
 *
 */
#ifndef USSD_H_20120830
#define USSD_H_20120830

#ifdef USSD_SUPPORT

//#define USSD_UCS2_TEST

#define SET_LOG_LEVEL_TO_USSD_LOGMASK_LEVEL    setlogmask(LOG_UPTO(log_db[LOGMASK_USSD].loglevel));
#define SET_LOG_LEVEL_TO_AT_LOGMASK_LEVEL      setlogmask(LOG_UPTO(log_db[LOGMASK_AT].loglevel));

int handle_ussd_notification(const char* s);
int default_handle_command_ussd(const struct name_value_t* args);
#endif
#endif  /* USSD_H_20120830 */
