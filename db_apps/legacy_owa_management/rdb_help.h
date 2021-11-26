/*
 * @file rdb_help.h
 * @brief rdb relevant functions
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY Casa Systems ``AS IS''
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
#ifndef RDB_HELP_H_11020009122019
#define RDB_HELP_H_11020009122019

/*
 * rdb initialize
 *
 * @return 0 on success; non-zero error code on failure
 */
int rdb_init();

/*
 * get rdb file descriptor
 *
 * @return rdb fd
 */
int rdb_getfd();

/*
 * rdb resource release
 */
void rdb_cleanup();

/*
 * set baudrate rdb key value
 *
 * @param baudrate integer stand for baudrate set in serial port
 * @return 0 on success; non-zero error code on failure
 */
int rdb_set_baudrate(int baudrate);

/*
 * get battery level from rdb key
 *
 * @return positive number on success; -1 error code on failure
 */
unsigned char rdb_get_battery_level();

/*
 * get battery capacity from rdb key
 *
 * @return 0-3 on success; -1 error code on failure
 */
char rdb_get_battery_status();

/*
 * get hardware version from rdb key
 * @param hw_str pointer to char buffer to store hardware version read back
 * @param len pointer to int to indicate the length of hw_str
 * @return 0 on success; -1 error code on failure
 */
int rdb_get_hw_ver (char * hw_str, int * len);

/*
 * get hardware version from rdb key
 * @param sw_str pointer to char buffer to store software version read back
 * @param len pointer to int to indicate the length of sw_str
 * @return 0 on success; -1 error code on failure
 */
int rdb_get_sw_ver (char * sw_str, int * len);

/*
 * get model name from rdb key
 * @param model_str pointer to char buffer to store model name
 * @param len pointer to int to indicate the length of sw_str
 * @return 0 on success; -1 error code on failure
 */
int rdb_get_hw_model (char * model_str, int * len);

/*
 * set led rdb key to enable led on/off/flashing
 * @param led_num the led number we want to control
 * @param led_color the led color we want to change to
 * @led_flash_feq the frequency led flashing in milliseconds
 * @return 0 on success; -1 error code on failure
 */
int rdb_set_led(int led_num, int led_color, int led_flash_feq);

/*
 * set legacy owa connection status rdb key
 * @param status the legacy owa connection status 1--connected, 3--disconnected, 4--detection timeout
 * @return 0 on success; -1 error code on failure
 */
int rdb_set_legacy_owa_status(int status);

/*
 * turn off leds if it is in power_off mode, and proceed the turning off sequence
 * @return 1 on success; 0 not at power off mode or error happens
 */
int rdb_shutting_down();
#endif