# lstring370

**Reentrant, length-prefixed string library for MVS 3.8j with a pluggable allocator.**

`lstring370` provides a rich set of string operations on length-prefixed
auto-growing buffers. It has no global state and no hardcoded allocator
— consumers inject `alloc`/`dealloc` callbacks via `struct lstr_alloc`,
so the same library can sit underneath REXX/370 (routed through
`irxstor`), HTTPD (`malloc`), UFSD (pool allocator), or any other
mvslovers project.

## Design goals

- **Reentrant.** No `static` mutable state, no `extern` mutables, no
  global error callback. The allocator is a parameter, not a global.
- **Embeddable.** Each consumer chooses its own allocator. The default
  allocator (`malloc`/`free` from `<stdlib.h>`) is provided for
  convenience and tests.
- **Memory-conscious.** Length-prefixed buffers, explicit capacity
  growth, no surprise reallocation. Targets MVS 3.8j (24-bit AMODE).
- **EBCDIC-safe.** Character classification via `<ctype.h>` from
  `libc370`. No hardcoded ASCII values in the logic.
- **No REXX type caching.** The `type` field on `Lstr` is always
  `LSTRING_TY` — REXX-specific number caching lives in the rexx370
  adapter (WP-11b), not here.

## Project layout

```
include/
  lstring.h        Lstr struct, macros, function prototypes
  lstralloc.h      pluggable allocator interface
src/
  lstr#cor.c       core: Lfx, Lscpy, Lstrcpy, Lcat, Lstrcat, Lfree,
                   Lupper, Llower
  lstr#sub.c       substring / position
  lstr#wrd.c       word operations
  lstr#srch.c      search / compare
  lstr#xlt.c       translate / strip / space
  lstr#cvt.c       base conversion (c2x, x2c, ...)
  lstr#fmt.c       output / number formatting
test/
  test_lstring.c   unit tests, built with the native compiler (see below)
mbt.toml           mbt 3 build configuration
```

## Build

The library builds entirely on the host with the cc370 toolchain and
[mbt 3](https://github.com/mvslovers/mbt), the `mbt` program installed on
your `PATH`. MVS is not touched at build time.

```sh
mbt build          # cc370 compile + ar370 archive -> build/lstring370.a
mbt package        # release tarball (lib + headers) in dist/
mbt clean          # remove build/ and dist/
mbt doctor         # verify the cc370 toolchain
```

`[toolchain]` in `mbt.toml` pins the mbt, cc370 and libc370 versions a
release is built with.

Consumers (rexx370, httpd, …) pull the library in via mbt dependencies —
`mbt deps` stages `build/lstring370.a` and the public headers automatically.

The unit tests are not yet an mbt test (`mbt.toml` excludes them in
`[tests]`, see issue #9), so `mbt test` runs nothing. Build and run them
with the native compiler:

```sh
gcc -I include -Wall -Wextra -std=gnu99 -o test/test_lstring \
    test/test_lstring.c 'src/lstr#'*.c
./test/test_lstring
```

## Status

Early development. Phase 2 of REXX/370 is the first consumer. See
`https://github.com/mvslovers/lstring370/issues` for open work packages.

## Provenance

The API naming (`Lstr`, `PLstr`, `Lfx`, `Lstrcpy`, `LSTRING_TY`, …) follows
the `lstring` interface of BREXX by Vasilis N. Vlachoudis, which REXX/370's
string handling was modelled on. The implementation is independent: no BREXX
source code is part of this library.

## License

MIT License, Copyright (c) 2026 Mike Großmann. See [LICENSE](LICENSE).
