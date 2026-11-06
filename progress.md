Paracine 2026 — Native Compiler Implementation Upgrade
Build: Paracine 2026 iterative-stdlib-upgrade-20261106.1 (development; non-1.0)

IMPORTANT: This is not a complete implementation of the full Paracine 1.0 normative specification.
The included 2,936-line specification is the design target. The compiler is a roughly 14,000-line C++23 development implementation with a tested subset. The modifications in this package are real source-level extensions and fixes—not a complete production compiler, proof of feature equivalence, or Windows runtime certification.
Changes made in this upgrade
1. Labeled loops from source to native IR: @label: while, @label: for, label: from ... to, and @label: continue until retain labels in AST transformations, semantic checks, and PCIR lowering. break @label and continue @label target the specified enclosing loop, with the existing defer/cleanup mechanism invoked before the transfer. Labels that are undefined, duplicated within active nesting, or applied to nonloops are rejected.
2. Guarded fixed-array iteration: for element in array<T,N> while condition now binds each element in order and halts after acquiring the first candidate whose guard is false. It also supports fixed-array literals such as [3, 4, 5]. Iterable expression evaluation happens once. for var over arrays is rejected explicitly until mutable borrowed element binding is implemented.
3. Inclusive endpoint safety: the for ... in start..=end backend checks completion before incrementing the last element, avoiding a hidden MAX + 1 on a signed 64-bit induction range.
4. Lexical binding restoration: the PCIR lowerer restores any shadowed loop variable after from and for scopes, rather than leaking the loop's register binding into later code.
5. Guard preservation in analysis: removed duplicated else if constexpr (EachStmt) visitor branches that made later guard-aware branches unreachable in SKM, effect inference, closure scanning, AST specialization, and closure discovery.
6. Formatter correctness: fixed a duplicated in ... clause in guarded for statements and preserved loop labels when re-rendering AST statements.
7. Explicit capability inventory: the new paracine capabilities command reports implemented versus partial/unimplemented functionality.
8. 15 additional self-tests (the source supplied with this request passed 40 self-tests; upgraded source passes 55).
9. External regression suite testing check, test -O0, test -O3, PE generation and structural verification, plus negative diagnostics.
Build
Windows with Microsoft Visual C++ (Visual Studio)
1. Extract the archive to a new folder, keeping main.cpp and examples/ together.
2. Open an x64 Native Tools Command Prompt for Visual Studio (or launch the corresponding environment from Visual Studio).
3. Run Build-And-Test-Paracine.cmd. It builds the actual C++23 compiler directly into build\paracine.exe, checks its version, runs the self-tests, invokes the compiler on representative .pcn programs, and executes the generated Hello World PE.
Manual build from the x64 developer prompt:
cl /nologo /std:c++latest /EHsc /O1 /W3 /bigobj main.cpp /Fe:paracine.exe
paracine.exe version
paracine.exe capabilities
paracine.exe selftest
paracine.exe test examples\hello.pcn -O3
paracine.exe test examples\guarded_array.pcn -O3
paracine.exe test examples\labeled_continue.pcn -O3
paracine.exe pe examples\hello.pcn -o hello.exe
paracine.exe verify-bin hello.exe
hello.exe
Linux / macOS compiler-development host
clang++ -std=c++23 -O0 main.cpp -o paracine
./paracine selftest
python3 smoke_test.py ./paracine
CMake is also provided: cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure.
Windows safety note
The supplied .exe artifacts have been structurally inspected by Paracine's PE verifier. They have not been launched using the Windows loader in this environment. Running Build-And-Test-Paracine.cmd on your own Windows x64 machine is the required direct-loader confirmation.
Observed validation in this environment
- Clang C++23 build: PASS.
- GCC C++23 build: PASS.
- selftest: 55 passed, 0 failed with both compilers.
- External smoke_test.py: PASS.
- hello.pcn: both PCIR interpreter and emitted x64 text-shim execution print Hello, World!; exit 0.
- guarded_array.pcn: emitted x64 and PCIR return 6 at O0 and O3.
- labeled_continue.pcn: emitted x64 and PCIR return 3 at O0 and O3.
- labeled_break.pcn: emitted x64 and PCIR return 1 at O0 and O3.
- PE32+ structural verifier: PASS for generated sample binaries.
The host-side x64 test executes emitted machine code using ABI-compatible shims on Linux. It is not a replacement for Windows loader, import and runtime testing.
Remaining work before true Paracine 1.0 compliance
The following are not resolved by these patches:
- Arbitrary iterator<I,T> / view<T> loop lowering; fixed array iteration is supported, not all iterables. The compiler produces E0611 instead of generating incorrect code.
- Exclusive element borrowing for for var over arrays (E0612).
- Full enforcement of all ownership, nonlexical borrow and closure-capture rules, and consistent effect/trap semantics across every construct.
- Complete modules, imports, capability resolution, generic monomorphization and all public ABI forms.
- Comprehensive runtime/standard library (std.net, std.thread, channels, synchronization, std.dynlib, filesystem, etc.).
- Complete Windows PE/COFF linker coverage, import libraries, resource/TLS/unwind/load-config/CFG/PDB behavior, signing and hardening.
- Full C++20 portability exporter for aggregates, closures, results/options and every runtime feature.
- Full Unicode NFC normalization, comprehensive diagnostic coverage, exhaustive optimizer and PCIR/PMIR differential proofs.
- Full Windows x64 end-to-end testing, fuzzing, reproducible-build qualification, self-hosting, security review and compatibility certification.
See Feature_Coverage.csv for a concise feature-by-feature status.
Suggested implementation order for the remaining standard
Priority 0: Build conformance gates. Each normative statement should map to a positive/negative diagnostic test and a native-versus-PCIR oracle. No feature is called supported merely because a token or opcode exists.

Priority 1: Complete frontend and semantics. Finish Unicode, all declaration forms, module resolution, type inference, ownership/borrow/effect checking, and complete loop iterator/capture semantics.

Priority 2: Complete PCIR and PMIR. Every semantic operation gets explicit lowering, verification and machine-code differential tests, including error/cleanup paths and control-flow dominance.

Priority 3: Complete Windows ABI, linker and runtime. Verify startup, Win64 ABI, imports/exports, TLS, relocations, unwind/PDB/CFG, standard I/O, allocator, concurrency, networking, and external COFF/LIB interoperability on Windows.

Priority 4: Verify production properties. Differential testing, fuzzers, performance and security regression tests, reproducible builds, hardening checks and a self-hosting bootstrap chain where required by the intended implementation.

The compiler is deliberately labeled development; non-1.0 until these milestones are proven.
