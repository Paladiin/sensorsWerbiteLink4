#ifndef TIMEOUT_H
#define TIMEOUT_H

struct timeout {
    /* Invariants:
     * tm_timeout <= 0, means no timeout, then tm_deadline is set as -1
     * tm_timeout > 0, then tm_deadline = timeout_gettime() + tm_timeout
     */
    double tm_timeout;          /* timeout time (in seconds) */
    double tm_deadline;         