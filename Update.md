PARACINE — ".pcn"

Final Native-First Normative Resolution and Production Completion Standard

Edition: Ultimate Multi-Output Production Standard Language edition: 2026 Specification level: 1.0 Final Status: Frozen production definition Canonical compiler implementation: C++23

Primary compilation model: Independent direct-native compilation Primary native object realization: COFF ".obj" Primary native executable realization: PE32+ ".exe"

Optional portability/export realization: Standards-oriented C++20 ".cpp" C++20 required for Paracine compilation: No

Canonical Tier-1 target: Windows x86-64 Canonical external ABI: Microsoft x64 ABI Canonical semantic IR: PCIR Canonical machine IR: PMIR Canonical compiler-analysis framework: SAGE — Semantic Analysis & Guidance Engine

This document completes and freezes the Paracine Language Specification and Paracine Backend Specifications.

Normative revision: Structured Loop Completion — November 5, 2026.

This revision incorporates "for ... while ...", "from ... to ...", and "continue until ..." into the normative grammar and semantics.

Normative revision: Runtime and Observable-Behavior Completion — November 7, 2026.

The revised baseline additionally incorporates owned file/resource handles, recoverable system errors, interactive input, process management, reliable networking, native GUI/window APIs, and integration tests of observable program behavior. It replaces the preceding freeze definition. The frozen-edition change policy below applies to this completed edition-2026 baseline.

Where this document conflicts with an earlier "[Decision]", "[Verify]", open issue, tentative statement, incomplete backend clause, or wording that implied C++20 was structurally necessary to Paracine, this document is authoritative.

All earlier provisions not changed here remain in force.

No "[Decision]", "[Verify]", “open issue,” “future specification,” or unspecified mandatory production component remains in Paracine 1.0.

The defining architectural distinction is now permanent:

«Paracine is a complete native compiler without C++20.»

The C++20 exporter is useful.

It is not required.

Final Problem Statement and Purpose

Paracine solves the problem of building readable, memory-safe, resource-controlled native software whose meaning remains consistent from source code through optimization, operating-system interaction, and executable delivery.

Its central engineering purpose is to reduce the amount of behavior that programmers must keep correct through convention alone. Types, ownership, borrows, effects, recoverable errors, native-resource lifetimes, and backend behavior belong to one explicit language contract.

This purpose describes the final language defined by this specification. The detailed normative rules remain authoritative for the precise scope of each guarantee.

1. Making Program Meaning Explicit

Application behavior becomes difficult to maintain when conversions, mutation, allocation, ownership transfer, or failure propagation are hidden or ambiguous. Paracine makes those decisions visible through exact static types, explicit parameter authority, immutable and mutable bindings, effect analysis, and structured result handling.

The compiler resolves those facts before machine lowering. Readable syntax therefore carries a precise meaning that each backend must preserve. The programmer can inspect the source to understand the operation and inspect compiler explanations to understand the verified lowering.

2. Controlling Memory and Native Resource Lifetimes

Native programs must manage memory and resources that outlive individual expressions: files, pipes, sockets, child processes, windows, and registered callbacks. Ownership and nonlexical borrowing define who controls each value and which uses may overlap.

Safe source prevents use after move, dangling safe borrows, double disposal, invalid shared/exclusive aliasing, uninitialized safe reads, and safe data races. Bounds checks prevent invalid indexed memory access by producing the defined failure behavior. Owned native handles extend these rules to operating-system resources, including consuming close operations and lifetime-safe callback state.

Normal scope exit receives deterministic cleanup. Fallible flush, sync, close, and commit operations remain explicit when the application needs confirmation of their outcome.

3. Preserving Readability While Removing Unnecessary Machine Work

Expressive source should permit efficient native realization. Paracine uses monomorphized generics, concrete static closure environments, iterator fusion, specialization, and semantic optimization to eliminate machinery whose removal preserves observable behavior.

The language requires no mandatory garbage collector, exception machinery, runtime scheduler, or boxed closure representation. Allocation and runtime work remain explicit where the program requires them. Abstractions are removed under their legality conditions; the source contract determines which work can disappear.

4. Making Failure Actionable

Real applications must distinguish a missing file from EOF, partial transfer from completion, cancellation from a timeout, and a failed process launch from a successfully observed nonzero exit status. Paracine represents recoverable failures through typed results and options, with native error identity and operation progress retained where required.

Programs can report, retry under an explicit policy, recover, or stop deliberately. Recoverable failure executes normal cleanup. Terminal traps form a separate process-termination contract and do not unwind scopes or become recoverable results.

5. Delivering an Independent Native Build

Building an application should have a defined path from its source to its deployment artifacts. Paracine supplies its semantic frontend, PCIR optimization, PMIR lowering, machine encoder, COFF writer, native linker, PE32+ construction, and supporting native tooling as one toolchain.

The mandatory direct-native target is Windows x86-64 using the Microsoft x64 ABI. Native products include .obj, .exe, .dll, and .lib. Reproducible builds pin declared inputs and remove uncontrolled output variation. C++20 export provides an optional portability and integration route while preserving the same program meaning.

6. Keeping Compiler Analysis Accountable

An optimization can be useful only when it preserves the language's defined behavior. SAGE separates authoritative semantic facts, descriptive structural classification, and optional stylometric suggestions. A project pattern can direct an investigation; independently established semantic facts authorize the resulting transformation.

Defined arithmetic, explicit effect and alias information, strict floating-point rules outside @fastmath, trap ordering, IR verification, and backend validation constrain optimization. Style familiarity cannot establish memory safety, choose identifier meaning, remove synchronization, or replace a proof of legality.

7. Supporting Complete Native Applications

A native language must connect program logic to useful operating-system behavior. Paracine's std tier provides reliable file APIs, bounded interactive input, managed child processes, concurrent pipe capture, deadline-aware networking, and thread-affine native windows and controls.

These facilities share the language's ownership, effects, and recoverable-error rules. A desktop tool can read user input, transform a file, launch a converter, exchange network data, and update a window under consistent resource and failure contracts. UI dispatch, worker-thread coordination, and blocking operations remain explicit application decisions.

8. Checking the Behavior of Finished Programs

Successful compilation establishes only the properties covered by compilation. Runtime integration qualification also executes the actual native program and checks its effects through independent observations.

Tests compare exact file bytes, stream contents, argument boundaries, child-process lifetimes, network exchanges, and window/control state. They distinguish expected failure recovery from fabricated success and missing evidence. Native/PCIR differential testing, optimization-profile comparisons, loader tests, and resource-lifetime tests connect the implementation back to its semantic contract.

Purpose Boundaries

Programmers remain responsible for algorithms, application authorization, protocol design, appropriate synchronization, and domain-specific correctness. Safe-code guarantees do not establish freedom from leaks, deadlocks, starvation, logical ordering errors, or excessive resource use. Unsafe and foreign-code boundaries require their stated contracts, and hardware/OS behavior outside the abstract machine retains its documented limits.

The final design objective is readable expression with precise semantics, checked resource authority, efficient direct-native realization, and executable evidence of required program behavior.


Final Language Identity
Paracine is a statically typed, ahead-of-time compiled, native general-purpose and systems programming language designed around a self-contained compiler and direct machine realization.

Its essential production path is:

.pcn ↓ canonical normalization ↓ lexer + layout ↓ parser ↓ semantic frontend ↓ SAGE semantic core ↓ ownership / effects / closure resolution ↓ PCIR ↓ semantic optimization ↓ target legalization ↓ PMIR ↓ x86-64 machine code ↓ COFF object model ↓ COFF writer / native linker ↓ .obj / PE32+ ↓ .exe

This path is sufficient to compile, link and execute a complete Paracine application.

It requires no generated C++ source.

It requires no foreign C++ compiler.

It requires no LLVM.

It requires no external assembler.

It requires no external linker.

The essential native architecture is therefore:

                PARACINE
                   .pcn
                    ↓
            semantic frontend
                    ↓
                   SAGE
                    ↓
                   PCIR
                    ↓
             optimization
                    ↓
                   PMIR
                    ↓
                x86-64
                    ↓
                  COFF
                    ↓
             native linker
                    ↓
                 PE32+
                    ↓
                  .exe
Paracine may additionally expose an optional portability/export path:

optimized PCIR ↓ optional C++20 exporter ↓ .cpp

That exporter exists for:

portability unsupported-target reach existing C++ project integration human inspection bootstrapping compiler validation differential testing foreign build systems

It is a strategic secondary backend.

It is not part of Paracine's semantic core.

The following statements are final:

C++23 = implementation language of the Paracine compiler

SAGE = semantic, structural and optional advisory analysis framework

PCIR = authoritative semantic optimization representation

PMIR = target-aware machine representation

x86-64 machine code = canonical Tier-1 native instruction realization

COFF = canonical Tier-1 native object realization

PE32+ = canonical Tier-1 executable realization

C++20 = optional portability/export representation

The native language remains complete if the C++20 exporter is not installed, not built, or entirely removed.

Final Resolution Ledger
Every former design question is resolved as follows.

ID| Final resolution R1| Indentation is exactly four spaces. Tabs remain forbidden for structural indentation. R2| Layout tokenization is purely lexical and does not depend on parser knowledge. R3| "text" is an immutable borrowed UTF-8 slice. "string" is its owning counterpart. R4| Paracine 1.0 supports zero-allocation capturing closures as statically typed, monomorphized closure values. R5| Ordinary routine pointers remain environment-free. A noncapturing closure may coerce to a routine pointer. R6| There are no implicit numeric conversions. Contextual literal fitting remains allowed. R7| Capability satisfaction remains structural by name/signature with deterministic lookup. Associated types are not part of edition 2026. R8| Record fields remain in declaration order. The compiler never silently reorders them. R9| Integer "+", "-", "*", and unary negation use defined modular two's-complement semantics. R10| Shift counts are reduced modulo operand width. R11| Signed "MIN / -1 = MIN"; "MIN % -1 = 0". R12| Default routine parameters are shared borrows; "var" is exclusive borrowing; "own" transfers ownership. R13| Explicit lifetime parameters do not exist in edition 2026. Borrow regions are inferred and nonlexical. R14| "@from(...)" remains the explicit returned-borrow disambiguator. R15| Traps terminate the process. They do not unwind Paracine scopes and are not catchable exceptions. R16| Thread traps obey the same terminal-trap model. The former recoverable "thread_panic" model is removed. R17| "give" automatically wraps values into "ok" or "some" where required by the declared result type. R18| "fail" automatically produces "err". R19| Floating contraction and reassociation remain disabled outside "@fastmath". R20| Default hash maps use process-randomized hashing. Stable deterministic hashing remains separately available. R21| Allocating growth operations are fallible. Convenience constructors may trap on OOM while retaining "try_" forms. R22| The import graph remains acyclic. R23| Visibility remains private / public / export. R24| "vec<T,N>" remains a compiler-defined exception to the no-user-operator rule. R25| One source file defines one module, and its path must agree with its module name. R26| General textual/preprocessor macros do not exist in edition 2026. R27| User procedural macros do not exist in edition 2026. Built-in "@derive" is the declaration-generation mechanism. R28| Async/await is not a language construct in edition 2026. Structured threads, synchronization, channels and nonblocking APIs form the concurrency model. R29| Windows x86-64 is the only mandatory direct-native Paracine 1.0 target. Additional platforms may be reached by future direct-native profiles or by the optional C++20 exporter. R30| Index disjointness is conservative unless statically proved or established through a disjointness primitive such as "split_at". R31| Associated types are deliberately excluded. Multi-parameter capabilities are canonical. R32| Trap cleanup is deliberately absent. Recoverable failure uses "result<T,E>" and "option". R33| Build-time metaprogramming uses const evaluation and explicit build-tool steps rather than arbitrary compiler-executed scripts. R34| Nullable "raw" is not guaranteed to use null niche optimization inside "option<raw>". R35| Safe "view", "span", "text" and empty "buffer" values use canonical non-null sentinels, enabling guaranteed null niches for their optional forms. R36| "raw" parameter mode is legal only when raw authority actually exists in the parameter type. R37| Tier-1 unwind information uses the Windows x64 unwind representation implemented and verified by Paracine. R38| PDB emission is a native tooling facility and not part of language-semantic equivalence. R39| Production executables use ASLR-compatible relocation, high-entropy VA and NX compatibility by default. R40| Control Flow Guard is enabled only when complete guard metadata is successfully emitted. R41| Reproducible builds contain no uncontrolled wall-clock timestamps. R42| Native executable linking is complete within Paracine. R43| C++20 generation is not required for a conforming native Paracine compiler. R44| ".obj" and ".exe" are Paracine's primary Tier-1 production realizations. R45| C++20 ".cpp" generation is an optional portability/export subsystem branching from optimized PCIR. R46| Absence of the C++20 exporter does not make a Paracine compiler incomplete. R47| Semantic Analysis is deterministic and authoritative. R48| Structural Analysis identifies computational form but does not change program meaning. R49| Stylometric Analysis is optional, local and advisory. It can suggest but cannot establish semantic truth. R50| Every optimization remains subject to independent semantic proof regardless of stylometric suggestion. R51| Stylometry cannot determine validity, type resolution, overload resolution, ownership, alias legality, synchronization legality, arithmetic meaning or unsafe correctness. R52| Paracine stylometry is not an authorship-identification facility. R53| The compiler may maintain a project-local Semantic-Stylometric Fingerprint, but it is not part of language semantics. R54| Default reproducible production builds do not permit unpinned stylometric state to alter emitted machine code. R55| Style-guided optimization is explicit and, when enabled, its exact profile hash becomes a declared build input.

R56| "for pattern in iterable while condition" is guarded iteration that stops at the first false guard after binding the candidate and before entering its body.
R57| "from index = first to last" is inclusive integer iteration with a fixed direction of +1 or -1, one-time left-to-right bound evaluation, and endpoint testing before any advance.
R58| "continue until condition" introduces a post-test loop: its body executes before each condition test, false repeats, and true exits.
R59| A continue jump targets next-candidate acquisition for a for loop, endpoint testing for a from loop, and post-test evaluation for a continue-until loop; normal cleanup precedes the target operation.
R60| Loop conditions require bool. From-loop bounds require the same built-in integer type with no implicit numeric conversion; the induction binding is immutable and local to each body iteration.
R61| "to" and "until" are reserved keywords. A continue jump and a continue-until compound statement are distinguished by the next token without changing lexical layout rules.
R62| Structured loops lower through verified PCIR and PMIR to direct native code. Their control mechanism introduces no mandatory allocation or runtime scheduler and does not depend on the optional C++20 exporter.
R63| Structured-loop semantics, integer-boundary handling, labels, cleanup, ownership, effect inference, and backend equivalence are mandatory conformance requirements.

