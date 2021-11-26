/*!
* Copyright Notice:
* Copyright (C) 2010 Call Direct Cellular Solutions Pty. Ltd.
*
* This file or portions thereof may not be copied or distributed in any form
* (including but not limited to printed or electronic forms and binary or object forms)
* without the expressed written consent of Call Direct Cellular Solutions Pty. Ltd
* Copyright laws and International Treaties protect the contents of this file.
* Unauthorized use is prohibited.
*
*
* THIS SOFTWARE IS PROVIDED BY CALL DIRECT CELLULAR SOLUTIONS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CALL DIRECT
* CELLULAR SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OFF
* SUCH DAMAGE.
*
*/

#ifndef TELEPHONY_PROFILE_H_
#define TELEPHONY_PROFILE_H_

/*---------------------------------------------------------------------------------
* Common profile
*--------------------------------------------------------------------------------- */
#define REGEX_SIZE 128
#define INTERNATIONAL_PREFIX_SIZE 6
#define DEFAULT_DIAL_TIMEOUT_SEC 5

/*---------------------------------------------------------------------------------
* Default Telephony profile
*--------------------------------------------------------------------------------- */
/* Australian Emergency call 000, New Zealand Emergency call 111, 112, 911 */
#define DEFAULT_REGEX "^0[23478][0-9]{9,9}|^[2-9][0-9]{9,9}|^000|^111|^112|^911"
#define DEFAULT_INTERNATIONAL_PREFIX "0011"

/*---------------------------------------------------------------------------------
* Canada Telephony profile
*--------------------------------------------------------------------------------- */
// need to check :  spec. requirement doc. and dial plan doc don't match
#define CANADAIAN_REGEX "^00[0-9]{13}|^[2-9]11|^11[^2]{2}|^112[0-9]{2}|^011[0-9]{13}|^01[0-9]{15}|^[0-1][2-9]{10,10}|^[2-9][0-9]{9,9}"
#define CANADIAN_INTERNATIONAL_PREFIX "0011"
#define CANADIAN_ROH_TIMEOUT	15		/* offhook alarm 15 seconds */
#define CANADIAN_VMWI_TIMEOUT	3		/* vmwi timeout 3 seconds */

/*---------------------------------------------------------------------------------
* Rogers Telephony profile - based on TS
*--------------------------------------------------------------------------------- */
/* added '911' for emergency call */
#define ROGERS_REGEX "^911|^00[0-9]{13}|^[2-9]11|^11[^2]{2}|^112[0-9]{2}|^011[0-9]{13}|^01[0-9]{15}|^[0-1][2-9]{10,10}|^[2-9][0-9]{9,9}"
#define CANADIAN_STUTTER_TONE_TIMEOUT	10		/* stutter tone for 10 seconds */

/*---------------------------------------------------------------------------------
* NZ Telecom Telephony profile
*--------------------------------------------------------------------------------- */
#define NZ_TELECOM_VMC     "+6483083210"

/*---------------------------------------------------------------------------------
* Telstra Telephony profile
*--------------------------------------------------------------------------------- */
#define TELSTRA_REGEX "^000|^111|^112|^911|^0[23478][0-9]{8,8}|^[2-9][0-9]{7,7}"
#define TELSTRA_ROH_TIMEOUT1	90		/* offhook alarm after 90 seconds */
#define TELSTRA_ROH_TIMEOUT2	30		/* congestion tone ON period */
#define TELSTRA_CPE_TIMEOUT	60		/* Calling party engaged / hangs up */
/*---------------------------------------------------------------------------------
* Malaysia (Celcom) Telephony profile (based on AUS/NZ)
*--------------------------------------------------------------------------------- */
/* Malaysian (Celcom) Emergency call 999, 994, 991, 112 */
#define CELCOM_REGEX "^0[2-9][0-9]{8,8}|^01[0-9]{9,9}|^999|^994|^991|^112"
#define MALAYSIAN_INTERNATIONAL_PREFIX "00"

extern tel_profile_type tel_profile;
extern tel_sp_type tel_sp_profile;

void initialize_sp(void);
void initialize_profile_variables(void);
void display_pots_setting(const char* device[], const char* instance, BOOL changed);

#endif /* TELEPHONY_PROFILE_H_ */

/*
* vim:ts=4:sw=4:
*/
