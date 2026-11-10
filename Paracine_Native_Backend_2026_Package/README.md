# Paracine 2026 — Native Backend Optimization Build

**Implementation:** C++23, single-file native compiler.  
**Canonical target:** Windows x86-64, Microsoft x64 ABI; `.obj` COFF and `.exe` PE32+.  
**Pipeline:** `.pcn` → SAGE → PCIR → optimized PCIR → PMIR → x86-64 machine code → COFF/PE32+.  
**Status:** working enhanced development compiler, not a complete or certified Paracine 1.0 implementation.

This package is an updated compiler source, **not** merely a design specification. It incorporates all changes from the prior frontend expansion and the backend improvements described here.

## What was implemented

1. **Compact x64 immediate materialization.** Choose 5-byte `mov r32,imm32` for zero-extendable immediates, 7-byte `mov r64,signextended_imm32` for signed small negatives, and 10-byte `movabs` for everything else. These forms preserve EFLAGS, unlike `xor reg,reg`; high-bit 64-bit constants remain intact.
2. **Short stack-memory addressing.** Emit disp8 where possible for RBP-relative loads/stores/LEAs and SSE stack operations. General memory load/store helpers select no displacement, disp8, or disp32 as appropriate, including SIB for RSP/R12 and special handling for RBP/R13.
3. **Dense PMIR frame layout.** Allocate stack cells for only the virtual registers actually referenced by lowered instructions and parameters. Preserve VReg identity, dedicated rich-value object storage, sret, ABI shadow space, and 16-byte stack alignment. No unsafe sharing of live cells is introduced.
4. **PMIR integer immediate instruction selector (-O2/-O3).** Track local constants conservatively with invalidation on updates and potential side effects. Select `add`, `sub`, `imul`, `and`, `or`, `xor`, shifts, and signed integer comparisons against legal immediates; support commutative operand ordering. Reject unsafe sign-extension of integers outside signed imm32 range. Preserve x64 shift modulo-64 semantics.
5. **CFG-aware fall-through emission.** Omit jumps to immediately adjacent blocks; invert conditional branches when the true edge falls through, avoiding an extra unconditional jump.
6. **Iterative short-branch relaxation.** Select x64 rel8 conditional/unconditional branches where permitted. Recompute all branch positions and rebase internal call, literal, imported-symbol, and machine-listing relocation offsets; reject branch relocation overflows.
7. **Verifier support and native regression coverage.** Validate PMIR immediate op shapes and permitted ranges. Five built-in backend tests were added for ALU and comparison semantics, wide immediates, dead-frame-cell elimination, and loop control flow. Includes additional differential stress and integration tests.

These are native backend and instruction-encoding improvements, not claims of complete register allocation, auto-vectorization, advanced interprocedural optimization, PGO/LTO, or full Windows ABI conformance.

## Measured code-size results

Using `-O2` and the attached representative programs, comparing the immediately preceding frontend-expanded compiler to this build on the same host:

| Source | Earlier x64 `.text` | Updated x64 `.text` | Reduction |
|---|---:|---:|---:|
| `arithmetic_and_branches.pcn` | 814 bytes | 541 bytes | **33.5%** |
| `memory_and_calls.pcn` | 694 bytes | 400 bytes | **42.4%** |

The associated COFF objects shrank from 1,368 to 1,096 bytes and from 1,242 to 946 bytes, respectively. These are specific **code-size** benchmarks and not runtime-speed measurements. Neither program failed its PCIR-native differential check.

## Building

### Windows, Visual Studio x64 Developer Command Prompt

```bat
cl /nologo /std:c++latest /EHsc /utf-8 /O2 /bigobj ParacineCompiler_2026_Native_Backend_Optimized.cpp /Fe:paracine.exe
paracine.exe selftest
paracine.exe test tests\arithmetic_and_branches.pcn -O3
paracine.exe build tests\memory_and_calls.pcn -O3 -o memory_and_calls.exe
paracine.exe verify-bin memory_and_calls.exe
```

Or run `build_windows.bat`. **Windows compilation/execution has not been independently tested in this session.**

### Linux x86-64 development host

```bash
g++ -std=c++23 -O0 -g0 ParacineCompiler_2026_Native_Backend_Optimized.cpp -o paracine
./paracine selftest
./paracine test tests/arithmetic_and_branches.pcn -O3
./paracine test tests/memory_and_calls.pcn -O3
python3 run_regressions.py --compiler ./paracine
python3 tests/backend_stress.py --compiler ./paracine
```

The Linux host compiler still **targets Windows** for output `.obj`/`.exe`. Its isolated native x86-64 execution test harness uses the Microsoft x64 ABI on the host; it is not a substitute for executing produced PE files on Windows.

### CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure -C Release
```

## Useful compiler commands

```bash
paracine mir tests/arithmetic_and_branches.pcn -o optimized.pmir
paracine asm tests/arithmetic_and_branches.pcn -o optimized.asm
paracine size tests/arithmetic_and_branches.pcn --json -O2
paracine obj tests/arithmetic_and_branches.pcn -o optimized.obj -O3
paracine build tests/arithmetic_and_branches.pcn -o optimized.exe -O3
paracine verify-bin optimized.obj optimized.exe
paracine test tests/arithmetic_and_branches.pcn -O0
paracine test tests/arithmetic_and_branches.pcn -O3
```

## Validation and remaining limitations

Verified on a Linux x86-64 host using GCC C++23 and Clang C++23 syntax checks: **101/101 built-in tests**, **6/6 prior external regressions**, **72 native-versus-PCIR differential runs** from 24 deterministic stress programs at three optimization levels, and both representative native tests. Structural COFF/PE32+ verification passes.

Full Paracine 1.0 remains larger than this build. In particular, a production register allocator, broad SIMD/vector legalization, a complete high-performance native runtime, PGO/LTO, production-grade exception/unwind/debug metadata, all network/thread/TLS/GUI APIs, comprehensive Windows-target validation, and verified self-hosting are not completed by this revision. Existing language/semantic limitations from the earlier compiler remain. This artifact does **not** claim universal optimization benefit, comprehensive conformance, or platform certification.

## Package files

- `ParacineCompiler_2026_Native_Backend_Optimized.cpp` — complete updated compiler implementation
- `Paracine_Native_Backend_Optimization.patch` — unified patch against the previously delivered frontend-expanded `.cpp`
- `PARACINE_SPECIFICATION_2026.txt` — supplied authoritative specification
- `CMakeLists.txt`, `build_windows.bat` — platform build entry points
- `run_regressions.py`, `tests/backend_stress.py`, `tests/*.pcn` — test drivers and programs
