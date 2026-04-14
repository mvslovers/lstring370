/* ------------------------------------------------------------------ */
/*  test_lstring.c - lstring370 core unit tests (cross-compile)       */
/*                                                                    */
/*  Build:                                                            */
/*    gcc -I include -Wall -Wextra -std=gnu99 -o test/test_lstring \  */
/*        test/test_lstring.c 'src/lstr#cor.c'                        */
/*                                                                    */
/*  (c) 2026 mvslovers                                                */
/* ------------------------------------------------------------------ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lstring.h"
#include "lstralloc.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) \
    do { \
        tests_run++; \
        if (cond) { \
            tests_passed++; \
            printf("  PASS: %s\n", msg); \
        } else { \
            tests_failed++; \
            printf("  FAIL: %s\n", msg); \
        } \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Tracking allocator (lets us verify allocator injection actually   */
/*  goes through the consumer's callbacks instead of malloc/free)     */
/* ------------------------------------------------------------------ */

struct track_state {
    int alloc_calls;
    int dealloc_calls;
    size_t bytes_live;
};

static void *track_alloc(size_t size, void *ctx)
{
    struct track_state *st = (struct track_state *)ctx;
    void *p = malloc(size);
    if (p != NULL) {
        st->alloc_calls++;
        st->bytes_live += size;
    }
    return p;
}

static void track_dealloc(void *ptr, size_t size, void *ctx)
{
    struct track_state *st = (struct track_state *)ctx;
    if (ptr != NULL) {
        st->dealloc_calls++;
        st->bytes_live -= size;
        free(ptr);
    }
}

/* ------------------------------------------------------------------ */
/*  Tests                                                             */
/* ------------------------------------------------------------------ */

static int lstr_eq_cstr(const Lstr *s, const char *cstr)
{
    size_t n = strlen(cstr);
    if (s->len != n) return 0;
    return memcmp(s->pstr, cstr, n) == 0;
}

static void test_default_alloc(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;
    int rc;

    printf("\n--- Test: default allocator end-to-end ---\n");

    Lzeroinit(&s);
    rc = Lscpy(a, &s, "hello");
    CHECK(rc == LSTR_OK, "Lscpy returns OK");
    CHECK(s.len == 5, "len == 5");
    CHECK(s.maxlen >= 5, "capacity >= 5");
    CHECK(lstr_eq_cstr(&s, "hello"), "content == 'hello'");

    rc = Lcat(a, &s, " world");
    CHECK(rc == LSTR_OK, "Lcat returns OK");
    CHECK(lstr_eq_cstr(&s, "hello world"), "content == 'hello world'");

    Lfree(a, &s);
    CHECK(s.pstr == NULL && s.len == 0 && s.maxlen == 0,
          "Lfree resets the struct");
}

static void test_growth(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;
    int  i;
    int  rc;

    printf("\n--- Test: capacity grows on append ---\n");

    Lzeroinit(&s);
    rc = Lscpy(a, &s, "");
    CHECK(rc == LSTR_OK, "Lscpy of empty returns OK");

    for (i = 0; i < 100; i++) {
        rc = Lcat(a, &s, "ab");
        if (rc != LSTR_OK) break;
    }
    CHECK(i == 100, "100 Lcat calls succeeded");
    CHECK(s.len == 200, "final len == 200");
    CHECK(s.maxlen >= 200, "capacity >= 200");

    /* Spot-check content */
    CHECK(s.pstr[0] == 'a' && s.pstr[1] == 'b' &&
          s.pstr[198] == 'a' && s.pstr[199] == 'b',
          "first and last bytes correct");

    Lfree(a, &s);
}

static void test_strcpy_strcat(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr a_str;
    Lstr b_str;
    int rc;

    printf("\n--- Test: Lstrcpy / Lstrcat ---\n");

    Lzeroinit(&a_str);
    Lzeroinit(&b_str);

    Lscpy(a, &a_str, "foo");
    Lscpy(a, &b_str, "bar");

    rc = Lstrcat(a, &a_str, &b_str);
    CHECK(rc == LSTR_OK, "Lstrcat returns OK");
    CHECK(lstr_eq_cstr(&a_str, "foobar"), "a == 'foobar'");
    CHECK(lstr_eq_cstr(&b_str, "bar"),    "b unchanged");

    rc = Lstrcpy(a, &b_str, &a_str);
    CHECK(rc == LSTR_OK, "Lstrcpy returns OK");
    CHECK(lstr_eq_cstr(&b_str, "foobar"), "b == 'foobar'");
    CHECK(lstr_eq_cstr(&a_str, "foobar"), "a unchanged");

    Lfree(a, &a_str);
    Lfree(a, &b_str);
}

