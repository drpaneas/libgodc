/* libgodc/runtime/chan.h - Go channels */
#ifndef GODC_CHAN_H
#define GODC_CHAN_H

#include "goroutine.h"

struct __go_type_descriptor;
struct sudog;

typedef struct waitq
{
    struct sudog *first;
    struct sudog *last;
} waitq;

typedef struct hchan
{
    uint32_t qcount;
    uint32_t dataqsiz;
    void *buf;
    uint16_t elemsize;
    uint8_t closed;
    uint8_t buf_mask_valid;

    struct __go_type_descriptor *elemtype;

    uint32_t sendx;
    uint32_t recvx;

    waitq recvq;
    waitq sendq;

    uint8_t locked;
} hchan;

typedef struct scase
{
    hchan *c;
    void *elem;
} scase;

static inline uint32_t chan_index(hchan *c, uint32_t i)
{
    if (c->buf_mask_valid)
        return i & (c->dataqsiz - 1);
    return i % c->dataqsiz;
}

static inline void *chanbuf(hchan *c, uint32_t i)
{
    uint32_t index = chan_index(c, i);
    return (void *)((uintptr_t)c->buf + (uintptr_t)index * c->elemsize);
}

void chan_lock(hchan *c);
void chan_unlock(hchan *c);

void waitq_enqueue(waitq *q, struct sudog *s);
struct sudog *waitq_dequeue(waitq *q);
void waitq_remove(waitq *q, struct sudog *s);
bool waitq_empty(waitq *q);

void sudog_pool_init(void);
struct sudog *acquireSudog(void);
void releaseSudog(struct sudog *s);

#endif
