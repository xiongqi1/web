#ifndef RBTREEHASH_DLIST_H_10000017042019
#define RBTREEHASH_DLIST_H_10000017042019
/*
 * RBT(Red-Black Tree) for RDB bridge daemon
 *
 * Original codes copied from db_apps/timedaemon
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

struct strbt_t;

void* strbt_find(struct strbt_t* srl, const char* str);
int strbt_del(struct strbt_t* srl, const char* str);
int strbt_add(struct strbt_t* srl, const char* str, void* cont);
void strbt_destroy(struct strbt_t* srl);
struct strbt_t* strbt_create(int cont_len);

void* strbt_get_first(struct strbt_t* srl, const char** str);
void* strbt_get_next(struct strbt_t* srl, const char** str);

#endif
