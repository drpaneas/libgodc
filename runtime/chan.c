/* libgodc/runtime/chan.c - Go channels */

#include "goroutine.h"
#include "chan.h"
#include "gc_semispace.h"
#include "godc_config.h"
#include "type_descriptors.h"
#include "panic_dreamcast.h"
#include "runtime.h"
#include "copy.h"
#include <string.h>

bool chansend(hchan *c, void *elem, bool block);
bool chanrecv(hchan *c, void *elem, bool block);

#define chan_copy(c, dst, src) fast_copy((dst), (src), (c)->elemsize)

void chan_lock(hchan *c)
{
    if (!c)
        runtime_throw("chan: nil channel");
    if (c->locked)
        runtime_throw("chan: recursive lock");
    c->locked = 1;
}

void chan_unlock(hchan *c)
{
    if (c)
        c->locked = 0;
}

static bool chanparkcommit(void *lock)
{
    chan_unlock((hchan *)lock);
    return true;
}

DEFINE_GO_TYPE_DESC(__hchan_type, hchan, GO_STRUCT, sizeof(hchan), NULL);

hchan *makechan(struct __go_type_descriptor *chantype, int64_t size)
{
    struct __go_chan_type *ct;
    struct __go_type_descriptor *elemtype;
    size_t elemsize, hchanSize, bufSize, totalSize;
    hchan *c;

    if (size < 0)
        runtime_throw("makechan: size < 0");
    if (size > 65536)
        runtime_throw("makechan: size too large");

    ct = (struct __go_chan_type *)chantype;
    elemtype = ct ? ct->__element_type : NULL;
    elemsize = elemtype ? elemtype->__size : 0;
    if (elemsize == 0)
        elemsize = 1;

    if (elemsize > 65536)
        runtime_throw("makechan: elem too large");

    hchanSize = sizeof(hchan);
    bufSize = (size_t)size * elemsize;
    totalSize = hchanSize + bufSize;

    c = (hchan *)gc_alloc(totalSize, &__hchan_type);
    if (!c)
        runtime_throw("makechan: out of memory");

    c->locked = 0;
    c->elemsize = (uint16_t)elemsize;
    c->elemtype = elemtype;
    c->dataqsiz = (uint32_t)size;
    c->buf_mask_valid = (size > 0 && (size & (size - 1)) == 0) ? 1 : 0;

    if (size > 0)
        c->buf = (void *)((uintptr_t)c + hchanSize);

    return c;
}

hchan *runtime_makechan(struct __go_type_descriptor *elemtype, int size) __asm__("_runtime.makechan");
hchan *runtime_makechan(struct __go_type_descriptor *elemtype, int size)
{
    return makechan(elemtype, (int64_t)size);
}

hchan *runtime_makechan64(struct __go_type_descriptor *elemtype, int64_t size) __asm__("_runtime.makechan64");
hchan *runtime_makechan64(struct __go_type_descriptor *elemtype, int64_t size)
{
    return makechan(elemtype, size);
}

void chansend1(hchan *c, void *elem)
{
    chansend(c, elem, true);
}

bool chansend(hchan *c, void *elem, bool block)
{
    sudog *sg;
    G *gp;
    sudog *mysg;
    bool success;

    if (!c) {
        if (!block)
            return false;
        gopark(NULL, NULL, waitReasonChanSend);
        runtime_throw("unreachable");
    }

    chan_lock(c);

    if (c->closed) {
        chan_unlock(c);
        runtime_throw("send on closed channel");
    }

    sg = waitq_dequeue(&c->recvq);
    if (sg) {
        gp = sg->g;
        chan_copy(c, sg->elem, elem);
        sg->success = true;
        chan_unlock(c);
        goready(gp);
        return true;
    }

    if (c->qcount < c->dataqsiz) {
        void *dst = chanbuf(c, c->sendx);
        chan_copy(c, dst, elem);
        c->sendx = chan_index(c, c->sendx + 1);
        c->qcount++;
        chan_unlock(c);
        return true;
    }

    if (!block) {
        chan_unlock(c);
        return false;
    }

    gp = getg();
    mysg = acquireSudog();
    if (!mysg)
        runtime_throw("acquireSudog failed");

    mysg->elem = elem;
    mysg->c = c;
    mysg->isSelect = false;

    waitq_enqueue(&c->sendq, mysg);
    gp->waiting = mysg;

    gopark(chanparkcommit, c, waitReasonChanSend);

    gp->waiting = NULL;
    success = mysg->success;
    releaseSudog(mysg);

    if (!success)
        runtime_throw("send on closed channel");

    return true;
}

void runtime_chansend1(hchan *c, void *elem) __asm__("_runtime.chansend1");
void runtime_chansend1(hchan *c, void *elem)
{
    chansend1(c, elem);
}

static bool chanrecv_internal(hchan *c, void *elem, bool block, bool *received);

void chanrecv1(hchan *c, void *elem)
{
    chanrecv(c, elem, true);
}

bool chanrecv2(hchan *c, void *elem)
{
    bool received = false;
    chanrecv_internal(c, elem, true, &received);
    return received;
}