static void test_case(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;

    printf("\n--- Test: Lupper / Llower ---\n");

    Lzeroinit(&s);
    Lscpy(a, &s, "Hello, World!");

    Lupper(&s);
    CHECK(lstr_eq_cstr(&s, "HELLO, WORLD!"), "Lupper");

    Llower(&s);
    CHECK(lstr_eq_cstr(&s, "hello, world!"), "Llower");

    Lfree(a, &s);
}

static void test_injected_allocator(void)
{
    struct track_state st;
    struct lstr_alloc  alloc;
    Lstr s;

    printf("\n--- Test: injected allocator routes through callbacks ---\n");

    st.alloc_calls   = 0;
    st.dealloc_calls = 0;
    st.bytes_live    = 0;

    alloc.alloc   = track_alloc;
    alloc.dealloc = track_dealloc;
    alloc.ctx     = &st;

    Lzeroinit(&s);
    Lscpy(&alloc, &s, "tracked");

    CHECK(st.alloc_calls == 1, "1 alloc call after first Lscpy");
    CHECK(st.dealloc_calls == 0, "0 dealloc calls yet");
    CHECK(st.bytes_live > 0, "live bytes accounted");

    /* Force a regrow by appending a long string. */
    Lcat(&alloc, &s, " and a much longer suffix to force a regrow");

    CHECK(st.alloc_calls >= 2, ">=2 allocs after regrow");
    CHECK(st.dealloc_calls >= 1, ">=1 dealloc from old buffer");

    Lfree(&alloc, &s);

    CHECK(st.bytes_live == 0,
          "all bytes deallocated after Lfree (no leaks)");
    CHECK(st.alloc_calls == st.dealloc_calls,
          "alloc count matches dealloc count");
}

/* ------------------------------------------------------------------ */
/*  lstr#sub.c tests                                                  */
/* ------------------------------------------------------------------ */

static void test_substr(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lsubstr ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "Hello, World");

    Lsubstr(a, &t, &s, 1, 5, ' ');
    CHECK(lstr_eq_cstr(&t, "Hello"), "Lsubstr(1,5) = 'Hello'");

    Lsubstr(a, &t, &s, 8, 5, ' ');
    CHECK(lstr_eq_cstr(&t, "World"), "Lsubstr(8,5) = 'World'");

    Lsubstr(a, &t, &s, 8, LSTR_REST, ' ');
    CHECK(lstr_eq_cstr(&t, "World"), "Lsubstr(8,REST) = 'World'");

    Lsubstr(a, &t, &s, 10, 5, '*');
    CHECK(lstr_eq_cstr(&t, "rld**"), "Lsubstr(10,5) pads past end");

    Lsubstr(a, &t, &s, 20, 3, '.');
    CHECK(lstr_eq_cstr(&t, "..."), "Lsubstr past end is all pad");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_left_right_center(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lleft / Lright / Lcenter ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "abc");

    Lleft(a, &t, &s, 5, '.');
    CHECK(lstr_eq_cstr(&t, "abc.."), "Lleft pad");
    Lleft(a, &t, &s, 2, '.');
    CHECK(lstr_eq_cstr(&t, "ab"),    "Lleft truncate");

    Lright(a, &t, &s, 5, '.');
    CHECK(lstr_eq_cstr(&t, "..abc"), "Lright pad");
    Lright(a, &t, &s, 2, '.');
    CHECK(lstr_eq_cstr(&t, "bc"),    "Lright truncate");

    Lcenter(a, &t, &s, 7, '.');
    CHECK(lstr_eq_cstr(&t, "..abc.."), "Lcenter pad even");
    Lcenter(a, &t, &s, 6, '.');
    CHECK(lstr_eq_cstr(&t, ".abc.."),  "Lcenter pad uneven");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_insert_overlay(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr ins, tgt, t;

    printf("\n--- Test: Linsert / Loverlay ---\n");

    Lzeroinit(&ins); Lzeroinit(&tgt); Lzeroinit(&t);
    Lscpy(a, &tgt, "abcdef");
    Lscpy(a, &ins, "XYZ");

    Linsert(a, &t, &ins, &tgt, 0, ' ');
    CHECK(lstr_eq_cstr(&t, "XYZabcdef"), "Linsert at 0 (start)");

    Linsert(a, &t, &ins, &tgt, 3, ' ');
    CHECK(lstr_eq_cstr(&t, "abcXYZdef"), "Linsert after pos 3");

    Linsert(a, &t, &ins, &tgt, 6, ' ');
    CHECK(lstr_eq_cstr(&t, "abcdefXYZ"), "Linsert after pos 6 (end)");

    Linsert(a, &t, &ins, &tgt, 9, '.');
    CHECK(lstr_eq_cstr(&t, "abcdef...XYZ"),
          "Linsert past end pads target");

    Loverlay(a, &t, &ins, &tgt, 2, ' ');
    CHECK(lstr_eq_cstr(&t, "aXYZef"), "Loverlay at pos 2");

    Loverlay(a, &t, &ins, &tgt, 5, ' ');
    CHECK(lstr_eq_cstr(&t, "abcdXYZ"), "Loverlay extends past end");

    Loverlay(a, &t, &ins, &tgt, 9, '.');
    CHECK(lstr_eq_cstr(&t, "abcdef..XYZ"),
          "Loverlay past end pads gap");

    Lfree(a, &ins); Lfree(a, &tgt); Lfree(a, &t);
}

