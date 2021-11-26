/**
 * @file utils.h
 * @brief Utilities for seriall port manager
 *
 * Copyright Notice:
 * Copyright (C) 2019 Casa Systems.
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

#ifndef UTILS_H_10253120082019
#define UTILS_H_10253120082019

#include <stdio.h>
#include <syslog.h>

#ifdef CLI_DEBUG
/*
 * Get time elapsed since the start of execution
 *
 * @return Time elapsed in milliseconds.
*/
long int time_since_start();

// log to stdout/stderr
#define TIME_TAG() printf("[%06ld] ", time_since_start())
#define TIME_TAG_ERR() fprintf(stderr, "[%06ld] ", time_since_start())

#define BLOG_ERR(...) do {TIME_TAG_ERR(); fprintf(stderr, __VA_ARGS__);}while(0)
#define BLOG_WARNING(...) do {TIME_TAG(); printf( __VA_ARGS__);}while(0)
#define BLOG_NOTICE(...) do {TIME_TAG(); printf( __VA_ARGS__);}while(0)
#define BLOG_INFO(...) do {TIME_TAG(); printf( __VA_ARGS__);}while(0)
#define BLOG_DEBUG(...) do {TIME_TAG(); printf(__VA_ARGS__);}while(0)
#define BLOG_DEBUG_DATA(...) printf(__VA_ARGS__)

#else
// log to syslog
#define BLOG_ERR(...) syslog(LOG_ERR, __VA_ARGS__)
#define BLOG_WARNING(...) syslog(LOG_WARNING, __VA_ARGS__)
#define BLOG_NOTICE(...) syslog(LOG_NOTICE, __VA_ARGS__)
#define BLOG_INFO(...) syslog(LOG_INFO, __VA_ARGS__)
#define BLOG_DEBUG(...) syslog(LOG_DEBUG, __VA_ARGS__)
#define BLOG_DEBUG_DATA(...) /*syslog(LOG_DEBUG, __VA_ARGS__)*/

#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#endif // UTILS_H_10253120082019
