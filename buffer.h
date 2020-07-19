#ifndef BUFFER_H
#define BUFFER_H
/**
 * String Buffer.
 */

#include <stdlib.h>
#include <string.h>

struct buffer {
    char *pos;      /* start position of string */