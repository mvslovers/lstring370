/* ------------------------------------------------------------------ */
/*  lstr#srch.c - search, verify, abbrev, compare                     */
/*                                                                    */
/*  All positions are 1-based to match REXX conventions. A return     */
/*  value of 0 means "not found" (or "every char matched" for         */
/*  Lverify).                                                         */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <string.h>

#include "lstring.h"

size_t Lpos(const PLstr needle, const PLstr haystack, size_t start)
{
    size_t i;
    size_t last;

    if (needle == NULL || haystack == NULL) return 0;
    if (needle->len == 0 || haystack->len == 0) return 0;
    if (needle->len > haystack->len) return 0;

    if (start == 0) start = 1;
    if (start > haystack->len) return 0;

    last = haystack->len - needle->len;
    for (i = start - 1; i <= last; i++) {
        if (haystack->pstr[i] == needle->pstr[0] &&
            memcmp(haystack->pstr + i, needle->pstr, needle->len) == 0) {
            return i + 1;
        }
    }
    return 0;
}

size_t Lindex(const PLstr needle, const PLstr haystack, size_t start)
{
    return Lpos(needle, haystack, start);
}

size_t Llastpos(const PLstr needle, const PLstr haystack, size_t start)
{
    size_t i;
    size_t upper;

    if (needle == NULL || haystack == NULL) return 0;
    if (needle->len == 0 || haystack->len == 0) return 0;
    if (needle->len > haystack->len) return 0;

    if (start == 0 || start > haystack->len) {
        upper = haystack->len - needle->len;
    } else if (start < needle->len) {
        return 0;
    } else {
        upper = start - needle->len;
        if (upper > haystack->len - needle->len) {
            upper = haystack->len - needle->len;
        }
    }

    /* Walk backwards from upper. Use i+1 so the loop still terminates
     * when upper is 0. */
    for (i = upper + 1; i > 0; i--) {
        size_t idx = i - 1;
        if (haystack->pstr[idx] == needle->pstr[0] &&
            memcmp(haystack->pstr + idx, needle->pstr,
                   needle->len) == 0) {
            return idx + 1;
        }
    }
    return 0;
}

size_t Lverify(const PLstr from, const PLstr ref,
               int mode, size_t start)
{
    size_t i;

    if (from == NULL || ref == NULL) return 0;
    if (from->len == 0) return 0;
    if (start == 0) start = 1;
    if (start > from->len) return 0;

    for (i = start - 1; i < from->len; i++) {
        /* present == 1 iff from->pstr[i] appears anywhere in ref */
        int present = 0;
        if (ref->len > 0) {
            present = (memchr(ref->pstr, from->pstr[i], ref->len) != NULL);
        }
        if (mode == LVERIFY_MATCH) {
            if (present) return i + 1;
        } else {
            if (!present) return i + 1;
        }
    }
    return 0;
}

int Labbrev(const PLstr long_str, const PLstr info, size_t min)
{
    if (long_str == NULL || info == NULL) return 0;
    if (min == 0) min = info->len;
    if (info->len < min) return 0;
    if (info->len > long_str->len) return 0;
    if (info->len == 0) return 1;   /* empty info is always an abbrev */
    return memcmp(long_str->pstr, info->pstr, info->len) == 0;
}

size_t Lcompare(const PLstr s1, const PLstr s2, char pad)
{
    size_t i;
    size_t longer;
    size_t shorter;
    int    s1_longer;

    if (s1 == NULL || s2 == NULL) return 0;

    if (s1->len >= s2->len) {
        longer = s1->len;
        shorter = s2->len;
        s1_longer = 1;
    } else {
        longer = s2->len;
        shorter = s1->len;
        s1_longer = 0;
    }

    /* Compare the common portion byte by byte. */
    for (i = 0; i < shorter; i++) {
        if (s1->pstr[i] != s2->pstr[i]) return i + 1;
    }

    /* Compare the tail of the longer string against pad. */
    for (i = shorter; i < longer; i++) {
        unsigned char b = s1_longer ? s1->pstr[i] : s2->pstr[i];
        if (b != (unsigned char)pad) return i + 1;
    }
    return 0;
}
