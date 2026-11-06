Paracine 2026 — Semantic / Premium Optimizer Upgrade
Build ID: semantic-optimizer-upgrade-20261106.2
Language implementation: C++23
Primary output: native Windows x64 PE32+ / COFF
Architecture retained: Paracine source → lexer/layout → parser → SAGE/SKM → PCIR → optimization → PMIR → x86-64 → COFF → PE32+.
What was actually implemented
The approximately 14,000-line compiler has been upgraded in-place, not replaced by a toy compiler.
1. Exhaustive PCIR opcode effect inventory. All 68 currently defined PCIR operations are explicitly classified as pure/total, may-trap, observable/ownership-sensitive, or control-flow. Classification does not mean every operation can be optimized. This gates rewrites and dead-code elimination.
2. Flow-sensitive scalar constant propagation. Integer and Boolean constants propagate across basic blocks using the intersection of reachable predecessor facts; values invalidated by writes, joins, loops, and calls are not assumed constant.
3. Exact constant folding. Safe integer arithmetic, comparisons, shifts, bitwise operations, and Boolean operations use defined modular 64-bit semantics. Known division or remainder by zero is not folded away. The special MIN / -1 and MIN % -1 cases are preserved.
4. Safe algebraic identities. Examples: integral x+0 → x, x*1 → x, x*0 → 0, x-x → 0, and x==x → true for integral operands. No equivalent assumptions are made for IEEE-754 NaNs/signed zero.
5. Local integer common-subexpression elimination. Repeated equivalent operations reuse earlier results. Each VReg write increments a local value version, preventing reuse after mutation; external calls and aggregate/string mutation act as barriers.
6. Register-write-aware copy propagation. Copies are followed only while both the alias and its source remain unchanged. PHI inputs from predecessor blocks are not rewritten as if they belonged to the current block.
7. Effect-aware dead-result pruning. Only instructions classified as pure and total are discarded when their results are unused. Trap-capable operations, I/O, ownership-sensitive operations and native calls remain observable.
8. Branch/path optimization. Constant conditions become unconditional jumps and unreachable blocks are removed at -O3. PHI predecessor lists are repaired after CFG pruning.
9. Small pure scalar-function inlining at -O3. Single-block integer/Boolean leaf routines with a verified read-only parameter contract are expanded with fresh caller registers. This enables interprocedural constant folding and removes some calls.
10. UTF-8 literal specialization. Literal text facts are propagated conservatively across blocks. Proven constant equals, compare, starts_with, ends_with, find, rfind, find_byte, byte_at (in bounds), count, codepoint_count, is_ascii, and hash64 operations can become constants. Any runtime bounds failure remains a trap.
11. Verification and regressions. Existing optimizer-after-lowering PCIR verifier remains enabled. Added eight built-in tests and external differential/case reports.
Optimization levels
Level	Behavior
-O0	Original verified PCIR, no optimizer rewrites; debugging/reference oracle
-O1	Local constant folding, scalar algebraic identities and safe copy propagation
-O2	-O1 plus CFG dataflow, UTF-8 literals, dead pure-result elimination, trivial PHI reduction
-O3	-O2 plus local value numbering, small pure scalar-function inlining, unreachable-block pruning


No universal optimality guarantee: optimal instructions depend on workload, CPU, data, profile and cost model. This implementation makes real source-to-machine improvements for defined subsets of supported semantics; it is not a proof of optimal code for every Paracine program or full Paracine 1.0 compliance.
Build in Visual Studio (Windows x64)
Open x64 Native Tools Command Prompt for Visual Studio, extract this package and run:
Build-And-Test-Paracine.cmd
The script uses cl /std:c++latest /EHsc /O1 /W3 /bigobj, writes the compiler to build\paracine.exe, verifies the exact build ID, tests representative programs at -O0 and -O3, generates hello.exe, structurally verifies PE32+, and executes Hello World on Windows.

