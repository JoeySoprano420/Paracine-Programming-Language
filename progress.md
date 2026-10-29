Paracine furnished developer-toolchain pass
Compiler build: Development native compiler (not 1.0).
Baseline: Paracine_Supreme_Expanded.cpp (8,933 lines).
Newly implemented and directly tested
- Conservative formatting with lexer token-preservation proof; check/write/print.
- Deterministic ZIP32 packaging of PCIR-generated COFF .obj, PE32+ .exe,
  original .pcn and a machine-readable manifest; CRC32 integrity and no unsafe
  archive names or unbounded ZIP32 offset overflows.
- JSON semantic and compiler errors for check --json, deterministic ordering.
- Per-stage compilation timing (profile) and repeated compiler benchmarking
  (bench), explicitly not source-program runtime profiling.
- init, doctor, version commands with overwrite safety and honest platform
  status.
- VS Code TextMate language grammar, snippets, file association, and tasks,
  but not full LSP/IntelliSense/debugger support.
- CMake portable presets, CPack host package configuration, dedicated developer
  workflow test. Heavy C++ IR harnesses are opt-in by default to avoid repeated
  full-compiler translation-unit builds; their tests are not removed.
Known major release blockers
Full Paracine 1.0 conformance is not achieved: complete path-sensitive SAGE
ownership, recursive owning-aggregate drop glue, dynamic containers and full
stdlib, all scalar widths, SIMD and atomics, arbitrary modules/imports/variants,
complete independent COFF/PE/archive linker, Windows x64 ABI loader tests,
security fuzzing and performance qualification remain to do.

The developer tools do not imply these compiler-core features are complete.
Test evidence from this engineering pass
- Standalone Clang C++23 native compiler build: passed.
- Standalone C++23 syntax check: passed.
- Original six scripted regression suites: 114 + 41 + 58 + 18 + 32 + 139
  = 402/402 assertions, all passed on the updated compiler.
- Newly added furnished_workflows.py: 35/35 assertions, passed.
- CMake Debug (Clang) default host compiler and runtime build: passed with
  PARACINE_BUILD_HEAVY_IR_HARNESSES=OFF.
- Default CTest after build: 8/8 passed sequentially. Native output ran
  under the Microsoft x64 ABI bridge on Linux; genuine Windows loader validation
  was not performed.
- The two extra C++ IR harnesses are optional in the new CMake configuration,
  because each compiles the complete compiler translation unit; they were not
  rerun as part of this pass.
- Clang emitted three preexisting misleading-indentation warnings during the
  CMake build. Warning-free compilation is not claimed.
- GCC and MSVC compiler builds, comprehensive ASan/UBSan, production Windows
  compatibility, and whole-specification conformance were not verified.