static void test_delstr(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Ldelstr ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "abcdefghij");

    Ldelstr(a, &t, &s, 4, 3);
    CHECK(lstr_eq_cstr(&t, "abcghij"), "Ldelstr(4,3)");

    Ldelstr(a, &t, &s, 1, 5);
    CHECK(lstr_eq_cstr(&t, "fghij"),   "Ldelstr from start");

    Ldelstr(a, &t, &s, 6, LSTR_REST);
    CHECK(lstr_eq_cstr(&t, "abcde"),   "Ldelstr to end");

    Ldelstr(a, &t, &s, 20, 5);
    CHECK(lstr_eq_cstr(&t, "abcdefghij"),
          "Ldelstr past end leaves string unchanged");

    Lfree(a, &s); Lfree(a, &t);
}

/* ------------------------------------------------------------------ */
/*  lstr#wrd.c tests                                                  */
/* ------------------------------------------------------------------ */

static void test_words(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lwords / Lword ---\n");

    Lzeroinit(&s); Lzeroinit(&t);

    Lscpy(a, &s, "  the quick   brown fox  ");
    CHECK(Lwords(&s) == 4, "Lwords counts 4 words ignoring whitespace");

    Lword(a, &t, &s, 1);
    CHECK(lstr_eq_cstr(&t, "the"),   "Lword(1)='the'");
    Lword(a, &t, &s, 3);
    CHECK(lstr_eq_cstr(&t, "brown"), "Lword(3)='brown'");
    Lword(a, &t, &s, 4);
    CHECK(lstr_eq_cstr(&t, "fox"),   "Lword(4)='fox'");
    Lword(a, &t, &s, 5);
    CHECK(t.len == 0, "Lword(5)=''");

    Lscpy(a, &s, "");
    CHECK(Lwords(&s) == 0, "Lwords('')==0");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_subword(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lsubword ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "the quick brown fox jumps");

    Lsubword(a, &t, &s, 2, 2);
    CHECK(lstr_eq_cstr(&t, "quick brown"), "Lsubword(2,2)");

    Lsubword(a, &t, &s, 3, LSTR_REST);
    CHECK(lstr_eq_cstr(&t, "brown fox jumps"),
          "Lsubword(3,REST)");

    Lsubword(a, &t, &s, 1, 1);
    CHECK(lstr_eq_cstr(&t, "the"), "Lsubword(1,1)");

    Lsubword(a, &t, &s, 10, 5);
    CHECK(t.len == 0, "Lsubword past end is empty");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_word_index_length(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;

    printf("\n--- Test: Lwordindex / Lwordlength ---\n");

    Lzeroinit(&s);
    Lscpy(a, &s, "  ab  cdef gh");

    CHECK(Lwordindex(&s, 1) == 3, "Lwordindex(1)==3");
    CHECK(Lwordindex(&s, 2) == 7, "Lwordindex(2)==7");
    CHECK(Lwordindex(&s, 3) == 12, "Lwordindex(3)==12");
    CHECK(Lwordindex(&s, 4) == 0, "Lwordindex(4)==0 (none)");

    CHECK(Lwordlength(&s, 1) == 2, "Lwordlength(1)==2");
    CHECK(Lwordlength(&s, 2) == 4, "Lwordlength(2)==4");
    CHECK(Lwordlength(&s, 4) == 0, "Lwordlength(4)==0");

    Lfree(a, &s);
}

static void test_wordpos(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr p, s;

    printf("\n--- Test: Lwordpos ---\n");

    Lzeroinit(&p); Lzeroinit(&s);
    Lscpy(a, &s, "now is the time for all good men");

    Lscpy(a, &p, "the time");
    CHECK(Lwordpos(&p, &s, 1) == 3, "Lwordpos('the time')==3");

    Lscpy(a, &p, "all good");
    CHECK(Lwordpos(&p, &s, 1) == 6, "Lwordpos('all good')==6");

    Lscpy(a, &p, "no match");
    CHECK(Lwordpos(&p, &s, 1) == 0, "Lwordpos('no match')==0");

    Lscpy(a, &p, "the time");
    CHECK(Lwordpos(&p, &s, 4) == 0, "Lwordpos with start past hit");

    Lfree(a, &p); Lfree(a, &s);
}

static void test_delword(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Ldelword ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "the quick brown fox");

    Ldelword(a, &t, &s, 2, 1);
    CHECK(lstr_eq_cstr(&t, "the brown fox"),
          "Ldelword(2,1) drops 'quick'");

    Ldelword(a, &t, &s, 2, 2);
    CHECK(lstr_eq_cstr(&t, "the fox"),
          "Ldelword(2,2) drops 'quick brown'");

    Ldelword(a, &t, &s, 2, LSTR_REST);
    CHECK(lstr_eq_cstr(&t, "the"),
          "Ldelword(2,REST) drops to end and trims trailing blanks");

    Lfree(a, &s); Lfree(a, &t);
}