Manual commands after building:
build\paracine.exe version
build\paracine.exe capabilities
build\paracine.exe selftest
build\paracine.exe check examples\hello.pcn
build\paracine.exe test examples\optimizer_cse.pcn -O0
build\paracine.exe test examples\optimizer_cse.pcn -O3
build\paracine.exe ir examples\optimizer_cse.pcn -O0
build\paracine.exe ir examples\optimizer_cse.pcn -O3
build\paracine.exe pe examples\hello.pcn -o build\hello.exe
build\paracine.exe verify-bin build\hello.exe
build\hello.exe
Optional Python 3 diagnostics:
python optimization_report.py build\paracine.exe examples\optimizer_cse.pcn
python optimization_report.py build\paracine.exe examples\optimizer_utf8_literals.pcn
python differential_stress.py build\paracine.exe
Validation completed in this environment (Linux x86-64)
- Clang C++23 source build: PASS.
- GCC C++23 source build: PASS.
- Built-in self-tests: 63/63 passed under Clang and GCC (original 55 plus eight optimizer tests).
- Random-seed-reproducible differential suite: 22/22 cases, comparing -O0 and -O3 PCIR and emitted x64 execution: PASS.
- Representative positive tests: I/O order, mutable register snapshots, loop-carried state, guarded arrays, labeled break/continue, scalar inlining, constant UTF-8 operations, and more: PASS.
- Dead, unused divide-by-zero operation remains in optimized PCIR: PASS (trap preserved).
- Native Windows PE32+ sample generation and structural verification: PASS, 5120-byte sample.
- Not verified here: execution by the actual Windows loader, Microsoft cl.exe compilation, Windows CPU timing/throughput, and complete 1.0 language conformance. The Linux-hosted x64 text-shim test is not a substitute for Windows loader testing.
Demonstrated IR reductions (representative, not speed benchmarks)
Program	-O0 PCIR instructions	-O3 PCIR instructions	Change
optimizer_cse.pcn	11	5	−54.5%
optimizer_across_branches.pcn	13	3	−76.9%
optimizer_utf8_literals.pcn	43	7	−83.7%
optimizer_io_order.pcn	22	12	−45.5%


The implementation can inline a pure scalar routine and reduce the caller to const.int 72; ret, but it does not currently remove all unused routine definitions from the final program. Instruction-count reductions do not imply proportional runtime speedups.
Feature coverage and planned work
See Optimization_Opcode_Coverage.csv for the per-opcode safety classification and currently permitted transformations, and Optimization_Architecture.md for how different language constructs lower into the optimization framework.

Important not implemented here: full alias/escape analysis, interprocedural ownership optimization, rich aggregate scalar replacement, generalized devirtualization, arbitrary iterator/view vectorization, bounds-check elimination with range proofs, loop interchange/unrolling/vectorization with guards, target-feature dispatch (AVX2/AVX-512), PGO/LTO, block reordering, production register allocation, CPU-cost autotuning, full parallel scheduling, complete runtime/standard-library optimization, and full Paracine 1.0 specification. These need independent semantic proofs, performance benchmarks and Windows ABI testing.

Never disable the PCIR verifier to conceal errors. Preserve -O0 as the reference and compare side effects, traps and return values at every optimization level.




