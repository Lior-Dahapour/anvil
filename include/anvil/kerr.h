#ifndef ANVIL_KERR_H
#define ANVIL_KERR_H

/* KERR_OK or a negative error code. */
typedef enum {
    KERR_OK = 0,
    /* Malformed input, e.g. a corrupt device tree. */
    KERR_INVALID = -1,
    /* Valid input without the requested item. */
    KERR_NOT_FOUND = -2,
} kerr_t;

/* For functions returning kerr_t: an ignored result fails the build (-Werror). */
#define KMUST_CHECK __attribute__((warn_unused_result))

#endif
