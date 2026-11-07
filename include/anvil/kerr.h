#ifndef ANVIL_KERR_H
#define ANVIL_KERR_H

/* Result of a fallible kernel function: KERR_OK, or a negative error code. */
typedef enum {
    KERR_OK = 0,
    /* The input is malformed (for example a corrupt device tree). */
    KERR_INVALID = -1,
    /* The input is valid, but does not contain what was asked for. */
    KERR_NOT_FOUND = -2,
} kerr_t;

/* Put on every function returning kerr_t: ignoring the result becomes a compile error. */
#define KMUST_CHECK __attribute__((warn_unused_result))

#endif
