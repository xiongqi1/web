/*
 * scrm_user_menu.c
 *    Screen UI user menu support. Allows an SDK user to insert menu
 *    items into the screen UI. User menu items are always inserted as
 *    entries in the top level menu. To add a user level menu, a
 *    shared library needs to be built via the SDK and installed into
 *    the screen manager system library path, USER_MENU_DL_SYS_DIR.
 *    All shared libraries in that directory are dynamically loaded.
 *    Each such shared library should contain at least a constructor to
 *    create the user menu and a destructor to clean up.
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Pty. Ltd.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or
 * object forms) without the expressed written consent of NetComm Wireless Pty.
 * Ltd Copyright laws and International Treaties protect the contents of this
 * file. Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * NETCOMM WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

#include <scrm.h>
#include <scrm_ops.h>
#include <scrm_ops_priv.h>
#include <scrm_features.h>

/* Max number of supported user menu dynamic libraries. */
#define NUM_USER_MENU_DL_MAX 10

#define USER_MENU_DL_SYS_DIR "/usr/lib/screen_manager"
#define USER_MENU_DL_EXT ".so"

static void *dlhandle[NUM_USER_MENU_DL_MAX];

/*
 * Clean up.
 */
static void
user_menu_destroy (void)
{
    unsigned int ix;

    for (ix = 0; (ix < NUM_USER_MENU_DL_MAX) && dlhandle[ix]; ix++) {
        dlclose(dlhandle[ix]);
        dlhandle[ix] = NULL;
    }
}

/*
 * Initialise user menu support. Note that this function always returns
 * a success. On error, a message is logged but the return is still a success
 * value. This is because an error return will abort the screen manager.
 * That is undesirable for this feature as its success/failure is dependant
 * on the user - the screen manager should still come up regardless of
 * any user code or installation errors.
 */
static int
user_menu_init (void)
{
    unsigned int num_dl;
    DIR *dir;
    struct dirent *dirent;
    struct stat file_stat;
    const char *char_p;
    char full_path[_POSIX_PATH_MAX + strlen(USER_MENU_DL_SYS_DIR) + 1];

    /* Open screen manager system directory. */
    dir = opendir(USER_MENU_DL_SYS_DIR);
    if (!dir) {
        errp("Unable to open directory %s", USER_MENU_DL_SYS_DIR);
        return 0;
    }

    /*
     * Load each dynamic library (.so) in the screen manager system directory.
     */
    num_dl = 0;
    while ((dirent = readdir(dir))) {

        if (num_dl >= NUM_USER_MENU_DL_MAX) {
            errp("Maximum (%d) user menu libraries exceeded",
                 NUM_USER_MENU_DL_MAX);
            break;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", USER_MENU_DL_SYS_DIR,
                 dirent->d_name);
        stat(full_path, &file_stat);

        /* Skip directories and other non-regular files. */
        if (S_ISREG(file_stat.st_mode)) {

            /*
             * Only load files where the file name ends in the expected
             * dynamic library extension.
             */
            char_p = strstr(dirent->d_name, USER_MENU_DL_EXT);
            if (char_p && ((char_p + strlen(USER_MENU_DL_EXT)) ==
                           (dirent->d_name + strlen(dirent->d_name)))) {

                dbgp("dyn load %d: %s\n", num_dl, full_path);

                dlhandle[num_dl] = dlopen(full_path, RTLD_NOW);
                if (!dlhandle[num_dl]) {
                    errp("dlopen of %s failed: %s", full_path, dlerror());
                    continue;
                }
                num_dl++;
            }
        }
    }

    closedir(dir);
    return 0;
}

scrm_feature_plugin_t scrm_user_menu_plugin = {
    user_menu_init,
    user_menu_destroy,
};
