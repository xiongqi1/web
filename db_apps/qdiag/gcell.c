/**
 * Implementation of ECGI (Global cell id) linked list
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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

#include "gcell.h"

#include "def.h"
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef offsetof
#undef offsetof
#endif

#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#define __countof(x) (sizeof(x) / sizeof(x[0]))

/* total available global cells */
static LIST_HEAD(gcell_collection);
static int count = 0;

/*
 Get next element of global cell information

 Params:
  gc : This function gets the next element of this element of global cell information

 Return:
  Next element of global cell information. Otherwise, NULL
*/
struct gcell_t *gcell_get_first_or_next(struct gcell_t *gc)
{
    struct list_head *e;

    /* get next element */
    if (gc)
        e = gc->list.next;
    else
        e = gcell_collection.next;

    /* bypass if we are at the end */
    return e == &gcell_collection ? NULL : container_of(e, struct gcell_t, list);
}

/*
 Create an element of global cell information

 Params:
  None

 Return:
  Newly created element of global cell information
*/
struct gcell_t *gcell_new()
{
    struct gcell_t *gc;

    /* allocate gc */
    gc = calloc(1, sizeof(*gc));
    if (!gc) {
        syslog(LOG_ERR, "failed to allocate global cell information");
        goto err;
    }

    return gc;

err:
    return NULL;
}

/*
 Relocate an element to the tail

 Params:
  gc : This function gets the next element of this element of global cell information

 Return:
  None
*/
void gcell_move_to_tail(struct gcell_t *gc)
{
    if (!gc)
        return;

    /* remove from list */
    list_del(&gc->list);
    /* add to the tail */
    list_add_tail(&gc->list, &gcell_collection);
}

/*
 Get total allocated global cell element count.

 Params:
  None

 Return:
  Total number of allocated global cell element.
*/
int gcell_get_count()
{
    return count;
}

/*
 Free an element of global cell information

 Params:
  gc : This function gets the next element of this element of global cell information

 Return:
  None
*/
void gcell_del(struct gcell_t *gc)
{
    if (!gc)
        return;

    list_del(&gc->list);
    free(gc);

    count--;
}

/*
 Compare 2 of global cell elements.

 Params:
  a : global cell element.
  b : global cell element.

 Return:
   Positive if a>b, negative if a<b. Otherwise, 0.

 Note:
   Compare order : PID, freq, MCC and MNC - global cell id is not used.
*/

int gcell_compare(struct gcell_t *a, struct gcell_t *b)
{
    int res;

    /* compare PID */
    res = a->phys_cell_id - b->phys_cell_id;
    if (!res) {
        /* compare frequency */
        res = a->freq - b->freq;
        if (!res) {
            /* compare MCC */
            res = strcmp(a->mcc, b->mcc);
            if (!res) {
                res = strcmp(a->mnc, b->mnc);
            }
        }
    }

    return res;
}

/*
 Find a global cell element

 Params:
  gc : global cell element

 Return:
   global cell ID. Otherwise, NULL
*/
struct gcell_t *gcell_find(struct gcell_t *gc)
{
    struct gcell_t *egc;

    /* search the element in the list */
    egc = gcell_get_first_or_next(NULL);
    while (egc) {
        /* break if found */
        if (!gcell_compare(egc, gc))
            break;

        egc = gcell_get_first_or_next(egc);
    }

    return egc;
}

/*
 Add an element of global cell information into the collection of global cell information

 Params:
  gc : global cell elmenet

 Return:
  None
*/
void gcell_add(struct gcell_t *gc)
{
    list_add_tail(&gc->list, &gcell_collection);
    count++;
}

/*
 Remove all elements in the collection of global cell information

 Params:
  None

 Return:
  None
*/
void gcell_remove_all()
{
    struct gcell_t *gc;

    while (!list_empty(&gcell_collection)) {
        gc = container_of(gcell_collection.next, struct gcell_t, list);
        gcell_del(gc);
    }
}

