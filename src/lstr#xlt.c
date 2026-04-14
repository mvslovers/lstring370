/* ------------------------------------------------------------------ */
/*  lstr#xlt.c - transform operations                                 */
/*                                                                    */
/*  Translate, strip, space, copies, reverse, changestr, countstr.    */
/*  None of these functions share state; the allocator travels with   */
/*  the call and all temporary buffers are freed on the error path.   */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <string.h>
#include <ctype.h>

#include "lstring.h"

int Ltranslate(struct lstr_alloc *a, PLstr to, const PLstr from,
               const PLstr tableo, const PLstr tablei, char pad)
{
    size_t i;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    rc = Lfx(a, to, from->len);
    if (rc != LSTR_OK) return rc;

    if (tableo == NULL && tablei == NULL) {
        /* REXX default: uppercase translation. */
        for (i = 0; i < from->len; i++) {
            to->pstr[i] = (unsigned char)toupper(from->pstr[i]);
        }
    } else {
        for (i = 0; i < from->len; i++) {
            unsigned char c = from->pstr[i];
            const unsigned char *hit;
            unsigned char result = (unsigned char)pad;

            if (tablei != NULL && tablei->len > 0) {
                hit = (const unsigned char *)
                      memchr(tablei->pstr, c, tablei->len);
            } else {
                /* NULL tablei means identity: position = c. */
                hit = NULL;
            }

            if (tablei == NULL) {
                /* Identity input: look up position c in tableo. */
                if (tableo != NULL && c < tableo->len) {
                    result = tableo->pstr[c];
                } else {
                    result = (unsigned char)pad;
                }
            } else if (hit != NULL) {
                size_t k = (size_t)(hit - tablei->pstr);
                if (tableo != NULL && k < tableo->len) {
                    result = tableo->pstr[k];
                } else {
                    result = (unsigned char)pad;
                }
            } else {
                /* Not in tablei: pass through unchanged. */
                result = c;
            }
            to->pstr[i] = result;
        }
    }
    to->len = from->len;
    return LSTR_OK;
}

int Lstrip(struct lstr_alloc *a, PLstr to, const PLstr from,
           int option, char strip_char)
{
    size_t start = 0;
    size_t end;
    size_t n;
    int    rc;
    unsigned char sc = (unsigned char)strip_char;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    end = from->len;

    if (option == LSTRIP_LEADING || option == LSTRIP_BOTH) {
        while (start < end && from->pstr[start] == sc) start++;
    }
    if (option == LSTRIP_TRAILING || option == LSTRIP_BOTH) {
        while (end > start && from->pstr[end - 1] == sc) end--;
    }

    n = end - start;
    rc = Lfx(a, to, n);
    if (rc != LSTR_OK) return rc;
    if (n > 0) memcpy(to->pstr, from->pstr + start, n);
    to->len = n;
    return LSTR_OK;
}

int Lspace(struct lstr_alloc *a, PLstr to, const PLstr from,
           size_t n, char pad)
{
    size_t i;
    size_t out_cap;
    int    rc;
    int    first_word = 1;
    unsigned char padb = (unsigned char)pad;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    /* Upper bound on output length: every original byte plus one
     * potential separator. Grow to that and shrink via to->len. */
    out_cap = from->len + (from->len / 2 + 1) * n + n;
    rc = Lfx(a, to, out_cap);
    if (rc != LSTR_OK) return rc;

    to->len = 0;
    i = 0;
    while (i < from->len) {
        /* Skip whitespace. */
        while (i < from->len && isspace(from->pstr[i])) i++;
        if (i >= from->len) break;

        if (!first_word) {
            size_t k;
            for (k = 0; k < n; k++) {
                to->pstr[to->len++] = padb;
            }
        }
        first_word = 0;

        /* Copy the word. */
        while (i < from->len && !isspace(from->pstr[i])) {
            to->pstr[to->len++] = from->pstr[i++];
        }
    }
    return LSTR_OK;
}

int Lcopies(struct lstr_alloc *a, PLstr to, const PLstr from, size_t n)
{
    size_t total;
    size_t k;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    if (n == 0 || from->len == 0) {
        rc = Lfx(a, to, 0);
        if (rc != LSTR_OK) return rc;
        to->len = 0;
        return LSTR_OK;
    }

    total = from->len * n;
    rc = Lfx(a, to, total);
    if (rc != LSTR_OK) return rc;

    for (k = 0; k < n; k++) {
        memcpy(to->pstr + k * from->len, from->pstr, from->len);
    }
    to->len = total;
    return LSTR_OK;
}

int Lreverse(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    size_t i;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = Lfx(a, to, from->len);
    if (rc != LSTR_OK) return rc;

    for (i = 0; i < from->len; i++) {
        to->pstr[i] = from->pstr[from->len - 1 - i];
    }
    to->len = from->len;
    return LSTR_OK;
}

int Lchangestr(struct lstr_alloc *a, PLstr to, const PLstr from,
               const PLstr old_str, const PLstr new_str)
{
    /* First pass: count occurrences of old_str in from so we can
     * size 'to' in one shot. Second pass: copy with substitution. */
    size_t i;
    size_t hits = 0;
    size_t total;
    int    rc;

    if (to == NULL || from == NULL ||
        old_str == NULL || new_str == NULL) return LSTR_ERR_BADARG;

    if (old_str->len == 0 || from->len < old_str->len) {
        return Lstrcpy(a, to, from);
    }

    for (i = 0; i + old_str->len <= from->len; ) {
        if (from->pstr[i] == old_str->pstr[0] &&
            memcmp(from->pstr + i, old_str->pstr, old_str->len) == 0) {
            hits++;
            i += old_str->len;
        } else {
            i++;
        }
    }

    if (hits == 0) return Lstrcpy(a, to, from);

    /* new_total = from->len - hits*old->len + hits*new->len */
    total = from->len - hits * old_str->len + hits * new_str->len;
    rc = Lfx(a, to, total);
    if (rc != LSTR_OK) return rc;

    {
        size_t src_i = 0;
        size_t dst_i = 0;
        while (src_i < from->len) {
            if (src_i + old_str->len <= from->len &&
                from->pstr[src_i] == old_str->pstr[0] &&
                memcmp(from->pstr + src_i, old_str->pstr,
                       old_str->len) == 0) {
                if (new_str->len > 0) {
                    memcpy(to->pstr + dst_i, new_str->pstr, new_str->len);
                    dst_i += new_str->len;
                }
                src_i += old_str->len;
            } else {
                to->pstr[dst_i++] = from->pstr[src_i++];
            }
        }
        to->len = dst_i;
    }
    return LSTR_OK;
}

size_t Lcountstr(const PLstr needle, const PLstr haystack)
{
    size_t i;
    size_t count = 0;

    if (needle == NULL || haystack == NULL) return 0;
    if (needle->len == 0 || haystack->len < needle->len) return 0;

    for (i = 0; i + needle->len <= haystack->len; ) {
        if (haystack->pstr[i] == needle->pstr[0] &&
            memcmp(haystack->pstr + i, needle->pstr,
                   needle->len) == 0) {
            count++;
            i += needle->len;
        } else {
            i++;
        }
    }
    return count;
}
