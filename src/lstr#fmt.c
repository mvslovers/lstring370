/* ------------------------------------------------------------------ */
/*  lstr#fmt.c - generic output                                       */
/*                                                                    */
/*  Currently contains only Lprint(). The REXX FORMAT() built-in      */
/*  (Lformat) depends on NUMERIC DIGITS / NUMERIC FORM settings of    */
/*  the running REXX environment and is therefore implemented in     */
/*  the rexx370 arithmetic engine (WP-20), not here.                  */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <stdio.h>

#include "lstring.h"

int Lprint(FILE *stream, const PLstr s)
{
    size_t written;

    if (stream == NULL || s == NULL) return -1;
    if (s->len == 0) return 0;

    written = fwrite(s->pstr, 1, s->len, stream);
    if (written != s->len) return -1;
    return (int)written;
}