Paracine 2026 — Optimization Architecture and Construct Coverage
The optimizer operates on semantic PCIR operations, not on the spelling of individual keywords. The same operation may arise from different source constructs. This table describes what the new upgrade does, as distinct from what the language specification ultimately demands.
Source-language construct or family	Current optimization behavior	Boundary / remaining work
module, import, aliases, namespace paths	Resolved before PCIR; qualified declarations are not local runtime register values	Complete package-resolution optimization and link-time deduplication pending
routine, give, calls	Small, one-block, pure scalar routines inlined at -O3; return values fold across call boundary	Generic inlining, recursion, larger CFG calls and call-cost estimation pending
let, var, assignment	Scalar constant facts, copy propagation with explicit invalidation on register writes	Full SSA conversion, memory SSA, escape analysis pending
Integer values and + - * / %	Constant folding with modular arithmetic; nonzero divisor proof; identities and value numbering for safe ops	General strength reduction and optimal instruction selection pending
Integer comparisons, Boolean and/or/not, bitwise and shifts	Integer-only constant folding and identities, safe branch pruning	Profile-informed branch probabilities and flag fusion pending
Floating float/double arithmetic	No unsafe algebraic reassociation; reference semantics preserved	Target-specific FP cost model and formally justified fast-math modes pending
char, UTF-8 text, owning string	Static literal queries, comparisons, search, byte-count, codepoint-count, ASCII test, hash	Dynamic text SIMD, rope/string-builder optimization, Unicode scalar range proofs pending
if, else, conditional branches	Constant-edge folding and unreachable CFG pruning at -O3	Branch layout, profile-directed decision-tree rewriting pending
while, for ... while, from ... to, continue until	Register-write-aware analysis preserves loop-carried state; constants and pure local expressions optimized inside blocks	Full iterator lowering, loop-invariant code motion, unrolling, SIMD, dependence analysis pending
break, continue, loop labels	Jump-target structure retained; unreachable paths may be removed with PHI edge reconciliation	Cross-loop cleanup proofs and systematic labeled jump differential tests still required for full 1.0
record, array, view, span	Existing backend lowering preserved; no speculative aggregate or bounds-check elimination	SROA, general view iteration, NLL alias proofs, range analysis pending
result, option, try, fail, choose	Existing representation and control flow retained, and side-effecting/trapping operations protected	Discriminant proofs and path-specific variant narrowing pending
Closures and generic specialization	Existing closure and specialization lowering preserved, small eligible generated scalar leaf routines may inline	General closure escape and monomorphization cost model pending
Effects with io, with alloc, with trap, unsafe	IR effects gate transforms; I/O and trap-capable ops are not dropped as dead computations	Precise interprocedural effect summary and devirtualization pending
std.io, foreign routines and other runtime operations	Observable calls preserved in original order; immutable calls only inlined under explicit proof	Whole-program side-effect analysis and foreign-call optimization pending
Parallel threads, synchronization, atomics, dynamic libraries	Classified to retain behavior if represented by the existing PCIR operations	Specialized concurrency, memory-order and synchronization optimization pending
Native COFF / PE32+ output	Optimized PCIR flows through existing PMIR and direct x86-64 emission, preserving current ABI pipeline	Advanced machine instruction combining, register allocation, LTO, PGO, CPU-feature dispatch pending


Correctness invariants
1. No universal optimality guarantee. There is no instruction sequence provably fastest for every architecture, workload, cache behavior and runtime input.
2. No speculative alias equivalence. Mutable PCIR registers are not treated as SSA values. Copy/value-number tables are invalidated on writes and effect barriers.
3. No hidden trap elimination. An unused division that divides by zero must still trap. String/array bounds checks remain unless the exact safe result is proven.
4. No unsafe FP assumptions. NaN, signed zero and floating rounding prevent common integer algebraic rewrites from being applied to floats.
5. Observable output order is stable. TextStreamWrite, foreign calls and owned memory mutations remain ordered.
6. The oracle remains available. Compare PCIR execution and emitted x64 execution at -O0 and -O3; review results and output, not just the process exit code.
Work still needed for premium native code quality
1. Canonical, dominance-correct SSA for scalar computation and memory SSA for mutable places.
2. Deeper cost-based inlining with recursion limits, code-size budgets and profile data.
3. Alias/escape/lifetime summaries, SROA, and allocation elimination with ownership proof.
4. Loop dependence/range analysis and optimizations with preserved post-test, inclusive-bound and guard semantics.
5. Native target-specific register allocation, scheduling, instruction selection, addressing modes, AVX2/AVX-512, and branch layout.
6. PGO, optional LTO, reproducible benchmarking, CPU-specific tuning and differential fuzzing.
7. Full Paracine 1.0 implementation coverage, Windows loader validation and MSVC qualification.
Those are design milestones, not features claimed to be implemented in this delivered build.
