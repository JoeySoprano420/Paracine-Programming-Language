# Paracine bootstrap workload expansion — 2026-11-05

## Verified incremental result

This is **not** a complete Paracine 1.0 implementation, self-hosted compiler, general-purpose dynamic runtime, production AST, or internal linker. It is an incremental upgrade and runnable native-compiler workload package from the user's newest C++23 source.

### Compiler patch

- **2026-11-05 regression fix:** repaired x64 aggregate/closure rich-field stores by resetting the destination base for each field, preventing cumulative-offset out-of-bounds writes. Added `borrowed_slice_ir_writer.pcn` to the integration suite. Fixed test runner relative compiler path normalization.

- Fixed a **real C++23 build failure**: a duplicate `std::error_code ec` declaration in the package-aware source import resolver at original line 11376. Reuses and clears the existing error code.
- Added genuinely executable **`text.is_empty()` and `string.is_empty()`** intrinsic methods: checked in semantic analysis and lowered through PCIR byte-count/comparison to PMIR and native AMD64, with tests for empty, nonempty, append and clear.
- Added **`text.find_from(needle, start)`**: checked `(text, integer)` arguments, emits a checked byte slice, searches for the needle, and returns an absolute position (or -1). Tested absent matches, offsets and empty needle at end.
- C++23 compiler builds under GCC on Linux x86-64; original built-in tests passed **24/24**.
- `changes.patch` is the exact source diff; these changes modify the actual compiler, not only example programs.

### Runnable Paracine source programs

Each program in `examples` was actually run through `paracine test` on Linux x86-64, comparing generated x64 execution with the PCIR interpreter. The package ships the test runner, inputs, and results.

- `bootstrap_core.pcn`: imported source module with character predicates, ASCII token counting, decimal parsing, symbol search, checksum.
- `bootstrap_driver.pcn`: `use bootstrap_core` verifies source-module resolution, cross-file calls, and native execution.
- `compiler_lexer.pcn`: a small **real lexer** that handles ASCII identifiers, decimal runs, punctuation, whitespace and line comments and builds a dynamic owned token stream. It is not the full Paracine lexical grammar.
- `dynamic_string.pcn`: growable owned `string` with append, clear, `to_text`, count.
- `empty_methods.pcn`: tests both new native is_empty intrinsics for text/string.
- `find_from.pcn`: tests new checked indexed text search intrinsic.
- `dynamic_tokens.pcn`: append-only dynamically sized delimiter-separated token sequence; random access is linear. The delimiter cannot occur inside an element.
- `dynamic_symbols.pcn`: append-only dynamically sized text key/value table with last-write-wins lookup. Keys and values must not contain tabs or newlines; linear search, no deletion.
- `ast_record_array.pcn`: AST-like records stored in indexed fixed arrays.
- `compiler_storage.pcn`: AST + symbol record arrays, hash lookup, indexed tree node evaluation. The arrays have **fixed capacity**, not general dynamic vector semantics.
- `mini_compiler.pcn`: limited numeric declaration parser, checks, AST store, symbol store, and example text IR builder; computes `result=7+11` to 18. It is **not** a standalone native compiler and its illustrative IR text is not a Paracine PCIR serialization format.
- `ir_builder.pcn`: dynamically constructed textual pseudo-IR.
- `hash_test.pcn`: hash smoke test.
- `slice_append_regression.pcn`: supported small nested borrowed-slice append scenario.
- `byte_output.pcn`: native `text.stdout_write` byte-stream output through Win32 calls/isolated execution oracle.

### Binary production and imported source files

The **C++23 Paracine compiler** can write Windows AMD64 COFF `.obj` and PE32+ `.exe` for the supported programs; the test runner generates and structurally validates both kinds. The **C++23 compilation driver** can open local `.pcn` modules via `use`, as demonstrated by `bootstrap_driver.pcn`.

This is **not** the same as exposing a general file read/write API to Paracine user programs or implementing a fully independent Paracine-written linker. The compiler retains its previously limited internal linker and optional external `lld-link` adapter.

### Tests

