#include "buffer.h"

/**
 * Create a buffer of given size.
 */
struct buffer *
buffer_create(size_t size)
{
    struct buffer *buf = malloc(sizeof(*buf));
    if (!buf)
        return NULL;

    buf->start = malloc(size);
    if (!buf->start) {
        free(buf);
        return NULL;
    }

    buf->pos = buf->start;
    buf->last = buf->start;
    buf->end = buf->start + size;

    return buf;
}

/**
 * Shrink the buffer.
 *
 * Move string to starting poin