# Paracine Stage 1 v0.4.1 — expanded Paracine-native compiler frontend

This release builds on the previously working `ParacineCompiler.pcn` and adds **many real, tested capabilities**. The frontend and `build`/`run` CLI are written entirely in Paracine. It emits executable-ready Paracine source; Stage 0 (C++23 bootstrap) still supplies the optimized native x64 COFF/PE32+ backend and synchronous Windows process launch.

## New in this release

- Declaration and routine-signature types: `int`, `bool`, `text`, `string`, `long`, `float`, `double`, `char`, `byte`, `short`, `ubyte`, `ushort`, `uint`, `ulong`, `size`, and `ssize`; `void` recognized in type grammar (not all void-body forms are in strict mode).
- Typed local declarations and return expressions, typed forward-call return lookup, and strict mode's 32-binding lexical-scope table.
- Literal strings and characters; floating-point/exponent syntax; `.to_string()`, `.to_text()`, `.count()`, `.to_int()`, `.equals()`, `.starts_with()`, `.ends_with()`, `.contains()`, `.byte_at()`, `.codepoint_at()`, and `.slice()` in the supported combinations.
- `std.io.println(value)` and `std.io.print(value)` for **text expressions**, not only inline string literals.
- Native file API call expressions (`std.fs.exists`, `std.fs.read_text`, `std.fs.write_text`), plus `std.io.read_all` and process launch via the Stage-0 runtime.
- New compound integer assignments (`&=`, `|=`, `^=`, `<<=`, `>>=`).
- Runtime `for item in [integer, literals]`, `from ... to`, `continue until`, and existing runtime loops/conditionals.
- Explicit input routine `with` effect clauses are now preserved exactly, so Stage 0 enforces their contracts. Unannotated strict-mode routines receive `io, alloc, trap` by default.
- **`--stage0-full` compatibility mode:** preserve full source and invoke Stage 0 for parsing, comprehensive checking of its current language coverage, PCIR/PMIR and native compilation. Examples with record construction and array iteration run successfully through this path.

See `FEATURE_COVERAGE.md` for feature boundaries. Strict-mode parsing is deliberately not represented as a complete Paracine 1.0 implementation.

## Get started on Windows

Open an **x64 Native Tools Command Prompt for Visual Studio**, extract this archive, and run:

```bat
Build-And-Test.cmd
```

If your installed Stage 0 already supports `std.fs.read_text`, `std.fs.write_text`, `std.fs.exists`, and `std.process.run`:

```bat
Build-With-Your-Stage0.cmd
```

Or use the included precompiled `ParacineCompiler.exe` with your previously built compatible Stage-0 compiler:

```bat
ParacineCompiler.exe check examples\feature_02.pcn
ParacineCompiler.exe emit examples\feature_02.pcn -o feature_02.generated.pcn
ParacineCompiler.exe build examples\feature_02.pcn -o feature_02.exe --stage0 "C:\path\to\Paracine.exe" -O3
ParacineCompiler.exe run examples\feature_02.pcn -o feature_02.exe --stage0 "C:\path\to\Paracine.exe" -O3
echo Program returned: %ERRORLEVEL%
```

The strict example returns **42**. The compiler also retains the original `tokens` command and can accept `-O0` as well as `-O3`.

To compile a more sophisticated program using the **full set of features implemented in Stage 0**, use the explicit compatibility flag:

```bat
ParacineCompiler.exe check examples\advanced_record_array.pcn --stage0-full --stage0 "C:\path\to\Paracine.exe"
ParacineCompiler.exe run examples\advanced_record_array.pcn -o advanced.exe --stage0-full --stage0 "C:\path\to\Paracine.exe"
echo Program returned: %ERRORLEVEL%
```

This example uses a record, a fixed array, record-field access, and array iteration, and returns **42**. It does **not** mean those constructs have been ported into the strict Stage-1 parser; they are compiled by Stage 0 through the Stage-1 driver.

## Subcommands

| Command | Effect |
|---|---|
| `tokens file.pcn` | Native Paracine-lexer token dump |
| `check file.pcn` | Stage-1 lexer, parser and supported type checks |
| `check file.pcn --stage0-full` | Delegate verification to Stage 0 |
| `emit file.pcn -o generated.pcn` | Strict Stage-1 translation and emission |
| `emit file.pcn --stage0-full -o copied.pcn` | Exact content forwarding (no Stage-1 translation) |
| `build file.pcn -o program.exe` | Translate and compile with Stage 0 to PE32+ |
| `run file.pcn -o program.exe` | Compile, execute, and propagate exit code |
| `build/run ... --stage0-full` | Delegate all syntax and native backend work to Stage 0 |

For `build/run`, the file `program.exe.stage1.pcn` remains available as an intermediate inspection artifact. The compiled program's **nonzero exit code is not automatically a compiler failure**; in the included tests, exit code `42` is intentional.

## Validation and limits

- The Paracine Stage-1 source passes Stage-0 syntax, SAGE/SKM, and PCIR verification.
- 91/91 Stage-0 self-tests passed with the unchanged bootstrap compiler.
- 34/34 targeted native x64 checks passed at O0 and O3: lexer, legacy parser, new parser, and 14 emitted programs. All 14 emitted programs returned expected exit code 42 at each optimization level.
- Windows PE32+ structural verification passed for the newly compiled Stage-1 `.exe`.
- The execution tests were performed in a Linux-hosted Win64 ABI native-code harness; **the newly built `.exe` still needs to be verified with the Windows loader on your computer**.

The tool remains **Stage-1 frontend + Stage-0 backend**, not a single self-hosting compiler. Implementing independently typed AST, SAGE/SKM, PCIR/PMIR, register allocation, x64 encoding, COFF, and PE32+ in Paracine is separate substantial work.
