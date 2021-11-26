/*
 * Generate QR code image
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

#include "print_msg.h"
#include "data_model.h"
#include "qr_code_image.h"
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>

#define print_direct_msg(...) fprintf(stderr, __VA_ARGS__)

/*
 * Show usage
 * @param prog_name Program name
 * @param data_model_guide Additional data model guide
 */
static void show_usage (const char *prog_name, const char *data_model_guide)
{
    print_direct_msg("Usage: %s <file-path> <image-size>%s\n", prog_name,
            data_model_guide ? " <data-model-arguments>..." : "");
    print_direct_msg("Generating QR code image in PNG format.\n");
    print_direct_msg("\nCommon arguments:\n");
    print_direct_msg("\tfile-path\t\tFile path to generate QR code image\n");
    print_direct_msg("\timage-size\t\tWidth in pixels of the generated QR code image\n");
    if (data_model_guide) {
        print_direct_msg("\nData model arguments:\n");
        print_direct_msg("%s", data_model_guide);
    }
    print_direct_msg("\n\n");
}

/*
 * main function
 */
int main(int argc, char *argv[])
{
#define COMMON_ARG_NUM 3
    int rval = -EINVAL;

    openlog("qr_code_generator", LOG_PID | LOG_PERROR, LOG_DEBUG);

    data_model_t *data_model = new_data_model();
    if (data_model) {
        if (argc < COMMON_ARG_NUM + data_model->arg_number) {
            show_usage(argv[0], data_model->guide);
            return rval;
        }

        const char *file_path = argv[1];
        char *endptr;
        long int image_size;
        image_size = strtol(argv[2], &endptr, 10);
        if (*endptr != '\0'){
            print_err("Invalid image size: %s\n", argv[2]);
            return rval;
        }

        if (!(rval = data_model->init(argc - COMMON_ARG_NUM, argv + COMMON_ARG_NUM))) {
            const char *data = data_model->get_data();
            if (data) {
                print_debug("Generating QR code for data:\n%s\n", data);
                rval = generate_qr_code_image(data, file_path, image_size);
            } else {
                print_err("Failed to get data.\n");
                rval = -ENODATA;
            }
            data_model->deinit();
        }

    }

    closelog();

    return rval;
}