R64| Safe native resources have distinct opaque move-only owner types, deterministic normal cleanup, checked borrows, and explicit consuming close/finalization operations.
R65| Expected OS and input failures return typed recoverable errors retaining native identity and transfer progress; they are not implicitly converted into traps or successful sentinel values.
R66| File APIs distinguish short transfer, EOF, flush, persistent-media sync, close, append, exclusive creation, and atomic replacement, with checked sizes and explicit sharing policy.
R67| Interactive text input distinguishes blank lines, unterminated final lines, EOF, malformed encoding, length bounds and cancellation; prompts require observable flush before blocking.
R68| Child commands use explicit argument vectors and handle inheritance; managed process scopes, bounded concurrent pipe capture, deadlines, termination and deliberate detach have defined ownership.
R69| Network APIs preserve stream/datagram distinctions, partial progress, half-close, peer errors, zero-length datagrams, deadlines and explicit cancellation policy.
R70| std.gui provides demand-linked native windows, controls and explicit event dispatch with checked thread affinity, Unicode, DPI coordinates, lifetime-safe callbacks and owned native resources.
R71| Behavioral integration tests compile and run actual native executables and use independent file, process, stream, network and GUI observations as their oracles.
R72| Full runtime qualification requires the mandatory behavior lanes; missing APIs, missing execution, skipped required facilities and self-reported success cannot qualify an implementation.
R73| E0440, E0441 and E0442 diagnose consumed-resource use, incompatible-borrow consumption and statically known thread-affinity violations respectively.

These resolutions are frozen for the revised edition-2026 baseline.

Corrected Lexical and Layout Definition
3.1 Source normalization

Before lexical analysis:

source is decoded as UTF-8;
a leading UTF-8 byte-order mark is discarded;
CRLF and CR are normalized to LF;
Unicode source is normalized to NFC outside raw byte-oriented compiler internals;
a missing final LF is conceptually added;
trailing horizontal whitespace outside literals is ignored.
Tabs remain legal inside:

text literals raw text escaped character content comments

Tabs are illegal in structural indentation.

Final Layout Algorithm
Layout tokenization is purely lexical.

The lexer does not need to know whether the previous line was a grammatically valid block opener.

The indentation stack initially contains:

[0]

For every significant physical line outside "()", "[]", and "{}":

indent = count of leading ASCII spaces

The count must be divisible by four.

Otherwise:

E0100 invalid indentation width

Let:

current = top(indent_stack)

Equal indentation

If:

indent == current

emit:

NEWLINE

before the first token of the next significant line.

Increased indentation

If:

indent > current

the increase must be exactly four spaces:

indent == current + 4

Then emit:

NEWLINE INDENT

and push "indent".

A larger jump is:

E0101 indentation skips a structural level

Whether that "INDENT" is grammatically legal is decided by the parser.

An unexpected block therefore becomes:

E0201 unexpected block

Decreased indentation

If:

indent < current

the target indentation must already occur in the stack.

Emit:

NEWLINE DEDENT...

until that value becomes the top.

Otherwise:

E0102 inconsistent dedent

Continuations

Inside:

(...) [...] {...}

physical newlines are ordinary whitespace.

A physical line whose first significant token is:

->

continues the preceding pipeline expression.

No backslash-continuation mechanism exists.

At EOF, emit one final "NEWLINE" if necessary and enough "DEDENT" tokens to restore indentation zero.

Final Reserved Vocabulary
Edition 2026 keywords are:

abi alias alloc and as break capability choose const continue defer else export fail false for foreign from give gives if import in io is let loop module move not nothing or own public pure raw record return_ routine static test then thread to trap true try unsafe until var variant when where while with yield_

"return_" and "yield_" are reserved without edition-2026 semantics.

Predeclared identifiers include:

byte short int long ubyte ushort uint ulong float double bool char text size ssize void never

option result view span ref mut buffer array raw atomic string ordering

Prelude suppression is controlled by package configuration:

no_prelude = true

or:

no_std = true

The former "import std.prelude as nothing" form is removed.

Completed Punctuation
Edition 2026 recognizes:

/ % & | ^ ~ << >> == != < <= > >= = += -= *= /= %= &= |= ^= <<= >>= -> => ... . .. ..= , : :: ? @ ( ) [ ] { }
"..." is legal only in:

foreign variadic declarations future-edition-reserved positions

Elsewhere:

E0208 illegal ellipsis

Corrected Statement Grammar
statement = simple_statement NEWLINE | compound_statement ;

simple_statement = let_stmt | var_stmt | assign_stmt | expr_stmt | give_stmt | fail_stmt | break_stmt | continue_stmt | defer_stmt ;

compound_statement = if_stmt | choose_stmt | loop_stmt | while_stmt | for_stmt | from_stmt | continue_until_stmt | defer_block | unsafe_block ;

block = NEWLINE INDENT statement { statement } DEDENT ;

defer_stmt = 'defer' expr ;

defer_block = 'defer' block ;

label = '@' identifier ;

break_stmt = 'break' [ label ] ;

continue_stmt = 'continue' [ label ] ;

loop_stmt = [ label ':' ] 'loop' block ;

while_stmt = [ label ':' ] 'while' expr block ;

for_stmt = [ label ':' ] 'for' [ 'var' ] pattern 'in' expr [ 'while' expr ] block ;

from_stmt = [ label ':' ] 'from' identifier [ ':' type ] '=' expr 'to' expr block ;

continue_until_stmt = [ label ':' ] 'continue' 'until' expr block ;

The 'type' in a from_stmt is restricted semantically to a built-in integer type. The expressions following 'while' and 'until' must resolve to bool.

At the current delimiter nesting level, the reserved 'while' token ends a for-loop iterable expression, and the reserved 'to' token separates the two from-loop bound expressions. These rules introduce no textual macro or parser-dependent indentation behavior.

Labeled jumps therefore use:

break @outer
continue @scan

The sequence 'continue until' introduces a compound statement, not a continue jump. Its indented block is required. Bare 'continue' and 'continue @label' retain their existing jump meanings.

The simple-statement alternatives explicitly include the already-defined expression form of defer, so loop cleanup examples follow the statement grammar.

Structured Loop Completion — For ... While / From ... To / Continue ... Until

These are normative structured statements in edition 2026. Source spelling is lowercase:

for ... while ...
from ... to ...
continue until ...

The dots above mark explanatory placeholders. They are not source tokens and do not authorize use of the reserved ellipsis token in loop headers.

All three forms use the existing four-space block syntax. Their behavior is resolved by the semantic frontend and represented in PCIR. They require no loop-specific allocation, closure object, scheduler, foreign compiler, or C++ export step. Operations written in their expressions and bodies retain their ordinary allocation, ownership, effect, and failure behavior.

Loop Form Summary

for item in items while condition
    Iterates through items and stops at the first false condition. The condition is checked after binding each candidate and before its body executes.

from index = first to last
    Visits both integer endpoints, moving by one toward last. Equal endpoints produce one iteration. Descending bounds count downward automatically.

continue until condition
    Executes the body, then checks condition. False repeats; true finishes. Reaching the loop normally executes its body at least once.

For ... While — Guarded Iteration

Canonical syntax:

for pattern in iterable while condition
    statements

The existing mutable-pattern form remains available:

for var pattern in mutable_iterable while condition
    statements

The existing unguarded "for ... in ..." form remains valid. The optional "while" clause adds a stopping guard to that form.

Execution order is exact:

1. Evaluate the iterable expression once and obtain its iterator under the existing iterator rules.
2. Request the next candidate.
3. If the iterator is exhausted, finish without evaluating the guard.
4. Bind the candidate to the pattern.
5. Evaluate the guard exactly once for this candidate. Its type must be bool.
6. If false, perform normal cleanup of the candidate and iterator state and finish the loop.
7. If true, execute the body.
8. On ordinary body completion, perform iteration cleanup and request the next candidate.

The guard may refer to names bound by the pattern and to names visible in the enclosing scope. It cannot refer to declarations introduced only inside the body.

The first candidate that fails the guard has already been obtained from the iterator. Its body does not execute; the candidate is not put back. Later candidates are not requested. If an externally retained iterator is resumable under its normal borrowing rules, its position reflects that candidate acquisition.

This is a stopping guard. It does not skip a rejected candidate and resume searching. Per-element selection uses an explicit body condition or an iterator filter.

"for var" retains the existing exclusive-borrow requirements. A guard does not create permission to mutate a shared iterable, overlap exclusive borrows, or keep a borrowed item alive across an incompatible iterator advance.

An ordinary "continue" targeting this loop performs iteration cleanup and advances to the next candidate. It does not test the current candidate again. The guard is evaluated when the next candidate has been bound.

An ordinary "break" targeting this loop performs normal cleanup and exits without requesting another candidate or evaluating another guard.

Example:

routine sum_before_limit(values: view<int>, limit: int) gives int
    var total: int = 0

    for value in values while value < limit
        total += value

    give total

For the input values 2, 4, 9, 1 with limit 8, the body sees 2 and 4. The result is 6. The value 9 stops the loop; the later value 1 is not visited.

From ... To — Inclusive Directed Integer Iteration

Canonical syntax:

from index = first to last
    statements

An explicit induction type may be supplied:

from index: int = first to last
    statements

"index" is a fresh immutable loop binding. It is in scope only within the body. Each iteration introduces that iteration's binding; assigning to it is an error. Ordinary copying, borrowing, and closure-capture rules apply.

The induction type must be one of Paracine's built-in signed or unsigned integer types, including size and ssize. bool, char, floating-point types, raw pointers, and user records are not induction types.

Both bounds must resolve to the same induction type. An explicit type provides context for literal fitting. Without an explicit type, ordinary expression inference and contextual literal fitting select the common integer type. No implicit numeric conversion is introduced. Differently typed nonliteral bounds require an explicit conversion.

Execution order is exact:

1. Evaluate first once.
2. Evaluate last once, after first.
3. Save both values using their resolved integer type.
4. Choose an ascending direction if first < last, a descending direction if first > last, or a single iteration if they are equal.
5. Bind the current value and execute the body.
6. After normal body cleanup, compare the current value with the saved endpoint.
7. If equal, finish immediately.
8. Otherwise, advance by exactly one toward the endpoint and execute the next iteration.

Both endpoints are included. "from index = 3 to 1" visits 3, 2, 1. It is not an empty range. A zero-iteration requirement must be expressed with an explicit condition or an appropriate existing iterator.

The direction and endpoint do not change if the source variables used to compute the bounds are later modified. No bound expression is reevaluated by the loop.

Completion is tested before advancing. The loop never forms MAX + 1 after its final ascending iteration or MIN - 1 after its final descending iteration. The unsigned zero endpoint is handled by the same rule.

The loop does not require computing last - first, an absolute difference, or an iteration count in the induction type. Traversing a signed range across zero therefore introduces no hidden overflow or implicit conversion. This rule does not change the language's modular arithmetic for explicit source operations.

An ordinary "continue" targeting this loop performs iteration cleanup and reaches the endpoint test. At the final endpoint it exits; otherwise it advances once toward the saved endpoint.

An ordinary "break" targeting this loop performs normal cleanup and exits without advancing.

No step keyword or implicit arbitrary stride is defined by this form. The progression is exactly +1 or -1 as selected above. Other progressions use existing iterator APIs or explicit "while" logic.

Examples:

routine ascending_sum() gives int
    var total: int = 0

    from index: int = 1 to 5
        total += index

    give total

The result is 15.

routine descending_digits() gives int
    var digits: int = 0

    from digit: int = 3 to 1
        digits = digits * 10 + digit

    give digits

The result is 321.

routine equal_endpoint_count() gives int
    var count: int = 0

    from index: int = 7 to 7
        count += 1

    give count

The result is 1.

Continue ... Until — Post-Test Repetition

Canonical syntax:

continue until condition
    statements

This is a standalone compound statement. It may introduce a loop even when there is no enclosing loop. Its header condition is evaluated after the body.

Execution order is exact:

1. Enter the body without evaluating condition.
2. Execute the body.
3. On ordinary body completion, execute normal cleanup for the completed iteration.
4. Evaluate condition exactly once. Its type must be bool.
5. If true, finish.
6. If false, begin the next iteration.

When reached through normal control flow, the body executes at least once, including when the condition would have been true before entry. A terminal trap or an explicit control transfer can prevent completion of that first body.

The condition resolves names in the enclosing lexical scope. Body-local declarations are outside its scope. Every path that reaches the condition must leave each referenced place initialized and legally usable. State needed by the condition is normally declared before the loop and updated by the body.

An ordinary "continue" targeting this loop performs iteration cleanup and then evaluates the condition. It does not jump directly to the next body entry and bypass the test.

An ordinary "break" targeting this loop performs normal cleanup and exits without evaluating the condition.

Example:

routine count_to_three() gives int
    var count: int = 0

    continue until count >= 3
        count += 1

    give count

The result is 3.

Example showing "continue" reaching the post-test:

routine post_test_continue() gives int
    var count: int = 0

    continue until count >= 3
        count += 1
        continue

    give count

The result is 3. The condition is tested after each "continue".

The construct performs sequential repetition. Any waiting, sleeping, yielding through a library, synchronization, or OS event processing must be explicitly provided by operations in its expressions or body.

Loop Labels and Statement Disambiguation

Each new form accepts the existing loop-label prefix:

@scan: for value in values while value >= 0
    statements

@count: from index: int = 0 to 9
    statements

@retry: continue until finished
    statements

"statements" in these three header illustrations is a placeholder, not a language keyword.

"break @label" exits the targeted enclosing loop. "continue @label" transfers to that loop's continuation point:

for ... while ... : iteration cleanup, then next candidate acquisition
from ... to ... : iteration cleanup, then endpoint test and possible advance
continue until ... : iteration cleanup, then the post-test condition

Labels must resolve to an enclosing loop. A jump cannot enter a loop, select a sibling loop, or cross a routine or closure boundary. Intervening scopes receive normal cleanup.

The token immediately following "continue" determines the statement form:

until : parse the compound "continue until" loop and require its condition and indented block
@ : parse a labeled continue jump
NEWLINE : parse an unlabeled continue jump

No other "continue expression" form is introduced. Bare "continue" outside a loop remains an error. "continue until condition" outside a loop is valid because it introduces its own loop.

"to" and "until" are reserved lowercase keywords. "from" was already reserved. The existing "@from(...)" returned-borrow attribute retains its meaning.

Layout tokenization remains purely lexical. It does not inspect loop meaning. The parser recognizes these headers and consumes their ordinary NEWLINE / INDENT / DEDENT blocks.

Ownership, Cleanup, Effects, and Failure

The new forms use the existing ownership and nonlexical-region rules. They create no exemption for escaping borrows, shared/exclusive aliasing, moves, thread safety, or initialization.

On ordinary iteration completion, continue, break, give, fail, or propagated recoverable failure, cleanup runs for every scope actually exited. Registered defers execute under the existing scope-exit rules before the targeted loop continuation point is reached.

For "continue until", body-scope cleanup precedes the condition test, so a defer that changes enclosing state is observable by that test. Inner scopes crossed by a labeled jump are also cleaned before the target's test or advance.

Loop-owned iterator state is cleaned on normal loop exit. Owned values and temporaries associated with guards and bounds follow the same normal expression and scope rules. An item rejected by a "for ... while" guard is cleaned even though its body never ran.

Terminal traps retain the final failure model: they terminate the process without unwinding, defer, or disposal. A trap in a bound, iterator operation, guard, body, cleanup operation, or post-test is not converted into a loop result.

The inferred effect set is the union of effects required by iterable acquisition, iterator advancement, guards, bounds, post-test conditions, body statements, and cleanup. A loop header is not implicitly pure. The induction mechanism itself requires no allocation and introduces no overflow trap.

A condition must have type bool. There is no implicit numeric truthiness. A false "for ... while" guard stops; a true "continue until" condition stops. Side-effect order follows the execution rules above.

Compiler and Backend Completion for Structured Loops

SAGE Semantic Analysis resolves loop bindings, expression types, ownership, effects, initialization, labels, and precise break/continue targets. The SKM records the resulting facts.

