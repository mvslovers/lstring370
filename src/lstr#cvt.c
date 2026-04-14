/* ------------------------------------------------------------------ */
/*  lstr#cvt.c - base conversions                                     */
/*                                                                    */
/*  C2X / X2C   bytes <-> hex digits                                  */
/*  C2D / D2C   bytes <-> unsigned decimal digits                     */
/*  D2X / X2D   decimal digits <-> hex digits                         */
/*  B2X / X2B   binary bits   <-> hex digits                          */
/*                                                                    */
/*  Hex / digit classification and conversion use <ctype.h> helpers   */
/*  plus arithmetic differences against character literals so the     */
/*  code is EBCDIC-correct via crent370 on MVS.                       */
/*                                                                    */
/*  Numeric conversions use long as the intermediate integer; values  */
/*  beyond LONG_MAX produce LSTR_ERR_BADARG.                          */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "lstring.h"

/* ------------------------------------------------------------------ */
/*  Character / digit helpers                                         */
/* ------------------------------------------------------------------ */

/* Convert a nibble value (0..15) to its hex digit character. Uses
 * character literals so the output is EBCDIC on MVS, ASCII elsewhere. */
static unsigned char nibble_to_hex(unsigned v)
{
    static const char digits[] = "0123456789ABCDEF";
    return (unsigned char)digits[v & 0xF];
}

/* Convert a hex digit character (EBCDIC or ASCII) to its 0..15 value.
 * Returns -1 if the character is not a valid hex digit. */
static int hex_to_nibble(int c)
{
    if (!isxdigit(c)) return -1;
    if (isdigit(c))  return c - '0';
    if (isupper(c))  return 10 + (toupper(c) - 'A');
    return 10 + (tolower(c) - 'a');
}

/* Convert a decimal digit char to its value, or -1 if not a digit. */
static int dec_to_val(int c)
{
    if (!isdigit(c)) return -1;
    return c - '0';
}

/* ------------------------------------------------------------------ */
/*  C2X / X2C                                                         */
/* ------------------------------------------------------------------ */

int Lc2x(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    size_t i;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = Lfx(a, to, from->len * 2);
    if (rc != LSTR_OK) return rc;

    for (i = 0; i < from->len; i++) {
        unsigned char b = from->pstr[i];
        to->pstr[2 * i]     = nibble_to_hex(b >> 4);
        to->pstr[2 * i + 1] = nibble_to_hex(b);
    }
    to->len = from->len * 2;
    return LSTR_OK;
}

int Lx2c(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    /* Hex strings may contain blanks between byte boundaries. We
     * scan once to filter blanks, then convert nibble pairs. */
    size_t i;
    size_t nibbles = 0;
    int    high = -1;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    /* Count hex digits to size output. */
    for (i = 0; i < from->len; i++) {
        unsigned char c = from->pstr[i];
        if (c == ' ' || c == '\t') continue;
        if (hex_to_nibble(c) < 0) return LSTR_ERR_BADARG;
        nibbles++;
    }
    if ((nibbles & 1) != 0) return LSTR_ERR_BADARG;

    rc = Lfx(a, to, nibbles / 2);
    if (rc != LSTR_OK) return rc;
    to->len = 0;

    for (i = 0; i < from->len; i++) {
        unsigned char c = from->pstr[i];
        int v;
        if (c == ' ' || c == '\t') continue;
        v = hex_to_nibble(c);
        if (high < 0) {
            high = v;
        } else {
            to->pstr[to->len++] = (unsigned char)((high << 4) | v);
            high = -1;
        }
    }
    return LSTR_OK;
}

/* ------------------------------------------------------------------ */
/*  Integer helpers                                                   */
/* ------------------------------------------------------------------ */

/* Parse a non-negative decimal digit string into *out. Returns
 * LSTR_OK on success, LSTR_ERR_BADARG for bad input or overflow. */
static int parse_udec(const PLstr s, unsigned long *out)
{
    unsigned long v = 0;
    size_t i;

    if (s->len == 0) return LSTR_ERR_BADARG;
    for (i = 0; i < s->len; i++) {
        int d = dec_to_val(s->pstr[i]);
        if (d < 0) return LSTR_ERR_BADARG;
        if (v > ULONG_MAX / 10) return LSTR_ERR_BADARG;
        v *= 10;
        if (v > ULONG_MAX - (unsigned long)d) return LSTR_ERR_BADARG;
        v += (unsigned long)d;
    }
    *out = v;
    return LSTR_OK;
}

/* Parse a hex digit string (no spaces) into *out. */
static int parse_uhex(const PLstr s, unsigned long *out)
{
    unsigned long v = 0;
    size_t i;

    if (s->len == 0) return LSTR_ERR_BADARG;
    if (s->len > sizeof(unsigned long) * 2) return LSTR_ERR_BADARG;
    for (i = 0; i < s->len; i++) {
        int n = hex_to_nibble(s->pstr[i]);
        if (n < 0) return LSTR_ERR_BADARG;
        v = (v << 4) | (unsigned long)n;
    }
    *out = v;
    return LSTR_OK;
}

