#include <dc/pvr.h>
#include <dc/sq.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define DR_MAX_VERTICES     65536
#define DR_MAX_HEADERS      1024

typedef struct {
    pvr_vertex_t    *vertices;
    pvr_poly_hdr_t  *headers;
    uint32_t        vertex_count;
    uint32_t        header_count;
    pvr_dr_state_t  dr_state;
    int             initialized;
} dr_state_t;

static dr_state_t dr = {0};

int __go_dr_init(void)
{
    if (dr.initialized)
        return 0;

    dr.vertices = (pvr_vertex_t *)memalign(32, DR_MAX_VERTICES * sizeof(pvr_vertex_t));
    if (!dr.vertices)
        return -1;

    dr.headers = (pvr_poly_hdr_t *)memalign(32, DR_MAX_HEADERS * sizeof(pvr_poly_hdr_t));
    if (!dr.headers) {
        free(dr.vertices);
        dr.vertices = NULL;
        return -1;
    }

    memset(dr.vertices, 0, DR_MAX_VERTICES * sizeof(pvr_vertex_t));
    memset(dr.headers, 0, DR_MAX_HEADERS * sizeof(pvr_poly_hdr_t));

    dr.initialized = 1;
    return 0;
}

void __go_dr_shutdown(void)
{
    if (dr.vertices) {
        free(dr.vertices);
        dr.vertices = NULL;
    }
    if (dr.headers) {
        free(dr.headers);
        dr.headers = NULL;
    }
    dr.initialized = 0;
}

void __go_dr_begin_frame(void)
{
    dr.vertex_count = 0;
    dr.header_count = 0;
    pvr_dr_init(&dr.dr_state);
}

void __go_dr_end_frame(void)
{
    pvr_dr_finish();
}

void *__go_dr_get_vertex(void)
{
    if (dr.vertex_count >= DR_MAX_VERTICES)
        return NULL;
    return &dr.vertices[dr.vertex_count++];
}

void *__go_dr_get_vertex_at(int index)
{
    if (index < 0 || index >= DR_MAX_VERTICES)
        return NULL;
    return &dr.vertices[index];
}

int __go_dr_get_vertex_count(void)
{
    return dr.vertex_count;
}

void __go_dr_set_vertex_count(int count)
{
    if (count >= 0 && count <= DR_MAX_VERTICES)
        dr.vertex_count = count;
}

void *__go_dr_get_header(void)
{
    if (dr.header_count >= DR_MAX_HEADERS)
        return NULL;
    return &dr.headers[dr.header_count++];
}

static inline void dr_submit_32(const void *data)
{
    pvr_vertex_t *dest = pvr_dr_target(dr.dr_state);
    memcpy(dest, data, 32);
    pvr_dr_commit(dest);
}

void __go_dr_submit_header(const void *hdr)
{
    dr_submit_32(hdr);
}

void __go_dr_submit_vertex(const void *vtx)
{
    dr_submit_32(vtx);
}

void __go_dr_submit_vertex_xyzc(uint32_t flags, float x, float y, float z, uint32_t argb)
{
    pvr_vertex_t *dest = pvr_dr_target(dr.dr_state);
    dest->flags = flags;
    dest->x = x;
    dest->y = y;
    dest->z = z;
    dest->u = 0.0f;
    dest->v = 0.0f;
    dest->argb = argb;
    dest->oargb = 0;
    pvr_dr_commit(dest);
}

void __go_dr_submit_vertex_full(uint32_t flags, float x, float y, float z,
                                 float u, float v, uint32_t argb, uint32_t oargb)
{
    pvr_vertex_t *dest = pvr_dr_target(dr.dr_state);
    dest->flags = flags;
    dest->x = x;
    dest->y = y;
    dest->z = z;
    dest->u = u;
    dest->v = v;
    dest->argb = argb;
    dest->oargb = oargb;
    pvr_dr_commit(dest);
}

void __go_dr_submit_vertices(int start, int end)
{
    if (start < 0 || end > (int)dr.vertex_count || start >= end)
        return;

    for (int i = start; i < end; i++) {
        dr_submit_32(&dr.vertices[i]);
    }
}

void __go_dr_submit_all_vertices(void)
{
    __go_dr_submit_vertices(0, dr.vertex_count);
}

void __go_dr_submit_strip(const void *hdr, const void *vertices, int vertex_count)
{
    const pvr_vertex_t *verts = (const pvr_vertex_t *)vertices;
    dr_submit_32(hdr);
    for (int i = 0; i < vertex_count; i++) {
        dr_submit_32(&verts[i]);
    }
}

int __go_dr_check_alignment(void)
{
    uintptr_t v = (uintptr_t)dr.vertices;
    uintptr_t h = (uintptr_t)dr.headers;
    if ((v & 31) != 0) return -1;
    if ((h & 31) != 0) return -2;
    return 0;
}

void *__go_dr_get_vertex_buffer(void)
{
    return dr.vertices;
}

void *__go_dr_get_header_buffer(void)
{
    return dr.headers;
}
