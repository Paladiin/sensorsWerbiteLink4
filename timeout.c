
#include "timeout.h"
#include <time.h>
#include <sys/time.h>

/**
 * Returns current time in ms. (since 1970)
 */
double
timeout_gettime(void)
{
    struct timeval v;
    gettimeofday(&v, NULL);
    return v.tv_sec + v.tv_usec / 1.0e6;
}