Structural Analysis may classify guarded iterators, directed integer inductions, and post-test loops. Stylometric suggestions cannot select a different stopping condition, change endpoint inclusion, infer a different direction, or authorize an invalid jump.

PCIR represents the verified control flow with explicit entry, test, body, continuation, and exit edges as applicable. Iterator advancement, guard evaluation, endpoint comparison, cleanup, and post-test evaluation have explicit sequencing. Synthetic induction values are ordinary defined values; no poison or undefined sentinel is introduced.

The PCIR verifier checks that a "for ... while" continuation advances before testing the next bound candidate; a "from ... to" continuation tests completion before advancing; and a "continue until" continuation reaches its post-test. A break edge bypasses the corresponding next-iteration work. On each normal exit or jump path, all cleanup required for that path completes before its destination is reached.

PMIR and the direct native backend lower those verified edges to ordinary branches, comparisons, and integer operations. The constructs need no new public ABI, object format, DLL contract, or runtime scheduler.

Optimization may remove, fuse, unroll, or vectorize loop work only after proving semantic equivalence. It preserves iterator consumption, one-time bound evaluation, guard effects, inclusive endpoints, normal cleanup, post-test entry behavior, trap order, and the targets of labeled jumps.

If present, the optional C++20 exporter preserves the same behavior. It may use generated loop structures or reconstructed control flow. It must not implement an inclusive integer loop by unconditionally incrementing after its final endpoint, or implement "continue until" with a jump that bypasses its condition or required cleanup.

All forms are eligible for const evaluation when every executed operation is otherwise const-legal. Const-evaluation step limits remain enforced; the syntax creates no exemption for nontermination or excessive work.

Structured Loop Conformance Requirements

Mandatory frontend cases cover each form with and without labels, guarded and unguarded "for" loops, mutable iterator bindings, nested loops, bare and labeled continue jumps, and disambiguation between a continue jump and a "continue until" compound statement.

Mandatory semantic cases include:

1. Empty guarded iterator: zero guard evaluations and zero body executions.
2. Initially false iterator guard: one candidate acquisition, one guard evaluation, zero body executions, normal candidate cleanup, and no later candidate request.
3. Candidate-dependent guard: 2, 4, 9, 1 under "value < 8" visits 2, 4 only.
4. Inclusive ascending range: 1 to 3 visits 1, 2, 3.
5. Inclusive descending range: 3 to 1 visits 3, 2, 1.
6. Equal endpoints: 7 to 7 executes once.
7. One-time bounds: bound-side effects occur once each, left to right; later source-variable mutation does not change the saved bounds.
8. Integer limits: ascending completion at MAX, descending completion at MIN, unsigned descent to zero, and "continue" on the final endpoint introduce no extra advance or overflow.
9. Post-test entry: a condition already true before entry still permits one body execution when that body falls through normally.
10. Post-test continue: every targeted continue reaches the condition after normal cleanup; true exits and false repeats.
11. Break and recoverable routine exits: required cleanup runs, while bypassed guards and post-tests do not run.
12. Nested labeled jumps: intervening cleanup executes and the targeted loop's own continuation rule is used.
13. Defer ordering: a body defer that updates enclosing state is visible to a "continue until" test.
14. Rejected programs: non-bool conditions, noninteger or mismatched range bounds, assignment to the immutable induction binding, out-of-scope condition names, invalid labels, invalid borrows, and malformed blocks produce diagnostics.
15. Trap behavior: traps in loop operations retain process termination and no-unwind semantics; optimizations preserve preceding observable effects.

The mandatory native semantic oracle compares these cases between the PCIR reference interpreter and direct native execution. When the optional C++20 exporter is installed, its differential suite includes the same loop behavior.


Completed Top-Level Grammar
item_body = routine_decl | record_decl | variant_decl | alias_decl | const_decl | static_decl | thread_static_decl | capability_decl | foreign_decl | test_decl ;

static_decl = 'var' 'static' identifier ':' type '=' const_expr ;

thread_static_decl = 'thread' 'var' identifier ':' type '=' const_expr ;

A "thread var":

has one native-thread-local instance;
requires a const-evaluable initializer;
must be trivially disposable in edition 2026;
may be mutated safely by its owning thread;
may not be borrowed across thread boundaries;
may not appear directly in stable DLL ABI-visible layout.
Nontrivial thread-local destructors are deliberately excluded.

Final Closure Model
Paracine supports zero-runtime-overhead statically typed closures.

There is:

no GC closure object no mandatory boxing no dynamic closure dispatch no hidden heap allocation

9.1 Syntax

closure_expr = 'routine' '(' [ closure_param_list ] ')' [ 'gives' type ] ( '=>' expr | block ) ;

closure_param_list = closure_param { ',' closure_param } [ ',' ] ;

closure_param = [ param_mode ] identifier [ ':' type ] ;

Example:

let twice = routine(x: int) => x * 2

let threshold = 10 let over = routine(x: int) => x > threshold

Parameter types may be inferred only from a known expected callable type.

Otherwise they must be explicit.

9.2 Capture semantics

Reading an outer place produces:

shared capture

Mutating an outer "var" place produces:

exclusive capture

Consuming a move-only outer value produces:

own capture

Explicit ownership-biased capture:

move routine(x: int) => x + base

captures used values by ownership wherever legal.

9.3 Representation

A capturing closure becomes conceptually:

closure$42 { captured fields... }

plus a statically known call implementation.

The closure is an ordinary monomorphic value.

It may:

live on the stack move be stored in another aggregate be specialized be passed generically cross a thread boundary when its captures satisfy sendability

9.4 Callable capability

The compiler defines an intrinsic callable relationship conceptually equivalent to:

capability callable<F, A, R>

meaning:

F may be called with argument tuple A and produces R

User code cannot forge this intrinsic capability.

9.5 Routine-pointer conversion

A noncapturing closure may convert to a matching:

routine(...) gives ...

pointer.

A capturing closure may not.

9.6 Escaping

A closure containing borrowed captures may not outlive those captures.

A closure may freely escape its lexical definition when every capture is owned by value.

The normal region analysis enforces this.

9.7 Public ABI

Anonymous closure types may not directly appear in:

public signatures export signatures foreign declarations

Public generic APIs may accept a generic type constrained by "callable".

Standard Iterators Updated for Closures
Standard adapters accept statically typed callables.

Conceptually:

routine map<I, T, U, F>( own it: I, own f: F ) gives map_iter<I, T, U, F> where I is iterator, F is callable<(T), U>

The same model applies to:

filter fold any all find sort_by_key parallel_for

Routine pointers satisfy the callable requirement.

Closure environments may disappear completely after:

specialization inlining SROA DCE

Pipeline fusion remains mandatory for the standard built-in fusion cases.

Associated Types Resolution
Associated types do not exist in edition 2026.

The iterator relationship remains:

iterator<I,T>

rather than:

I::Item

This preserves:

simpler name resolution simpler interface serialization simpler monomorphization no projection-normalization system fewer cyclic type constraints straightforward PCIR specialization

A future language edition may add associated types.

Edition 2026 does not depend upon them.

Type-System Representation Corrections
12.1 "never"

"never" is an uninhabited predeclared type with no runtime representation.

Expressions producing "never" include:

trap(...) unreachable() process.exit(...) calls to routines declared gives never

"never" coerces to every expected type.

12.2 "ref" and "mut"

They are aliases:

ref = view constrained to count == 1 mut = span constrained to count == 1

They do not introduce another ownership mechanism.

12.3 Raw-pointer options

"raw" is nullable.

Therefore:

option<raw>

does not have a guaranteed null niche.

It may use an explicit tag or another proven representation.

The representation remains nonobservable at source level.

12.4 Canonical safe empty pointer

Safe empty:

view span text

values use a canonical aligned non-null sentinel.

An empty "buffer" uses:

ptr = canonical sentinel count = 0 capacity = 0

This permits guaranteed null-niche option encodings for:

option<view> option<span> option option<buffer>

Unsafe constructors must normalize raw representations before exposing a safe value.

12.5 "option"

Tier-1 representation:

0 = false 1 = true 2 = none

12.6 "option"

Tier-1 uses the invalid scalar:

0x00110000

as "none".

"raw" Parameter Mode Resolution
The syntax:

raw x: T

grants raw authority associated with "T".

It is valid only when "T" is:

raw an @raw_authority record an explicitly @raw_authority aggregate

Valid:

raw p: raw

Invalid:

raw x: int

Diagnostic:

E0408 meaningless raw parameter mode

"raw" itself remains the machine-pointer type.

Final Effect System
The effect set is:

alloc io thread trap unsafe

"pure" means the empty set.

It cannot be combined with another effect.

Examples:

with io, alloc

and:

with pure

14.1 Inference

Routine bodies receive minimum inferred effect sets.

Typical contributions:

allocation → alloc file/console/network/process → io thread/synchronization block → thread possible defined trap → trap unsafe authority operation → unsafe

Calls union callee effects.

14.2 Explicit annotations

A "with" clause is an allowed upper bound.

The inferred set must fit within it.

Violation:

E0360 undeclared effect

Private routines without explicit effects infer them.

Public/export routine interfaces record their resolved effects.

14.3 Unsafe encapsulation

A routine may internally use an "unsafe" block without exposing "with unsafe" if callers need not uphold raw invariants.

A routine whose correctness depends on caller-supplied unsafe invariants must declare:

with unsafe

14.4 Trap effect

Potential bounds failure, division failure and explicit assertion contribute "trap".

Optimization may prove individual traps unreachable.

Public effect identity is established before machine optimization.

Final Failure Model
Paracine has two failure classes.

Recoverable failure

Represented by:

result<T,E> option

using:

try fail choose

Normal cleanup executes.

Terminal trap

Includes:

explicit trap bounds violation division by zero assertion failure unreachable stack exhaustion configured OOM trap

A trap:

does not unwind does not execute defer does not execute dispose does not become a result does not resume does not become thread_panic

Trap handler:

routine(trap_code, text) gives never

Thread Trap Resolution
There is no recoverable language trap across thread boundaries.

There is no:

std.thread.on_trap(recover)

and no trap-derived:

thread_panic

A trap on any normal Paracine thread terminates the process.

"join" is:

routine join(own h: join_handle) gives result<R, thread_error>

"thread_error" reports operating-system/runtime join failures.

It does not represent language traps.

Structured Thread Scope Resolution
Canonical scoped threading is generic over callable values:

routine scope(own f: F) where F is callable<(var thread_scope), void>

Example:

var values = ...

scope(routine(var s) spawn_scoped(var s, routine() use(values) ) )

Captured borrows are known to the compiler.

"scope" cannot return until all scoped child threads have completed.

Borrow lifetimes therefore terminate before the enclosing scope call returns.

Interior Mutability Resolution
Safe interior mutability is confined to compiler-known and standard-library abstractions whose implementation encapsulates unsafe authority.

Edition 2026 includes:

atomic mutex rwlock once channel internal state approved thread-safe runtime handles

Thread-affine std.gui runtime state is also an approved interior-mutable standard abstraction under its checked thread-affinity contract.

Ordinary user records may not mutate through a shared borrow.

Index Disjointness Resolution
The borrow checker treats:

a[i] a[j]

as potentially overlapping unless it proves otherwise.

Proof may use:

constant inequality normalized ranges dominating comparisons induction-variable facts range facts

Programs requiring guaranteed independently mutable regions may use:

split_at chunks_mut split_first_mut

or equivalent standard disjointness primitives.

Macro and Compile-Time Generation Resolution
Edition 2026 provides no:

textual macros token macros syntax macros procedural compiler plugins arbitrary source-generating compiler callbacks

It does provide:

const evaluation const generics monomorphization @derive @feature explicit build-tool steps

Built-in derives:

equatable ordered hashable printable defaultable

Async Resolution
Edition 2026 contains no:

async await coroutine syntax mandatory event loop hidden scheduler

Asynchronous systems may use:

threads nonblocking APIs channels callbacks explicit state machines libraries

No coroutine ABI is reserved in 1.0.

Other Native Targets Resolution
The mandatory direct-native target is:

windows-x86_64-msx64

Additional native platforms require complete target profiles defining:

primitive widths endianness pointer width ABI instruction lowering object format executable format relocation model unwind model TLS atomics runtime bindings standard-library realization

The optional C++20 exporter may provide portability to environments without a direct Paracine backend.

That portability path is a convenience and compatibility mechanism.

It is not required to establish Paracine as a complete language.

A Paracine implementation that supports only the Tier-1 direct-native path may still be fully conforming.

Hardened Hashing Resolution
Default:

map<K,V> set

use a process-randomized SipHash-family state seeded from OS entropy.

Iteration order is unspecified and may differ between runs.

Stable deterministic hashing remains available through:

stable_hasher hash_of fnv1a crc32 xxh3_64

A deterministic table can be explicitly created with:

map_with_hasher(stable_hasher())

Standard-Library Network Completion
The "std" tier includes:

std.net

Tier-1 networking uses Winsock through:

std.win.ws2_32

Core types:

ip_addr ipv4_addr ipv6_addr socket_addr tcp_stream tcp_listener udp_socket shutdown socket_error

Representative API:

routine resolve(host: text, service: text) gives result<buffer<socket_addr>, socket_error>

routine tcp_connect(addr: socket_addr) gives result<tcp_stream, socket_error>

routine tcp_bind(addr: socket_addr) gives result<tcp_listener, socket_error>

routine accept(var listener: tcp_listener) gives result<(tcp_stream, socket_addr), socket_error>

routine read(var stream: tcp_stream, dst: span) gives result<size, socket_error>

routine write(var stream: tcp_stream, src: view) gives result<size, socket_error>

routine udp_bind(addr: socket_addr) gives result<udp_socket, socket_error>

routine send_to( var socket: udp_socket, data: view, addr: socket_addr ) gives result<size, socket_error>

routine recv_from( var socket: udp_socket, dst: span ) gives result<(size, socket_addr), socket_error>

Socket initialization is demand-linked and one-time.

"no_std" provides no network runtime.

Dynamic-Library Completion
"std.dynlib" provides:

routine open(path: text) gives result<library, io_error>

routine symbol(self: library, name: text) gives result<raw, io_error> with unsafe

routine close(own lib: library)

Typed conversion of dynamically obtained symbols requires unsafe authority.

Global Allocator Completion
The global allocator is initialized before "main".

Replacement is legal only through the allocator startup phase.

Example:

@startup routine configure_allocator()

Once the first allocation occurs, allocator replacement locks permanently.

Only the root package may select the global allocator provider.

Dependency packages cannot silently replace it.

Production Runtime and Observable-Behavior Completion

Normative revision: November 7, 2026.

This completion defines reliable owned resources, recoverable operating-system failures, interactive input, child-process control, networking, native windows, and behavior-based integration qualification. It is part of the revised edition-2026 baseline. API names below are normative library interfaces, not new language keywords. Existing ownership, effects, terminal traps, and native-first architecture govern every interface.

1. Resource Ownership and Reliable Handle Law

A resource token is a move-only value whose concrete native representation is private. The standard library provides distinct types for file, directory, pipe reader, pipe writer, child, process scope, socket, listener, window, and paint context. A file is not a socket or window. Native invalid sentinels are never exposed as successfully constructed safe handles.

Successful construction returns exactly one owner. Borrowing a resource does not transfer ownership. Moving or explicitly closing it consumes that owner. Ordinary disposal releases its owned resources exactly once. Use after move, duplicate disposal, and closing through an outstanding incompatible borrow are static errors. Resource acquisition that fails returns no owner and releases partially acquired internal resources.

