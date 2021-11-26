/*
 * Install Tool IO Manager
 *
 * This daemon monitors widget link and powers on/off OWA accordingly
 * Relevant RDBs are set to reflect the IOs
 *
 * It also monitors WakeupSignal IO to signal charger IC to power off.
 *
 * Copyright Notice:
 * Copyright (C) 2019 NetComm Wireless Limited.
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
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <rdb_ops.h>
#include <libgpio.h>

#include "utils.h"

#define WIDGET_LINK_GPIO 2
#define WIDGET_LINK_ACTIVE_LOW 1

#define SUPPLY_ENABLE_GPIO 11
#define SUPPLY_ENABLE_ACTIVE_LOW 0

#define WAKEUP_SIGNAL_GPIO 40
#define WAKEUP_SIGNAL_ACTIVE_LOW 0

#define OWA_CONNECTED_RDB "owa.connected"
#define OWA_POWER_ENABLED_RDB "owa.power.enabled"

#define POWEROFF_TIME_MS 4000
#define POWEROFF_RDB "service.system.poweroff"
#define DC_PRESENT_RDB "system.battery.dcin_present"

#define BATTERY_LEVEL_RDB "system.battery.capacity"
#define OWAONOFF_TIME_MS 100
#define INSTALL_TOOL_MODE_RDB "install_tool.mode"
#define OWA_IN_UPDATE "flash_owa"

/* get logical level given physical level and active low flag */
#define GPIO_LEVEL(val, active_low) (active_low ? !val : !!val)

static struct rdb_session * rdb_s;
static int rdbfd;

volatile static int terminate = 0;

static unsigned int battery_in_critical_level = 0;

/* signal handler for SIGINT, SIGQUIT and SIGTERM */
static void sig_handler(int sig)
{
    (void)sig;
    terminate = 1;
}

/*
 * gpio callback for WIDGET_LINK_GPIO
 *
 * @param arg A void pointer set by gpio_register_callback. We actually pass in
 * an integer of 0 or 1, representing the physical electrical level low or high.
 * So the callback should cast it to int first.
 * @param time Unused
 */
static void io_cb(void * arg, unsigned time)
{
    static volatile sig_atomic_t last_level = -1;
    if (time >= OWAONOFF_TIME_MS) {
        int level = GPIO_LEVEL((int)arg, WIDGET_LINK_ACTIVE_LOW);
	/* Read the GPIO pin. If it's different from the callback level, ignore */
	int level_read = GPIO_LEVEL(gpio_read(WIDGET_LINK_GPIO), WIDGET_LINK_ACTIVE_LOW);
	if (level == level_read && level != last_level) {
            /* if battery is in critical level, it will prevent turning on power but allow
               clear status */
            if (gpio_write(SUPPLY_ENABLE_GPIO, GPIO_LEVEL(level&&(!battery_in_critical_level), SUPPLY_ENABLE_ACTIVE_LOW))) {
                BLOG_ERR("Failed to %s gpio %d\n", level&&(!battery_in_critical_level) ? "set" : "clear", SUPPLY_ENABLE_GPIO);
            }
            if (rdb_set_string(rdb_s, OWA_CONNECTED_RDB, level ? "1" : "0")) {
                BLOG_ERR("Failed to set RDB %s\n", OWA_CONNECTED_RDB);
            }
            if (rdb_set_string(rdb_s, OWA_POWER_ENABLED_RDB, level&&(!battery_in_critical_level)? "1" : "0")) {
                BLOG_ERR("Failed to set RDB %s\n", OWA_POWER_ENABLED_RDB);
            }
            BLOG_DEBUG("io_cb invoked: level=%d\n", level);
            last_level = level;
	}
    }
}

/*
 * Check if power adapter is plugged in
 *
 * @return 1 if adapter is plugged in, 0 otherwise
 */
static int is_dcin_present(void)
{
    int status;
    int ret;
    ret = rdb_get_int(rdb_s, DC_PRESENT_RDB, &status);
    return !ret && status == 1;
}

/*
 * Check if battery level in critical level 0
 *
 * @return 1 if battery in critical level, 0 otherwise
 */
static int is_battery_in_critical(void)
{
    int status;
    int ret;
    ret = rdb_get_int(rdb_s, BATTERY_LEVEL_RDB, &status);
    return !ret && status == 0;
}

/*
 * Check if any flashing operation is in progress
 *
 * @return 1 if there is owa flash or lark flash in progress, 0 otherwise
 */
static int is_sys_in_update(void)
{
    char rdbv[16];;
    int ret;
    ret = rdb_get_string(rdb_s, INSTALL_TOOL_MODE_RDB, rdbv, sizeof(rdbv));
    return !ret && !strcmp(rdbv,OWA_IN_UPDATE);
}


/*
 * Trun off OWA power and clear related RDB
 */
static void turn_off_owa()
{
    gpio_write(SUPPLY_ENABLE_GPIO, GPIO_LEVEL(0, SUPPLY_ENABLE_ACTIVE_LOW));
    rdb_set_string(rdb_s, OWA_POWER_ENABLED_RDB, "0");
    BLOG_DEBUG("OWA is shut off because of battery in critical level\n");

}


/*
 * gpio callback for WAKEUP_SIGNAL_GPIO
 *
 * @param arg A void pointer set by gpio_register_callback. Unused.
 * @param time Duration in millisec wakeup signal IO is active for
 */
static void wakeup_signal_cb(void * arg, unsigned time)
{
    (void) arg;
    BLOG_DEBUG("wakeup_signal_cb invoked: time=%d\n", time);
    if (is_dcin_present()) {
	BLOG_NOTICE("Key press is ignored when charger is plugged in\n");
    } else if (time >= POWEROFF_TIME_MS) {
        if (rdb_set_string(rdb_s, POWEROFF_RDB, "1")) {
            BLOG_ERR("Failed to set RDB %s\n", POWEROFF_RDB);
        } else {
            BLOG_NOTICE("Key is long pressed. Powering off...\n");
        }
    }
}

