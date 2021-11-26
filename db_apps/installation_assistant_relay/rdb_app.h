#ifndef RDB_APP_H_14150820012016
#define RDB_APP_H_14150820012016
/**
 * @file rdb_app.h
 * @brief Provides public functions and data structures to use rdb variables
 *
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
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LRDB_APP
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE PRDB_APPIBILITY OF
 * SUCH DAMAGE.
 */

/*******************************************************************************
 * Define public macros
 ******************************************************************************/

/*******************************************************************************
 * Declare public structures
 ******************************************************************************/

/*******************************************************************************
 * Declare public functions
 ******************************************************************************/

/**
 * @brief Initalises the rdb application
 *
 * @return 0 on success, or a negative value on error
 */
int rdb_app_init(void);

/**
 * @brief Terminates the rdb application
 *
 * @return Void
 */
void rdb_app_term(void);

/**
 * @brief Sets whether the device is attached or not to the rdb variable
 *
 * @param attached Indicates whether the device is attached or not
 * @return Void
 */
void rdb_app_set_device_attached(int attached);

/**
 * @brief Sets the remaining capacity of the battery to the rdb variable
 *
 * @param percentage The remaining capacity of the battery in percentage
 * @return Void
 */
void rdb_app_set_battery_remaining_capacity(unsigned char percentage);

/**
 * @brief Sets charging status of the battery to the rdb variable
 *
 * @param status Charging status of the battery
 * @return Void
 */
void rdb_app_set_battery_charging_status(const char *status);

/**
 * @brief Sets SW version to the rdb variable
 *
 * @param version The software version for Installation Assistant
 * @return Void
 */
void rdb_app_set_sw_version(const char *version);

/**
 * @brief Sets HW version to the rdb variable
 *
 * @param version The hardware version for Installation Assistant
 * @return Void
 */
void rdb_app_set_hw_version(const char *version);

/**
 * @brief Sets the debug mode. It will create the RDB before setting the mode
 *        if not exists.
 *
 * @param mode A string representing the debug mode
 * @return Void
 */
void rdb_app_create_or_set_debug_mode(const char *mode);

/**
 * @brief Sets the authentication status. It will create the RDB before setting
 *        the mode if not exists.
 *
 * @param mode A string representing whether the attached device is
 *             authenticated or not.
 * @return Void
 */
void rdb_app_create_or_set_authenticated(const char *authenticated);

/**
 * @brief Gets the status of the LED
 *
 * @param id The led ID to be controlled
 * @param color The color to be displayed on the led
 * @param blink_interval Indicates how quickly the led blinks
 * @return 0 on success, or a negative value on error
 */
int rdb_app_get_led_status(int id, unsigned char *color, unsigned short *blink_interval);

#endif
