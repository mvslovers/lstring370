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
    test_bad_args();

    printf("\n=== Results: %d/%d passed",
           tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
