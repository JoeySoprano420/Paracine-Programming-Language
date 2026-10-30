Paracine Ultimate — Verified Native Compiler Extension
Edition: 2026-10-30 engineering pass (not Paracine 1.0 certification)
Deliverable
The complete C++23 compiler implementation extends the genuine Paracine_Premium_Compiler.cpp source and preserves the existing lexer, parser, SAGE/SKM, PCIR, PMIR, Windows-x64 native encoder, COFF/PE32+ generation, runtime and developer commands. The complete project includes the updated compiler in Paracine_Project/src/Paracine.cpp.
New executable operations
- text.codepoint_at(byte_index) -> int: returns a Unicode scalar value. Generated x86-64 checks the unsigned byte boundary, UTF-8 lead/continuation sequences, overlong forms, surrogates and U+10FFFF maximum; invalid indices and malformed values trap. The PCIR interpreter has an independent checked implementation.
- text.codepoint_count() -> int: counts leading UTF-8 bytes (one per scalar) in a valid UTF-8 text. Validity is a text invariant; this is not a general-purpose validation operation for arbitrary binary buffers.
- text.is_ascii() -> bool: native scanning predicate with an early exit on any byte >= 0x80.
- All three operations are implemented in source typing, effect inference, PCIR opcode, verifier, optimizer safety handling, PCIR oracle, PMIR lowering, native x86-64 and tooling stringification.
- New bootstrap demonstration bootstrap/unicode_lexer.pcn performs UTF-8-aware iteration using real Paracine-native instructions.
Fail-closed optional backend
The optional C++20 exporter now raises explicit errors for text literals, closure calls, ownership conversions, rich mutation, PHI nodes and rich aggregate/result/option semantics it cannot legally emit; it no longer generates comment-only substitutes that silently skip behavior. Simple supported integer C++20 export was compiled using the host C++20 compiler and returned the expected exit status of 21.
Verified on this host
- Clean Clang C++23 CMake Debug build: PASS.
- GCC C++23 and Clang C++23 strict warnings-as-errors source syntax: PASS.
- CTest: 17/17 suites passed on Linux x86-64.
- Deterministic generated Unicode regression: 86/86 assertions passed. Includes randomized UTF-8 strings, lead-byte boundaries, all Unicode UTF-8 widths, out-of-range and continuation-byte traps, and compile-time type rejection.
- Optional C++20 exporter: 3/3 safety assertions passed.
- Dedicated Unicode Paracine-written lexer demonstration: -O0, -O1, -O2, -O3 all returned 97 and matched PCIR oracle.
- Patch against Paracine_Premium_Compiler.cpp: applies and reproduces the updated C++23 source byte for byte.
- Tested generated x86-64 instructions with Microsoft-x64 ABI on Linux; Windows/MSVC compilation and Windows PE loader execution were not performed.
Remaining blockers to complete industry-grade 1.0
The complete published Paracine language still cannot be truthfully claimed fully implemented. Missing or incomplete areas include full recursive ownership/drop of aggregate owners, complete dynamic vectors/buffers available in generated .pcn, all module/namespace/capability semantics, all integer widths/atomics/SIMD, full internal COFF/PE linker (arbitrary .obj/`.lib`, import/export, TLS, ASLR/PDB), complete I/O standard library, standalone Windows qualification, security/fuzz coverage of the entire compiler, and a real Paracine-written stage-1 and stage-2 self-build.

No placeholders were used for the newly implemented Unicode native operations. Unsupported optional C++20 exporter constructs fail explicitly instead of emitting false-success code. Remaining language-wide gaps are not filled by this patch.



## *** ##



Build and use Paracine Ultimate
The full compiler is in Paracine_Project/src/Paracine.cpp; the standalone identical source is Paracine_Ultimate.cpp.
Windows / MSVC (reproduction instructions, not host-verified)
cmake -S Paracine_Project -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
build\Debug\paracine.exe test Paracine_Project\bootstrap\unicode_lexer.pcn -O3
The project requires MSVC C++23 support and uses /permissive- /EHsc /utf-8 /Zc:__cplusplus. Do not silently replace the project source with an older local main.cpp. If Visual Studio reports C3530, inspect the actual source path and failing declaration; the tested source does not contain an auto char variable.
Linux build / executed here
cmake -S Paracine_Project -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG='-O0 -g0'
cmake --build build -j2
ctest --test-dir build -j4 --output-on-failure
./build/paracine test Paracine_Project/bootstrap/unicode_lexer.pcn -O3
New Paracine code
routine main() gives int with trap
    let input: text = "λ中🙂"
    if input.codepoint_count() == 3 and input.codepoint_at(2) == 20013 and not input.is_ascii()
        give 97
    give 1
codepoint_at interprets its index as a UTF-8 byte offset, not a Unicode scalar ordinal. For example, "λ".codepoint_at(1) traps because byte 1 is a continuation byte.



## *** ##



