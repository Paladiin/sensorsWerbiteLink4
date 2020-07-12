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