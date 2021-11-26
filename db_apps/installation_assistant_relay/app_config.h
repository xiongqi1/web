#ifndef APP_CONFIG_H_16140010122015
#define APP_CONFIG_H_16140010122015
/**
 * @file app_config.h
 * @brief Defines configuration for the application
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
 */

#define MAX_LED_NUM 3

/**
 * @note "poll" works better than "select" on NRB-51.
 *       "poll" is tested on NRB-51, but "select" is not fully tested.
 *       If you want to use "select", more tests are needed.
 */
#define USE_POLL

#define DEFAULT_DEVICE_NAME "NRB-51"
#define DEFAULT_SOCKET_HOSTNAME "127.0.0.1"
#define DEFAULT_SERIAL_DEVICE_NAME "/dev/ttyHSL0"
#define DEFAULT_SERIAL_BAUD_RATE 115200

#endif