#ifdef TEST_MODE_MODULE
int unit_test_gcell()
{
    struct gcell_t *gc;
    const struct gcell_t *sgc;
    int i;

    struct gcell_t example_gcs[] = {
        { .mcc = "001", .mnc = "01", .phys_cell_id = 1, .cell_id = 0xf0ffff, .freq = 900 },
        { .mcc = "003", .mnc = "02", .phys_cell_id = 2, .cell_id = 0xfff0ff, .freq = 910 },
        { .mcc = "002", .mnc = "32", .phys_cell_id = 3, .cell_id = 0xff0fff, .freq = 920 },
        { .mcc = "001", .mnc = "302", .phys_cell_id = 4, .cell_id = 0xff0fff, .freq = 980 },
    };

    /* allocate and add gc into list */
    for (i = 0; i < __countof(example_gcs); i++) {
        sgc = &example_gcs[i];

        /* clone gc */
        gc = gcell_new();
        memcpy(gc, sgc, sizeof(*gc));

        /* add to tail */
        gcell_add(gc);
    }
    INFO("[unit-test-gcell] gcell_add() OK");

    /* loop for each of global cell elements */
    i = 0;
    gc = gcell_get_first_or_next(NULL);
    while (gc) {
        sgc = &example_gcs[i++];

        DEBUG("[unit-test-gcell] gc  GC=%d, mcc=%s,mnc=%s,freq=%d,pci=%d,cgi=%d", i, gc->mcc, gc->mnc, gc->freq, gc->phys_cell_id, gc->cell_id);
        DEBUG("[unit-test-gcell] sgc GC=%d, mcc=%s,mnc=%s,freq=%d,pci=%d,cgi=%d", i, sgc->mcc, sgc->mnc, sgc->freq, sgc->phys_cell_id, sgc->cell_id);

        if (strcmp(gc->mcc, sgc->mcc) || strcmp(gc->mnc, sgc->mnc) || gc->freq != sgc->freq || gc->phys_cell_id != sgc->phys_cell_id ||
            gc->cell_id != sgc->cell_id) {
            ERR("[unit-test-gcell] malfunction detected in gcell_add() or gcell_get_first_or_next()");
            goto err;
        }

        gc = gcell_get_first_or_next(gc);
    }
    INFO("[unit-test-gcell] gcell_get_first_or_next() OK");

    /* delete first entry */
    gc = gcell_get_first_or_next(NULL);
    gcell_del(gc);

    /* loop for each of global cell elements */
    i = 1;
    gc = gcell_get_first_or_next(NULL);
    while (gc) {
        sgc = &example_gcs[i++];

        DEBUG("[unit-test-gcell] gc  GC=%d, mcc=%s,mnc=%s,freq=%d,pci=%d,cgi=%d", i, gc->mcc, gc->mnc, gc->freq, gc->phys_cell_id, gc->cell_id);
        DEBUG("[unit-test-gcell] sgc GC=%d, mcc=%s,mnc=%s,freq=%d,pci=%d,cgi=%d", i, sgc->mcc, sgc->mnc, sgc->freq, sgc->phys_cell_id, sgc->cell_id);

        if (strcmp(gc->mcc, sgc->mcc) || strcmp(gc->mnc, sgc->mnc) || gc->freq != sgc->freq || gc->phys_cell_id != sgc->phys_cell_id ||
            gc->cell_id != sgc->cell_id) {
            ERR("[unit-test-gcell] malfunction detected in gcell_add() or gcell_get_first_or_next()");
            goto err;
        }

        gc = gcell_get_first_or_next(gc);
    }
    INFO("[unit-test-gcell] gcell_del() OK");

    /* remove all */
    gcell_remove_all();

    /* get first */
    gc = gcell_get_first_or_next(NULL);
    if (gc) {
        ERR("[unit-test-gcell] malfunction detected in gcell_remove_all()");
        goto err;
    }

    INFO("[unit-test-gcell] gcell_remove_all() OK");

    return 0;

err:
    return -1;
}

#endif
