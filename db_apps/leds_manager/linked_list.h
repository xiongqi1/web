/*
 * linked_list.h
 * Simple doubly linked list implementation
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
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

#ifndef LINKED_LIST_H_00000010102018
#define LINKED_LIST_H_00000010102018

#include <stddef.h>

/*
 * struct defines a doubly-linked list
 */
struct linked_list {
    struct linked_list *next;
    struct linked_list *prev;
};

/*
 * initialise a list
 * @param list list to be initialised
 */
static inline void linked_list_init(struct linked_list *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * add a new entry to the head of the specified list
 * @param list list to add new entry
 * @param entry new entry to be added
 */
static inline void linked_list_add_head(struct linked_list *list, struct linked_list *entry)
{
    entry->next = list->next;
    entry->prev = list;
    list->next->prev = entry;
    list->next = entry;
}

/*
 * add a new entry at the tail of the specified list
 * @param list list to add new entry
 * @param entry new entry to be added
 */
static inline void linked_list_add_tail(struct linked_list *list, struct linked_list *entry)
{
    linked_list_add_head(list->prev, entry);
}

/*
 * delete a list entry
 * @param entry entry to be deleted
 */
static inline void linked_list_del(struct linked_list *entry)
{
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = NULL;
    entry->prev = NULL;
}

/*
 * test whether the list is empty
 * @param list list to be tested
 *
 * @return 1 if list is empty, 0 otherwise
 */
static inline int linked_list_empty(struct linked_list *list)
{
    return list->next == list;
}

/*
 * container_of - cast a member of a structure out to the containing structure
 * @param ptr the pointer to the member.
 * @param type the type of the container struct this is embedded in.
 * @param member the name of the member within the struct.
 */
#ifndef offsetof
#define offsetof(type, member) ((long) &((type *) 0)->member)
#endif
#ifndef container_of
#define container_of(ptr, type, member)	((type *)( (char *)ptr - offsetof(type, member) ))
#endif

/*
 * get the struct for this entry
 * @param entry the struct linked_list* pointer
 * @param type the type of the struct this is embedded in
 * @param member the name of the linked_list whithin the struct
 */
#define linked_list_entry(entry, type, member) container_of(entry, type, member)

/*
 * get the struct for the first element of a list
 * @param list the list to take element from
 * @param type the type of the struct this is embedded in
 * @param member the name of the linked_list whithin the struct
 */
#define linked_list_first(list, type, member) \
        (linked_list_empty((list)) ? NULL : linked_list_entry((list)->next, type, member))

/*
 * get the struct for the last element of a list
 * @param list the list to take element from
 * @param type the type of the struct this is embedded in
 * @param member the name of the linked_list whithin the struct
 */
#define linked_list_last(list, type, member) \
        (linked_list_empty((list)) ? NULL : linked_list_entry((list)->prev, type, member))

/*
 * iterate over a list
 * @param ptr loop cursor as a pointer of type of the struct this is embedded in
 * @param list the list to iterate
 * @param type the type of the struct this is embedded in
 * @param member the name of the linked_list whithin the struct
 */
#define linked_list_for_each(ptr, list, type, member) \
        for (ptr = linked_list_entry((list)->next, type, member); \
        &ptr->member != (list); \
        ptr = linked_list_entry(ptr->member.next, type, member))

/*
 * iterate over a list safe against removal of list entry
 * @param ptr loop cursor as a pointer of type of the struct this is embedded in
 * @param saved_ptr another pointer of type of the struct this is embedded in to use a temporary storage
 * @param list the list to iterate
 * @param type the type of the struct this is embedded in
 * @param member the name of the linked_list whithin the struct
 */
#define linked_list_for_each_safe(ptr, saved_ptr, list, type, member) \
        for (ptr = linked_list_entry((list)->next, type, member), \
                saved_ptr = linked_list_entry(ptr->member.next, type, member); \
                &ptr->member != (list); \
                ptr = saved_ptr, saved_ptr = linked_list_entry(saved_ptr->member.next, type, member))

#endif
