/* ------------------------------------------------------------------ */
/*  lstr#cor.c - lstring370 core operations                           */
/*                                                                    */
/*  This module is the only place where the injected allocator's      */
/*  alloc/dealloc callbacks are invoked. All other lstring370         */
/*  modules grow and release buffers through Lfx and Lfree from       */
/*  here.                                                             */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lstring.h"
#include "lstralloc.h"

/* ------------------------------------------------------------------ */
/*  Default allocator (malloc/free)                                   */
/* ------------------------------------------------------------------ */

static void *default_alloc(size_t size, void *ctx)
{
    (void)ctx;
    return malloc(size);
}

static void default_dealloc(void *ptr, size_t size, void *ctx)
{
    (void)size;
    (void)ctx;
    free(ptr);
}

/* The default allocator is a static struct of function pointers.
 * Function pointers are immutable; sharing the struct between
 * threads / environments is reentrancy-safe. */
static struct lstr_alloc default_allocator = {
    default_alloc,
    default_dealloc,
    NULL
};

struct lstr_alloc *lstr_default_alloc(void)
{
    return &default_allocator;
}

/* ------------------------------------------------------------------ */
/*  Capacity management                                               */
/* ------------------------------------------------------------------ */

/* Minimum allocation: small enough not to waste space on tiny
 * strings, large enough to avoid a regrow on the very next byte. */
#define LSTR_MIN_CAPACITY 16

static size_t round_capacity(size_t need)
{
    size_t cap = LSTR_MIN_CAPACITY;
    while (cap < need) {
        cap *= 2;
    }
    return cap;
}

int Lfx(struct lstr_alloc *a, PLstr s, size_t need)
{
    unsigned char *new_buf;
    size_t         new_cap;

    if (a == NULL || s == NULL) return LSTR_ERR_BADARG;
    if (need <= s->maxlen) return LSTR_OK;

    new_cap = round_capacity(need);
    new_buf = (unsigned char *)(*a->alloc)(new_cap, a->ctx);
    if (new_buf == NULL) return LSTR_ERR_NOMEM;

    if (s->pstr != NULL) {
        if (s->len > 0) {
            memcpy(new_buf, s->pstr, s->len);
        }
        (*a->dealloc)(s->pstr, s->maxlen, a->ctx);
    }

    s->pstr   = new_buf;
    s->maxlen = new_cap;
    return LSTR_OK;
}

void Lfree(struct lstr_alloc *a, PLstr s)
{
    if (s == NULL) return;
    if (s->pstr != NULL && a != NULL) {
        (*a->dealloc)(s->pstr, s->maxlen, a->ctx);
    }
    Lzeroinit(s);
}

/* ------------------------------------------------------------------ */
/*  Copy / concatenate                                                */
/* ------------------------------------------------------------------ */

int Lscpy(struct lstr_alloc *a, PLstr s, const char *src)
{
    size_t n;
    int    rc;

    if (s == NULL || src == NULL) return LSTR_ERR_BADARG;
    n = strlen(src);
    rc = Lfx(a, s, n);
    if (rc != LSTR_OK) return rc;
    if (n > 0) memcpy(s->pstr, src, n);
    s->len = n;
    return LSTR_OK;
}

int Lstrcpy(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = Lfx(a, to, from->len);
    if (rc != LSTR_OK) return rc;
    if (from->len > 0) memcpy(to->pstr, from->pstr, from->len);
    to->len = from->len;
    return LSTR_OK;
}

int Lcat(struct lstr_alloc *a, PLstr s, const char *src)
{
    size_t n;
    int    rc;

    if (s == NULL || src == NULL) return LSTR_ERR_BADARG;
    n = strlen(src);
    if (n == 0) return LSTR_OK;
    rc = Lfx(a, s, s->len + n);
    if (rc != LSTR_OK) return rc;
    memcpy(s->pstr + s->len, src, n);
    s->len += n;
    return LSTR_OK;
}

int Lstrcat(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    if (from->len == 0) return LSTR_OK;
    rc = Lfx(a, to, to->len + from->len);
    if (rc != LSTR_OK) return rc;
    memcpy(to->pstr + to->len, from->pstr, from->len);
    to->len += from->len;
    return LSTR_OK;
}

/* ------------------------------------------------------------------ */
/*  Case translation (EBCDIC-aware via <ctype.h>)                     */
/* ------------------------------------------------------------------ */

void Lupper(PLstr s)
{
    size_t i;
    if (s == NULL || s->pstr == NULL) return;
    for (i = 0; i < s->len; i++) {
        s->pstr[i] = (unsigned char)toupper(s->pstr[i]);
    }
}

void Llower(PLstr s)
{
    size_t i;
    if (s == NULL || s->pstr == NULL) return;
    for (i = 0; i < s->len; i++) {
        s->pstr[i] = (unsigned char)tolower(s->pstr[i]);
    }
}
