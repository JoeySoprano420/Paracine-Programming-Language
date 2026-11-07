# Verified results — Paracine Stage 1 v0.4.1

Environment: Linux-hosted Stage-0 C++23 host with the Windows x64 ABI native execution harness. Target binaries: Windows AMD64 PE32+. Direct execution on Windows has **not** been observed in this release.

- Stage-0 self-tests: **91 passed / 0 failed**.
- Updated Stage-1 source: `Paracine check` **passed**, 68 routines and 8 declarations; SAGE/SKM and PCIR verified.
- Previous Stage-1 regression harness: **22/22** source cases passed, at O0 and O3. Test 17 was updated because `float` declarations are now supported in strict mode.
- Lexer regression harness: **22/22** cases passed at O0 and O3.
- New Stage-1 parser regression harness: **15/15** positive/negative cases passed at O0 and O3.
- Native source-to-source integration: a Paracine-native harness read each fixture, invoked the Stage-1 `compile_source` routine, and wrote actual emitted `.pcn` files using `std.fs.write_text`. All 14 emitted programs were compiled and run at O0 and O3, returning 42 as expected (**28/28**).
- Combined repeatable native execution checks (3 suite harnesses × 2 optimization levels + 14 output programs × 2): **34/34 passing** (`TEST_RESULTS.json`).
- Advanced Stage-0 compatibility fixture (`record Pair`, `array<int,3>`, `for n in numbers`) passed Stage-0 `check` and `run`, returning 42 at O3.
- Compiled Stage-1 Windows executable: PE32+ AMD64 structural verification **passed**.

The Windows `Build-And-Test.cmd` command in the distribution exercises strict mode and `--stage0-full` through the **actual Windows loader** and is the final platform verification step. The `--stage0-full` command-line branch was compiler-checked but has not been directly executed on Windows in this environment.

**Known limitations:** strict Stage 1 is not the entire Paracine language, not a new native backend, and not independently self-hosted. Builtin/external call signatures and advanced effect rules are checked by Stage 0 during the final backend pass. Stage 0 itself is not asserted to fully implement every feature in the 2026 specification.