Default-created OS handles are noninheritable. A raw native-handle export requires an explicit unsafe contract describing ownership, rights, lifetime, and how the handle may be returned. Borrowing a native representation does not grant permission to close it. Duplicating an OS object is a fallible, explicit operation; it is not an implicit copy of a resource token.

An explicit close consumes its token even when close reports an error. An application must not retry using the consumed native number: the OS may already have recycled it. Implementations retain internal state needed to finish an outstanding operation and must not release borrowed storage while the kernel can still access it.

Normal disposal is nonthrowing and does not turn routine I/O failure into a trap. Fallible finalization uses explicit close, flush, sync, or commit APIs. An optional nonthrowing cleanup-error observer can record disposal failures; it cannot report a failed commit as successful. Programs requiring confirmed persistence must use and check explicit finalization before the owner leaves scope. Terminal traps retain the no-unwind rule.

Resource rights are checked before dispatch. Closed-state or generation checks protect registered native callbacks and queued events against stale native IDs; such checks supplement static ownership. They do not make raw-handle forgery safe. Thread transfer requires the resource's declared sendability. A shared borrow cannot change a file cursor or mutate an ordinary resource.

2. Recoverable System Errors

std.error defines system_error, error_kind, error_domain, and allocation-free error classification. io_error, socket_error, process_error, and gui_error are domain-specific aliases of system_error unless an API explicitly returns an owned failure aggregate.

A system_error preserves:

kind : stable Paracine error classification
domain : Paracine, Win32, Winsock, HRESULT, or another declared native domain
native_code : original native status bits, interpreted within domain
operation : stable operation identifier
bytes_transferred : completed progress for a failed compound byte transfer

The base error requires no heap allocation, remote lookup, or dynamic message formatting. Native status is captured immediately at the failing call, before cleanup or another native call can replace it. Diagnostic formatting is separate and fallible when it allocates. Optional owned context and cause chains are explicit values with documented size limits.

Stable error kinds include:

not_found already_exists permission_denied invalid_argument invalid_encoding invalid_state unsupported busy would_block interrupted cancelled timed_out broken_pipe connection_refused connection_reset address_in_use resource_exhausted write_zero unexpected_eof limit_exceeded os_failure

Classification never discards the original native domain/code. Unknown native failures map to os_failure and retain their native identity. Missing files, denied access, failed process launches, timeouts, cancellation, disconnected peers, malformed user input, and GUI creation failures return recoverable errors. They are not assertions or terminal traps.

Public classification helpers include:

routine is_kind(problem: system_error, expected: error_kind) gives bool with pure
routine make(kind: error_kind) gives system_error with pure

Each listed kind is exported as a named error_kind constant in std.error. make constructs a Paracine-domain error with zero transferred bytes. It cannot forge a native success status.

An empty successful stream read and an OS failure are distinguishable. Failure must never be disguised as an empty string, zero exit status, invalid safe handle, zero-valued socket, or successful zero-byte write to a nonempty request. Invalid use detected statically remains a diagnostic. Unsafe contract violations retain erroneous-execution semantics.

No implicit error swallowing is introduced. Ignoring a result requires the existing explicit discard mechanism or deliberate handling. A nonzero child exit status is a successfully observed process outcome; it is not a launch error.

3. Deadlines, Cancellation, and Transfer Progress

std.time.deadline is an absolute monotonic deadline or an explicit infinite deadline. std.time.after_ms(milliseconds: ulong) constructs a saturating monotonic deadline without wall-clock dependence. A timeout never depends on local calendar changes.

An I/O context contains a deadline and an optional cancellation token. An operation that cannot honor the requested cancellation/deadline mode returns unsupported before beginning that mode. It must not silently ignore an option. Cancellation requests can race with successful completion; the returned result describes the actual selected completion, including progress. Cancellation is not rollback.

Deadline-aware operations report timed_out only after accounting for completed work and settling all kernel access to borrowed memory. A cancellation request alone is not completion. Implementations may use native overlapped operations, bounded polling, or retained worker state, with the same lifetime and result contract. No language async/await construct or mandatory scheduler is introduced.

read_some and write_some may complete short. Compound read_exact/write_all operations loop over progress and distinguish EOF, write_zero, cancellation, timeout, and other native errors. On failure bytes_transferred reports the cumulative completed prefix for that call; partial filesystem or network effects remain observable. Retrying an entire failed transfer is never automatically declared safe.

A zero-length request succeeds without accessing its data buffer; callers must not use that success as evidence of stream EOF or peer liveness. For a nonempty byte-stream read, ok(0) means EOF. A nonempty write that makes no progress is write_zero, not a success loop that spins forever. Zero-length datagrams have distinct message semantics.

Native read-side pipe-closed statuses denoting normal peer closure normalize to stream EOF. Write-side broken pipe remains a recoverable failure. Connection reset and other exceptional failures do not normalize to EOF.

4. File and Filesystem Completion

std.fs.file is an owned, move-only file resource. Paths are validated Unicode text: UTF-8 at the language boundary and checked UTF-16 at the Windows boundary. Embedded NUL is rejected. Names are not silently truncated, case-folded, redirected, or reinterpreted as shell commands. Long-path support is explicit in the Tier-1 manifest/profile. Reparse points follow the declared open policy; opening a path does not establish a sandbox or prove a prior path check race-free.

Open options separately specify access, creation, sharing, append behavior, reparse handling, and transfer mode. Invalid combinations fail before acquisition. Truncation is never implied by a read operation. The following constructors return documented options:

read_only() : open an existing file for reading; allow other readers
create_new() : create exclusively for writing; fail if the name exists
create_or_truncate() : explicitly create or truncate for writing
append_existing() : open an existing file for append-only writing

These constructors allow sharing for readers and disallow sharing for writers/deleters unless explicitly overridden. All produce noninheritable handles. Sharing violations are recoverable. They are distinct from permissions failures in retained native status even if a common policy groups them as busy.

Canonical std.fs APIs include:

routine open(path: text, options: open_options) gives result<file, io_error> with io
routine read_some(var f: file, dst: span<ubyte>) gives result<size, io_error> with io
routine write_some(var f: file, src: view<ubyte>) gives result<size, io_error> with io
routine read_exact(var f: file, dst: span<ubyte>) gives result<void, io_error> with io
routine write_all(var f: file, src: view<ubyte>) gives result<void, io_error> with io
routine read_at(f: file, offset: ulong, dst: span<ubyte>) gives result<size, io_error> with io
routine write_at(f: file, offset: ulong, src: view<ubyte>) gives result<size, io_error> with io
routine seek(var f: file, offset: long, origin: seek_origin) gives result<ulong, io_error> with io
routine tell(f: file) gives result<ulong, io_error> with io
routine size_of(f: file) gives result<ulong, io_error> with io
routine truncate(var f: file, length: ulong) gives result<void, io_error> with io
routine flush(var f: file) gives result<void, io_error> with io
routine sync_data(var f: file) gives result<void, io_error> with io
routine close(own f: file) gives result<void, io_error> with io
routine read_bytes(path: text, limit: size) gives result<buffer<ubyte>, io_error> with io, alloc
routine rename(source: text, destination: text, policy: replace_policy) gives result<void, io_error> with io

Deadline/cancellation variants take an explicit I/O context and obey section 3. Positional reads/writes do not change the shared cursor. Their span borrows still require disjoint mutable destinations; operating on a shared file resource does not permit unsynchronized access to the same user memory.

File offsets and lengths use the declared 64-bit interface. Native limits, offset addition, buffer-size products, and narrowing conversions are checked and return recoverable errors. A file need not fit address space merely because its length is representable. read_bytes enforces its limit while reading, including if the file grows after the initial size query.

std.mem.view_of(buffer<ubyte>) returns a shared byte view. std.mem.bytes(text) returns a shared view of its exact UTF-8 bytes. These conversions borrow storage and allocate nothing.

flush drains library buffering to the OS; sync_data additionally requests the target's supported persistent-media synchronization and reports its failure. close alone is not a persistence guarantee. The specification does not promise survival of arbitrary device failure, dishonest hardware caches, or filesystem behavior beyond the declared platform guarantee.

Atomic replacement is a separate same-volume operation with explicit replace/no-replace policy. std.fs.no_replace is the no-overwrite rename policy. Cross-volume copy-and-delete is not advertised as atomic rename. Durable replacement exposes its actual platform guarantee, including metadata limitations. Cancellation after data was written cannot claim the old file is intact without a separately proven transaction.

Append-only mode prevents ordinary cursor writes from overwriting earlier bytes. Atomicity applies only to the native write unit guaranteed by the selected filesystem; multi-write records and multi-process logging require explicit framing/synchronization. File locks, metadata queries, directory iteration, and watchers return typed results and document their consistency and cancellation limits.

5. Interactive Input and Standard Streams

std.io provides owned pipe endpoints, borrowed standard-stream access, bounded buffered readers, text encoding, and console event APIs. Acquiring a line_reader owns its decoder/buffer state; disposing it does not close the process's borrowed stdin handle. Independent readers of the same stdin stream require explicit coordination. stdin, stdout, and stderr are distinct endpoints.

Canonical APIs include:

routine stdin_reader() gives result<line_reader, io_error> with io, alloc
routine read_line(var input: line_reader, max_bytes: size) gives result<option<string>, io_error> with io, alloc
routine discard_line(var input: line_reader) gives result<void, io_error> with io
routine write_stdout(src: text) gives result<void, io_error> with io
routine write_stderr(src: text) gives result<void, io_error> with io
routine write_stdout_bytes(src: view<ubyte>) gives result<void, io_error> with io
routine write_stderr_bytes(src: view<ubyte>) gives result<void, io_error> with io
routine flush_stdout() gives result<void, io_error> with io
routine flush_stderr() gives result<void, io_error> with io

std.text.as_text(string) returns a borrowed immutable UTF-8 text slice. Some line is matched as some(line); absent input is matched as nothing.

read_line returns some(empty string) for a blank line, some(final text) for a final unterminated line, and nothing only when EOF occurs before any byte of the next line. It removes exactly one LF or CRLF line terminator. Bare CR is ordinary text unless an explicit alternative policy is selected. A line may span multiple native reads or UTF-8 decoding boundaries.

The strict text API validates UTF-8 from redirected byte streams and checked UTF-16 console input. Invalid encoding is recoverable and is not silently replaced. max_bytes bounds the returned UTF-8 payload, excluding its terminator, and bounds retained decoder/prefix storage. On invalid encoding or limit_exceeded, the reader requires discard_line before another line read; discard consumes through that line's terminator or EOF. This makes resynchronization explicit.

Console detection uses handle capabilities. Actual Windows consoles use appropriate Unicode console APIs; redirected pipes/files use byte I/O. The implementation does not call a console-only function on a redirected handle and then return fabricated EOF. Output text is emitted as UTF-8 when redirected and as checked Unicode when attached to a console. Binary output APIs preserve bytes exactly on redirected streams; binary-to-console behavior is explicit and may be unsupported.

Prompts become externally visible after a successful flush_stdout before blocking for input. A read does not itself guarantee a prompt was flushed. The API supports separate console and redirected-stream tests. Terminal echo, raw/cooked input, key events, and Ctrl+C handling are explicit policies. A console-mode guard restores settings on normal scope exit. Traps and forced termination do not promise restoration through unwinding.

Deadline/cancellation input variants must settle native reads before releasing decoder storage. Applications can distinguish interrupted input, cancellation, EOF, invalid text, length limits, and OS failure. Secure no-echo input does not echo or retain data in diagnostic strings; it makes only the documented memory-erasure guarantee of its buffer type.

6. Process Management Completion

std.process defines command, child, process_scope, process_output, capture_failure, and exit_status. std.env.get_required(name: text) gives result<string, io_error> with io, alloc; absence returns not_found. This helper is used by the integration fixtures.

Canonical APIs include:

routine command(executable: text) gives result<command, process_error> with alloc
routine arg(var cmd: command, argument: text) gives result<void, process_error> with alloc
routine set_stdin_bytes(var cmd: command, input: view<ubyte>) gives result<void, process_error> with alloc
routine spawn(own cmd: command) gives result<child, process_error> with io, alloc
routine wait(var process: child, deadline: deadline) gives result<exit_status, process_error> with io
routine poll(process: child) gives result<option<exit_status>, process_error> with io
routine request_stop(var process: child) gives result<void, process_error> with io
routine terminate_tree(var process: child) gives result<void, process_error> with io
routine detach(own process: child) gives result<process_id, process_error> with io
routine capture(own cmd: command, limit_per_stream: size, deadline: deadline) gives result<process_output, capture_failure> with io, alloc, thread
routine code(status: exit_status) gives uint with pure

command copies an explicitly resolved executable path. It does not invoke a command shell. Executable search and shell invocation are separate opt-in APIs with documented trust boundaries. Arguments are a vector, with distinct empty, whitespace-containing, quoted, Unicode, and trailing-backslash entries. NUL is rejected. Tier-1 default argument encoding follows the documented Microsoft CRT-compatible argv convention; a target using a different parser requires an explicit raw-command-line API. No encoding can promise a universal argv interpretation for every Windows program.

Commands support explicit working directory, environment snapshot/overrides, inheritance policy, standard-stream routing, creation flags, and process-scope membership. Invalid configuration is rejected before execution. Environment names/values and working directories are validated; mutable global environment state does not race with command construction. Only explicitly selected handles reach a child, using the native handle-list mechanism when inheritance is needed.

spawn obtains an owned process identity and native wait handle; a reused PID cannot redirect operations on that owner. A managed child is assigned to its process scope before user instructions execute. Tier-1 uses a Job Object or an equivalently verified mechanism. Failure to establish required containment is a recoverable launch failure, not an uncontained success. Startup failure releases pipes, process/thread handles, and partially created processes.

The default managed child owns a kill-on-close process scope. Normal disposal without explicit detach closes that scope and requests termination of its remaining members. Deliberately detached processes have explicit independent lifetime. A borrowed wait timeout leaves the owned child available for further management; it does not silently detach or kill it.

capture owns its child scope, closes child stdin after the supplied input is delivered, and drains stdout and stderr concurrently while servicing input. Sequential full-stream reads that can deadlock against pipe backpressure do not conform. Reaching an output bound, timeout, or cancellation triggers the declared capture cleanup policy: stop the managed tree, settle I/O, and observe completion before returning a fully settled failure. If OS cleanup fails, that failure is retained explicitly and no claim of successful termination is made.

process_output contains status, stdout, and stderr, with independent byte buffers. A normal child exit code other than zero still returns ok(process_output). capture_failure owns error, partial_stdout, partial_stderr, and optional cleanup_error. It preserves captured prefixes without pretending the command rolled back. Output limits apply separately to both streams and are enforced during draining. set_stdin_bytes copies input into command-owned storage.

Exit status distinguishes normal completion and externally forced termination where the platform supplies that distinction; arbitrary numeric codes are not reverse-engineered into invented language exceptions. wait is idempotent after completion. Graceful stop is a request, not guaranteed termination; forced termination is explicit. Signal/console-control behavior and privileges are platform-documented. No API silently kills unrelated processes by a loosely matched executable name.

7. Networking Reliability Completion

