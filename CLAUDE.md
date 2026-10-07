# CLAUDE.md — lstring370 Project Instructions

## What is this project?

`lstring370` is a standalone, reentrant string library for MVS 3.8j
with a pluggable allocator. It is the foundation for string handling
in REXX/370 and is also intended for HTTPD, FTPD, UFSD, and any other
mvslovers project that needs richer string operations than the
libc370 `<string.h>`.

The library is a **clean reimplementation** of the BREXX/370 `lstring`
subsystem with:

- No global state — the allocator is passed as a parameter
- No `Lerror` callback — errors via return codes
- No REXX type caching — `type` is always `LSTRING_TY`
- EBCDIC-safe via `<ctype.h>` from libc370

## Build system

- **Compiler:** cc370 host toolchain (`cc370` / `as370` / `ar370`;
  compile + archive run on the host, no MVS round-trip)
- **C library:** libc370, supplied by the cc370 sysroot (`-lc`) —
  `malloc` / `free`, `<string.h>`, `<ctype.h>`, MVS APIs. It ships with
  the toolchain, so it is *not* a declared dependency.
- **Build tool:** mbt 3 — the installed `mbt` program, reading
  `mbt.toml` (schema 3). No submodule, no `Makefile`, no `VERSION`:
  the version lives in `[project] version` only.
- **Toolchain pins:** `[toolchain]` in `mbt.toml` names the mbt, cc370
  and libc370 versions; `release3.yml` builds with them, `build3.yml`
  floats on the tip of cc370 / libc370.
- **Kind:** `library` — `mbt build` builds the static archive
  `build/lstring370.a`, `mbt package` the release tarball; consumers
  pull it in via mbt dependencies (`mbt deps` stages the `.a` plus the
  public headers). There are no load modules, so `mbt deploy` does not
  apply.
- **Tests:** `test/test_lstring.c` is excluded in `[tests]` and built
  with the native compiler for now; making it an mbt test is issue #9.
- **Target:** MVS 3.8j, AMODE 24, RMODE 24, RENT

## File naming convention

Source files use `lstr#xxxx.c` (mapping to PDS member `LSTR#XXXX`).
Headers use plain names in `include/`.

| Source            | PDS member | Functions |
|-------------------|------------|-----------|
| `lstr#cor.c`      | `LSTR#COR` | core: `Lfx`, `Lscpy`, `Lstrcpy`, `Lcat`, `Lstrcat`, `Lfree`, `Lupper`, `Llower` |
| `lstr#sub.c`      | `LSTR#SUB` | substring/position |
| `lstr#wrd.c`      | `LSTR#WRD` | word operations |
| `lstr#srch.c`     | `LSTR#SRCH` | search/compare |
| `lstr#xlt.c`      | `LSTR#XLT` | translate/strip/space |
| `lstr#cvt.c`      | `LSTR#CVT` | base conversion (c2x, x2c, …) |
| `lstr#fmt.c`      | `LSTR#FMT` | output / number formatting |

## Cardinal rules

1. **No globals.** Every function takes the allocator (or a
   pre-allocated `Lstr`) as a parameter. The allocator pointer is
   never stored in a `static` variable. Multiple consumers with
   different allocators must be able to use the library from the
   same address space.

2. **Allocator only via `lstr_alloc`.** `lstr#cor.c` is the only
   place where `(*alloc->alloc)(...)` and `(*alloc->dealloc)(...)`
   are called. Everything else routes through `Lfx` / `Lfree`.

3. **No POSIX, no dynamic linking.** Strict 24-bit. No `mmap`,
   no `pthread`, no `fork`. No standard Unix paths.

4. **Strict gnu99 (C89-compatible).** Compiled with cc370
   (GCC 3.4.6 `i370` cross-compiler). `-Wall -Werror` is enforced.

5. **EBCDIC.** Use `isalpha`, `isdigit`, `isalnum`, `isspace`,
   `toupper`, `tolower` from `<ctype.h>`. Never compare against
   raw byte values like `0x41` for `'A'`.

6. **Function-name 8-char collisions.** `c2asm370` truncates
   external C symbols to 8 characters, replacing `_` with `@`.
   Public function names must produce unique 8-char asm names,
   or use `asm("ALIAS")` aliases in the header.

   The convention here is `Lxxxx` (capital L + lowercase verb,
   ≤7 chars) — naturally unique under the 8-char rule:
   `Lfx`, `Lscpy`, `Lstrcpy`, `Lcat`, `Lstrcat`, `Lfree`,
   `Lupper`, `Llower`, `Lsubstr`, `Lword`, `Lpos`, `Lc2x`, …

   `Lstrcpy` is 8 chars exactly, fine. `Lstrcat` likewise. If a
   future name would collide, add an `asm()` alias in `lstring.h`.

7. **No comments mentioning AI tools.** English comments,
   describe the *why* not the *what*.

## Knowledge sources

- `brexx370/lstring/*.c` and `brexx370/inc/lstring.h` — algorithms
  for string operations, but reimplemented clean (no globals)
- SC28-1883-0 — REXX semantics for the functions that mirror REXX
  built-ins (`SUBSTR`, `WORD`, `TRANSLATE`, …)

Comments and documentation in English. German only for user-facing
docs (manual etc.).
