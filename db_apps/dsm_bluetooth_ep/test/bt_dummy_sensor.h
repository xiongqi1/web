#ifndef BT_DUMMY_SENSOR_H_11450923022016
#define BT_DUMMY_SENSOR_H_11450923022016
/*
 * Bluetooth Dummy Sensor that implements Serial Port Profile (SPP).
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
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>

#define SPP_UUID "spp"

/* DBus bus, paths and interfaces. */
#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_PROFILEMGR_PATH "/org/bluez"
#define BLUEZ_PROFILEMGR_INTF "org.bluez.ProfileManager1"
#define BT_DUMMY_SENSOR_PATH "/com/netcommwireless/BtDummySensor"
#define BLUEZ_PROFILE_INTF "org.bluez.Profile1"
#define BLUEZ_DEVICE_INTF "org.bluez.Device1"
#define DBUS_PROPERTIES_INTF "org.freedesktop.DBus.Properties"

#define bail(ctx, status, msg_fmt, ...) do {        \
        ctx->exit_status = status;                  \
        g_main_loop_quit(ctx->loop);                \
        errp(msg_fmt, ##__VA_ARGS__);               \
        return;                                     \
    } while (0)

#define IO_BUF_MAX_LEN 128
#define DBUS_PATH_MAX_LEN 64
#define BT_MAC_ADDR_BUF_LEN 18

typedef struct sensor_context_ {
    GDBusConnection *connection;
    GMainLoop *loop;
    GDBusNodeInfo *profile_bus_node_info;
    char dev_dbus_path[DBUS_PATH_MAX_LEN];
    GSource *peer_source;
    guint bus_watch_id;
    int exit_status;
    int fd;
} sensor_context_t;

/* Introspection data for the bluez Profile service we are exporting */
static const gchar profile_introspection_xml_g[] =
  "<node>"
  "  <interface name='org.bluez.Profile1'>"
  "    <method name='Release'>"
  "    </method>"
  "    <method name='NewConnection'>"
  "      <arg type='o' name='device' direction='in'/>"
  "      <arg type='h' name='fd' direction='in'/>"
  "      <arg type='a{sv}' name='fd_properties' direction='in'/>"
  "    </method>"
  "    <method name='RequestDisconnection'>"
  "      <arg type='o' name='device' direction='in'/>"
  "    </method>"
  "    <method name='Cancel'>"
  "    </method>"
  "  </interface>"
  "</node>";

#endif /* BT_DUMMY_SENSOR_H_11450923022016 */
