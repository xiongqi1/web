/*
 * setup_leds_mode.c
 *
 * Setup LEDs model object to be used in the program
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "leds_model.h"

#ifdef V_IOBOARD_bordeaux
#include "leds_model_bordeaux.h"
#define leds_model__new(BASE) bordeaux_leds_model__new(BASE)
#endif

#if defined(V_IOBOARD_nrb0206) || defined(V_IOBOARD_lark)
#include "leds_model_nrb0206.h"
#define leds_model__new(BASE) lark_leds_model__new(BASE)
#endif

#include <stdlib.h>

/*
 * setup_leds_model. See leds_model.h.
 */
leds_model *setup_leds_model() {
    leds_model *model = leds_model__new(NULL);
    if (!model) {
        return NULL;
    }
    model->init(model);
    return model;
}
