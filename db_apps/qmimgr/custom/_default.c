/*
 * This is a list of custom band information.
 * Each entry consists of hex and name.
 * It provides a means of limiting the available bands and
 * customising the band names.
 *
 * Developers must make sure that name is unique.
 * It is OK to have entries that are beyond module's band capabilities.
 * Only the overlap will be used in the system.
 *
 * This is the default list (full list).
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

const struct custom_band_t custom_band_qmi[] = {
    {  40, "GSM 450"},
    {  41, "GSM 480"},
    {  42, "GSM 750"},
    {  43, "GSM 850"},
    {  44, "GSM 900 (Extended)"},
    {  45, "GSM 900 (Primary)"},
    {  46, "GSM 900 (Railways)"},
    {  47, "GSM 1800"},
    {  48, "GSM 1900"},
    {  80, "WCDMA 2100"},
    {  81, "WCDMA PCS 1900"},
    {  82, "WCDMA DCS 1800"},
    {  83, "WCDMA 1700 (US)"},
    {  84, "WCDMA 850"},
    {  85, "WCDMA 800"},
    {  86, "WCDMA 2600"},
    {  87, "WCDMA 900"},
    {  88, "WCDMA 1700 (Japan)"},
    {  90, "WCDMA 1500"},
    {  91, "WCDMA 850 (Japan)"},

    { 120, "LTE Band 1 - 2100MHz"},
    { 121, "LTE Band 2 - 1900MHz"},
    { 122, "LTE Band 3 - 1800MHz"},
    { 123, "LTE Band 4 - 1700MHz"},
    { 124, "LTE Band 5 - 850MHz"},
    { 125, "LTE Band 6 - 850MHz"},
    { 126, "LTE Band 7 - 2600MHz"},
    { 127, "LTE Band 8 - 900MHz"},
    { 128, "LTE Band 9 - 1800MHz"},
    { 129, "LTE Band 10 - 1700MHz"},
    { 130, "LTE Band 11 - 1500MHz"},
    { 131, "LTE Band 12 - 700MHz"},
    { 132, "LTE Band 13 - 700MHz"},
    { 133, "LTE Band 14 - 700MHz"},
    { 134, "LTE Band 17 - 700MHz"},
    { 143, "LTE Band 18 - 850MHz"},
    { 144, "LTE Band 19 - 850MHz"},
    { 145, "LTE Band 20 - 800MHz"},
    { 146, "LTE Band 21 - 1500MHz"},
    { 152, "LTE Band 23 - 2000MHz"},
    { 147, "LTE Band 24 - 1600MHz"},
    { 148, "LTE Band 25 - 1900MHz"},
    { 153, "LTE Band 26 - 850MHz"},
    { 158, "LTE Band 28 - 700MHz"},
    { 159, "LTE Band 29 - 700MHz"},
    { 154, "LTE Band 32 - 1500MHz"},
    { 135, "LTE Band 33 - TDD 2100"},
    { 136, "LTE Band 34 - TDD 2100"},
    { 137, "LTE Band 35 - TDD 1900"},
    { 138, "LTE Band 36 - TDD 1900"},
    { 139, "LTE Band 37 - TDD 1900"},
    { 140, "LTE Band 38 - TDD 2600"},
    { 141, "LTE Band 39 - TDD 1900"},
    { 142, "LTE Band 40 - TDD 2300"},
    { 149, "LTE Band 41 - TDD 2500"},
    { 150, "LTE Band 42 - TDD 3500"},
    { 151, "LTE Band 43 - TDD 3700"},
    { 155, "LTE Band 125"},
    { 156, "LTE Band 126"},
    { 157, "LTE Band 127"},

    { 249, "GSM all"},
    { 250, "WCDMA all"},
    { 251, "LTE all"},
    { 252, "GSM/WCDMA all"},
    { 253, "WCDMA/LTE all"},
    { 254, "GSM/LTE all"},
    { 255, "All bands"}

};

