Paracine Enhanced — tested compiler implementation
Baseline
This is a source-preserving expansion of the user-supplied corrected Paracine_Ultimate.cpp (10,708 physical lines), not a new compiler written from scratch. The updated implementation is 10,832 lines of C++23. The bundled CMake project preserves the existing Paracine architecture, runtime, examples, and tests.
New executable features
text.compare(other: text) -> int
Lexicographic ordering by unsigned UTF-8 bytes. Returns -1 if less, 0 if equal, and 1 if greater. This is bytewise ordering, not locale-sensitive/collation or Unicode normalization. Implemented in semantic analysis, source/IR type inference, PCIR, PCIR verification and oracle, PMIR, native Win64 x86-64 instruction encoder, optimizer classification and IR listings. It works with empty strings and multibyte UTF-8 data.
text.rfind(needle: text) -> int
Reverse substring search. Returns the last matching byte offset, or -1 if absent. For empty needle, returns the haystack byte length. Full semantic → PCIR → PMIR → native execution path. Native encoder performs a reverse candidate-offset search without calling the host standard library. PCIR oracle uses std::string::rfind as an independent reference.
text.contains(needle: text) -> bool
Lowers to the existing executable TextFindText PCIR search followed by an integer comparison against -1. No added runtime dependency or handwaved native opcode. The return value is type-checked Boolean.
Test evidence (this environment)
- Clean CMake project configured and built under Clang C++23.
- ctest -j4: 18/18 CTest suites passed (17 existing + 1 new suite).
- New regression suite calculates 660 expected results independently in Python (300 fixed cross-product cases + 360 deterministic randomized cases) and exercises generated x86-64 against the reference at -O0 and -O3.
- Additional dedicated source programs pass at -O0, -O1, -O2, -O3 (PCIR oracle and generated x64 values match).
- New suite also verifies three incorrectly typed method calls are rejected.
- GCC and Clang pass -std=c++23 -Wall -Wextra -Wpedantic -Werror -fsyntax-only.
- Unified source patch tested against the exact uploaded baseline.
Build and test
cmake -S Paracine_Project -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build -j 4 --output-on-failure
Native execution checks on this Linux/x86-64 host use the compiler's Microsoft-x64-ABI test facility. Windows PE loading and direct MSVC compilation were not run here. The user previously encountered a C3530 error in a different local main.cpp; the corrected uploaded baseline did not reproduce it under the available compilers. Use the included full replacement source rather than mixing sections from older editions.
Known limits
This is a tested incremental expansion, not proof of complete language conformance, complete automatic recursive cleanup, full linker functionality, or self-hosting. Those major requirements remain outstanding in the underlying compiler. The three new text primitives do not change that status.
