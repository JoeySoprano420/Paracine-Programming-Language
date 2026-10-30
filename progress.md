Paracine Fast Output — verification and limitations
Updated compiler: 10,302 C++23 LOC, extending 9,844-line Paracine Bootstrap OS Expansion.
Implemented
- Reuse verified PMIR and x86-64 machine code for PE32+/COFF emission.
- SHA-256 keyed, dependency-aware incremental caching for native build, pe, obj.
- Persistent compiler-binary SHA-256 fingerprint stamp, refreshed on executable size/mtime change.
- SHA-256 payload corruption rejection, recompile on cache miss, source-change recheck before cache storage.
- Do not rewrite byte-identical generated COFF/PE, preserving timestamps.
- size / size --json, four-level audit / audit --json, deps, and streamed hash tools.
Verified here
- Clang C++23 CMake build succeeded.
- 14/14 default CTest suites passed; new fast-output suite 58/58 assertions passed.
- 24/24 previous-vs-upgraded COFF/PE bytes comparisons matched for 6 source programs at -O0/-O3.
- GCC C++23 syntax check passed, Clang C++23 -Wall -Wextra -Wpedantic -Werror syntax check passed.
- Exact unified patch applied and reproduced updated source byte-for-byte.
- SHA-256 outputs checked against Python hashlib.sha256 vectors.
Representative measured build times
- Previous uncached median: ~55 ms.
- Upgraded uncached median: ~47 ms.
- Upgraded warm-cache median: ~6.4 ms.
- Initial cache-enabled build: ~1.55 s due to one-time compiler-binary hashing in this unoptimized host build.
  These are representative for one arithmetic compiler program, not universal benchmarks.
Not completed or certified
- Complete Paracine frontend/borrow checker/runtime/standard library or self-hosting stages.
- Full COFF archival linking, TLS/ASLR/PDB, production Windows/MSVC verification.
- Parallel IR/codegen, profile-guided machine optimization, or execution-performance superiority.
- Cache does not defend against maliciously preserved compiler executable timestamps and sizes, or untrusted local cache poisoning.
- MSVC build with locally edited main.cpp is not validated here.

## *** ##

Paracine Fast Native Output Edition
This is an executable development compiler expansion based on Paracine_Bootstrap_OS_Expansion.cpp (9,844 LOC). It is not a declaration of complete language conformance, full bootstrap self-hosting, or Windows production certification.
New capabilities
Backend reuse: The native builder reuses the already verified PMIR and x86-64 machine representation when packaging COFF .obj and PE32+ .exe; no second full PMIR lowering/encoding pass is needed. The original PECOFFGenerator APIs remain usable as compatibility entry points.

Native artifact caching (opt-in): build, pe, and obj accept --cache or --cache-dir PATH. The key incorporates the compiler executable SHA-256, canonical local .pcn source dependency graph contents, target artifact type, -O setting, and selected entry routine. Modified or missing imports invalidate the key; cycles and invalid imports still fail. Each cached artifact stores its own SHA-256 payload integrity checksum. Corrupt cache data produces a miss and regeneration. The compiler itself is fully hashed after a binary update and a persistent SHA-256 fingerprint is reused as long as executable size and high-resolution modification time stay unchanged. Deleting the cache directory forces full compiler identity rehashing. A deliberate adversary preserving executable size/mtime is outside the cache threat model. This is not a cryptographic code-signing or remote-trust system.

Unchanged output preservation: Generated COFF/PE files are compared byte-for-byte before overwriting. Repeated identical builds retain file modification times, reducing downstream build invalidation.

Tools:
paracine build app.pcn --cache -O3 -o app.exe
paracine obj app.pcn --cache --cache-dir build/cache -O3 -o app.obj
paracine size app.pcn -O3
paracine size app.pcn --json
paracine audit app.pcn
paracine audit app.pcn --json -o audit.json
paracine deps app.pcn
paracine hash app.pcn app.exe
paracine profile app.pcn -O3
size shows .text bytes, imports, IR instructions, and the 20 largest machine-code functions. audit compiles all four optimization levels and reports compilation duration, generated code size, PCIR instruction counts, and block counts. Neither command misrepresents compilation time as generated-program execution speed. hash streams SHA-256 without loading the whole input file. deps reports canonical local sources and per-file SHA-256 values.
Reproducibility and correctness checks
The native_fast_output CTest suite runs 58 assertions covering cache miss/hit behavior, import modification invalidation, optimization-level segregation, object and executable byte equality, corruption recovery, no-touch outputs, hashes matching Python's SHA-256 implementation, size/JSON/audit tools, and missing-import errors.

Separately, 24 output comparisons between the previous and upgraded compiler matched byte-for-byte on six .pcn programs, for both .obj/`.exe` and -O0/`-O3` combinations.

The default 14 CTest suites passed on Linux x86-64 using Clang C++23. GCC and strict Clang C++23 syntax checks passed. MSVC/Windows PE loader tests and the optional heavyweight IR harnesses were not run in this pass. The compiler is still a development compiler; safe ownership, full packages/modules, stdlib, and a complete internal COFF linker remain release blockers.
Performance example (this host, local development build)
For the Paracine-written arithmetic compiler program, repeated executable builds measured around 55 ms with the prior binary, 47 ms with the upgraded compiler uncached, and 6.4 ms median on cache hits. The first cache-enabled build was around 1.55 s, mostly due to a one-time full SHA-256 of the host compiler executable. Performance changes with build configuration, input complexity, filesystem, and CPU; these numbers are workload-specific, not broad throughput promises.
Build and test
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure -j 3
For MSVC on Windows (commands provided, not verified here):
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
Cache identity note: compiler executable content is fully hashed once per updated compiler binary per cache directory; subsequent invocations rely on metadata to avoid repeating that expensive work. Cache entries must reside in a trusted local directory. To invalidate manually, remove --cache-dir or .paracine-cache.

## *** ##