/* ------------------------------------------------------------------ */
/*  lstr#srch.c tests                                                 */
/* ------------------------------------------------------------------ */

static void test_pos_lastpos(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr needle, hay;

    printf("\n--- Test: Lpos / Llastpos ---\n");

    Lzeroinit(&needle); Lzeroinit(&hay);
    Lscpy(a, &hay, "abracadabra");

    Lscpy(a, &needle, "bra");
    CHECK(Lpos(&needle, &hay, 1) == 2, "Lpos('bra')==2");
    CHECK(Lpos(&needle, &hay, 3) == 9, "Lpos('bra', start=3)==9");
    CHECK(Llastpos(&needle, &hay, 0) == 9, "Llastpos('bra')==9");
    CHECK(Llastpos(&needle, &hay, 8) == 2, "Llastpos('bra', start=8)==2");

    Lscpy(a, &needle, "xyz");
    CHECK(Lpos(&needle, &hay, 1) == 0, "Lpos('xyz')==0 (miss)");
    CHECK(Llastpos(&needle, &hay, 0) == 0, "Llastpos('xyz')==0");

    Lscpy(a, &needle, "");
    CHECK(Lpos(&needle, &hay, 1) == 0, "Lpos('')==0 by convention");

    Lfree(a, &needle); Lfree(a, &hay);
}

static void test_verify(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr from, ref;

    printf("\n--- Test: Lverify ---\n");

    Lzeroinit(&from); Lzeroinit(&ref);
    Lscpy(a, &from, "12.34");
    Lscpy(a, &ref,  "0123456789");

    CHECK(Lverify(&from, &ref, LVERIFY_NOMATCH, 1) == 3,
          "Lverify(NOMATCH) finds '.' at pos 3");
    CHECK(Lverify(&from, &ref, LVERIFY_MATCH, 1) == 1,
          "Lverify(MATCH) finds digit at pos 1");

    Lscpy(a, &from, "12345");
    CHECK(Lverify(&from, &ref, LVERIFY_NOMATCH, 1) == 0,
          "Lverify(NOMATCH) all digits returns 0");

    Lfree(a, &from); Lfree(a, &ref);
}

