#ifndef BUFFER_H
#define BUFFER_H
/**
 * String Buffer.
 */

#include <stdlib.h>
#include <string.h>

struct buffer {
    char *pos;      /* start position of string */
    char *last;     /* end position of string */
    char *start;    /* start of buffer */
    char *end;      /* end of buffer */
};

#define buffer_size(buf)      (buf->last - buf->pos)
#define buffer_available(buf) (buf->end - buf->last)
#define buffer_capacity(buf)