The earlier std.net API remains available. This section completes its ownership, transfer, deadline, and failure contracts. Tier-1 demand-links Winsock initialization and keeps the subsystem alive for all sockets and outstanding operations. Sockets use their correct native close operation. Resource acquisition and partial initialization are failure-atomic with respect to owned handles.

Canonical deadline-aware and allocating convenience APIs include:

routine parse_address(value: text) gives result<socket_addr, socket_error> with pure
routine tcp_connect_until(address: socket_addr, deadline: deadline) gives result<tcp_stream, socket_error> with io
routine write_all(var stream: tcp_stream, data: view<ubyte>, deadline: deadline) gives result<void, socket_error> with io
routine read_to_end(var stream: tcp_stream, limit: size, deadline: deadline) gives result<buffer<ubyte>, socket_error> with io, alloc
routine read_to_end_into(var stream: tcp_stream, var dst: buffer<ubyte>, limit: size, deadline: deadline) gives result<void, socket_error> with io, alloc
routine shutdown(var stream: tcp_stream, how: shutdown) gives result<void, socket_error> with io
routine send_datagram(var socket: udp_socket, data: view<ubyte>, destination: socket_addr, deadline: deadline) gives result<void, socket_error> with io
routine recv_datagram_owned(var socket: udp_socket, limit: size, deadline: deadline) gives result<datagram, socket_error> with io, alloc

parse_address handles numeric IPv4:port and bracketed IPv6:port without DNS; port zero is valid for local binding. DNS resolution remains a separate fallible operation with explicit limits and deadline/cancellation policy. std.net.shutdown_write selects TCP send-half closure. A datagram contains data, source, and truncation information; the owned convenience API returns limit_exceeded on truncation rather than silently delivering a complete-looking prefix.

TCP is a byte stream. Reads do not correspond to peer write boundaries. Short reads/writes and orderly EOF are normal outcomes. Reset, refusal, timeout, cancellation, and graceful EOF are distinguishable. write_all reports its completed prefix on failure. A successful write means local acceptance, not application-level acknowledgment by the peer. Protocol delivery confirmation requires the protocol's own acknowledgment.

read_to_end_into retains received prefixes in the caller's buffer on failure and reports cumulative transferred progress. The allocating read_to_end convenience function consumes and disposes its private partial buffer on error; callers requiring that partial data use read_to_end_into or incremental reads. Neither API silently retries the consumed portion against a new connection.

Half-close allows a program to stop sending and continue receiving. Closing a stream does not promise delivery of pending bytes to a remote application. There is no implicit reconnect/retry of operations whose remote effects may have occurred. Reuse-address, no-delay, keepalive, backlog, IPv6-only behavior, local-address queries, and nonblocking operation are explicit checked options. Unsupported options return errors.

UDP preserves datagram boundaries. Sending or receiving a zero-length datagram is a successful message operation, not EOF. Oversized/truncated datagrams, message size, peer address, and native errors have explicit results. The original span-based receive API reports limit_exceeded for truncation and records copied progress; it does not silently classify a truncated packet as a complete one.

Nonblocking APIs report would_block and provide pollable readiness registration. Readiness is advisory and can be followed by would_block; it is not permission to remove lifetime checks or assume progress. The standard library supplies blocking/deadline and readiness-oriented interfaces without adding language-level async/await.

Plain TCP/UDP provides no confidentiality, authentication, or certificate checking. Optional std.net.tls uses a separately specified provider, defaults to peer identity/chain verification, and exposes handshake/verification errors. A TCP stream is never described as TLS merely because it uses a common TLS port. Public Internet access is not required for the local networking qualification lane.

8. Native GUI and Window APIs

std.gui is a demand-linked std-tier native window/event library. Tier-1 uses Unicode Win32 windows, explicit message dispatch, native controls, and optional rendering providers. std.win remains the raw platform binding tier. Ordinary safe GUI programming does not require application-written unsafe window-procedure callbacks. Console-only/no_std programs acquire no GUI subsystem merely by linking unrelated std modules.

Core types include window, window_id, control_id, event, monitor_info, logical_rect, physical_rect, paint_context, and clipboard_guard. Window owners and paint contexts are thread-affine and non-Send. Child controls belong to their parent window; a returned control_id is a checked identity, not a second native-resource owner. Queued IDs include a generation identity to reject stale events after destruction.

Representative std.gui APIs include:

routine create_window(title: text, width: int, height: int) gives result<window, gui_error> with io, alloc
routine create_button(var parent: window, id: uint, title: text, x: int, y: int, width: int, height: int) gives result<control_id, gui_error> with io, alloc
routine create_label(var parent: window, id: uint, title: text, x: int, y: int, width: int, height: int) gives result<control_id, gui_error> with io, alloc
routine show(var target: window) gives result<void, gui_error> with io
routine set_text(var parent: window, id: uint, value: text) gives result<void, gui_error> with io
routine next_event() gives result<event, gui_error> with io, alloc
routine is_button_clicked(value: event, target: window, id: uint) gives bool with pure
routine is_close_requested(value: event, target: window) gives bool with pure
routine is_quit(value: event) gives bool with pure
routine destroy(own target: window) gives result<void, gui_error> with io

Coordinates in these convenience constructors are logical 96-DPI units. create_window describes client-area dimensions, not an accidental decorated outer size. Negative dimensions and unsupported native limits are recoverable invalid_argument. More detailed constructors accept an explicit options record covering visibility, parenting, sizing, styles, DPI, and accessibility.

next_event explicitly drives the current UI thread's native queue. It dispatches the native messages needed for controls to work, handles native message-loop failure separately from quit, and returns a typed event. Calling it is a library decision; it is not a language-mandated scheduler. Deadline/poll variants support responsive application loops. A close request permits application veto; destruction is explicit, and quit is an event with a distinct meaning.

Creation, resizing, painting, text input, keyboard/mouse input, focus, commands, close, destruction, and DPI changes have documented event payloads and ordering. Text events preserve Unicode scalar meaning, including IME/composition state through dedicated composition events. Mouse coordinates and dimensions identify their coordinate space. DPI changes update the selected layout/render policy and do not silently mix logical and physical pixels.

Each paint context pairs native begin/end painting and borrows its target until disposal. GDI/graphics objects have owned cleanup; selected objects are restored before destruction. Invalidation and exposure are handled without an infinite paint-message loop. The default window bridge validates/paints its background even if the application selects no custom renderer.

Blocking file/network work on the UI thread is explicit application behavior. A checked UI dispatcher accepts owned sendable messages from worker threads and executes them on the owning UI thread. It never transports a borrowed stack address in a posted message whose sender can return first. Standard thread-affine GUI state is an approved interior-mutable runtime abstraction; ordinary user records do not gain shared-mutation rights.

Native command, text, close, and destruction events are not silently discarded under load. Coalescing of paint/resize/pointer-motion events follows explicit documented rules. Accessibility exposes meaningful names, roles, focus behavior, and control state through native controls or a verified provider. Clipboard, dialogs, file pickers, drag/drop, and monitor APIs return typed results and document ownership of transferred data.

Wrong-thread safe operations are rejected statically when provable and otherwise return a checked wrong-thread invalid_state error. Native callback state outlives native registrations. Callbacks never unwind across a platform ABI boundary. A terminal language trap still terminates the process. Unexpected native creation/dispatch failure remains recoverable.

9. Behavior-Based Integration Qualification

An integration test compiles a .pcn program, starts the actual resulting native executable through the Windows loader, drives its inputs, and checks externally observed effects. Compilation success, MZ bytes, the existence of an output file, a screenshot alone, or a program printing PASS is insufficient evidence.

The delivered Paracine_Runtime_Integration_Suite contains executable Python test orchestration and .pcn fixtures for the APIs above. It is a conformance client of the defined interfaces, not a compiler implementation. Missing APIs, rejected valid programs, unavailable native execution, unavailable required desktop access, missing evidence, and timed-out programs cannot be reported as passed tests.

Each case has a fresh private directory, unique executable path, explicit input bytes/environment, bounded process/output budgets, recorded exit status, and captured failure evidence. The test controller invokes a compiler argument vector without a command shell and rejects stale preexisting executables. It checks PE32+/AMD64 identity and actually executes the image. It records compiler commands, outputs, artifact hashes, and the exact test selection.

The default suite runs with optimization levels 0 and 3. Identical external behavior is required. Controlled builds pin target/CPU/profile inputs and SAGE state; advisory stylometry remains irrelevant to executable semantics. Optional C++ exporter comparisons are additional evidence and do not replace native execution.

Mandatory independent oracles include:

Files : exact binary payload plus append bytes, original-name disappearance after rename, create-new refusal without overwriting an existing file, and a successfully recovered missing-file error.
Handles : external native handle counts after warm-up and repeated acquisition/close, plus reopening the final artifact with exclusive sharing.
Interactive input : observe a flushed prompt before supplying UTF-8 input; distinguish CRLF, a final unterminated line, and EOF; reject invalid UTF-8 through the typed error path.
Processes : an external helper records real argument boundaries and emits more than pipe capacity to both stdout and stderr; the Paracine program forwards exact captured bytes and observes a deliberate nonzero exit code.
Process deadlines : a real helper creates a real descendant; timeout recovery must settle the process scope, and an independent retained native process handle must show that the descendant terminated.
TCP : an external loopback peer observes exact request bytes and send-half closure, then sends binary response bytes; the Paracine program must receive the complete response.
Network deadlines : an accepted peer deliberately sends no data, and the program must recover timed_out rather than fabricate EOF or hang.
UDP : an external peer receives an actual empty datagram and replies with a binary datagram, preserving message and byte semantics.
GUI : an external controller finds a visible window owned by the test PID, activates its native button, reads the changed native label state, requests close, and observes window destruction and process exit.
Ownership diagnostics : a program using a consumed file handle must fail with E0440 and must not leave a runnable executable.

Further production qualification includes file offset/large-file cases, denial/sharing failure, injected partial completions, disk-full and flush failure, cancellation races, std-stream redirection combinations, input bounds/resynchronization, child launch failure, handle inheritance isolation, capture output limits, IPv6 and DNS-policy cases, resets/refusals, datagram truncation, UI DPI/keyboard/focus behavior, and repeated resource-lifetime stress. Fault injection supplements real end-to-end runs; it cannot substitute for all OS integration.

Tests synchronize on observed prompts, accepted sockets, readiness artifacts, or window state. A fixed sleep is not evidence that an operation completed. Deadlines bound failure and cleanup. Network ports are obtained by binding port zero and retaining the bound socket. External test services use loopback; Internet availability, third-party server behavior, and privileged ports are not baseline dependencies.

GUI qualification requires a real interactive Windows desktop. A headless or incompatible host produces an explicit environment/unavailable outcome. Selecting only a subset, omitting a required GUI lane, or skipping a platform requirement does not produce full-suite qualification. Test commands return nonzero on failures or required unavailable facilities. JSON results distinguish pass, fail, and environment errors and include actual run evidence.

The supplied runner has its own cross-platform regression tests for concurrent pipe draining, prompt synchronization, bounded output, timeout enforcement, and real child/network helper behavior. Passing those runner checks validates the harness paths tested; it is not a claim that the Paracine fixtures passed Windows qualification.

10. Compiler, Runtime, and Release Integration

The resource checker emits E0440 for use of a moved/consumed resource, E0441 for consuming a resource with an incompatible live borrow, and E0442 for a statically known cross-thread transfer of a thread-affine GUI resource. Other type/effect diagnostics retain their established families.

SAGE resolves resource ownership, rights, lifetime, fallible return types, effect bounds, closure transfer, thread affinity, and callback capture regions. Structural or stylometric analysis cannot turn an unchecked OS outcome into a proven success, discard an error branch because it looks rare, remove required cleanup, or assume an external peer follows the application's protocol.

PCIR preserves external calls, memory effects, synchronization, error values, progress counts, and conditional cleanup. PMIR preserves the Microsoft x64 ABI, stack/unwind correctness, UTF-16 boundary storage, and pointer-width native handles. The native linker demand-links kernel32, ws2_32, user32, gdi32, and other explicitly required OS imports. No C/C++ runtime, generated C++ source, external assembler, or external linker becomes mandatory.

The core tier supplies types, results, views, and pure support. Alloc supplies owning buffers/strings and bounded helpers. The std tier supplies OS file, console, process, network, and GUI realization. GUI, process capture workers, and Winsock startup remain demand-linked. no_std retains freestanding behavior and acquires none of these services implicitly.

Release qualification requires the relevant actual program-behavior suite to pass for the claimed Tier-1 profile, alongside existing frontend, SAGE, ownership, PCIR, machine, object, loader, unwind, and debug suites. API declarations or test manifests alone do not qualify an implementation. Claims of complete runtime support require all mandatory behavior lanes and retained evidence for that implementation revision.

Platform reference baseline

The language-level contracts above are Paracine requirements. The following primary references ground the selected Tier-1 OS mechanisms; they do not certify a Paracine implementation:

https://learn.microsoft.com/en-us/windows/win32/fileio/file-handles
https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers
https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes
https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects
https://learn.microsoft.com/en-us/windows/console/readconsole
https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv
https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues
https://learn.microsoft.com/en-us/windows/win32/learnwin32/closing-the-window


SAGE — Semantic Analysis & Guidance Engine
SAGE is Paracine's unified analysis framework.

It is divided into three distinct layers:

Semantic Analysis Structural Analysis Stylometric Analysis

Their authority is deliberately unequal.

The governing rule is:

«Stylometry may suggest. Semantics must prove.»

The SAGE law is:

«Observe the programmer's expression. Understand the program. Prove the transformation.»

The compiler must never confuse familiarity with proof.

27.1 SAGE pipeline

The canonical frontend-analysis pipeline is:

.pcn source ↓ normalization ↓ lexer + layout ↓ parser ↓ typed syntax ↓ Semantic Analysis ↓ Semantic Knowledge Model ↓ Structural Analysis ↓ Structural Model ↓ optional Stylometric Analysis ↓ Semantic-Stylometric Fingerprint ↓ Intent / Pattern Advisory Facts ↓ ownership + effects + specialization completion ↓ semantic verification ↓ PCIR

Not every program requires the stylometric stage.

Semantic Analysis is mandatory.

Structural Analysis is part of the standard optimization-analysis architecture.

Stylometric Analysis is optional.

27.2 Semantic Analysis

Semantic Analysis answers:

«What is true about this program?»

It is authoritative.

It resolves:

identifiers modules imports types literal types overloads generic arguments capability satisfaction ownership moves borrows borrow regions effects closure captures constants layouts result propagation control-flow reachability initialization state aliasing facts unsafe boundaries FFI compatibility loop bindings guarded-iterator stopping conditions inclusive integer progression post-test conditions loop-label resolution break/continue targets native-resource rights fallible OS results resource-owner cleanup thread-affine GUI state native-callback lifetimes

No later heuristic system may override a resolved semantic fact.

Semantic Analysis must be:

deterministic reproducible testable independent of personal writing style independent of stylometric confidence

27.3 Semantic Knowledge Model

SAGE materializes resolved compiler knowledge into a Semantic Knowledge Model, abbreviated:

SKM

The SKM may contain:

symbol identities resolved types generic substitutions capability resolutions ownership graph borrow-region graph effect graph closure-capture graph constant facts range facts initialization lattice alias facts source spans public/private visibility known layout facts known call relationships loop-continuation edges directed-induction facts saved-bound identities cleanup-before-test relationships

The SKM is compiler-internal.

It is not a new source-language abstraction.

