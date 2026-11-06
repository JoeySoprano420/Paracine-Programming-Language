TinyParacine — a compiler written in Paracine
This is a tested Stage-1 subset compiler, not yet the complete self-hosting Paracine 1.0 compiler. stage1/tiny_compiler.pcn contains the compiler's scanner, recursive-descent arithmetic parser, fixed-slot symbol table, type-constrained constant evaluator, folding optimizer, diagnostics and output writer in Paracine. No C++ code implements any of those stages.

The output is optimized Paracine source, which the existing Paracine C++23 Stage-0 compiler then turns into a native Windows x64 PE32+ executable. The PowerShell wrapper supplies disk input/output because the present Paracine Stage-0 standard library does not yet provide a production-ready file I/O interface to Paracine applications.
Input subset
Exactly one zero-argument routine of the following form:
module arithmetic.core
routine main() gives int
    let base: int = 6 * 7
    let adjustment: int = (8 - 5) * 2
    give base + adjustment - 6
- module dotted.name is optional.
- routine main() gives int is mandatory, with one or more four-space-indented statements.
- Zero to four immutable let name: int = expression declarations are permitted before the final give.
- Expressions: decimal integer literals, previously declared local constants, parentheses, unary +/`-`, binary + - * / %; multiplication has higher precedence than addition.
- Expressions are constant-only (there is no dynamic input at runtime). The accepted result and each intermediate are restricted to -2,147,483,647 through +2,147,483,647 to avoid ambiguous overflow and make decimal emission safe.
- One statement per line; no multiline expressions, var, functions with parameters, calls, loops, records, conditionals, floating-point types, I/O inside source being compiled, or imports. Comments are not accepted by this subset parser.
- All source operators have integer semantics and division truncates toward zero. The output native program's exit code is the low Windows exit-status bits of the returned int.
Compiled output
The example above is translated to:
module generated
routine main() gives int
    give 42
Then Stage-0 compiles the generated source to a working PE32+ .exe. This is a genuine compiler pipeline, but it is not yet independently self-hosting: the native code generator/linker remain in Stage-0.
Windows: compile any supported input .pcn
In PowerShell, with the working Paracine executable available on PATH:
powershell -NoProfile -ExecutionPolicy Bypass -File .\Compile-TinyParacine.ps1 `
  -Stage0 Paracine `
  -Source .\examples\constant_math.pcn `
  -Output .\constant_math.exe `
  -Run
The wrapper embeds the source file as a string literal into a temporary copy of the Stage-1 compiler, compiles/runs that compiler, captures its generated .pcn, checks it, and invokes Stage-0 for PE generation. The generated native process returns 42 for the example. A nonzero exit code is expected because the example deliberately returns an integer result, not a success/failure status.

Use your existing Paracine version to confirm the Stage-0 binary is the one you have tested on Windows. The driver requires standard Windows PowerShell and the existing Paracine compiler, no external assembler or LLVM.
Manual compile
Paracine stage1\tiny_compiler.pcn -o tiny_compiler.exe
tiny_compiler.exe > generated.pcn
Paracine generated.pcn -o generated.exe
generated.exe
echo Exit code: %ERRORLEVEL%
By default tiny_compiler.pcn contains an example input yielding 42. To compile another file without editing the embedded input manually, use the PowerShell driver.
Diagnostics
Errors are printed to stderr and terminate the Stage-1 compiler with code 1. Codes include E2 bad routine header, E5 wrong indentation, E8 unknown constant, E10 division by zero, E11 more than four bindings, E12 extra tokens after give, E13 arithmetic outside the supported integer range, E14 duplicate binding. Offsets are zero-based UTF-8 bytes, not Unicode character columns; this compiler intentionally scans ASCII identifiers.
Verification performed
- The Paracine source passes Stage-0's SAGE checks and PCIR verification.
- tiny_compiler.pcn ran via the x64 test-execution path and printed optimized Paracine source.
- Generated generated.pcn passed SAGE and PCIR, and Paracine test returned 42 from the generated x64 and PCIR oracle at O0 and O3.
- Generated Windows AMD64 PE32+ files passed the Stage-0 structural verifier.
- 18 positive/negative fixture programs were executed at O0/O3: 36/36 regression checks passed.
- Windows-native execution of the Stage-1 binary and the PowerShell driver is not tested in this Linux environment; please run on your Windows PC.
Path toward full self-hosting
1. Add proper Paracine file I/O and CLI argument handling to eliminate the PowerShell injection wrapper.
2. Add a real typed AST and runtime expression code generation, not just constant evaluation.
3. Add multiple routines, scopes, parameters, records, conditionals, loops and diagnostics with source spans.
4. Build a Paracine-written PCIR/PMIR backend and PE linker.
5. Stage-0 builds Stage-1; Stage-1 builds Stage-2; Stage-2 rebuilds itself, with differential and binary hash tests.