/*
 * initialise and set up GPIOs
 *
 * @return 0 on success; negative error code on failure
 */
int init(void)
{
    int ret;
    ret = gpio_init("/dev/gpio");
    if (ret) {
        BLOG_ERR("Failed to init GPIO\n");
        return ret;
    }

    ret = gpio_request_pin(WIDGET_LINK_GPIO);
    if (ret) {
        BLOG_ERR("Failed to request GPIO %d\n", WIDGET_LINK_GPIO);
        return ret;
    }
    ret = gpio_request_pin(SUPPLY_ENABLE_GPIO);
    if (ret) {
        BLOG_ERR("Failed to request GPIO %d\n", SUPPLY_ENABLE_GPIO);
        return ret;
    }
    ret = gpio_request_pin(WAKEUP_SIGNAL_GPIO);
    if (ret) {
        BLOG_ERR("Failed to request GPIO %d\n", WAKEUP_SIGNAL_GPIO);
        return ret;
    }

    ret = gpio_set_input(WIDGET_LINK_GPIO);
    if (ret) {
        BLOG_ERR("Failed to set GPIO %d as input\n", WIDGET_LINK_GPIO);
        return ret;
    }
    ret = gpio_set_output(SUPPLY_ENABLE_GPIO, 0);
    if (ret) {
        BLOG_ERR("Failed to set GPIO %d as input\n", SUPPLY_ENABLE_GPIO);
        return ret;
    }
    ret = gpio_set_input(WAKEUP_SIGNAL_GPIO);
    if (ret) {
        BLOG_ERR("Failed to set GPIO %d as input\n", WAKEUP_SIGNAL_GPIO);
        return ret;
    }

    ret = gpio_register_callback_time(WIDGET_LINK_GPIO, 0, OWAONOFF_TIME_MS, io_cb, (void *)0);
    if (ret) {
        BLOG_ERR("Failed to register cb on %d level 0\n", WIDGET_LINK_GPIO);
        return ret;
    }
    ret = gpio_register_callback_time(WIDGET_LINK_GPIO, 1, OWAONOFF_TIME_MS, io_cb, (void *)1);
    if (ret) {
        BLOG_ERR("Failed to register cb on %d level 1\n", WIDGET_LINK_GPIO);
        return ret;
    }
    ret = gpio_register_callback_time(WAKEUP_SIGNAL_GPIO, GPIO_LEVEL(1, WAKEUP_SIGNAL_ACTIVE_LOW), POWEROFF_TIME_MS, wakeup_signal_cb, NULL);
    if (ret) {
        BLOG_ERR("Failed to register cb on %d level 1\n", WAKEUP_SIGNAL_GPIO);
        return ret;
    }
    /* register battery percentage key changing as rdb trigger */
    ret = rdb_subscribe(rdb_s, BATTERY_LEVEL_RDB);
    if (ret < 0 && ret != -ENOENT) {
        BLOG_ERR("Failed to subscribe %s\n", BATTERY_LEVEL_RDB);
        return ret;
    }

    return 0;
}

/* clean up GPIO resources */
void term(void)
{
    gpio_unregister_callbacks(WAKEUP_SIGNAL_GPIO);
    gpio_unregister_callbacks(WIDGET_LINK_GPIO);
    gpio_free_pin(WAKEUP_SIGNAL_GPIO);
    gpio_free_pin(SUPPLY_ENABLE_GPIO);
    gpio_free_pin(WIDGET_LINK_GPIO);
    gpio_exit();
}

int main_loop(void)
{
    int ret;
    fd_set fdset;

    while (!terminate) {
        // wait for rdb triggers
        FD_ZERO(&fdset);
        FD_SET(rdbfd, &fdset);
        ret = select(rdbfd + 1, &fdset, NULL, NULL, NULL);
        if (ret < 0) { // error
            if (errno == EINTR) {
		/* as all gpio callback are async and will interrupt select
                 * and then caused EINTR error from select call, please see:
                 * http://man7.org/linux/man-pages/man7/signal.7.html
                 * section: Interruption of system calls and library
                 * functions by signal handlers
                 */
                continue;
            }
            BLOG_ERR("select returned %d, errno: %d\n", ret, errno);
            return ret;
        } else { // rdb triggered
            battery_in_critical_level = is_battery_in_critical();
            if (battery_in_critical_level && !is_sys_in_update()) {
                turn_off_owa();
            }
        }
    }

    return 0;
}

int main(void)
{
    int ret;

    openlog("ioman", LOG_CONS, LOG_USER);

    if (rdb_open(NULL, &rdb_s) < 0 || !rdb_s) {
        BLOG_ERR("Failed to open RDB\n");
        ret = -1;
        goto fin;
    }
    rdbfd = rdb_fd(rdb_s);
    if (rdbfd < 0) {
        BLOG_ERR("Failed to get rdb_fd\n");
        ret = -1;
        goto fin;
    }

    ret = init();
    if (ret < 0) {
        BLOG_ERR("Failed to init\n");
        goto fin;
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR ||
        signal(SIGQUIT, sig_handler) == SIG_ERR ||
        signal(SIGTERM, sig_handler) == SIG_ERR) {
        ret = -errno;
        BLOG_ERR("Failed to register signal handler\n");
        goto fin;
    }

    ret = main_loop();
    if (ret < 0) {
        BLOG_ERR("main loop terminated unexpectedly\n");
    } else {
        BLOG_NOTICE("main loop exited normally\n");
    }

fin:
    rdb_close(&rdb_s);
    closelog();

    return ret;
}