static void test_abbrev_compare(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr l, info, s1, s2;

    printf("\n--- Test: Labbrev / Lcompare ---\n");

    Lzeroinit(&l); Lzeroinit(&info);
    Lzeroinit(&s1); Lzeroinit(&s2);

    Lscpy(a, &l, "PRINT");
    Lscpy(a, &info, "PR");
    CHECK(Labbrev(&l, &info, 0) != 0, "'PR' is an abbrev of 'PRINT'");
    CHECK(Labbrev(&l, &info, 3) == 0, "'PR' needs length>=3 -> no");

    Lscpy(a, &info, "PRO");
    CHECK(Labbrev(&l, &info, 0) == 0, "'PRO' is not an abbrev of 'PRINT'");

    Lscpy(a, &s1, "abc  ");
    Lscpy(a, &s2, "abc");
    CHECK(Lcompare(&s1, &s2, ' ') == 0,
          "Lcompare equal with pad=' '");

    Lscpy(a, &s2, "abcd");
    CHECK(Lcompare(&s1, &s2, ' ') == 4,
          "Lcompare finds difference at pos 4");

    Lfree(a, &l);  Lfree(a, &info);
    Lfree(a, &s1); Lfree(a, &s2);
}

/* ------------------------------------------------------------------ */
/*  lstr#xlt.c tests                                                  */
/* ------------------------------------------------------------------ */

static void test_translate(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr from, tableo, tablei, t;

    printf("\n--- Test: Ltranslate ---\n");

    Lzeroinit(&from); Lzeroinit(&tableo); Lzeroinit(&tablei); Lzeroinit(&t);
    Lscpy(a, &from, "Hello, World!");

    /* default = uppercase */
    Ltranslate(a, &t, &from, NULL, NULL, ' ');
    CHECK(lstr_eq_cstr(&t, "HELLO, WORLD!"), "Ltranslate default = upper");

    /* ROT-13-ish: swap a<->A using tableo/tablei */
    Lscpy(a, &from, "abc");
    Lscpy(a, &tableo, "XYZ");
    Lscpy(a, &tablei, "abc");
    Ltranslate(a, &t, &from, &tableo, &tablei, '?');
    CHECK(lstr_eq_cstr(&t, "XYZ"), "Ltranslate table mapping");

    /* char not in tablei is passed through */
    Lscpy(a, &from, "abcd");
    Ltranslate(a, &t, &from, &tableo, &tablei, '?');
    CHECK(lstr_eq_cstr(&t, "XYZd"), "Ltranslate passes through unmapped");

    Lfree(a, &from);   Lfree(a, &tableo);
    Lfree(a, &tablei); Lfree(a, &t);
}

