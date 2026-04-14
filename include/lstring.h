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

/* ================================================================== */
/*  Substring / Position (lstr#sub.c)                                 */
/*                                                                    */
/*  Position arguments are 1-based to match REXX conventions.         */
/*  The output `to` and the input string(s) must be distinct          */
/*  Lstr instances; aliasing is not supported.                        */
/* ================================================================== */

/* Sentinel meaning "from start to end of string". */
#define LSTR_REST  ((size_t)-1)

int Lsubstr (struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t start, size_t len, char pad);
int Lleft   (struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t len, char pad);
int Lright  (struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t len, char pad);
int Lcenter (struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t len, char pad);
int Linsert (struct lstr_alloc *a, PLstr to, const PLstr ins,
             const PLstr target, size_t pos, char pad);
int Loverlay(struct lstr_alloc *a, PLstr to, const PLstr ins,
             const PLstr target, size_t pos, char pad);
int Ldelstr (struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t start, size_t len);

/* ================================================================== */
/*  Word operations (lstr#wrd.c)                                      */
/*                                                                    */
/*  Words are maximal runs of non-whitespace separated by one or      */
/*  more whitespace characters. Whitespace classification uses        */
/*  isspace() from <ctype.h>, so the rules are EBCDIC-correct on      */
/*  MVS via crent370. Word indices are 1-based.                       */
/* ================================================================== */

int    Lword       (struct lstr_alloc *a, PLstr to,
                    const PLstr from, size_t n);
size_t Lwords      (const PLstr s);
int    Lsubword    (struct lstr_alloc *a, PLstr to,
                    const PLstr from, size_t n, size_t count);
size_t Lwordindex  (const PLstr s, size_t n);
size_t Lwordlength (const PLstr s, size_t n);
size_t Lwordpos    (const PLstr phrase, const PLstr s, size_t start);
int    Ldelword    (struct lstr_alloc *a, PLstr to,
                    const PLstr from, size_t n, size_t count);

/* ================================================================== */
/*  Search / Compare (lstr#srch.c)                                    */
/*                                                                    */
/*  Positions are 1-based. A return value of 0 means "not found".     */
/* ================================================================== */

/* Find the first occurrence of needle in haystack starting at the
 * 1-based position 'start'. Returns the 1-based position of the
 * match, or 0 if not found. An empty needle always returns 0. */
size_t Lpos     (const PLstr needle, const PLstr haystack, size_t start);

/* Alias for Lpos matching REXX INDEX semantics. */
size_t Lindex   (const PLstr needle, const PLstr haystack, size_t start);

/* Find the last occurrence of needle in haystack at or before the
 * 1-based position 'start'. If start is 0, search the whole string. */
size_t Llastpos (const PLstr needle, const PLstr haystack, size_t start);

/* Verify mode for Lverify. */
#define LVERIFY_NOMATCH   0   /* default: find first char NOT in ref   */
#define LVERIFY_MATCH     1   /* 'M': find first char that IS in ref   */

/* Find the 1-based position of the first character of 'from' (starting
 * at 'start') that is / is-not present in 'ref'. Returns 0 if every
 * character matches the criterion. */
size_t Lverify  (const PLstr from, const PLstr ref,
                 int mode, size_t start);

/* REXX ABBREV semantics: returns non-zero (truthy) if 'info' is a
 * leading substring of 'long_str' and its length is at least 'min'.
 * If min == 0, min defaults to info->len (any non-empty prefix counts). */
int    Labbrev  (const PLstr long_str, const PLstr info, size_t min);

/* Compare two strings, padding the shorter one with 'pad'. Returns 0
 * if the (possibly padded) contents are equal, else the 1-based
 * position of the first differing byte. */
size_t Lcompare (const PLstr s1, const PLstr s2, char pad);

/* ================================================================== */
/*  Transform (lstr#xlt.c)                                            */
/* ================================================================== */

/* Translate characters in `from` by mapping through (tablei, tableo).
 * For each byte b in from: look up b in tablei; if found at offset k
 * and k < tableo->len, replace with tableo->pstr[k]; otherwise
 * replace with pad. If tablei is NULL, a full 256-byte identity map
 * is assumed. If tableo is NULL, the default is REXX-style uppercase
 * (tolower -> toupper is applied when tableo and tablei are both
 * NULL). */
int Ltranslate(struct lstr_alloc *a, PLstr to, const PLstr from,
               const PLstr tableo, const PLstr tablei, char pad);

/* Strip mode options. */
#define LSTRIP_BOTH     'B'
#define LSTRIP_LEADING  'L'
#define LSTRIP_TRAILING 'T'

/* Remove leading/trailing occurrences of `strip_char` from `from`.
 * option is LSTRIP_BOTH / LSTRIP_LEADING / LSTRIP_TRAILING. */
int Lstrip    (struct lstr_alloc *a, PLstr to, const PLstr from,
               int option, char strip_char);

/* Normalise whitespace: strip leading/trailing whitespace and replace
 * each interior run of whitespace with exactly n copies of pad. */
int Lspace    (struct lstr_alloc *a, PLstr to, const PLstr from,
               size_t n, char pad);

/* Concatenate `from` with itself `n` times. */
int Lcopies   (struct lstr_alloc *a, PLstr to, const PLstr from,
               size_t n);

/* Reverse the bytes of `from` into `to`. */
int Lreverse  (struct lstr_alloc *a, PLstr to, const PLstr from);

/* Replace every non-overlapping occurrence of `old_str` in `from`
 * with `new_str`. An empty old_str copies from unchanged. */
int Lchangestr(struct lstr_alloc *a, PLstr to, const PLstr from,
               const PLstr old_str, const PLstr new_str);

/* Count non-overlapping occurrences of `needle` in `haystack`. */
size_t Lcountstr(const PLstr needle, const PLstr haystack);

/* ================================================================== */
/*  Base conversion (lstr#cvt.c)                                      */
/*                                                                    */
/*  C2X / X2C   character <-> hex digits                              */
/*  C2D / D2C   character <-> decimal digit string                    */
/*  D2X / X2D   decimal digit string <-> hex digits                   */
/*  B2X / X2B   binary bits <-> hex digits                            */
/*                                                                    */
/*  Numeric conversions (C2D/D2C/D2X/X2D) use 'long' as the           */
/*  intermediate integer and are bounded by sizeof(long). The         */
/*  rexx370 arithmetic engine (WP-20) will layer arbitrary-precision  */
/*  variants on top when needed.                                      */
/* ================================================================== */

int Lc2x(struct lstr_alloc *a, PLstr to, const PLstr from);
int Lx2c(struct lstr_alloc *a, PLstr to, const PLstr from);
int Lc2d(struct lstr_alloc *a, PLstr to, const PLstr from);
int Ld2c(struct lstr_alloc *a, PLstr to, const PLstr from);
int Ld2x(struct lstr_alloc *a, PLstr to, const PLstr from);
int Lx2d(struct lstr_alloc *a, PLstr to, const PLstr from);
int Lb2x(struct lstr_alloc *a, PLstr to, const PLstr from);
int Lx2b(struct lstr_alloc *a, PLstr to, const PLstr from);

#endif /* LSTRING_H */