It provides structured semantic facts to:

diagnostics structural analysis PCIR construction pcn explain optimization reasoning

27.4 Structural Analysis

Structural Analysis answers:

«What computational shape does this semantically valid program have?»

It may classify:

reduction loops guarded iteration inclusive ascending/descending integer loops post-test loops map/filter chains sentinel loops state machines lookup-table patterns arena-allocation phases hot/cold branch patterns invariant initialization data-parallel loops serialization passes deserialization passes validation pipelines producer/consumer loops accumulators search loops finite-state dispatch resource-acquisition sequences

Structural Analysis operates from semantic facts, not textual guesswork alone.

Its Structural Model may include:

control-flow shape data-flow shape loop morphology dependency graph branch topology allocation topology ownership transfer shape pipeline topology call topology mutation density

Structural classification may help select analyses.

It does not by itself prove an optimization legal.

27.5 Stylometric Analysis

Stylometric Analysis asks:

«How does this project habitually express computational ideas?»

It may observe patterns such as:

guard-clause placement preferred immutable intermediates common loop shapes pipeline use error-handling patterns buffer naming relationships validation order allocation order hot/cold organization closure use iterator use module organization generic usage ownership-transfer patterns branch structure data-layout tendencies

Paracine stylometry is codebase-oriented.

It is not an authorship-identification facility.

It does not attempt to determine:

who wrote a file a person's identity demographics personality location background

Its purpose is compiler guidance.

27.6 Semantic-Stylometric Fingerprint

The project-local advisory profile is the:

Semantic-Stylometric Fingerprint

abbreviated:

SSF

It may summarize:

control-flow morphology mutation density ownership patterns error-handling morphology loop forms pipeline forms allocation sequencing branch structure generic patterns closure patterns iterator patterns naming relationships module organization resource-order patterns

Example SSF inference:

This project commonly validates input before allocating output storage.

This project commonly expresses integer reductions as either explicit accumulators or std.iter.sum pipelines.

Such observations are not semantic truths.

27.7 Advisory facts

Stylometric output enters the compiler only as:

advisory facts

An advisory fact may say:

likely reduction likely data-parallel kernel likely map/filter pipeline likely state machine likely resource-order anomaly

It may not say:

this transformation is legal this pointer does not alias this overload was intended this unsafe operation is safe this synchronization can be removed

The latter require semantic proof.

27.8 Optimization interaction

SAGE may use advisory patterns to decide that an expensive analysis is worth attempting.

For example:

Stylometric classification ↓ likely data-parallel kernel ↓ run stronger dependence analysis ↓ semantic proof succeeds ↓ vectorize

The vectorization occurs because dependence analysis proved legality.

It does not occur because stylometry guessed correctly.

If semantic proof fails:

no transformation

even if stylometric confidence is 100%.

27.9 Forbidden stylometric authority

Stylometric Analysis may never determine:

whether source is valid identifier meaning name resolution overload selection type inference where semantics are ambiguous ownership legality borrow legality pointer aliasing legality unsafe correctness FFI correctness error-propagation legality synchronization removal atomic ordering changes integer arithmetic semantics floating-point semantics trap legality control-flow equivalence optimization correctness

These are semantic responsibilities.

27.10 Diagnostics

Stylometry may improve explanation after semantics has diagnosed the actual issue.

Example:

E0501: try cannot propagate this error from the current routine.

This module usually handles parse_error locally.

Likely project-consistent alternatives:

choose parse(data)
    when ok(value)
        ...
    when err(problem)
        ...
or change the routine result to:

result<..., parse_error>
The error comes from Semantic Analysis.

The suggestion comes from advisory analysis.

27.11 Consistency warnings

SAGE may issue nonfatal advisory warnings.

Example:

W0814 unusual resource-order pattern

This routine allocates output storage before validating input.

Most structurally similar routines in this package validate before allocation.

This is not a correctness error.

Such warnings are never elevated to language errors solely because a pattern is unusual.

27.12 "pcn explain"

SAGE powers richer compiler explanations.

Example:

pcn explain sum.pcn

may produce:

Detected source pattern: total += value

Structural classification: wrapping integer reduction

Project advisory pattern: similar reductions often use std.iter.sum

Semantic proof: accumulator has no observable intermediate use integer addition is wrapping and associative loop-carried dependence is reduction-only input regions do not overlap output

Optimization: normalized to reduction form bounds check removed vectorized scalar remainder retained

No source rewrite required.

Every listed optimization proof must correspond to actual compiler evidence.

27.13 Profile location

A project may maintain:

.paracine/ semantic-style.pcs

The ".pcs" file is an implementation-defined versioned SAGE profile.

It may contain aggregated project patterns.

It does not contain executable program semantics.

27.14 Locality and privacy

Stylometric analysis is local by default.

The compiler does not require:

network upload remote behavioral profiling external identity analysis cross-project author tracking

to use SAGE.

A build service may explicitly provide a profile as an input, but that profile is treated like any other build artifact.

27.15 SAGE modes

Canonical modes are:

off advisory guided

off

No stylometric analysis.

Semantic and structural compilation continue normally.

advisory

Default.

Stylometry may influence:

diagnostic ranking lint suggestions pcn explain formatter recommendations analysis explanations developer-facing hints

It does not alter emitted native code.

guided

Explicit opt-in.

Stylometry may additionally influence:

analysis prioritization nonmandatory optimization search order compile-time heuristic budgets specialization investigation priority vectorization-analysis priority

Every resulting transformation still requires normal semantic legality proof.

27.16 Reproducibility

In "advisory" mode:

SSF changes must not change emitted machine code.

Therefore ordinary reproducible production output remains independent of changing style profiles.

In "guided" mode, a style profile may change which legal optimizations are discovered within finite optimization budgets.

Therefore:

the exact SSF version and content hash become declared build inputs

for reproducible output.

Two guided builds are byte-reproducible only when they use the same:

source compiler target optimization profile SSF version SSF hash

Program semantics remain identical regardless.

27.17 PCIR relationship

Unproven stylometric facts do not become semantic PCIR metadata.

They may exist as:

analysis hints

outside the authoritative IR.

A fact may become ordinary PCIR metadata only after a semantic analysis independently proves it.

Thus:

stylometric guess ↓ analysis request ↓ semantic proof ↓ proven PCIR fact

not:

stylometric guess ↓ PCIR assumption

27.18 SAGE conformance law

The defining SAGE invariant is:

«No program accepted, rejected or semantically transformed solely because of stylometric inference may differ from what the authoritative semantic rules permit.»

PCIR Completion
PCIR remains Paracine's semantic center.

It receives only verified program meaning.

28.1 Poison and undef

PCIR contains no:

undef poison freeze

Every ordinary value is defined.

Unsafe erroneous execution is modeled through violated language guarantees rather than IR poison propagation.

28.2 Provenance

Pointer-producing operations retain known provenance identities:

alloca global heap allocation foreign/raw unknown

"gep" preserves provenance.

Pointer-to-integer conversion loses safe reconstruction authority.

"inttoptr" is unsafe and yields raw/unknown provenance.

Integer inequality alone does not establish nonaliasing.

28.3 Calls and memory effects

Calls carry effect summaries such as:

readnone readonly(classes) write(classes) alloc free sync io trap unknown

Foreign calls default conservatively to:

unknown + io + trap

unless a trusted compiler/runtime declaration provides stronger facts.

28.4 Debug locations

Source-originating PCIR instructions retain source spans.

Synthetic instructions inherit meaningful origin information.

Every trap-capable source operation preserves its diagnostic location.

28.5 ABI independence

PCIR does not contain:

RCX RDX RSP XMM0 COFF relocation types PE RVAs

Those belong to later machine stages.

28.6 Serialization

Serialized PCIR contains:

magic major version minor version type table layout table symbol table functions semantic metadata content hash

SAGE advisory state is not serialized into semantic PCIR unless converted into independently proven facts.

28.7 Structured loop control flow

The structured-loop forms are resolved before PCIR construction. Their verified control-flow graphs preserve candidate acquisition before a for-while guard, saved inclusive bounds and completion-before-advance for from-to iteration, and body-before-test execution for continue-until repetition. Normal cleanup precedes continuation operations. Break and continue edges retain their resolved label targets.

Optimizer Completion
29.1 Determinism

Optimization passes are deterministic for identical:

PCIR compiler version target profile optimization profile declared guided-analysis inputs

Unordered host-container iteration may not determine optimization choices.

29.2 Code-growth budgets

Inlining, specialization, path compression and unrolling have independent budgets.

Default "opt=3" module growth limit:

300% of pre-inline PCIR instruction count

unless a selected optimization profile explicitly changes it.

29.3 Recursion

Direct and mutual recursion are legal.

Recursive expansion is not performed without finite proof and budget compliance.

29.4 Floating semantics

Outside "@fastmath":

no reassociation no reciprocal approximation no implicit FMA contraction no signed-zero elimination no NaN elimination

29.5 Trap ordering

The optimizer may not turn:

observable effect A possible trap observable effect B

into an execution where B becomes observable before that trap.

29.6 SAGE guidance

In advisory mode, stylometry cannot alter optimization output.

In guided mode, SAGE may prioritize optimization investigations.

It may never waive optimizer legality conditions.

29.7 Structured loop preservation

Loop transformations preserve the Structured Loop Completion contract, including guard evaluation and iterator consumption, one-time bound evaluation, endpoint inclusion and direction, absence of hidden endpoint overflow, post-test entry behavior, cleanup order, effects, trap order, and labeled jump destinations. Instruction-count or performance estimates never authorize weakening those semantics.

PMIR Completion and Corrections
30.1 Windows unwind model

Tier-1 uses the Windows x64 unwind representation supported by Paracine's frame generator and verifier.

30.2 Leaf functions

A function that:

does not modify nonvolatile registers does not allocate a stack frame does not call another function

may omit ".pdata/.xdata".

Nonleaf functions receive unwind information.

30.3 Stack probes

Large fixed stack frames trigger Windows-compatible stack probing.

Tier-1 uses the Paracine-provided Windows-compatible "__chkstk" support routine.

Ordinary safe source does not expose arbitrary dynamic stack allocation.

30.4 "INC" and "DEC"

Instruction selection may use "INC"/"DEC" where legal and profitable.

They are not semantically forbidden.

30.5 Red zone

Tier-1 has no red zone.

Generated code does not store below current "RSP".

30.6 Internal ABI

Paracine's private internal calling convention may be used only when machine ABI is entirely compiler-controlled.

Address-taken, exported or foreign-visible routines use the relevant stable external convention.

Necessary thunks are generated automatically.

Native Linker — Complete Specification
The native linker is an essential Paracine component.

It is not an optional foreign-toolchain integration.

Its pipeline is:

object discovery ↓ archive discovery ↓ directive processing ↓ symbol construction ↓ archive extraction ↓ COMDAT resolution ↓ duplicate-definition resolution ↓ reachability graph ↓ section garbage collection ↓ identical-code folding when enabled ↓ section contribution merging ↓ import synthesis ↓ export synthesis ↓ TLS synthesis ↓ unwind synthesis ↓ load-config synthesis ↓ resource synthesis ↓ final section ordering ↓ RVA assignment ↓ file-offset assignment ↓ relocation application ↓ base-relocation synthesis ↓ PE headers ↓ debug directory ↓ checksum/signature reservation ↓ image verification ↓ PE32+

Link Input Discovery
Inputs may include:

in-memory Paracine object models COFF .obj static .lib import .lib explicit DLL imports resource .res Paracine runtime archives

Input processing is deterministic.

Library search paths come from:

package manifest target profile explicit command line configured SDK locations

The linker does not silently search arbitrary current-directory libraries to satisfy unresolved names.

Symbol Model
Symbols are classified as:

defined strong defined weak COMDAT undefined absolute import foreign common-compatible

Paracine source itself does not generate C tentative/common definitions.

Foreign objects may contain them.

Duplicate strong definitions produce:

L1001 duplicate strong symbol

Unresolved final symbols produce:

L1002 unresolved external symbol

Archive Extraction
Static libraries use iterative demand extraction:

build unresolved set scan archive indexes extract satisfying members add their new unresolved references repeat to fixed point

A member is loaded once.

Whole-archive behavior is available through:

pcn link --whole-archive library.lib

Archive extraction does not depend upon fragile one-pass library ordering.

COMDAT Resolution
NODUPLICATES

Multiple live candidates are an error.

ANY

Select the deterministic canonical candidate.

Preference:

root package earlier dependency order lexicographically smaller object identity

SAME_SIZE

All candidates must have equal byte size.

EXACT_MATCH

Candidate bytes and normalized relocation graphs must match.

ASSOCIATIVE

Contribution lifetime follows its leader.

Used for:

pdata xdata debug fragments associated constant data

LARGEST

Choose the largest candidate.

Deterministic tie-breaking applies.

Section Garbage Collection
Release profiles enable section GC by default.

Roots include:

entry point exports forced includes TLS structures TLS callbacks load-config roots resource roots @used symbols startup hooks shutdown hooks

Edges include:

relocations associative COMDAT relationships unwind references import thunks export forwarders startup tables

Unreachable removable contributions are discarded.

Identical Code Folding
ICF is optional.

Default release "opt=3" may enable it for eligible nonexported functions.

Two functions may fold only when:

normalized bytes match relocation topology matches unwind behavior matches observable address identity is irrelevant neither has @unique_address neither requires distinct export identity

Address-taken functions are not folded by default.

Final Native Image Sections
A Tier-1 image may contain:

.text .rdata .data .pdata .xdata .idata .edata .reloc .tls .rsrc .debug

".bss" semantics occupy zero-filled writable virtual storage without requiring matching raw bytes.

Loader semantics are defined by PE structures and directories, not by section names alone.

Section Permissions
Paracine enforces W^X by default.

Typical final permissions:

.text read + execute .rdata read .pdata read .xdata read .idata loader-compatible read/write as required .edata read .data read + write .tls read + write template semantics .rsrc read .reloc read + discardable

Ordinary production output does not contain writable-executable sections.

Final Section Layout
Defaults:

FileAlignment = 512 bytes SectionAlignment = 4096 bytes

Final RVAs and file offsets use alignment-aware layout.

"SizeOfRawData" is file-aligned.

"VirtualSize" reflects actual virtual contribution size.

"SizeOfImage" and "SizeOfHeaders" are computed from finalized layout.

Import Construction
Imports are grouped by DLL.

Paracine emits:

Import Directory Import Lookup Tables Import Address Tables DLL names Hint/Name records

Each import table is correctly terminated.

Imported named functions use the required hint/name representation.

Ordinal imports use the PE32+ ordinal encoding.

References to:

__imp_symbol

resolve to IAT slots.

Typical direct call:

call qword ptr [rip + IAT-slot]

Delay Imports
Delay loading is explicit.

It may be requested through:

@delay_import

or package configuration.

The linker emits the required delay-import structures and runtime resolver support.

If unused, the directory is zero.

Export Construction
Exports contain:

public name target symbol optional ordinal optional NONAME optional forwarder

Ordinals are unique.

Unassigned ordinals are deterministically assigned.

The linker emits:

Export Directory Export Address Table Name Pointer Table Ordinal Table strings DLL identity

Anonymous closure types and private internal-ABI routines cannot be directly exported.

Resource Construction
Paracine accepts ".res" input and manifest-defined resources.

Resource trees normalize by:

type name or ID language

The linker emits the complete ".rsrc" directory tree and payloads.

