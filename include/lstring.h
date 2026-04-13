/* ------------------------------------------------------------------ */
/*  lstring.h - Length-prefixed string library for MVS 3.8j           */
/*                                                                    */
/*  All strings are length-prefixed Lstr records with explicit        */
/*  capacity and an injectable allocator. Functions never call        */
/*  malloc/free directly; growth and release go through the           */
/*  lstr_alloc the caller passes in.                                  */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#ifndef LSTRING_H
#define LSTRING_H

#include <stddef.h>
#include "lstralloc.h"

/* ================================================================== */
/*  Type tag                                                          */
/* ================================================================== */

#define LSTRING_TY  0   /* The only type lstring370 itself uses.
                         * REXX-side type caching (LINTEGER_TY,
                         * LREAL_TY) is added by the rexx370 adapter
                         * (WP-11b), not in this library. */

/* ================================================================== */
/*  Lstr record                                                       */
/* ================================================================== */

struct Lstr_st {
    unsigned char *pstr;    /* data pointer (not NUL-terminated)    */
    size_t         len;     /* live length in bytes                 */
    size_t         maxlen;  /* allocated capacity in bytes          */
    short          type;    /* always LSTRING_TY                    */
};
typedef struct Lstr_st  Lstr;
typedef Lstr           *PLstr;

/* ================================================================== */
/*  Return codes                                                      */
/* ================================================================== */

#define LSTR_OK              0
#define LSTR_ERR_NOMEM      20  /* allocator returned NULL          */
#define LSTR_ERR_BADARG     21  /* NULL pointer where non-NULL req  */

/* ================================================================== */
/*  Macros                                                            */
/* ================================================================== */

#define Llen(s)        ((s)->len)
#define Lmaxlen(s)     ((s)->maxlen)
#define Lpstr(s)       ((s)->pstr)
#define Lzeroinit(s)   do { (s)->pstr = NULL; (s)->len = 0; \
                            (s)->maxlen = 0; \
                            (s)->type = LSTRING_TY; } while (0)

/* ================================================================== */
/*  Core operations (lstr#cor.c)                                      */
/* ================================================================== */

/* Ensure s has at least 'need' bytes of capacity. Grows by doubling
 * with a minimum of 16 bytes. On growth, existing content (up to
 * s->len bytes) is preserved. Returns LSTR_OK or LSTR_ERR_NOMEM. */
int Lfx(struct lstr_alloc *a, PLstr s, size_t need);

/* Release the buffer of s. After Lfree, s is reset to the
 * zero-initialized state and may be reused. */
void Lfree(struct lstr_alloc *a, PLstr s);

/* Copy a NUL-terminated C string into s, replacing previous content.
 * Returns LSTR_OK or LSTR_ERR_NOMEM. */
int Lscpy(struct lstr_alloc *a, PLstr s, const char *src);

/* Copy from one Lstr into another, replacing previous content. */
int Lstrcpy(struct lstr_alloc *a, PLstr to, const PLstr from);

/* Append a NUL-terminated C string to s. */
int Lcat(struct lstr_alloc *a, PLstr s, const char *src);

/* Append the contents of one Lstr to another. */
int Lstrcat(struct lstr_alloc *a, PLstr to, const PLstr from);

/* Translate s to upper / lower case in place, EBCDIC-aware via
 * <ctype.h> from crent370. Both are no-ops when len == 0. */
void Lupper(PLstr s);
void Llower(PLstr s);

#endif /* LSTRING_H */
