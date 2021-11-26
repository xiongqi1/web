/*
 * Implementing generating QR code image functions
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

#include "qr_code_image.h"
#include "qrcodegen.h"
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

// border of QR code in QR code modules
#define QR_BORDER 4
// Set bit k in bit array A
#define SET(A, k)    (A[(k/8)] |= (1 << (7-(k%8))))
// Clear bit k in bit array A
#define CLEAR(A, k)    (A[(k/8)] &= ~(1 << (7-(k%8))))

/*
 * Write QR code data to 1-bit black/white PNG image
 * @param qr_code QR code data
 * @param path file path of generated PNG image
 * @param image_size Width in pixels of generated PNG image
 * @return 0 on success, negative code on error
 */
static int write_qr_code_to_png(uint8_t *qr_code, const char *path, int image_size)
{
    FILE * fp;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    int scale, margin, qr_size;
    int x, y, x_qr, y_qr, x_qr_no_border, y_qr_no_border, x_no_margin, y_no_margin;
    png_byte ** row_pointers = NULL;
    int status = -EINVAL;
    int row_bytes_num = image_size/8 + 1;

    fp = fopen (path, "wb");
    if (!fp) {
        goto fopen_failed;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        goto png_create_write_struct_failed;
    }

    info_ptr = png_create_info_struct (png_ptr);
    if (info_ptr == NULL) {
        goto png_create_info_struct_failed;
    }

    /* Set up error handling. */
    if (setjmp (png_jmpbuf (png_ptr))) {
        goto png_failure;
    }

    qr_size = qrcodegen_getSize(qr_code) + 2*QR_BORDER;
    if (image_size > qr_size) {
        scale = image_size/qr_size;
        margin = (image_size%qr_size)/2;
    } else {
        scale = 1;
        margin = 0;
        image_size = qr_size;
    }

    png_set_IHDR(png_ptr,
            info_ptr,
            image_size,
            image_size,
            1, // 1-bit depth
            PNG_COLOR_TYPE_GRAY,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

    // Set rows of PNG image
    row_pointers = png_malloc(png_ptr, image_size*sizeof(png_byte *));
    if (!row_pointers) {
        goto png_failure;
    }
    memset(row_pointers, 0, image_size*sizeof(png_byte *));

    for (y = 0; y < image_size; y++) {
        y_no_margin = y - margin;
        y_qr = y_no_margin/scale;
        y_qr_no_border = y_qr - QR_BORDER;

        png_byte *row = png_malloc (png_ptr, row_bytes_num);
        if (!row) {
            goto png_row_failure;
        }
        // all black initially
        memset(row, 0, row_bytes_num);
        row_pointers[y] = row;

        for (x = 0; x < image_size; x++) {
            x_no_margin = x - margin;
            x_qr = x_no_margin/scale;
            x_qr_no_border = x_qr - QR_BORDER;

            if (qrcodegen_getModule(qr_code, x_qr_no_border, y_qr_no_border)) {
                // set bit for white pixel
                SET(row, x);
            }
        }
    }

    // write to png file
    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    // write with inverting black/white
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_INVERT_MONO, NULL);

    status = 0;

    png_row_failure:
    for (y = 0; y < image_size; y++) {
        if (row_pointers[y]) {
            png_free(png_ptr, row_pointers[y]);
        }
    }

    png_free (png_ptr, row_pointers);
png_failure:
png_create_info_struct_failed:
    png_destroy_write_struct (&png_ptr, &info_ptr);
png_create_write_struct_failed:
    fclose (fp);
fopen_failed:
    return status;
}

/*
 * See qr_code_image.h
 */
int generate_qr_code_image(const char *data, const char *path, int image_size)
{
    int status;
    uint8_t *qr_code = NULL;
    uint8_t *temp_buffer = NULL;

    status = -EINVAL;

    if (!data || !path) {
        return status;
    }

    qr_code = malloc(qrcodegen_BUFFER_LEN_MAX);
    temp_buffer = malloc(qrcodegen_BUFFER_LEN_MAX);

    bool qr_ok = qrcodegen_encodeText(data, temp_buffer, qr_code,
            qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
    if (!qr_ok || (status = write_qr_code_to_png(qr_code, path, image_size))) {
        goto qr_code_fail;
    }

    status = 0;
qr_code_fail:
    free(qr_code);
    free(temp_buffer);

    return status;
}
