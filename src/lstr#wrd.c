/* ------------------------------------------------------------------ */
/*  lstr#wrd.c - word-oriented string operations                      */
/*                                                                    */
/*  A "word" is a maximal run of non-whitespace bytes. Runs of        */
/*  whitespace separate words; leading and trailing whitespace are    */
/*  ignored. Whitespace is whatever isspace() recognises, so the      */
/*  rules are EBCDIC-correct on MVS via crent370.                     */
/*                                                                    */
/*  Word indices are 1-based to match REXX semantics.                 */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <string.h>
#include <ctype.h>

#include "lstring.h"

/* ------------------------------------------------------------------ */
/*  Internal helper: locate the nth word (1-based)                    */
/*                                                                    */
/*  On success returns 1 and writes the byte offset of the first      */
/*  character of the nth word into *out_off and its length into       */
/*  *out_len. Returns 0 if there is no nth word.                      */
/* ------------------------------------------------------------------ */

static int find_word(const PLstr s, size_t n,
                     size_t *out_off, size_t *out_len)
{
    size_t i = 0;
    size_t count = 0;

    if (s == NULL || s->pstr == NULL || s->len == 0 || n == 0) return 0;

    while (i < s->len) {
        /* Skip whitespace. */
        while (i < s->len && isspace(s->pstr[i])) i++;
        if (i >= s->len) return 0;

        count++;
        if (count == n) {
            size_t start = i;
            while (i < s->len && !isspace(s->pstr[i])) i++;
            *out_off = start;
            *out_len = i - start;
            return 1;
        }
        /* Skip the rest of this word and continue looking. */
        while (i < s->len && !isspace(s->pstr[i])) i++;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

size_t Lwords(const PLstr s)
{
    size_t i = 0;
    size_t count = 0;

    if (s == NULL || s->pstr == NULL) return 0;
    while (i < s->len) {
        while (i < s->len && isspace(s->pstr[i])) i++;
        if (i >= s->len) break;
        count++;
        while (i < s->len && !isspace(s->pstr[i])) i++;
    }
    return count;
}

int Lword(struct lstr_alloc *a, PLstr to, const PLstr from, size_t n)
{
    size_t off, wlen;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    if (!find_word(from, n, &off, &wlen)) {
        rc = Lfx(a, to, 0);
        if (rc != LSTR_OK) return rc;
        to->len = 0;
        return LSTR_OK;
    }

    rc = Lfx(a, to, wlen);
    if (rc != LSTR_OK) return rc;
    if (wlen > 0) memcpy(to->pstr, from->pstr + off, wlen);
    to->len = wlen;
    return LSTR_OK;
}

int Lsubword(struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t n, size_t count)
{
    size_t i = 0;
    size_t cur = 0;
    size_t start_off = 0;
    size_t end_off   = 0;
    size_t took      = 0;
    int    started   = 0;
    int    rc;

    if (to == NULL || from == NULL || n == 0) return LSTR_ERR_BADARG;

    while (i < from->len && (count == LSTR_REST || took < count)) {
        while (i < from->len && isspace(from->pstr[i])) i++;
        if (i >= from->len) break;

        cur++;
        if (cur >= n) {
            if (!started) {
                start_off = i;
                started = 1;
            }
        }

        {
            size_t word_start = i;
            while (i < from->len && !isspace(from->pstr[i])) i++;
            if (started) {
                end_off = i;
                took++;
                /* Include any trailing whitespace between words inside
                 * the selected range, but not after the last word. */
                if (count != LSTR_REST && took >= count) break;
                (void)word_start;
            }
        }
    }

    if (count == LSTR_REST && started) {
        /* Take everything from start_off to end of last word seen. */
        end_off = i;
        /* Walk back over trailing whitespace. */
        while (end_off > start_off && isspace(from->pstr[end_off - 1])) {
            end_off--;
        }
    }

    if (!started) {
        rc = Lfx(a, to, 0);
        if (rc != LSTR_OK) return rc;
        to->len = 0;
        return LSTR_OK;
    }

    {
        size_t out_len = end_off - start_off;
        rc = Lfx(a, to, out_len);
        if (rc != LSTR_OK) return rc;
        if (out_len > 0) memcpy(to->pstr, from->pstr + start_off, out_len);
        to->len = out_len;
    }
    return LSTR_OK;
}

size_t Lwordindex(const PLstr s, size_t n)
{
    size_t off, wlen;
    if (!find_word(s, n, &off, &wlen)) return 0;
    return off + 1;   /* 1-based */
}

size_t Lwordlength(const PLstr s, size_t n)
{
    size_t off, wlen;
    if (!find_word(s, n, &off, &wlen)) return 0;
    return wlen;
}

/* Helper for Lwordpos: scan the next word in s starting at *cursor.
 * Returns 1 if a word is found (writes start/len), else 0. Advances
 * *cursor past the word. */
static int next_word(const PLstr s, size_t *cursor,
                     size_t *out_start, size_t *out_len)
{
    size_t i = *cursor;
    size_t start;

    while (i < s->len && isspace(s->pstr[i])) i++;
    if (i >= s->len) {
        *cursor = i;
        return 0;
    }
    start = i;
    while (i < s->len && !isspace(s->pstr[i])) i++;
    *out_start = start;
    *out_len   = i - start;
    *cursor    = i;
    return 1;
}

size_t Lwordpos(const PLstr phrase, const PLstr s, size_t start)
{
    size_t s_cursor;
    size_t s_word_index;

    if (phrase == NULL || s == NULL) return 0;
    if (Lwords(phrase) == 0) return 0;

    if (start == 0) start = 1;

    /* Walk s word-by-word; for each starting position try to match
     * the entire phrase word-by-word. */
    s_cursor = 0;
    s_word_index = 0;
    while (s_cursor < s->len) {
        size_t s_start, s_wlen;
        if (!next_word(s, &s_cursor, &s_start, &s_wlen)) break;
        s_word_index++;
        if (s_word_index < start) continue;

        /* Try to match phrase from this point. */
        {
            size_t p_cursor = 0;
            size_t s_probe  = s_start;
            int    matched  = 1;
            size_t p_start, p_wlen;

            while (next_word(phrase, &p_cursor, &p_start, &p_wlen)) {
                /* Skip leading whitespace at s_probe. */
                while (s_probe < s->len && isspace(s->pstr[s_probe])) {
                    s_probe++;
                }
                if (s_probe >= s->len) { matched = 0; break; }
                {
                    size_t s_word_start = s_probe;
                    size_t s_word_end   = s_probe;
                    while (s_word_end < s->len &&
                           !isspace(s->pstr[s_word_end])) s_word_end++;
                    if (s_word_end - s_word_start != p_wlen ||
                        memcmp(s->pstr + s_word_start,
                               phrase->pstr + p_start, p_wlen) != 0) {
                        matched = 0;
                        break;
                    }
                    s_probe = s_word_end;
                }
            }
            if (matched) return s_word_index;
        }
    }
    return 0;
}

int Ldelword(struct lstr_alloc *a, PLstr to, const PLstr from,
             size_t n, size_t count)
{
    /* Delete a range of words. cut_start is the first byte of word n.
     * cut_end is the first byte of the word after the deleted range
     * (= start of word n + count). If there is no such word (count
     * runs past the end, or count == LSTR_REST), the deletion extends
     * to the end of the string and any trailing whitespace is
     * trimmed. Whitespace preceding word n is preserved. */
    size_t i = 0;
    size_t cur = 0;
    size_t cut_start = 0;
    size_t cut_end   = 0;
    int    start_found = 0;
    int    end_found   = 0;
    int    rc;
    size_t total;

    if (to == NULL || from == NULL || n == 0) return LSTR_ERR_BADARG;

    if (count == 0) return Lstrcpy(a, to, from);

    while (i < from->len) {
        while (i < from->len && isspace(from->pstr[i])) i++;
        if (i >= from->len) break;

        cur++;
        if (cur == n) {
            cut_start = i;
            start_found = 1;
        }
        if (start_found && count != LSTR_REST && cur == n + count) {
            cut_end = i;
            end_found = 1;
            break;
        }

        /* Skip this word's body. */
        while (i < from->len && !isspace(from->pstr[i])) i++;
    }

    if (!start_found) return Lstrcpy(a, to, from);

    if (!end_found) {
        /* Deletion extends to end of string; trim trailing blanks
         * from the preceding portion (per SC28-1883-0 DELWORD). */
        cut_end = from->len;
        while (cut_start > 0 && isspace(from->pstr[cut_start - 1])) {
            cut_start--;
        }
    }

    total = from->len - (cut_end - cut_start);
    rc = Lfx(a, to, total);
    if (rc != LSTR_OK) return rc;

    if (cut_start > 0) {
        memcpy(to->pstr, from->pstr, cut_start);
    }
    if (cut_end < from->len) {
        memcpy(to->pstr + cut_start, from->pstr + cut_end,
               from->len - cut_end);
    }
    to->len = total;
    return LSTR_OK;
}