static bool chanrecv_internal(hchan *c, void *elem, bool block, bool *received)
{
    sudog *sg;
    sudog *mysg;
    bool success;
    void *src;
    G *gp;

    if (!c) {
        if (!block)
            return false;
        gopark(NULL, NULL, waitReasonChanReceive);
        runtime_throw("unreachable");
    }

    chan_lock(c);

    if (c->closed && c->qcount == 0) {
        chan_unlock(c);
        if (elem && c->elemsize > 0)
            memset(elem, 0, c->elemsize);
        if (received)
            *received = false;
        return true;
    }

    sg = waitq_dequeue(&c->sendq);
    if (sg) {
        gp = sg->g;

        if (c->dataqsiz == 0) {
            chan_copy(c, elem, sg->elem);
        } else {
            src = chanbuf(c, c->recvx);
            chan_copy(c, elem, src);
            chan_copy(c, src, sg->elem);
            c->recvx = chan_index(c, c->recvx + 1);
            c->sendx = c->recvx;
        }

        sg->success = true;
        chan_unlock(c);
        goready(gp);

        if (received)
            *received = true;
        return true;
    }

    if (c->qcount > 0) {
        src = chanbuf(c, c->recvx);
        chan_copy(c, elem, src);
        if (c->elemsize > 0)
            memset(src, 0, c->elemsize);
        c->recvx = chan_index(c, c->recvx + 1);
        c->qcount--;
        chan_unlock(c);
        if (received)
            *received = true;
        return true;
    }

    if (!block) {
        chan_unlock(c);
        return false;
    }

    gp = getg();
    mysg = acquireSudog();
    if (!mysg)
        runtime_throw("acquireSudog failed");

    mysg->elem = elem;
    mysg->c = c;
    mysg->isSelect = false;

    waitq_enqueue(&c->recvq, mysg);
    gp->waiting = mysg;

    gopark(chanparkcommit, c, waitReasonChanReceive);

    gp->waiting = NULL;
    success = mysg->success;
    releaseSudog(mysg);

    if (received)
        *received = success;
    if (!success && elem && c->elemsize > 0)
        memset(elem, 0, c->elemsize);

    return true;
}

bool chanrecv(hchan *c, void *elem, bool block)
{
    return chanrecv_internal(c, elem, block, NULL);
}

void runtime_chanrecv1(hchan *c, void *elem) __asm__("_runtime.chanrecv1");
void runtime_chanrecv1(hchan *c, void *elem)
{
    chanrecv1(c, elem);
}

bool runtime_chanrecv2(hchan *c, void *elem) __asm__("_runtime.chanrecv2");
bool runtime_chanrecv2(hchan *c, void *elem)
{
    return chanrecv2(c, elem);
}

void closechan(hchan *c)
{
    G *wake_list = NULL;
    G *wake_tail = NULL;
    sudog *sg;
    G *gp;

    chan_lock(c);

    if (c->closed) {
        chan_unlock(c);
        runtime_throw("close of closed channel");
    }

    c->closed = 1;

    while ((sg = waitq_dequeue(&c->recvq)) != NULL) {
        sg->success = false;
        gp = sg->g;
        if (!gp || gp->atomicstatus == Gdead)
            continue;
        if (sg->elem && c->elemsize > 0)
            memset(sg->elem, 0, c->elemsize);
        gp->schedlink = NULL;
        if (wake_tail)
            wake_tail->schedlink = gp;
        else
            wake_list = gp;
        wake_tail = gp;
    }

    while ((sg = waitq_dequeue(&c->sendq)) != NULL) {
        sg->success = false;
        gp = sg->g;
        if (!gp || gp->atomicstatus == Gdead)
            continue;
        gp->schedlink = NULL;
        if (wake_tail)
            wake_tail->schedlink = gp;
        else
            wake_list = gp;
        wake_tail = gp;
    }

    chan_unlock(c);

    while (wake_list) {
        gp = wake_list;
        wake_list = gp->schedlink;
        goready(gp);
    }
}

void runtime_closechan(hchan *c) __asm__("_runtime.closechan");
void runtime_closechan(hchan *c)
{
    closechan(c);
}

typedef struct {
    bool selected;
    bool received;
} SelectNbrecvResult;

bool runtime_selectnbsend(hchan *c, void *elem) __asm__("_runtime.selectnbsend");
bool runtime_selectnbsend(hchan *c, void *elem)
{
    return chansend(c, elem, false);
}

SelectNbrecvResult runtime_selectnbrecv(void *elem, hchan *c) __asm__("_runtime.selectnbrecv");
SelectNbrecvResult runtime_selectnbrecv(void *elem, hchan *c)
{
    SelectNbrecvResult result = {false, false};
    bool received = false;

    if (!c)
        return result;

    if (chanrecv_internal(c, elem, false, &received)) {
        result.selected = true;
        result.received = received;
    }

    return result;
}

int chanlen(hchan *c) { return c ? (int)c->qcount : 0; }
int chancap(hchan *c) { return c ? (int)c->dataqsiz : 0; }

int runtime_chanlen(hchan *c) __asm__("_runtime.chanlen");
int runtime_chanlen(hchan *c) { return chanlen(c); }

int runtime_chancap(hchan *c) __asm__("_runtime.chancap");
int runtime_chancap(hchan *c) { return chancap(c); }
