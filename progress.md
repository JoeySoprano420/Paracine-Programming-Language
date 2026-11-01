Paracine native compiler — expanded development implementation
Scope and fidelity
This directory contains the extended C++23 Paracine compiler from the attached compiler source, retaining the native-first .pcn → SAGE → PCIR → PMIR → x86-64 → COFF → PE32+ toolchain. The normative document describes the target Paracine 1.0 language, not the achieved conformance of this implementation. In particular, native source imports remain a flattened subset, some closure and aggregate operations remain unimplemented, and the production security, complete Windows x64 unwind support, full standard library, and self-hosted Paracine compiler are not certified or complete.
Added in this revision
- Structural delimiter matching rejects mixed pairs such as ([)], rather than accepting balanced counts alone.
- paracine verify-bin file.obj file.exe parses generated or external AMD64 COFF/PE32+ binaries with bounds-checked reads and validates signatures, section tables, raw section byte ranges, relocation bounds, required header fields and executable entry placement. This is a structural consistency check, not an authenticode/security check, OS loader or full PE/COFF conformance proof.
- paracine selftest exercises UTF-8, negative lexical cases, integer boundaries, COFF/PE32+ generation, structural corruption detection, the PCIR reference interpreter, and — on Linux x86-64 — the isolated emitted-machine-code executor.
- CMake project with CTest integration, plus a native/reference loop smoke program.
Build
Windows (Visual Studio 2022 or later)
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\paracine.exe build smoke.pcn -o smoke.exe
.\build\Release\paracine.exe verify-bin smoke.exe
Linux x86-64 (development host)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/paracine emit obj smoke.pcn -o smoke.obj
./build/paracine emit exe smoke.pcn -o smoke.exe
./build/paracine verify-bin smoke.obj smoke.exe
Linux host paracine run and paracine test can execute the emitted Microsoft-x64-ABI machine-code entry in an isolated process, but do not launch Windows PE files through the Windows loader. The generated .exe requires Windows x86-64 for ordinary execution. The shipped prebuilt paracine is a Linux x86-64 development binary; Windows users should build from source.
CLI
In addition to existing build, check, sage, test, run, emit, ir, mir, asm, explain, inspect, fmt, deps, hash, audit, bench, link, package, doctor, and init commands:
paracine selftest
paracine verify-bin path/to/file.obj path/to/file.exe
Known gaps before a conforming final native compiler
Complete edition-2026 module resolution and visibility, capabilities, monomorphization/closures, full borrow and lifetime analysis, native aggregate lowering, broad standard-library/runtime coverage, executable/DLL and archive link completeness, PDB and Windows unwinding completeness, rigorous machine-code validation and fuzzing, runtime resource constraints, cross-platform CI, and a genuine self-hosting bootstrap remain substantial independent milestones. Do not treat passing these tests as evidence that these are finished.
