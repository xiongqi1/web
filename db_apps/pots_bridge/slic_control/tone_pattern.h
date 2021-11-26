/*!
* Copyright Notice:
* Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
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


#ifndef TONE_PATTERN_H_20090401_
#define TONE_PATTERN_H_20090401_

#include "slic/slic_control.h"

typedef enum {   notes_rest = 0
			, notes_c3 = 131, notes_c3sharp = 139, notes_d3 = 147, notes_d3sharp = 156, notes_e3 = 165, notes_f3 = 175, notes_f3sharp = 185, notes_g3 = 196, notes_g3sharp = 208, notes_a3 = 220, notes_a3sharp = 233, notes_b3 = 247
			, notes_c4 = 262, notes_c4sharp = 277, notes_d4 = 294, notes_d4sharp = 311, notes_e4 = 330, notes_f4 = 349, notes_f4sharp = 370, notes_g4 = 392, notes_g4sharp = 415, notes_a4 = 440, notes_a4sharp = 466, notes_b4 = 523 } notes_enum;

struct tone_t
{
	unsigned int tone; // frequency in Hz
	unsigned int volume; // volume in % of max; anything above 100% will be set 100%
	unsigned int duration; // duration in milliseconds
};

void tone_to_oscillator_settings( struct slic_oscillator_settings_t* settings, const struct tone_t* tone );
unsigned int tones_to_oscillator_settings( struct slic_oscillator_settings_t* settings, const struct tone_t* tone, unsigned int* duration );


#endif /* TONE_PATTERN_H_20090401_ */

