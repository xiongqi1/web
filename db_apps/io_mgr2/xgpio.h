#ifndef __XGPIO_H__27112015
#define __XGPIO_H__27112015
/*
 * Copyright Notice:
 * Copyright (C) 2016 NetComm Wireless limited.
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
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file contains the Gpio class. A thin OO wrapper around the libgio functionality
 * For Modcomms there is some discovery and initialization code
 */
#include <sys/types.h>

class Gpio {
public:
    int gpio;
    Gpio(int gpio);
    ~Gpio();
    uint readValue();
    void writeValue(int val);
    bool setMode(uint mode);
    static void close();
private:
    uint mode;
};

void gpio_addModcomms();

#endif