No resources means a zero Resource data directory.

TLS Construction
"thread var" items become native TLS template content.

The linker emits:

initialized TLS bytes zero-fill requirement TLS index storage optional callbacks IMAGE_TLS_DIRECTORY64

Edition-2026 source TLS initialization is const-evaluable and requires no arbitrary user callback.

Runtime TLS callbacks may still exist where the runtime requires them.

Exception and Unwind Construction
Surviving nonleaf functions receive:

RUNTIME_FUNCTION UNWIND_INFO

The linker resolves final RVAs, sorts ".pdata", verifies ranges and installs the Exception data directory.

Malformed unwind information is a link failure or internal compiler error depending on source.

Base Relocation Construction
Paracine uses RIP-relative native code whenever practical.

Remaining image-base-dependent addresses generate base relocations.

Tier-1 primarily uses:

DIR64

Relocations are grouped by 4 KiB page.

Production dynamic-base images contain required relocation information.

Relocation Application
For relocation evaluation:

S = resolved symbol A = addend P = fixup address

Typical Tier-1 rules:

ADDR64 → S + A ADDR32NB → RVA(S) + A REL32 → S + A - address_after_fixup SECTION → section index SECREL → section-relative offset

Overflow is diagnosed.

No silent truncation is allowed.

PE32+ Header Construction
The final image contains:

DOS header minimal DOS stub PE signature COFF image header PE32+ optional header data directories section table section data optional certificate data

Tier-1 machine:

AMD64

Optional header:

PE32+

The writer computes all required image sizing, subsystem, stack, heap and directory fields.

Reserved fields are zero.

Image Base
Default EXE preferred image base:

0x0000000140000000

DLL image base is linker-selected and correctly aligned.

ASLR means code must not depend upon actual preferred-base loading.

Subsystem
Applications select:

subsystem = "console"

or:

subsystem = "windows"

Console is the default.

"pcn_start" adapts the ordinary Paracine "main" contract to either process type.

Default PE Hardening
Production images normally enable:

HIGH_ENTROPY_VA DYNAMIC_BASE NX_COMPAT TERMINAL_SERVER_AWARE

"NO_SEH" is not set by default.

"FORCE_INTEGRITY", "APPCONTAINER" and specialized image characteristics are explicit deployment options.

Control Flow Guard
CFG is optional production hardening.

The linker sets "GUARD_CF" only after constructing all required guard metadata.

Indirect targets may include:

address-taken routines ABI thunks foreign-visible exports runtime callbacks

Incomplete CFG generation causes link failure rather than a misleading partial image.

Load Configuration
A load-config directory is synthesized when required by enabled hardening facilities.

Unused fields are zero.

The chosen structure size matches the target-profile version actually emitted.

Stack and Heap Defaults
Defaults:

stack reserve = 8 MiB stack commit = 64 KiB

heap reserve = 1 MiB heap commit = 64 KiB

These are image defaults, not language semantics.

Entry Point
EXE entry:

pcn_start

DLL startup entry when required:

pcn_dll_start

"pcn_start" performs:

runtime initialization OS argument acquisition UTF-8 argument conversion standard-stream initialization trap-handler installation random hash seeding runtime subsystem initialization startup hooks user main stream flush process exit

No C runtime is required.

DLL Startup
DLL process attachment performs only loader-safe work.

General allocation, thread creation and arbitrary application callbacks do not run under loader lock.

Richer initialization uses:

initialize()

or safe lazy initialization.

Debug Directory
Native debug output may contain a PE debug directory referring to CodeView data.

PDB identity uses deterministic information when reproducible mode is active.

Uncontrolled absolute build paths are not embedded in reproducible mode.

PDB Completion
The native PDB writer supports:

line information public symbols private symbols at -g2 types modules source mapping register/stack variable location ranges

It maintains the necessary MSF/PDB streams.

Conformance is judged by successful consumption by expected Windows debugging tooling and accurate source/symbol resolution.

PDB generation is not required to define language semantics.

Native Debug Levels
-g0 no debug information

-g1 source and line mappings public function names

-g2 full supported type, lexical scope, local variable, and location-range information

Optimized-away values are reported honestly as unavailable or optimized away.

PE Checksum
Default ordinary EXE/DLL policy:

Checksum = 0

unless explicitly required.

Command:

pcn pe --checksum

may compute it before signing.

Authenticode and Certificate Table
Signing is a tooling/deployment operation.

It is not part of Paracine semantics.

Commands may include:

pcn sign pcn verify-signature

Signing occurs after deterministic image construction.

Private keys are never stored directly in normal package manifests.

Reproducible Native Linking
Identical declared build inputs produce byte-identical native outputs except intentionally external signatures.

Rules include:

zeroed reproducibility-sensitive timestamps deterministic symbols deterministic sections deterministic padding deterministic PDB identity normalized source paths no random binary padding

Runtime randomized hash seeds are generated at process startup and do not affect binary reproducibility.

SAGE advisory mode does not affect emitted code.

SAGE guided mode requires its SSF hash to be treated as a declared build input.

Build Identity
A build may emit:

program.build.json

containing:

source hashes package graph lockfile hash compiler version language edition SAGE version SAGE mode SSF version/hash when guided mode is used PCIR version PMIR version optimization profile target triple CPU profile native backend version COFF writer version linker version PE writer version standard-library version feature set reproducibility mode

If the optional C++20 exporter participates, it additionally records:

C++20 exporter version portable runtime-helper version

If it does not participate, no C++ exporter identity is required.

Import Library Completion
DLL builds may also emit:

module.lib

Import library names and ordinals are validated against the final DLL export table.

Conventional:

__imp_Name Name

symbols are provided where required.

Static Library Completion
"pcn lib" emits deterministic Windows archives containing:

linker members long-name tables when needed object members symbol indexes

The writer supports independent read-back validation.

Bigobj Resolution
Paracine uses ordinary COFF while legal.

It switches to Windows bigobj when ordinary section/symbol limits are exceeded.

Bigobj is an object-file mechanism only.

It does not alter PE image semantics.

Native Link Diagnostics
Diagnostic families:

L1000 input/object L1100 symbol L1200 relocation L1300 COMDAT/archive L1400 import/export L1500 image layout L1600 unwind/TLS/load-config L1700 debug/PDB L1800 signing/certificate L1900 internal image verification

Representative diagnostics:

L1001 duplicate strong symbol L1002 unresolved external L1104 incompatible symbol ABI L1204 relocation overflow L1302 COMDAT exact-match disagreement L1401 duplicate export name L1402 duplicate export ordinal L1501 image exceeds supported small code model L1601 malformed unwind contribution L1602 invalid TLS template L1901 generated PE failed self-validation

Final Image Verification
Before final output, an independent PE reader reparses the produced image.

It verifies:

signatures machine type PE32+ format header bounds section-table bounds file alignment section alignment virtual overlap raw overlap entry-point location directory bounds imports IAT exports pdata sorting unwind references TLS relocations load config resources debug directory certificate bounds SizeOfImage SizeOfHeaders permissions reserved fields

Failure deletes the output and raises an internal compiler error.

Independent Loader Test
Conformance testing launches generated images through the Windows loader and validates:

ASLR imports TLS unwind console startup GUI startup DLL loading exports thread creation trap termination large stack frames debug symbolication

Optional C++20 Exporter
C++20 generation is no longer described as a required backend of Paracine.

It is the:

optional portability/export backend

A conforming native compiler may omit it entirely.

Its pipeline, when installed, is:

optimized PCIR ↓ C++20 legalization ↓ type lowering ↓ control-flow reconstruction ↓ ownership/runtime mapping ↓ C++20 source emission ↓ .cpp

The exporter may also generate:

.hpp .h

when requested.

Its absence does not affect:

parsing semantic analysis SAGE PCIR PMIR machine encoding COFF generation native linking PE generation native execution

Optional C++20 Exporter Semantics
If the exporter is present, it must preserve edition-2026 semantics.

72.1 Closures

Capturing closures become concrete generated aggregates or equivalent helper structures.

No "std::function" is required.

72.2 Sequencing

Generated temporaries preserve Paracine's defined left-to-right evaluation.

72.3 Arithmetic

Signed modular arithmetic is implemented without relying on C++ signed-overflow UB.

72.4 Shifts

Shift counts are normalized to Paracine semantics.

72.5 Division

Generated code explicitly handles:

divisor == 0 MIN / -1

72.6 Float-to-int

Conversion implements:

truncate toward zero saturate NaN → 0

without depending on undefined host behavior.

72.7 Layout

ABI-relevant representations receive static layout assertions.

72.8 Atomics

C++20 atomics use explicit corresponding memory orders.

72.9 TLS

"thread var" lowers to suitable "thread_local" storage under the same edition-2026 trivial-destruction constraints.

72.10 Structured loops

The exporter preserves guarded for iteration, inclusive directed from-to iteration, and post-test continue-until repetition. Generated control flow retains one-time left-to-right bound evaluation, candidate consumption, cleanup and jump behavior. It tests inclusive completion before advancing at integer extremes and directs a continue-until continuation through its post-test. No C++ signed-overflow assumption or control-flow reconstruction may change these rules.

Optional Cross-Backend Differential Validation
If the optional C++20 exporter is installed, Paracine should use it as an additional validation oracle.

Then:

Paracine ↓ C++20 exporter ↓ MSVC/Clang

may be compared against:

Paracine ↓ PMIR ↓ native PE

and against:

PCIR reference interpreter

This is valuable.

It is not required to prove that the native compiler exists independently.

A native-only conforming compiler does not fail conformance because C++20 differential tests are unavailable.

Module and Manifest Completion
Example:

[package] name = "acme.tool" version = "1.0.0" edition = "2026" kind = "app" root = "src" no_std = false no_prelude = false

[target.windows-x86_64] abi = "msx64" cpu = "x86-64" subsystem = "console" image_base = 0x140000000 stack_reserve = 8388608 stack_commit = 65536 heap_reserve = 1048576 heap_commit = 65536 cfg = true

[sage] stylometry = "advisory" profile = ".paracine/semantic-style.pcs"

[profile.debug] opt = 0 debug = 2 reproducible = true emit = ["obj", "exe"]

[profile.release] opt = 3 debug = 1 lto = true icf = true gc_sections = true reproducible = true emit = ["obj", "exe"]

[profile.portable] optional_backend = "cxx20" emit = ["cpp"]

[link] libraries = ["kernel32.lib"] search = [] whole_archive = []

[resources] files = []

[signing] enabled = false

If the optional C++20 exporter is unavailable, requesting:

emit = ["cpp"]

produces a clear backend-unavailable diagnostic.

It does not make the installation incapable of ordinary native compilation.

Build-Step Completion
Arbitrary compiler-time dependency scripts remain forbidden.

Explicit root-workspace steps may be declared:

[[step]] name = "generate_protocol" tool = "tools.protocol_generator" inputs = ["schema/protocol.pcn-schema"] outputs = ["generated/protocol.pcn"]

The build engine:

hashes inputs hashes the tool runs it under declared workspace rules records outputs rejects undeclared writes in sandboxed mode

Generated source becomes ordinary Paracine source.

Capability Lookup Freeze
Capability lookup order remains:

module defining the subject type;
module defining the capability;
current module.
Multiple exact candidates at equal precedence produce:

E0340 ambiguous capability member

Compiler-intrinsic relationships such as "callable" bypass ordinary user lookup.

Generic Coherence
Two visible definitions cannot simultaneously satisfy the same effective capability member signature at equal precedence without ambiguity.

Paracine uses no global impl-block registry.

Ambiguity is resolved or diagnosed at the use site.

Const Evaluation Completion
Const evaluation supports:

pure arithmetic records variants arrays bounded loops (including const-legal for-while, from-to, and continue-until forms that complete within configured evaluation limits) pure routine calls const generics supported const text operations

It cannot use:

I/O threads foreign calls unsafe memory access runtime randomness time environment state dynamic libraries networking stylometric project state

SAGE stylometric state is never available to const evaluation.

Thus program constants cannot depend on stylistic profiling.

Static Initialization Completion
There is no hidden user dynamic-global initialization.

Allowed:

const X = const_expr

var static X: T = const_expr

thread var X: T = const_expr

Runtime startup hooks are explicit.

Startup and Shutdown Hooks
Attributes:

@startup @shutdown

apply only to:

routine() gives void

Startup order is deterministic.

Shutdown runs in reverse order on normal "main" completion.

Shutdown does not execute after:

trap process.exit forced process termination

Main Routine Completion
Accepted forms:

routine main() gives void

routine main() gives int

routine main(args: view) gives int

routine main() gives result<void, E> where E is printable

routine main(args: view) gives result<void, E> where E is printable

Windows UTF-16 arguments are converted to valid UTF-8 before user code receives them.

Invalid source UTF-16 input becomes a startup failure.

No-Std Completion
With:

no_std = true

available facilities include:

primitive types records variants option/result arrays views/spans raw pointers core arithmetic PCIR/native code generation required compiler intrinsics

A freestanding package may provide:

@entry

The native compiler does not require the optional C++ exporter in "no_std" mode or any other mode.

Unsafe Boundary Completion
Public unsafe-containing routines are classified as either:

Safe encapsulation

Caller has no raw invariant obligation.

No "with unsafe" is exposed.

Unsafe contract

Caller must uphold raw/lifetime/alias/platform requirements.

The declaration includes:

with unsafe

and documentation states the contract.

FFI ABI Compatibility
C/MS x64-compatible types include:

fixed-width integers float double documented bool representation raw routine pointers @layout(c) compatible records

Paracine-native abstractions such as:

text view span buffer option result variant string closure

are not automatically foreign-ABI types.

They require explicit wrappers or thunks.

Exported Paracine APIs
Ordinary Paracine-to-Paracine exports may use Paracine-native types because interface metadata communicates their ABI.

A foreign-facing:

@export_name

routine must use foreign-compatible types.

Violation:

E0624 foreign export exposes non-FFI type

Runtime Version Compatibility
Paracine native libraries record:

language edition public ABI version standard-library ABI version target profile

The native linker rejects incompatible combinations.

PCIR-to-PMIR Machine Verification
The backend verifies:

virtual registers resolved frame indexes resolved ABI placement correct RSP aligned at calls msx64 shadow space valid nonvolatile registers restored direction flag valid instruction/register combinations legal branch targets valid relocations representable unwind information matches prologue epilogs satisfy target rules

Failure is an internal compiler error.

Native Encoder Completion
The integrated encoder covers:

legacy prefixes REX VEX EVEX where allowed opcode maps ModRM SIB displacements immediates rel8 rel32 RIP-relative addressing

A separate decoder validates final instruction bytes.

"pcn asm" displays decoded final machine output rather than merely echoing PMIR intent.

CPU Feature Safety
The CPU baseline is a declared build input.

The compiler never emits instructions beyond that baseline except inside properly dispatched multiversion bodies.

Deployment to an incompatible CPU is a configuration error, not silent emulation.

Multiversion Completion
Example:

@multiversion("x86-64", "x86-64-v3") routine work(...)

may generate:

work$base work$v3 work$resolver work$slot

The resolver uses baseline instructions, detects CPU features once and publishes the selected implementation atomically.

Exports retain a stable thunk.

Runtime Independence
Tier-1 Paracine has no hidden dependency on:

MSVCRT UCRT C++ runtime LLVM runtime libgcc

unless explicitly introduced by linked foreign code.

The runtime is demand-linked from Paracine native objects.

