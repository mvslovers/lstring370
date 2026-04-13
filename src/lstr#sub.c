/* ------------------------------------------------------------------ */
/*  lstr#sub.c - substring, left, right, center, insert, overlay,     */
/*               delstr                                                */
/*                                                                    */
/*  Position arguments throughout this module are 1-based to match    */
/*  REXX conventions. The destination Lstr must be distinct from any  */
/*  input Lstr; the implementations do not handle aliasing.           */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <string.h>

#include "lstring.h"

int Lsubstr(struct lstr_alloc *a, PLstr to, const PLstr from,
            size_t start, size_t len, char pad)
{
    size_t copy_n;
    int    rc;

    if (to == NULL || from == NULL || start == 0)
        return LSTR_ERR_BADARG;

    if (len == LSTR_REST) {
        len = (start <= from->len) ? from->len - start + 1 : 0;
    }

    rc = Lfx(a, to, len);
    if (rc != LSTR_OK) return rc;

    if (len == 0) {
        to->len = 0;
        return LSTR_OK;
    }

    if (start <= from->len) {
        copy_n = from->len - start + 1;
        if (copy_n > len) copy_n = len;
        memcpy(to->pstr, from->pstr + start - 1, copy_n);
        if (copy_n < len) {
            memset(to->pstr + copy_n, (unsigned char)pad, len - copy_n);
        }
    } else {
        memset(to->pstr, (unsigned char)pad, len);
    }
    to->len = len;
    return LSTR_OK;
}

int Lleft(struct lstr_alloc *a, PLstr to, const PLstr from,
          size_t len, char pad)
{
    return Lsubstr(a, to, from, 1, len, pad);
}

int Lright(struct lstr_alloc *a, PLstr to, const PLstr from,
           size_t len, char pad)
{
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = Lfx(a, to, len);
    if (rc != LSTR_OK) return rc;

    if (len == 0) {
        to->len = 0;
        return LSTR_OK;
    }

    if (from->len >= len) {
        memcpy(to->pstr, from->pstr + (from->len - len), len);
    } else {
        size_t pad_n = len - from->len;
        memset(to->pstr, (unsigned char)pad, pad_n);
        if (from->len > 0) {
            memcpy(to->pstr + pad_n, from->pstr, from->len);
        }
    }
    to->len = len;
    return LSTR_OK;
}

int Lcenter(struct lstr_alloc *a, PLstr to, const PLstr from,
            size_t len, char pad)
{
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = Lfx(a, to, len);
    if (rc != LSTR_OK) return rc;

    if (len == 0) {
        to->len = 0;
        return LSTR_OK;
    }

    if (from->len >= len) {
        /* Truncate by dropping characters equally from both ends. */
        size_t excess    = from->len - len;
        size_t left_drop = excess / 2;
        memcpy(to->pstr, from->pstr + left_drop, len);
    } else {
        size_t total_pad = len - from->len;
        size_t left_pad  = total_pad / 2;
        size_t right_pad = total_pad - left_pad;
        if (left_pad > 0) {
            memset(to->pstr, (unsigned char)pad, left_pad);
        }
        if (from->len > 0) {
            memcpy(to->pstr + left_pad, from->pstr, from->len);
        }
        if (right_pad > 0) {
            memset(to->pstr + left_pad + from->len,
                   (unsigned char)pad, right_pad);
        }
    }
    to->len = len;
    return LSTR_OK;
}

int Linsert(struct lstr_alloc *a, PLstr to, const PLstr ins,
            const PLstr target, size_t pos, char pad)
{
    /* Insert ins immediately after position pos (1-based) of target.
     * pos == 0  -> insert before the start.
     * pos > target->len -> pad target out to pos first, then append ins. */
    size_t take_target;   /* bytes of target before the insertion point */
    size_t gap;           /* pad bytes added when pos > target->len     */
    size_t tail;          /* bytes of target after the insertion point  */
    size_t total;
    int    rc;

    if (to == NULL || ins == NULL || target == NULL)
        return LSTR_ERR_BADARG;

    if (pos > target->len) {
        take_target = target->len;
        gap         = pos - target->len;
        tail        = 0;
    } else {
        take_target = pos;
        gap         = 0;
        tail        = target->len - pos;
    }

    total = take_target + gap + ins->len + tail;
    rc = Lfx(a, to, total);
    if (rc != LSTR_OK) return rc;

    if (take_target > 0) {
        memcpy(to->pstr, target->pstr, take_target);
    }
    if (gap > 0) {
        memset(to->pstr + take_target, (unsigned char)pad, gap);
    }
    if (ins->len > 0) {
        memcpy(to->pstr + take_target + gap, ins->pstr, ins->len);
    }
    if (tail > 0) {
        memcpy(to->pstr + take_target + gap + ins->len,
               target->pstr + pos, tail);
    }
    to->len = total;
    return LSTR_OK;
}

int Loverlay(struct lstr_alloc *a, PLstr to, const PLstr ins,
             const PLstr target, size_t pos, char pad)
{
    /* Overlay ins on target starting at 1-based pos. Cells of target
     * outside the overlay are preserved; cells in the [pos .. pos +
     * ins->len - 1] range are replaced with ins. If pos - 1 + ins->len
     * exceeds target->len, the result is extended and any gap before
     * pos is filled with `pad`. */
    size_t end;
    size_t result_len;
    int    rc;

    if (to == NULL || ins == NULL || target == NULL || pos == 0)
        return LSTR_ERR_BADARG;

    end        = pos - 1 + ins->len;
    result_len = (target->len > end) ? target->len : end;

    rc = Lfx(a, to, result_len);
    if (rc != LSTR_OK) return rc;

    if (result_len > 0) {
        memset(to->pstr, (unsigned char)pad, result_len);
    }
    if (target->len > 0) {
        memcpy(to->pstr, target->pstr, target->len);
    }
    if (ins->len > 0) {
        memcpy(to->pstr + pos - 1, ins->pstr, ins->len);
    }
    to->len = result_len;
    return LSTR_OK;
}

int Ldelstr(struct lstr_alloc *a, PLstr to, const PLstr from,
            size_t start, size_t len)
{
    size_t before_len;
    size_t after_start;
    size_t after_len;
    size_t total;
    int    rc;

    if (to == NULL || from == NULL || start == 0)
        return LSTR_ERR_BADARG;

    if (start > from->len) {
        return Lstrcpy(a, to, from);
    }

    before_len = start - 1;
    if (len == LSTR_REST || start - 1 + len > from->len) {
        after_start = from->len;
        after_len   = 0;
    } else {
        after_start = start - 1 + len;
        after_len   = from->len - after_start;
    }
    total = before_len + after_len;

    rc = Lfx(a, to, total);
    if (rc != LSTR_OK) return rc;

    if (before_len > 0) {
        memcpy(to->pstr, from->pstr, before_len);
    }
    if (after_len > 0) {
        memcpy(to->pstr + before_len, from->pstr + after_start, after_len);
    }
    to->len = total;
    return LSTR_OK;
}
