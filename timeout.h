#ifndef TIMEOUT_H
#define TIMEOUT_H

struct timeout {
    /* Invariants:
     * tm_timeout <= 0, means no timeout, then tm_deadline is set as -1
    