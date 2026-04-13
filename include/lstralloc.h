/* ------------------------------------------------------------------ */
/*  lstralloc.h - Pluggable allocator interface for lstring370        */
/*                                                                    */
/*  Consumers inject their own allocator by populating an             */
/*  lstr_alloc struct and passing it to every lstring operation that  */
/*  may grow or release a buffer. The library never stores the        */
/*  allocator pointer in static state; reentrancy depends on the      */
/*  allocator travelling with each call.                              */
/*                                                                    */
/*  Default allocator (malloc/free) is provided for tests and for     */
/*  consumers that don't need anything special.                       */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#ifndef LSTRALLOC_H
#define LSTRALLOC_H

#include <stddef.h>

/* Allocator interface. Both function pointers must be non-NULL. */
struct lstr_alloc {
    void *(*alloc)(size_t size, void *ctx);
    void  (*dealloc)(void *ptr, size_t size, void *ctx);
    void  *ctx;            /* opaque - passed through unchanged   */
};

/* Default allocator backed by malloc()/free() from <stdlib.h>.
 * Returns a pointer to a static struct (no allocation needed) -
 * the static contains only function pointers, which are immutable
 * and reentrancy-safe. */
struct lstr_alloc *lstr_default_alloc(void);

#endif /* LSTRALLOC_H */