static void test_strip_space(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lstrip / Lspace ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "   hello   ");

    Lstrip(a, &t, &s, LSTRIP_BOTH, ' ');
    CHECK(lstr_eq_cstr(&t, "hello"), "Lstrip BOTH");
    Lstrip(a, &t, &s, LSTRIP_LEADING, ' ');
    CHECK(lstr_eq_cstr(&t, "hello   "), "Lstrip LEADING");
    Lstrip(a, &t, &s, LSTRIP_TRAILING, ' ');
    CHECK(lstr_eq_cstr(&t, "   hello"), "Lstrip TRAILING");

    Lscpy(a, &s, "  the   quick brown    fox  ");
    Lspace(a, &t, &s, 1, ' ');
    CHECK(lstr_eq_cstr(&t, "the quick brown fox"),
          "Lspace n=1 normalises whitespace");
    Lspace(a, &t, &s, 2, ' ');
    CHECK(lstr_eq_cstr(&t, "the  quick  brown  fox"),
          "Lspace n=2 double spacing");
    Lspace(a, &t, &s, 0, '-');
    CHECK(lstr_eq_cstr(&t, "thequickbrownfox"),
          "Lspace n=0 joins words");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_copies_reverse(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lcopies / Lreverse ---\n");

    Lzeroinit(&s); Lzeroinit(&t);
    Lscpy(a, &s, "ab");

    Lcopies(a, &t, &s, 3);
    CHECK(lstr_eq_cstr(&t, "ababab"), "Lcopies(3)");

    Lcopies(a, &t, &s, 0);
    CHECK(t.len == 0, "Lcopies(0) is empty");

    Lscpy(a, &s, "abcdef");
    Lreverse(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "fedcba"), "Lreverse");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_changestr_countstr(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, old_str, new_str, t;

    printf("\n--- Test: Lchangestr / Lcountstr ---\n");

    Lzeroinit(&s);       Lzeroinit(&old_str);
    Lzeroinit(&new_str); Lzeroinit(&t);

    Lscpy(a, &s, "banana bandana");
    Lscpy(a, &old_str, "an");
    CHECK(Lcountstr(&old_str, &s) == 4, "Lcountstr('an' in 'banana bandana')==4");

    Lscpy(a, &new_str, "AN");
    Lchangestr(a, &t, &s, &old_str, &new_str);
    CHECK(lstr_eq_cstr(&t, "bANANa bANdANa"), "Lchangestr 'an'->'AN'");

    /* Grow: replace short with long */
    Lscpy(a, &new_str, "XYZW");
    Lchangestr(a, &t, &s, &old_str, &new_str);
    CHECK(lstr_eq_cstr(&t, "bXYZWXYZWa bXYZWdXYZWa"),
          "Lchangestr growth");

    /* Shrink: replace with empty */
    Lscpy(a, &new_str, "");
    Lchangestr(a, &t, &s, &old_str, &new_str);
    CHECK(lstr_eq_cstr(&t, "ba bda"), "Lchangestr shrink (delete pattern)");

    /* No match */
    Lscpy(a, &old_str, "zz");
    Lchangestr(a, &t, &s, &old_str, &new_str);
    CHECK(lstr_eq_cstr(&t, "banana bandana"),
          "Lchangestr no match = copy");

    Lfree(a, &s);       Lfree(a, &old_str);
    Lfree(a, &new_str); Lfree(a, &t);
}

/* ------------------------------------------------------------------ */
/*  lstr#cvt.c tests                                                  */
/* ------------------------------------------------------------------ */

static void test_c2x_x2c(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lc2x / Lx2c ---\n");

    Lzeroinit(&s); Lzeroinit(&t);

    /* Build a byte sequence without embedding non-portable literals. */
    {
        unsigned char data[3];
        data[0] = 0x00;
        data[1] = 0x7F;
        data[2] = 0xA5;
        Lfx(a, &s, 3);
        memcpy(s.pstr, data, 3);
        s.len = 3;
    }

    Lc2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "007FA5"), "Lc2x produces hex digits");

    /* Round-trip: back through Lx2c */
    {
        Lstr rt;
        Lzeroinit(&rt);
        Lx2c(a, &rt, &t);
        CHECK(rt.len == 3 &&
              rt.pstr[0] == 0x00 &&
              rt.pstr[1] == 0x7F &&
              rt.pstr[2] == 0xA5,
              "Lx2c round-trips");
        Lfree(a, &rt);
    }

    /* Lx2c with blanks and odd-digit error */
    {
        Lstr hex;
        Lstr out;
        Lzeroinit(&hex); Lzeroinit(&out);
        Lscpy(a, &hex, "01 02 03");
        Lx2c(a, &out, &hex);
        CHECK(out.len == 3 && out.pstr[0] == 1 &&
              out.pstr[1] == 2 && out.pstr[2] == 3,
              "Lx2c accepts blanks between bytes");

        Lscpy(a, &hex, "ABC");   /* odd number of hex digits */
        CHECK(Lx2c(a, &out, &hex) == LSTR_ERR_BADARG,
              "Lx2c rejects odd hex digit count");

        Lfree(a, &hex); Lfree(a, &out);
    }

    Lfree(a, &s); Lfree(a, &t);
}

static void test_c2d_d2c(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lc2d / Ld2c ---\n");

    Lzeroinit(&s); Lzeroinit(&t);

    {
        unsigned char data[2];
        data[0] = 0x01;
        data[1] = 0x00;
        Lfx(a, &s, 2);
        memcpy(s.pstr, data, 2);
        s.len = 2;
    }
    Lc2d(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "256"), "Lc2d(01 00) = 256");

    Lscpy(a, &s, "65");
    Ld2c(a, &t, &s);
    CHECK(t.len == 1 && t.pstr[0] == 65, "Ld2c('65') = 1 byte 0x41");

    Lscpy(a, &s, "256");
    Ld2c(a, &t, &s);
    CHECK(t.len == 2 && t.pstr[0] == 1 && t.pstr[1] == 0,
          "Ld2c('256') = 2 bytes 01 00");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_d2x_x2d(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Ld2x / Lx2d ---\n");

    Lzeroinit(&s); Lzeroinit(&t);

    Lscpy(a, &s, "255");
    Ld2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "FF"), "Ld2x(255)=FF");
    Lscpy(a, &s, "65535");
    Ld2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "FFFF"), "Ld2x(65535)=FFFF");

    Lscpy(a, &s, "DEAD");
    Lx2d(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "57005"), "Lx2d(DEAD)=57005");

    Lfree(a, &s); Lfree(a, &t);
}