```sh
g++ -std=c++23 -O0 -w main.cpp -o paracine
./paracine selftest
python3 test_bootstrap.py --compiler ./paracine --report test_results.json
./paracine test examples/mini_compiler.pcn
./paracine obj examples/mini_compiler.pcn -o mini_compiler.obj
./paracine pe examples/mini_compiler.pcn -o mini_compiler.exe
./paracine verify-bin mini_compiler.obj mini_compiler.exe
```

- 24/24 existing compiler selftests passed.
- 21/21 new integration cases passed (14 native-vs-oracle examples including the former regression, 4 expected semantic rejections, COFF write, PE write, and verify-bin). Results are in `test_results.json`.
- `paracine test` uses an isolated Linux x86-64 harness that exercises generated Win64-ABI code and an oracle. It does **not** demonstrate running `.exe` on Windows.

### Resolved native-versus-PCIR corruption: rich aggregate field offsets

`known_failures/borrowed_slice_ir_writer.pcn` is now a permanent **positive regression** (kept in its original directory to preserve its history): native x64 and PCIR both return **18**. In the original x64 encoder, `MakeAggregate` loaded its destination address once before a loop of fields. The rich-field copy helper incremented R11 for an offset and retained that increment for the next field. For three `AstNode` elements (12 bytes each), the third write landed at offset **36** instead of **24**, immediately after the 36-byte array, corrupting the adjacent borrowed text `rhs_digits` (native length changed from 2 to 0). The patch reloads the base pointer **on each field** for both `MakeAggregate` and `MakeClosure`.

**Verification after patch:** C++23 build passed; 24/24 built-in selftests passed; 21/21 integration tests passed including the formerly failing reproducer, COFF/PE generation and structural checks. These tests do not prove all programs or all Windows runtime scenarios agree. Runtime file I/O, independent linking and other feature gaps remain.

### Remaining acceptance gates — NOT complete

1. General dynamic `vector<T>`, maps, deletion, safe growable binary byte buffers, allocator and recursive drop glue.
2. Runtime source-file read/write, directory access, robust Windows and portable I/O interfaces.
3. Arbitrarily large/reusable AST arenas with sound ownership/lifetimes.
4. General symbol interning and compiler-grade hash maps; full SAGE and ownership soundness.
5. Complete semantic analysis and PCIR/PMIR lowering for every Paracine 1.0 construct.
6. Native binary writing from **Paracine-written** code, not only through the C++23 compiler or standard-output byte streams.
7. Full multi-object COFF linker, import-library resolution, relocation application and PE/DLL emission independent of external `lld-link`.
8. End-to-end Windows tests, language conformance, reproducibility, and compiler self-hosting stages 1–3.

The right interpretation is **a tested bootstrap-workload foundation**, not a declaration that all requested systems are complete.




## *** ##



# Rich aggregate native-versus-PCIR mismatch — fixed

## Reproducer

`known_failures/borrowed_slice_ir_writer.pcn`, now an expected passing case.

Before: native x64=-8, PCIR=18. After: native x64=18, PCIR=18.

## Cause

In `PMIROpcode::MakeAggregate`, the destination address register R11 was loaded once outside the field loop. `storeValueInto` adjusts R11 by the field offset for rich aggregate fields. With `array<AstNode,3>` holding 12-byte elements, offsets were mistakenly accumulated: 0, 12, then 12+24=36. The last store wrote just beyond the array into the next rich stack object, a text-slice `{ptr,len}`. The native length was overwritten to 0; the oracle's independent object model did not have this corruption.

`PMIROpcode::MakeClosure` used the same unsafe pattern. Both now reload the base pointer for each field before the potentially offset-adjusting helper.

## Verified

- `g++ -std=c++23 -O0 -w main.cpp -o paracine` succeeds.
- `./paracine test known_failures/borrowed_slice_ir_writer.pcn`: x64=18 and PCIR=18.
- `./paracine selftest`: 24 passed, 0 failed.
- `python3 test_bootstrap.py --compiler ./paracine --report test_results.json`: 21 passed, 0 failed.
- COFF and PE32+ structural verification included in the integration tests.

## Limits

These are Linux x86-64 isolated-machine-code and PCIR-oracle tests. The change does not establish universal semantic equivalence, memory safety for every aggregate, or actual Windows execution validation. Independent Windows testing and further fuzzing remain required.



