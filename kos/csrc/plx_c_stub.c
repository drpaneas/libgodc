#include <dc/matrix.h>
#include <dc/pvr.h>
#include <string.h>

extern void pvr_dr_init(pvr_dr_state_t *vtx_buf_ptr);
extern void pvr_dr_finish(void);

void __go_mat_trans_single(float *x, float *y, float *z)
{
    float tx = *x, ty = *y, tz = *z;
    mat_trans_single(tx, ty, tz);
    *x = tx;
    *y = ty;
    *z = tz;
}

void __go_plx_dr_init(pvr_dr_state_t *state)
{
    pvr_dr_init(state);
}

void __go_plx_dr_finish(void)
{
    pvr_dr_finish();
}

#define ALIGNED_POOL_SIZE 4096

static pvr_vertex_t __attribute__((aligned(32))) aligned_vertex_pool[ALIGNED_POOL_SIZE];
static int aligned_pool_index = 0;

void __go_aligned_pool_reset(void)
{
    aligned_pool_index = 0;
}

void *__go_aligned_pool_get(void)
{
    if (aligned_pool_index >= ALIGNED_POOL_SIZE)
        return NULL;
    return &aligned_vertex_pool[aligned_pool_index++];
}

int __go_aligned_pool_index(void)
{
    return aligned_pool_index;
}

void *__go_aligned_pool_base(void)
{
    return aligned_vertex_pool;
}

int __go_pvr_prim_vertex(const void *data)
{
    if (((uintptr_t)data & 7) == 0)
        return pvr_prim((void *)data, sizeof(pvr_vertex_t));

    pvr_vertex_t __attribute__((aligned(8))) buf;
    memcpy(&buf, data, sizeof(buf));
    return pvr_prim(&buf, sizeof(buf));
}

int __go_pvr_prim_vertex_fast(const void *data)
{
    return pvr_prim((void *)data, sizeof(pvr_vertex_t));
}

int __go_pvr_prim_hdr(const void *data)
{
    if (((uintptr_t)data & 7) == 0)
        return pvr_prim((void *)data, sizeof(pvr_poly_hdr_t));

    pvr_poly_hdr_t __attribute__((aligned(8))) buf;
    memcpy(&buf, data, sizeof(buf));
    return pvr_prim(&buf, sizeof(buf));
}

void __go_sq_flush(void *sq_addr)
{
    __asm__ volatile("pref @%0" : : "r"(sq_addr) : "memory");
}

void __go_pvr_submit_batch_sq(void *dest, const void *vertices, int count)
{
    const uint32_t *src = (const uint32_t *)vertices;
    uint32_t *sq = (uint32_t *)((uintptr_t)dest | 0xE0000000);

    while (count >= 4) {
        __asm__ volatile("pref @%0" : : "r"(src + 32));

        sq[0] = src[0]; sq[1] = src[1]; sq[2] = src[2]; sq[3] = src[3];
        sq[4] = src[4]; sq[5] = src[5]; sq[6] = src[6]; sq[7] = src[7];
        __asm__ volatile("pref @%0" : : "r"(sq));

        sq += 8; src += 8;
        sq[0] = src[0]; sq[1] = src[1]; sq[2] = src[2]; sq[3] = src[3];
        sq[4] = src[4]; sq[5] = src[5]; sq[6] = src[6]; sq[7] = src[7];
        __asm__ volatile("pref @%0" : : "r"(sq));

        sq += 8; src += 8;
        sq[0] = src[0]; sq[1] = src[1]; sq[2] = src[2]; sq[3] = src[3];
        sq[4] = src[4]; sq[5] = src[5]; sq[6] = src[6]; sq[7] = src[7];
        __asm__ volatile("pref @%0" : : "r"(sq));

        sq += 8; src += 8;
        sq[0] = src[0]; sq[1] = src[1]; sq[2] = src[2]; sq[3] = src[3];
        sq[4] = src[4]; sq[5] = src[5]; sq[6] = src[6]; sq[7] = src[7];
        __asm__ volatile("pref @%0" : : "r"(sq));

        sq += 8; src += 8;
        count -= 4;
    }

    while (count > 0) {
        sq[0] = src[0]; sq[1] = src[1]; sq[2] = src[2]; sq[3] = src[3];
        sq[4] = src[4]; sq[5] = src[5]; sq[6] = src[6]; sq[7] = src[7];
        __asm__ volatile("pref @%0" : : "r"(sq));
        sq += 8; src += 8;
        count--;
    }
}
