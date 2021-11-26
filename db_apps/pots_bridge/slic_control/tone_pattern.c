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

#include <math.h>
#include "./tone_pattern.h"

#define SAMPLING_RATE			8000.00
#define SAMPLES_PER_MILISEC		(SAMPLING_RATE / 1000)
#define PI_CONSTANT				3.14159265
void tone_to_oscillator_settings(struct slic_oscillator_settings_t* settings, const struct tone_t* tone)
{
	double coeff = cos( (((2 * PI_CONSTANT) * tone->tone) / (double)SAMPLING_RATE));
	double volume = tone->volume > 100 ? 1 : (double)tone->volume / 100;
	settings->frequency = (int)(coeff * 0x8000);
	settings->amplitude = sqrt((1 - coeff) / (1 + coeff)) * (double)(0x3fff) / 4 * volume / 1.11;
	settings->cycles = SAMPLES_PER_MILISEC * tone->duration;
}

static int tones_end(const struct tone_t* tone)
{
	return tone->duration == 0;
}

unsigned int tones_to_oscillator_settings(struct slic_oscillator_settings_t* settings, const struct tone_t* tone, unsigned int* duration)
{
	unsigned int duration1 = 0;
	unsigned int duration2 = 0;
	struct slic_oscillator_settings_t* begin = settings;
	for (; !tones_end(tone); ++settings, ++tone)
	{
		tone_to_oscillator_settings(settings, tone);
		duration1 += tone->duration;
	}
	settings->frequency = 0;
	settings->amplitude = 0;
	settings->cycles = 0;
	for (++settings, ++tone; !tones_end(tone); ++settings, ++tone)
	{
		tone_to_oscillator_settings(settings, tone);
		duration2 += tone->duration;
	}
	settings->frequency = 0;
	settings->amplitude = 0;
	settings->cycles = 0;
	if (duration)
	{
		*duration = duration1 > duration2 ? duration1 : duration2;
	}
	return (settings + 1 - begin) * sizeof(struct slic_oscillator_settings_t);
}

