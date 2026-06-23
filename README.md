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
  `crent370`. No hardcoded ASCII values in the logic.
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
  ... more in follow-up PRs (sub, wrd, srch, xlt, cvt, fmt)
test/
  test_lstring.c   cross-compile unit tests (Linux/gcc)
project.toml       mbt build configuration
```

## Build

The library builds entirely on the host with the cc370 toolchain (mbt v2);
MVS is not touched at build time.

```sh
make lib           # cc370 compile + ar370 archive -> build/lstring370.a
make package       # release tarball (lib + headers) in dist/
make doctor        # verify the cc370 toolchain
```

Consumers (rexx370, httpd, …) pull the library in via mbt dependencies —
`make deps` stages `build/lstring370.a` and the public headers automatically.

Cross-compile the host unit tests with the native compiler:

```sh
gcc -I include -Wall -Wextra -std=gnu99 -o test/test_lstring \
    test/test_lstring.c 'src/lstr#'*.c
./test/test_lstring
```

## Status

Early development. Phase 2 of REXX/370 is the first consumer. See
`https://github.com/mvslovers/lstring370/issues` for open work packages.
