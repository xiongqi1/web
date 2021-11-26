#ifndef __CALL_PREFIX_H_20181123__
#define __CALL_PREFIX_H_20181123__
/*
 * Functions for converting the international call prefix for incoming and
 * outgoing calls.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
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

/**
 * @brief replaces international call prefix with abbreviated plus(+) as required.
 *
 * @param num is a phone number.
 *
 * @return processed phone number.
 */
const char* call_prefix_get_outgoing_num_for_network(const char* num);

/**
 * @brief obtains caller id from CPE after processing plus(+) sign.
 *
 * @param cid_num is a phone number.
 *
 * @return processed phone number.
 */
const char* call_prefix_get_incoming_cid_for_rj11(const char* cid_num);

#endif