static void test_b2x_x2b(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s, t;

    printf("\n--- Test: Lb2x / Lx2b ---\n");

    Lzeroinit(&s); Lzeroinit(&t);

    Lscpy(a, &s, "1111 0000");
    Lb2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "F0"), "Lb2x('1111 0000')=F0");

    Lscpy(a, &s, "11001010");
    Lb2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "CA"), "Lb2x('11001010')=CA");

    /* Odd bit count (not multiple of 4) is left-padded with zeros */
    Lscpy(a, &s, "110");
    Lb2x(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "6"), "Lb2x('110')=6 (left-padded)");

    Lscpy(a, &s, "F0");
    Lx2b(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "11110000"), "Lx2b(F0)=11110000");

    Lscpy(a, &s, "CA FE");
    Lx2b(a, &t, &s);
    CHECK(lstr_eq_cstr(&t, "1100101011111110"),
          "Lx2b(CA FE)=11001010 11111110");

    Lfree(a, &s); Lfree(a, &t);
}

/* ------------------------------------------------------------------ */
/*  lstr#fmt.c tests                                                  */
/* ------------------------------------------------------------------ */

static void test_lprint(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;
    FILE *fp;
    char  buf[64];
    size_t got;
    int    rc;

    printf("\n--- Test: Lprint ---\n");

    Lzeroinit(&s);
    Lscpy(a, &s, "hello world");

    fp = tmpfile();
    if (fp == NULL) {
        CHECK(0, "tmpfile() available");
        Lfree(a, &s);
        return;
    }

    rc = Lprint(fp, &s);
    CHECK(rc == 11, "Lprint returns bytes-written");

    rewind(fp);
    got = fread(buf, 1, sizeof(buf), fp);
    CHECK(got == 11 && memcmp(buf, "hello world", 11) == 0,
          "Lprint wrote exactly the Lstr bytes (no NUL, no extras)");

    fclose(fp);
    Lfree(a, &s);
}

static void test_bad_args(void)
{
    struct lstr_alloc *a = lstr_default_alloc();
    Lstr s;

    printf("\n--- Test: NULL / bad-arg handling ---\n");

    Lzeroinit(&s);
    CHECK(Lfx(NULL, &s, 10) == LSTR_ERR_BADARG, "Lfx(NULL alloc)");
    CHECK(Lfx(a, NULL, 10) == LSTR_ERR_BADARG, "Lfx(NULL str)");
    CHECK(Lscpy(a, &s, NULL) == LSTR_ERR_BADARG, "Lscpy(NULL src)");
    CHECK(Lscpy(a, NULL, "x") == LSTR_ERR_BADARG, "Lscpy(NULL str)");
    CHECK(Lstrcpy(a, &s, NULL) == LSTR_ERR_BADARG, "Lstrcpy(NULL from)");

    /* Lfree on zero-init is a no-op */
    Lfree(a, &s);
    CHECK(s.pstr == NULL, "Lfree on zero-init is safe");
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== lstring370 core tests ===\n");

    test_default_alloc();
    test_growth();
    test_strcpy_strcat();
    test_case();
    test_injected_allocator();
    test_substr();
    test_left_right_center();
    test_insert_overlay();
    test_delstr();
    test_words();
    test_subword();
    test_word_index_length();
    test_wordpos();
    test_delword();
    test_pos_lastpos();
    test_verify();
    test_abbrev_compare();
    test_translate();
    test_strip_space();
    test_copies_reverse();
    test_changestr_countstr();
    test_c2x_x2c();
    test_c2d_d2c();
    test_d2x_x2d();
    test_b2x_x2b();
    test_lprint();
    test_bad_args();

    printf("\n=== Results: %d/%d passed",
           tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