The optional C++20 exporter does not participate in the normal ".obj" or ".exe" production chain.

Security Baseline
Production defaults include:

safe-code bounds checks NX compatibility ASLR compatibility high-entropy VA W^X randomized default hashing stack probing valid unwind metadata UTF-8 validation checked allocation-size arithmetic hardened object parsing hardened archive parsing no automatic dependency build scripts

SAGE advisory data cannot weaken safety checking.

Stylometric familiarity is never evidence that unsafe code is safe.

Compiler Input Hardening
External object/archive/PDB/resource/package inputs are treated as untrusted.

Checked arithmetic is required for:

offset + size count * element_size alignment section ranges string ranges relocation indexes symbol indexes archive offsets resource recursion

Malformed inputs produce diagnostics rather than unchecked compiler memory operations.

Resource Limits
Configurable limits include:

parser nesting generic instantiation depth const-eval steps const recursion module count import edges object sections symbol count optimizer growth diagnostic cascade SAGE structural-analysis budget SAGE stylometric-profile size

Limits do not change language semantics.

Final Conformance Suites
A complete native Paracine 1.0 implementation must pass the following mandatory suites. A conforming complete std-tier runtime additionally passes the Runtime and Observable-Behavior Completion requirements, including actual execution of the file, input, process, network, and GUI behavior lanes.

Frontend

normalization layout grammar name resolution modules overloads generics closures capabilities effects const evaluation

Structured Loops

All cases in Structured Loop Conformance Requirements are mandatory, including guarded iterator exhaustion and first-false behavior; inclusive ascending, descending and equal endpoints; saved bounds; signed and unsigned limits; post-test entry; break/continue disambiguation; nested labeled jumps; cleanup-before-test ordering; and rejection of invalid types, borrows and labels.

SAGE Semantic Core

semantic resolution determinism SKM correctness ownership facts borrow facts effect facts closure capture facts alias facts structural-model consistency

SAGE Stylometric Boundary

When stylometry is implemented:

advisory facts cannot change semantic validity advisory mode cannot change emitted code no stylometric override of type/name/borrow rules guided transformations require semantic proof guided reproducibility records SSF hash

Type and ownership

copy/move partial move drop flags shared borrow exclusive borrow returned borrow closure capture regions thread-scoped borrows raw boundary FFI compatibility

Semantics

integer operators shift semantics signed division extremes float conversion NaNs signed zero try/fail/give cleanup trap order

PCIR

construction verification interpretation optimization passes Path Compression SSA Reduction fusion vectorization legality effect preservation

PMIR

instruction selection ABI lowering register allocation spilling stack alignment large frames unwind branch relaxation encoding

COFF

sections symbols relocations COMDAT bigobj archives import libraries foreign linker interoperability

PE

headers imports exports resources TLS pdata/xdata relocations ASLR load config CFG DLLs debug directory PDB self-verification actual Windows loading

Runtime and Observable Behavior

Owned handle lifecycle and error progress; binary filesystem effects; console and redirected input; exact child argument boundaries and concurrent stdout/stderr capture; child-tree deadline cleanup; real TCP/UDP loopback exchanges; UI-thread affinity, native event dispatch and externally observed control/window state. The delivered executable fixture suite records a concrete subset of this larger production matrix at optimization levels 0 and 3.

Native semantic oracle

Mandatory comparison targets include:

PCIR reference interpreter direct Paracine native executable

If the optional C++20 exporter is present, an additional optional-backend suite compares:

C++20/MSVC C++20/Clang direct native PCIR interpreter

Failure to ship a C++20 exporter does not fail native language conformance.

Release Qualification
A native component may be described as production only when:

mandatory conformance tests pass release-blocking semantic miscompilations are zero native image verification passes PCIR/native differential tests pass ABI matrix passes unwind matrix passes claimed std-runtime behavior lanes pass with independently observed evidence resource-lifecycle and native-error contracts pass

If the optional C++20 exporter is shipped as production, it must additionally pass its C++ differential suite.

A compiler may ship:

native production backend + experimental or absent C++ exporter

without weakening the status of its native compiler.

Final Implementation Order
The canonical implementation strategy is native-first.

Phase 1

UTF-8 normalization lexer layout engine parser typed syntax basic diagnostics structured-loop grammar and continue/until disambiguation

Phase 2

name resolution types generics capabilities effects ownership closure capture analysis loop binding/condition/bound checks and labeled-jump resolution

Phase 3

SAGE Semantic Core Semantic Knowledge Model Structural Analysis optional advisory stylometry

Phase 4

PCIR construction PCIR verifier reference interpreter full structured-loop evaluation, cleanup and continuation semantics

Phase 5

basic optimizer SCCP mem2reg SROA DCE range/BCE

Phase 6

x86-64 PMIR instruction selection register allocation frame layout machine encoder

Phase 7

COFF writer object reader archive reader/writer

Phase 8

native linker imports exports relocations PE headers runtime entry first .exe

Phase 9

unwind TLS resources DLLs hardening debug support

Phase 10

advanced optimizer inlining specialization fusion vectorization Path Compression SSA Reduction

Phase 11

full standard library owned file/resource layer recoverable system errors interactive input managed child processes reliable network operations thread-affine native GUI/window APIs observable-behavior integration tests PDB tooling SAGE explain/lint refinement profiling/toolchain polish

Optional Phase 12

C++20 exporter .hpp/.h generation foreign C++ integration helpers C++ differential testing

The native compiler is architecturally complete before Optional Phase 12 begins.

First Native Milestone Contract
The first architecture-proof compiler supports:

int bool text literals routines let/var arithmetic comparison if while calls give minimal output or Windows foreign calls

and directly produces:

hello.pcn ↓ semantic frontend ↓ PCIR ↓ PMIR ↓ x86-64 ↓ COFF/native link ↓ hello.exe

without:

C++ generation external assembler external linker LLVM MSVC compilation Clang compilation

This is the foundational proof of Paracine's independent architecture.

Final Standard-Library Tiers
core

std.core std.mem core std.num std.math std.slice std.iter std.atomic std.hash std.fmt core std.text core std.simd

alloc

std.buffer std.text.string std.collections std.sort std.mem allocators

std

std.error std.io std.fs std.path std.env std.time std.thread std.sync std.process std.random std.net std.gui std.dynlib std.win std.test

No tier silently depends upon a higher tier.

No tier requires C++ source generation.

Final Philosophy of Memory Safety
Safe Paracine guarantees:

no use after move no double dispose no dangling safe borrow no shared/mutable alias violation no out-of-bounds memory access no safe data race no uninitialized safe read

It does not guarantee:

no leak no deadlock no starvation no logical message-order race no stack exhaustion no valid-computation denial of service

Unsafe contract violations create erroneous execution.

Stylometric analysis never weakens this boundary.

Final Definition of No Undefined Behavior in Safe Code
Safe source has no language-level undefined behavior.

Every operation results in one of:

defined value recoverable result defined trap defined scheduling nondeterminism documented target-platform behavior

Compiler optimizations cannot invent stronger assumptions.

Erroneous behavior enters only through:

unsafe contract violation foreign ABI/contract violation external hardware/OS behavior outside the abstract machine

A SAGE advisory inference is never a source of undefined assumptions.

Final Production Architecture
The completed native-first Paracine toolchain is:

                 PARACINE SOURCE
                       .pcn
                        │
                        ▼
              canonical normalization
                        │
                        ▼
               lexer + layout
                        │
                        ▼
             parser + typed syntax
                        │
                        ▼
                 SAGE CORE
          ┌─────────────┴─────────────┐
          │                           │
          ▼                           ▼
  Semantic Analysis           Structural Analysis
   authoritative                 descriptive
          │                           │
          └─────────────┬─────────────┘
                        │
                        ▼
            Semantic Knowledge Model
                        │
               optional│
                        ▼
             Stylometric Advisor
                        │
                advisory facts
                        │
                        ▼
          semantic proof / resolution
                        │
                        ▼
                       PCIR
                        │
                        ▼
             PCIR verification
                        │
                        ▼
             semantic optimization
                        │
                        ▼
              target legalization
                        │
                        ▼
                       PMIR
                        │
                        ▼
               x86-64 encoding
                        │
                        ▼
                 object model
                  ┌─────┴─────┐
                  │           │
                  ▼           ▼
             COFF writer   native linker
                  │           │
                  ▼           ▼
                .obj        PE32+
                              │
                              ▼
                             .exe
This is the essential Paracine architecture.

The optional portability/export branch is separate:

                optimized PCIR
                     │
            optional│
                     ▼
              C++20 exporter
                     │
                     ▼
                    .cpp
It is not on the path from ".pcn" to ".exe".

DLL and Static-Library Production
DLL production

PCIR ↓ PMIR ↓ x86-64 ↓ native linker ↓ PE32+ DLL ├── module.dll └── module.lib

Static library production

PCIR ↓ PMIR ↓ COFF objects ↓ archive writer ↓ module.lib

Neither requires C++20.

Optional portable textual export

PCIR ↓ optional C++20 exporter ↓ module.cpp module.hpp

Final One-Language Rule
Paracine's source of truth is:

.pcn

Primary native products include:

.obj .exe .dll .lib

Supporting products include:

.pdb .h

Optional portability/export products include:

.cpp .hpp

None are separate language dialects.

They are realizations or supporting artifacts of one Paracine program.

Final Backend Law
A backend may choose a different representation.

It may not choose a different meaning.

The governing rule is:

«Semantics flow downward. Backends do not flow upward into the language.»

The optional C++20 exporter therefore does not define Paracine semantics.

PCIR and the language definition do.

Final Analysis Law
Paracine's compiler-analysis hierarchy is:

Semantic Analysis What is true?

Structural Analysis What computational shape exists?

Stylometric Analysis What recurring expression patterns exist?

Their authority is:

Semantic truth > Structural classification > Stylometric suggestion

The final rule is:

«Observe. Classify. Suggest. Prove.»

No advisory system may bypass proof.

Final Optimization Law
The optimization law remains:

«Resolve what is known. Remove what is unnecessary. Represent what remains directly.»

Resolve includes:

constant evaluation name resolution type resolution capability resolution ownership resolution borrow resolution closure capture resolution effect resolution range proof layout proof specialization semantic pattern proof

Remove includes:

dead code dead storage dead checks dead cleanup dead abstraction dead iterator layers dead branches dead symbols dead closure fields

Represent directly means:

smallest semantically valid PCIR then smallest target-appropriate PMIR then smallest legal machine realization

Stylometry may help the compiler decide where to look.

It may not decide what is true.

Final Output Hierarchy
Paracine's production hierarchy is:

«Primary native realization: ".pcn → .obj / .exe"»

Additional native deployment realizations include:

.pcn → .lib .pcn → .dll

The optional portable realization is:

«Optional portability/export realization: ".pcn → .cpp"»

Supporting optional artifacts include:

.hpp .h .pdb

The C++20 exporter may be completely absent while Paracine remains a complete independent native language.

Final Multi-Output Law
«One Paracine semantic program may be realized directly as native object code, native libraries, native DLLs, or complete PE32+ executables, and may optionally be exported as standards-oriented C++20 source, without changing the Paracine language contract.»

Native production is primary.

C++ export is secondary.

Neither changes the source language.

Final Production Principle
«Write the meaning once. Resolve it once. Prove it once. Optimize it once. Materialize only what deployment requires.»

And for SAGE:

«Stylometry may suggest. Semantics must prove.»

Final Language Definition
Paracine is a mature statically typed ahead-of-time native programming language with:

explicit data explicit ownership inferred nonlexical borrowing guarded for-while iteration inclusive directed from-to iteration post-test continue-until repetition static zero-allocation closures monomorphized generics structural capabilities defined arithmetic recoverable result semantics terminal traps structured native concurrency opaque owned resource handles recoverable native errors bounded interactive input managed process scopes reliable TCP/UDP APIs thread-affine native GUI APIs observable-behavior integration qualification authoritative semantic analysis structural computational analysis optional advisory stylometric analysis PCIR semantic optimization PMIR machine lowering direct x86-64 machine encoding direct COFF generation direct native linking direct PE32+ construction no mandatory GC no mandatory exceptions no mandatory scheduler no hidden dynamic initialization no mandatory external compiler no mandatory external assembler no mandatory external linker no mandatory C++ generation

Its essential Tier-1 machine path is:

.pcn ↓ frontend ↓ Semantic Analysis ↓ SAGE / semantic resolution ↓ PCIR ↓ PMIR ↓ x86-64 ↓ COFF ↓ native link ↓ PE32+

Its optional portability/export path is:

optimized PCIR ↓ optional C++20 exporter ↓ .cpp

The first path defines Paracine's independence.

The second extends Paracine's reach.

Frozen Edition-2026 Statement
The revised edition-2026 baseline includes the normative for-while, from-to, and continue-until forms and the Runtime and Observable-Behavior Completion contract. The earlier freeze declaration is superseded by this document. Edition 2026 is frozen when the reference native implementation conforms to this completed grammar, its structured-loop semantics, ownership rules, SAGE semantic boundary, PCIR contract, PMIR contract and Tier-1 native backend described by the complete 1.0 specification.

The optional C++20 exporter is not a condition of language completeness.

Compatible additions may include:

new optimizations new diagnostics new SAGE advisory patterns new standard-library APIs new optional exporters new native target profiles

provided existing semantics remain unchanged.

A new language edition is required for incompatible changes to:

grammar type semantics ownership borrowing arithmetic trap semantics module resolution capability semantics closure semantics safe-code guarantees ABI-visible language representation not protected by versioning

A new SAGE stylometric heuristic alone does not require a new language edition because stylometry has no semantic authority.

Final Canonical Summary
PARACINE ".pcn"

Readable source + Exact static types + Guarded for-while iteration + Inclusive directed from-to iteration + Post-test continue-until repetition + Reliable owned file/resource handles + Recoverable native errors + Interactive input + Managed processes + Reliable networking + Native windows + Observable-behavior integration tests + Authoritative semantic analysis + Structural computational analysis + Optional advisory stylometry + Explicit ownership + Nonlexical borrowing + Zero-allocation static closures + Recoverable result handling + Defined machine arithmetic + PCIR semantic optimization + Path Compression + SSA Reduction + Direct PMIR lowering + Direct x86-64 machine encoding + Direct COFF generation + Direct native linking + Direct PE32+ generation ──────────────────────────────── Independent Native Program ──────────────────────────────── ↓ .obj / .exe

Optional portability exists beside that architecture:

optimized PCIR ↓ optional C++20 exporter ↓ .cpp

The definitive production hierarchy is:

PRIMARY NATIVE

.pcn → .obj .pcn → .exe

ADDITIONAL NATIVE

.pcn → .lib .pcn → .dll

OPTIONAL PORTABILITY / EXPORT

.pcn → .cpp

The definitive compiler-analysis rule is:

«Semantic Analysis decides what is true.»

«Structural Analysis decides what shape is present.»

«Stylometric Analysis suggests what may be worth investigating.»

«Only proof authorizes transformation.»

The definitive architecture rule is:

«C++20 is useful to Paracine. Paracine does not depend on C++20.»

The definitive production rule is:

«One language. One semantic truth. Direct native realization. Optional portable export.»

PARACINE

.pcn → PCIR → PMIR → x86-64 → COFF → PE32+ → .exe

Optional:

.pcn → PCIR → C++20 → .cpp

Clear to people. Obvious to machines.