/* Render an unsigned integer to `to` as decimal digits. */
static int render_udec(struct lstr_alloc *a, PLstr to, unsigned long v)
{
    char buf[32];
    int  n = 0;
    int  rc;

    if (v == 0) {
        buf[n++] = '0';
    } else {
        while (v > 0 && n < (int)sizeof(buf)) {
            buf[n++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }

    rc = Lfx(a, to, (size_t)n);
    if (rc != LSTR_OK) return rc;
    {
        int i;
        for (i = 0; i < n; i++) to->pstr[i] = (unsigned char)buf[n - 1 - i];
    }
    to->len = (size_t)n;
    return LSTR_OK;
}

/* Render an unsigned integer to `to` as hex digits (uppercase, no
 * leading zero unless v == 0). */
static int render_uhex(struct lstr_alloc *a, PLstr to, unsigned long v)
{
    char buf[32];
    int  n = 0;
    int  rc;

    if (v == 0) {
        buf[n++] = '0';
    } else {
        while (v > 0 && n < (int)sizeof(buf)) {
            buf[n++] = (char)nibble_to_hex((unsigned)(v & 0xF));
            v >>= 4;
        }
    }

    rc = Lfx(a, to, (size_t)n);
    if (rc != LSTR_OK) return rc;
    {
        int i;
        for (i = 0; i < n; i++) to->pstr[i] = (unsigned char)buf[n - 1 - i];
    }
    to->len = (size_t)n;
    return LSTR_OK;
}

/* ------------------------------------------------------------------ */
/*  C2D / D2C                                                         */
/* ------------------------------------------------------------------ */

int Lc2d(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    unsigned long v = 0;
    size_t i;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    if (from->len > sizeof(unsigned long)) return LSTR_ERR_BADARG;

    for (i = 0; i < from->len; i++) {
        v = (v << 8) | (unsigned long)from->pstr[i];
    }
    return render_udec(a, to, v);
}

int Ld2c(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    unsigned long v;
    int    rc;
    size_t n;
    unsigned long tmp;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = parse_udec(from, &v);
    if (rc != LSTR_OK) return rc;

    /* Count bytes needed (at least 1). */
    n = 0;
    tmp = v;
    do { n++; tmp >>= 8; } while (tmp != 0);

    rc = Lfx(a, to, n);
    if (rc != LSTR_OK) return rc;

    {
        size_t i;
        for (i = 0; i < n; i++) {
            to->pstr[n - 1 - i] = (unsigned char)(v & 0xFF);
            v >>= 8;
        }
    }
    to->len = n;
    return LSTR_OK;
}

/* ------------------------------------------------------------------ */
/*  D2X / X2D                                                         */
/* ------------------------------------------------------------------ */

int Ld2x(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    unsigned long v;
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = parse_udec(from, &v);
    if (rc != LSTR_OK) return rc;
    return render_uhex(a, to, v);
}

int Lx2d(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    unsigned long v;
    int rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;
    rc = parse_uhex(from, &v);
    if (rc != LSTR_OK) return rc;
    return render_udec(a, to, v);
}

/* ------------------------------------------------------------------ */
/*  B2X / X2B                                                         */
/* ------------------------------------------------------------------ */

int Lb2x(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    /* Binary string (0/1/blank) -> hex digits, 4 bits per hex digit.
     * Leading bits are left-padded to a multiple of 4. */
    size_t bits;
    size_t pad;
    size_t i;
    size_t j = 0;
    int    rc;
    size_t out_len;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    bits = 0;
    for (i = 0; i < from->len; i++) {
        unsigned char c = from->pstr[i];
        if (c == ' ' || c == '\t') continue;
        if (c != '0' && c != '1') return LSTR_ERR_BADARG;
        bits++;
    }
    if (bits == 0) {
        rc = Lfx(a, to, 0);
        if (rc != LSTR_OK) return rc;
        to->len = 0;
        return LSTR_OK;
    }

    pad = (4 - (bits % 4)) % 4;
    out_len = (bits + pad) / 4;
    rc = Lfx(a, to, out_len);
    if (rc != LSTR_OK) return rc;

    {
        unsigned nibble = 0;
        size_t   filled = 0;

        /* Virtual left-pad zeros first. */
        filled = pad;

        for (i = 0; i < from->len; i++) {
            unsigned char c = from->pstr[i];
            if (c == ' ' || c == '\t') continue;
            nibble = (nibble << 1) | (unsigned)(c - '0');
            filled++;
            if ((filled & 3) == 0) {
                to->pstr[j++] = nibble_to_hex(nibble);
                nibble = 0;
            }
        }
    }
    to->len = j;
    return LSTR_OK;
}

int Lx2b(struct lstr_alloc *a, PLstr to, const PLstr from)
{
    /* Hex string -> binary bit string, 4 bits per hex digit. Blanks
     * between byte boundaries are allowed on input and discarded. */
    size_t nibbles;
    size_t i;
    int    rc;

    if (to == NULL || from == NULL) return LSTR_ERR_BADARG;

    nibbles = 0;
    for (i = 0; i < from->len; i++) {
        unsigned char c = from->pstr[i];
        if (c == ' ' || c == '\t') continue;
        if (hex_to_nibble(c) < 0) return LSTR_ERR_BADARG;
        nibbles++;
    }

    rc = Lfx(a, to, nibbles * 4);
    if (rc != LSTR_OK) return rc;
    to->len = 0;

    for (i = 0; i < from->len; i++) {
        unsigned char c = from->pstr[i];
        int v;
        int k;
        if (c == ' ' || c == '\t') continue;
        v = hex_to_nibble(c);
        for (k = 3; k >= 0; k--) {
            to->pstr[to->len++] =
                (unsigned char)('0' + ((v >> k) & 1));
        }
    }
    return LSTR_OK;
}
