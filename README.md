PARACINE — ".pcn"

Final Native-First Normative Resolution and Production Completion Standard

Edition: Ultimate Multi-Output Production Standard
Language edition: 2026
Specification level: 1.0 Final
Status: Frozen production definition
Canonical compiler implementation: C++23

Primary compilation model: Independent direct-native compilation
Primary native object realization: COFF ".obj"
Primary native executable realization: PE32+ ".exe"

Optional portability/export realization: Standards-oriented C++20 ".cpp"
C++20 required for Paracine compilation: No

Canonical Tier-1 target: Windows x86-64
Canonical external ABI: Microsoft x64 ABI
Canonical semantic IR: PCIR
Canonical machine IR: PMIR
Canonical compiler-analysis framework: SAGE — Semantic Analysis & Guidance Engine

This document completes and freezes the Paracine Language Specification and Paracine Backend Specifications.

Where this document conflicts with an earlier "[Decision]", "[Verify]", open issue, tentative statement, incomplete backend clause, or wording that implied C++20 was structurally necessary to Paracine, this document is authoritative.

All earlier provisions not changed here remain in force.

No "[Decision]", "[Verify]", “open issue,” “future specification,” or unspecified mandatory production component remains in Paracine 1.0.

The defining architectural distinction is now permanent:

«Paracine is a complete native compiler without C++20.»

The C++20 exporter is useful.

It is not required.

---

1. Final Language Identity

Paracine is a statically typed, ahead-of-time compiled, native general-purpose and systems programming language designed around a self-contained compiler and direct machine realization.

Its essential production path is:

.pcn
 ↓
canonical normalization
 ↓
lexer + layout
 ↓
parser
 ↓
semantic frontend
 ↓
SAGE semantic core
 ↓
ownership / effects / closure resolution
 ↓
PCIR
 ↓
semantic optimization
 ↓
target legalization
 ↓
PMIR
 ↓
x86-64 machine code
 ↓
COFF object model
 ↓
COFF writer / native linker
 ↓
.obj / PE32+
          ↓
         .exe

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

optimized PCIR
     ↓
optional C++20 exporter
     ↓
.cpp

That exporter exists for:

portability
unsupported-target reach
existing C++ project integration
human inspection
bootstrapping
compiler validation
differential testing
foreign build systems

It is a strategic secondary backend.

It is not part of Paracine's semantic core.

The following statements are final:

C++23 = implementation language of the Paracine compiler

SAGE  = semantic, structural and optional advisory analysis framework

PCIR  = authoritative semantic optimization representation

PMIR  = target-aware machine representation

x86-64 machine code
      = canonical Tier-1 native instruction realization

COFF  = canonical Tier-1 native object realization

PE32+ = canonical Tier-1 executable realization

C++20 = optional portability/export representation

The native language remains complete if the C++20 exporter is not installed, not built, or entirely removed.

---

2. Final Resolution Ledger

Every former design question is resolved as follows.

ID| Final resolution
R1| Indentation is exactly four spaces. Tabs remain forbidden for structural indentation.
R2| Layout tokenization is purely lexical and does not depend on parser knowledge.
R3| "text" is an immutable borrowed UTF-8 slice. "string" is its owning counterpart.
R4| Paracine 1.0 supports zero-allocation capturing closures as statically typed, monomorphized closure values.
R5| Ordinary routine pointers remain environment-free. A noncapturing closure may coerce to a routine pointer.
R6| There are no implicit numeric conversions. Contextual literal fitting remains allowed.
R7| Capability satisfaction remains structural by name/signature with deterministic lookup. Associated types are not part of edition 2026.
R8| Record fields remain in declaration order. The compiler never silently reorders them.
R9| Integer "+", "-", "*", and unary negation use defined modular two's-complement semantics.
R10| Shift counts are reduced modulo operand width.
R11| Signed "MIN / -1 = MIN"; "MIN % -1 = 0".
R12| Default routine parameters are shared borrows; "var" is exclusive borrowing; "own" transfers ownership.
R13| Explicit lifetime parameters do not exist in edition 2026. Borrow regions are inferred and nonlexical.
R14| "@from(...)" remains the explicit returned-borrow disambiguator.
R15| Traps terminate the process. They do not unwind Paracine scopes and are not catchable exceptions.
R16| Thread traps obey the same terminal-trap model. The former recoverable "thread_panic" model is removed.
R17| "give" automatically wraps values into "ok" or "some" where required by the declared result type.
R18| "fail" automatically produces "err".
R19| Floating contraction and reassociation remain disabled outside "@fastmath".
R20| Default hash maps use process-randomized hashing. Stable deterministic hashing remains separately available.
R21| Allocating growth operations are fallible. Convenience constructors may trap on OOM while retaining "try_" forms.
R22| The import graph remains acyclic.
R23| Visibility remains private / public / export.
R24| "vec<T,N>" remains a compiler-defined exception to the no-user-operator rule.
R25| One source file defines one module, and its path must agree with its module name.
R26| General textual/preprocessor macros do not exist in edition 2026.
R27| User procedural macros do not exist in edition 2026. Built-in "@derive" is the declaration-generation mechanism.
R28| Async/await is not a language construct in edition 2026. Structured threads, synchronization, channels and nonblocking APIs form the concurrency model.
R29| Windows x86-64 is the only mandatory direct-native Paracine 1.0 target. Additional platforms may be reached by future direct-native profiles or by the optional C++20 exporter.
R30| Index disjointness is conservative unless statically proved or established through a disjointness primitive such as "split_at".
R31| Associated types are deliberately excluded. Multi-parameter capabilities are canonical.
R32| Trap cleanup is deliberately absent. Recoverable failure uses "result<T,E>" and "option<T>".
R33| Build-time metaprogramming uses const evaluation and explicit build-tool steps rather than arbitrary compiler-executed scripts.
R34| Nullable "raw<T>" is not guaranteed to use null niche optimization inside "option<raw<T>>".
R35| Safe "view", "span", "text" and empty "buffer" values use canonical non-null sentinels, enabling guaranteed null niches for their optional forms.
R36| "raw" parameter mode is legal only when raw authority actually exists in the parameter type.
R37| Tier-1 unwind information uses the Windows x64 unwind representation implemented and verified by Paracine.
R38| PDB emission is a native tooling facility and not part of language-semantic equivalence.
R39| Production executables use ASLR-compatible relocation, high-entropy VA and NX compatibility by default.
R40| Control Flow Guard is enabled only when complete guard metadata is successfully emitted.
R41| Reproducible builds contain no uncontrolled wall-clock timestamps.
R42| Native executable linking is complete within Paracine.
R43| C++20 generation is not required for a conforming native Paracine compiler.
R44| ".obj" and ".exe" are Paracine's primary Tier-1 production realizations.
R45| C++20 ".cpp" generation is an optional portability/export subsystem branching from optimized PCIR.
R46| Absence of the C++20 exporter does not make a Paracine compiler incomplete.
R47| Semantic Analysis is deterministic and authoritative.
R48| Structural Analysis identifies computational form but does not change program meaning.
R49| Stylometric Analysis is optional, local and advisory. It can suggest but cannot establish semantic truth.
R50| Every optimization remains subject to independent semantic proof regardless of stylometric suggestion.
R51| Stylometry cannot determine validity, type resolution, overload resolution, ownership, alias legality, synchronization legality, arithmetic meaning or unsafe correctness.
R52| Paracine stylometry is not an authorship-identification facility.
R53| The compiler may maintain a project-local Semantic-Stylometric Fingerprint, but it is not part of language semantics.
R54| Default reproducible production builds do not permit unpinned stylometric state to alter emitted machine code.
R55| Style-guided optimization is explicit and, when enabled, its exact profile hash becomes a declared build input.

These resolutions are frozen for edition 2026.

---

3. Corrected Lexical and Layout Definition

3.1 Source normalization

Before lexical analysis:

1. source is decoded as UTF-8;
2. a leading UTF-8 byte-order mark is discarded;
3. CRLF and CR are normalized to LF;
4. Unicode source is normalized to NFC outside raw byte-oriented compiler internals;
5. a missing final LF is conceptually added;
6. trailing horizontal whitespace outside literals is ignored.

Tabs remain legal inside:

text literals
raw text
escaped character content
comments

Tabs are illegal in structural indentation.

---

4. Final Layout Algorithm

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

NEWLINE
INDENT

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

NEWLINE
DEDENT...

until that value becomes the top.

Otherwise:

E0102 inconsistent dedent

Continuations

Inside:

(...)
[...]
{...}

physical newlines are ordinary whitespace.

A physical line whose first significant token is:

->

continues the preceding pipeline expression.

No backslash-continuation mechanism exists.

At EOF, emit one final "NEWLINE" if necessary and enough "DEDENT" tokens to restore indentation zero.

---

5. Final Reserved Vocabulary

Edition 2026 keywords are:

abi
alias
alloc
and
as
break
capability
choose
const
continue
defer
else
export
fail
false
for
foreign
from
give
gives
if
import
in
io
is
let
loop
module
move
not
nothing
or
own
public
pure
raw
record
return_
routine
static
test
then
thread
trap
true
try
unsafe
var
variant
when
where
while
with
yield_

"return_" and "yield_" are reserved without edition-2026 semantics.

Predeclared identifiers include:

byte short int long
ubyte ushort uint ulong
float double
bool char text
size ssize
void never

option result
view span ref mut
buffer array
raw atomic string
ordering

Prelude suppression is controlled by package configuration:

no_prelude = true

or:

no_std = true

The former "import std.prelude as nothing" form is removed.

---

6. Completed Punctuation

Edition 2026 recognizes:

+ - * / % & | ^ ~ << >>
== != < <= > >=
= += -= *= /= %= &= |= ^= <<= >>=
-> => ... . .. ..=
, : :: ? @
( ) [ ] { }

"..." is legal only in:

foreign variadic declarations
future-edition-reserved positions

Elsewhere:

E0208 illegal ellipsis

---

7. Corrected Statement Grammar

statement =
      simple_statement NEWLINE
    | compound_statement
    ;

simple_statement =
      let_stmt
    | var_stmt
    | assign_stmt
    | expr_stmt
    | give_stmt
    | fail_stmt
    | break_stmt
    | continue_stmt
    ;

compound_statement =
      if_stmt
    | choose_stmt
    | loop_stmt
    | while_stmt
    | for_stmt
    | defer_block
    | unsafe_block
    ;

block =
    NEWLINE INDENT statement { statement } DEDENT
    ;

defer_stmt =
    'defer' expr
    ;

defer_block =
    'defer' block
    ;

label =
    '@' identifier
    ;

break_stmt =
    'break' [ label ]
    ;

continue_stmt =
    'continue' [ label ]
    ;

loop_stmt =
    [ label ':' ] 'loop' block
    ;

while_stmt =
    [ label ':' ] 'while' expr block
    ;

for_stmt =
    [ label ':' ] 'for' [ 'var' ] pattern 'in' expr block
    ;

Labeled jumps therefore use:

break @outer
continue @scan

---

8. Completed Top-Level Grammar

item_body =
      routine_decl
    | record_decl
    | variant_decl
    | alias_decl
    | const_decl
    | static_decl
    | thread_static_decl
    | capability_decl
    | foreign_decl
    | test_decl
    ;

static_decl =
    'var' 'static' identifier ':' type '=' const_expr
    ;

thread_static_decl =
    'thread' 'var' identifier ':' type '=' const_expr
    ;

A "thread var":

1. has one native-thread-local instance;
2. requires a const-evaluable initializer;
3. must be trivially disposable in edition 2026;
4. may be mutated safely by its owning thread;
5. may not be borrowed across thread boundaries;
6. may not appear directly in stable DLL ABI-visible layout.

Nontrivial thread-local destructors are deliberately excluded.

---

9. Final Closure Model

Paracine supports zero-runtime-overhead statically typed closures.

There is:

no GC closure object
no mandatory boxing
no dynamic closure dispatch
no hidden heap allocation

9.1 Syntax

closure_expr =
    'routine' '(' [ closure_param_list ] ')'
    [ 'gives' type ]
    ( '=>' expr | block )
    ;

closure_param_list =
    closure_param { ',' closure_param } [ ',' ]
    ;

closure_param =
    [ param_mode ] identifier [ ':' type ]
    ;

Example:

let twice = routine(x: int) => x * 2

let threshold = 10
let over = routine(x: int) => x > threshold

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

closure$42 {
    captured fields...
}

plus a statically known call implementation.

The closure is an ordinary monomorphic value.

It may:

live on the stack
move
be stored in another aggregate
be specialized
be passed generically
cross a thread boundary when its captures satisfy sendability

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

public signatures
export signatures
foreign declarations

Public generic APIs may accept a generic type constrained by "callable".

---

10. Standard Iterators Updated for Closures

Standard adapters accept statically typed callables.

Conceptually:

routine map<I, T, U, F>(
    own it: I,
    own f: F
) gives map_iter<I, T, U, F>
    where I is iterator<T>,
          F is callable<(T), U>

The same model applies to:

filter
fold
any
all
find
sort_by_key
parallel_for

Routine pointers satisfy the callable requirement.

Closure environments may disappear completely after:

specialization
inlining
SROA
DCE

Pipeline fusion remains mandatory for the standard built-in fusion cases.

---

11. Associated Types Resolution

Associated types do not exist in edition 2026.

The iterator relationship remains:

iterator<I,T>

rather than:

I::Item

This preserves:

simpler name resolution
simpler interface serialization
simpler monomorphization
no projection-normalization system
fewer cyclic type constraints
straightforward PCIR specialization

A future language edition may add associated types.

Edition 2026 does not depend upon them.

---

12. Type-System Representation Corrections

12.1 "never"

"never" is an uninhabited predeclared type with no runtime representation.

Expressions producing "never" include:

trap(...)
unreachable()
process.exit(...)
calls to routines declared gives never

"never" coerces to every expected type.

12.2 "ref<T>" and "mut<T>"

They are aliases:

ref<T> = view<T> constrained to count == 1
mut<T> = span<T> constrained to count == 1

They do not introduce another ownership mechanism.

12.3 Raw-pointer options

"raw<T>" is nullable.

Therefore:

option<raw<T>>

does not have a guaranteed null niche.

It may use an explicit tag or another proven representation.

The representation remains nonobservable at source level.

12.4 Canonical safe empty pointer

Safe empty:

view<T>
span<T>
text

values use a canonical aligned non-null sentinel.

An empty "buffer<T>" uses:

ptr      = canonical sentinel
count    = 0
capacity = 0

This permits guaranteed null-niche option encodings for:

option<view<T>>
option<span<T>>
option<text>
option<buffer<T>>

Unsafe constructors must normalize raw representations before exposing a safe value.

12.5 "option<bool>"

Tier-1 representation:

0 = false
1 = true
2 = none

12.6 "option<char>"

Tier-1 uses the invalid scalar:

0x00110000

as "none".

---

13. "raw" Parameter Mode Resolution

The syntax:

raw x: T

grants raw authority associated with "T".

It is valid only when "T" is:

raw<U>
an @raw_authority record
an explicitly @raw_authority aggregate

Valid:

raw p: raw<int>

Invalid:

raw x: int

Diagnostic:

E0408 meaningless raw parameter mode

"raw<T>" itself remains the machine-pointer type.

---

14. Final Effect System

The effect set is:

alloc
io
thread
trap
unsafe

"pure" means the empty set.

It cannot be combined with another effect.

Examples:

with io, alloc

and:

with pure

14.1 Inference

Routine bodies receive minimum inferred effect sets.

Typical contributions:

allocation                    → alloc
file/console/network/process  → io
thread/synchronization block  → thread
possible defined trap         → trap
unsafe authority operation    → unsafe

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

---

15. Final Failure Model

Paracine has two failure classes.

Recoverable failure

Represented by:

result<T,E>
option<T>

using:

try
fail
choose

Normal cleanup executes.

Terminal trap

Includes:

explicit trap
bounds violation
division by zero
assertion failure
unreachable
stack exhaustion
configured OOM trap

A trap:

does not unwind
does not execute defer
does not execute dispose
does not become a result
does not resume
does not become thread_panic

Trap handler:

routine(trap_code, text) gives never

---

16. Thread Trap Resolution

There is no recoverable language trap across thread boundaries.

There is no:

std.thread.on_trap(recover)

and no trap-derived:

thread_panic

A trap on any normal Paracine thread terminates the process.

"join" is:

routine join<R>(own h: join_handle<R>)
    gives result<R, thread_error>

"thread_error" reports operating-system/runtime join failures.

It does not represent language traps.

---

17. Structured Thread Scope Resolution

Canonical scoped threading is generic over callable values:

routine scope<F>(own f: F)
    where F is callable<(var thread_scope), void>

Example:

var values = ...

scope(routine(var s)
    spawn_scoped(var s, routine()
        use(values)
    )
)

Captured borrows are known to the compiler.

"scope" cannot return until all scoped child threads have completed.

Borrow lifetimes therefore terminate before the enclosing scope call returns.

---

18. Interior Mutability Resolution

Safe interior mutability is confined to compiler-known and standard-library abstractions whose implementation encapsulates unsafe authority.

Edition 2026 includes:

atomic<T>
mutex<T>
rwlock<T>
once<T>
channel<T> internal state
approved thread-safe runtime handles

Ordinary user records may not mutate through a shared borrow.

---

19. Index Disjointness Resolution

The borrow checker treats:

a[i]
a[j]

as potentially overlapping unless it proves otherwise.

Proof may use:

constant inequality
normalized ranges
dominating comparisons
induction-variable facts
range facts

Programs requiring guaranteed independently mutable regions may use:

split_at
chunks_mut
split_first_mut

or equivalent standard disjointness primitives.

---

20. Macro and Compile-Time Generation Resolution

Edition 2026 provides no:

textual macros
token macros
syntax macros
procedural compiler plugins
arbitrary source-generating compiler callbacks

It does provide:

const evaluation
const generics
monomorphization
@derive
@feature
explicit build-tool steps

Built-in derives:

equatable
ordered
hashable
printable
defaultable

---

21. Async Resolution

Edition 2026 contains no:

async
await
coroutine syntax
mandatory event loop
hidden scheduler

Asynchronous systems may use:

threads
nonblocking APIs
channels
callbacks
explicit state machines
libraries

No coroutine ABI is reserved in 1.0.

---

22. Other Native Targets Resolution

The mandatory direct-native target is:

windows-x86_64-msx64

Additional native platforms require complete target profiles defining:

primitive widths
endianness
pointer width
ABI
instruction lowering
object format
executable format
relocation model
unwind model
TLS
atomics
runtime bindings
standard-library realization

The optional C++20 exporter may provide portability to environments without a direct Paracine backend.

That portability path is a convenience and compatibility mechanism.

It is not required to establish Paracine as a complete language.

A Paracine implementation that supports only the Tier-1 direct-native path may still be fully conforming.

---

23. Hardened Hashing Resolution

Default:

map<K,V>
set<T>

use a process-randomized SipHash-family state seeded from OS entropy.

Iteration order is unspecified and may differ between runs.

Stable deterministic hashing remains available through:

stable_hasher
hash_of
fnv1a
crc32
xxh3_64

A deterministic table can be explicitly created with:

map_with_hasher(stable_hasher())

---

24. Standard-Library Network Completion

The "std" tier includes:

std.net

Tier-1 networking uses Winsock through:

std.win.ws2_32

Core types:

ip_addr
ipv4_addr
ipv6_addr
socket_addr
tcp_stream
tcp_listener
udp_socket
shutdown
socket_error

Representative API:

routine resolve(host: text, service: text)
    gives result<buffer<socket_addr>, socket_error>

routine tcp_connect(addr: socket_addr)
    gives result<tcp_stream, socket_error>

routine tcp_bind(addr: socket_addr)
    gives result<tcp_listener, socket_error>

routine accept(var listener: tcp_listener)
    gives result<(tcp_stream, socket_addr), socket_error>

routine read(var stream: tcp_stream, dst: span<ubyte>)
    gives result<size, socket_error>

routine write(var stream: tcp_stream, src: view<ubyte>)
    gives result<size, socket_error>

routine udp_bind(addr: socket_addr)
    gives result<udp_socket, socket_error>

routine send_to(
    var socket: udp_socket,
    data: view<ubyte>,
    addr: socket_addr
) gives result<size, socket_error>

routine recv_from(
    var socket: udp_socket,
    dst: span<ubyte>
) gives result<(size, socket_addr), socket_error>

Socket initialization is demand-linked and one-time.

"no_std" provides no network runtime.

---

25. Dynamic-Library Completion

"std.dynlib" provides:

routine open(path: text)
    gives result<library, io_error>

routine symbol<T>(self: library, name: text)
    gives result<raw<T>, io_error>
    with unsafe

routine close(own lib: library)

Typed conversion of dynamically obtained symbols requires unsafe authority.

---

26. Global Allocator Completion

The global allocator is initialized before "main".

Replacement is legal only through the allocator startup phase.

Example:

@startup
routine configure_allocator()

Once the first allocation occurs, allocator replacement locks permanently.

Only the root package may select the global allocator provider.

Dependency packages cannot silently replace it.

---

27. SAGE — Semantic Analysis & Guidance Engine

SAGE is Paracine's unified analysis framework.

It is divided into three distinct layers:

Semantic Analysis
Structural Analysis
Stylometric Analysis

Their authority is deliberately unequal.

The governing rule is:

«Stylometry may suggest. Semantics must prove.»

The SAGE law is:

«Observe the programmer's expression. Understand the program. Prove the transformation.»

The compiler must never confuse familiarity with proof.

---

27.1 SAGE pipeline

The canonical frontend-analysis pipeline is:

.pcn source
    ↓
normalization
    ↓
lexer + layout
    ↓
parser
    ↓
typed syntax
    ↓
Semantic Analysis
    ↓
Semantic Knowledge Model
    ↓
Structural Analysis
    ↓
Structural Model
    ↓
optional Stylometric Analysis
    ↓
Semantic-Stylometric Fingerprint
    ↓
Intent / Pattern Advisory Facts
    ↓
ownership + effects + specialization completion
    ↓
semantic verification
    ↓
PCIR

Not every program requires the stylometric stage.

Semantic Analysis is mandatory.

Structural Analysis is part of the standard optimization-analysis architecture.

Stylometric Analysis is optional.

---

27.2 Semantic Analysis

Semantic Analysis answers:

«What is true about this program?»

It is authoritative.

It resolves:

identifiers
modules
imports
types
literal types
overloads
generic arguments
capability satisfaction
ownership
moves
borrows
borrow regions
effects
closure captures
constants
layouts
result propagation
control-flow reachability
initialization state
aliasing facts
unsafe boundaries
FFI compatibility

No later heuristic system may override a resolved semantic fact.

Semantic Analysis must be:

deterministic
reproducible
testable
independent of personal writing style
independent of stylometric confidence

---

27.3 Semantic Knowledge Model

SAGE materializes resolved compiler knowledge into a Semantic Knowledge Model, abbreviated:

SKM

The SKM may contain:

symbol identities
resolved types
generic substitutions
capability resolutions
ownership graph
borrow-region graph
effect graph
closure-capture graph
constant facts
range facts
initialization lattice
alias facts
source spans
public/private visibility
known layout facts
known call relationships

The SKM is compiler-internal.

It is not a new source-language abstraction.

It provides structured semantic facts to:

diagnostics
structural analysis
PCIR construction
pcn explain
optimization reasoning

---

27.4 Structural Analysis

Structural Analysis answers:

«What computational shape does this semantically valid program have?»

It may classify:

reduction loops
map/filter chains
sentinel loops
state machines
lookup-table patterns
arena-allocation phases
hot/cold branch patterns
invariant initialization
data-parallel loops
serialization passes
deserialization passes
validation pipelines
producer/consumer loops
accumulators
search loops
finite-state dispatch
resource-acquisition sequences

Structural Analysis operates from semantic facts, not textual guesswork alone.

Its Structural Model may include:

control-flow shape
data-flow shape
loop morphology
dependency graph
branch topology
allocation topology
ownership transfer shape
pipeline topology
call topology
mutation density

Structural classification may help select analyses.

It does not by itself prove an optimization legal.

---

27.5 Stylometric Analysis

Stylometric Analysis asks:

«How does this project habitually express computational ideas?»

It may observe patterns such as:

guard-clause placement
preferred immutable intermediates
common loop shapes
pipeline use
error-handling patterns
buffer naming relationships
validation order
allocation order
hot/cold organization
closure use
iterator use
module organization
generic usage
ownership-transfer patterns
branch structure
data-layout tendencies

Paracine stylometry is codebase-oriented.

It is not an authorship-identification facility.

It does not attempt to determine:

who wrote a file
a person's identity
demographics
personality
location
background

Its purpose is compiler guidance.

---

27.6 Semantic-Stylometric Fingerprint

The project-local advisory profile is the:

Semantic-Stylometric Fingerprint

abbreviated:

SSF

It may summarize:

control-flow morphology
mutation density
ownership patterns
error-handling morphology
loop forms
pipeline forms
allocation sequencing
branch structure
generic patterns
closure patterns
iterator patterns
naming relationships
module organization
resource-order patterns

Example SSF inference:

This project commonly validates input
before allocating output storage.

This project commonly expresses integer
reductions as either explicit accumulators
or std.iter.sum pipelines.

Such observations are not semantic truths.

---

27.7 Advisory facts

Stylometric output enters the compiler only as:

advisory facts

An advisory fact may say:

likely reduction
likely data-parallel kernel
likely map/filter pipeline
likely state machine
likely resource-order anomaly

It may not say:

this transformation is legal
this pointer does not alias
this overload was intended
this unsafe operation is safe
this synchronization can be removed

The latter require semantic proof.

---

27.8 Optimization interaction

SAGE may use advisory patterns to decide that an expensive analysis is worth attempting.

For example:

Stylometric classification
        ↓
likely data-parallel kernel
        ↓
run stronger dependence analysis
        ↓
semantic proof succeeds
        ↓
vectorize

The vectorization occurs because dependence analysis proved legality.

It does not occur because stylometry guessed correctly.

If semantic proof fails:

no transformation

even if stylometric confidence is 100%.

---

27.9 Forbidden stylometric authority

Stylometric Analysis may never determine:

whether source is valid
identifier meaning
name resolution
overload selection
type inference where semantics are ambiguous
ownership legality
borrow legality
pointer aliasing legality
unsafe correctness
FFI correctness
error-propagation legality
synchronization removal
atomic ordering changes
integer arithmetic semantics
floating-point semantics
trap legality
control-flow equivalence
optimization correctness

These are semantic responsibilities.

---

27.10 Diagnostics

Stylometry may improve explanation after semantics has diagnosed the actual issue.

Example:

E0501: `try` cannot propagate this error
from the current routine.

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

---

27.11 Consistency warnings

SAGE may issue nonfatal advisory warnings.

Example:

W0814 unusual resource-order pattern

This routine allocates output storage
before validating input.

Most structurally similar routines in
this package validate before allocation.

This is not a correctness error.

Such warnings are never elevated to language errors solely because a pattern is unusual.

---

27.12 "pcn explain"

SAGE powers richer compiler explanations.

Example:

pcn explain sum.pcn

may produce:

Detected source pattern:
    total += value

Structural classification:
    wrapping integer reduction

Project advisory pattern:
    similar reductions often use std.iter.sum

Semantic proof:
    accumulator has no observable intermediate use
    integer addition is wrapping and associative
    loop-carried dependence is reduction-only
    input regions do not overlap output

Optimization:
    normalized to reduction form
    bounds check removed
    vectorized
    scalar remainder retained

No source rewrite required.

Every listed optimization proof must correspond to actual compiler evidence.

---

27.13 Profile location

A project may maintain:

.paracine/
    semantic-style.pcs

The ".pcs" file is an implementation-defined versioned SAGE profile.

It may contain aggregated project patterns.

It does not contain executable program semantics.

---

27.14 Locality and privacy

Stylometric analysis is local by default.

The compiler does not require:

network upload
remote behavioral profiling
external identity analysis
cross-project author tracking

to use SAGE.

A build service may explicitly provide a profile as an input, but that profile is treated like any other build artifact.

---

27.15 SAGE modes

Canonical modes are:

off
advisory
guided

off

No stylometric analysis.

Semantic and structural compilation continue normally.

advisory

Default.

Stylometry may influence:

diagnostic ranking
lint suggestions
pcn explain
formatter recommendations
analysis explanations
developer-facing hints

It does not alter emitted native code.

guided

Explicit opt-in.

Stylometry may additionally influence:

analysis prioritization
nonmandatory optimization search order
compile-time heuristic budgets
specialization investigation priority
vectorization-analysis priority

Every resulting transformation still requires normal semantic legality proof.

---

27.16 Reproducibility

In "advisory" mode:

SSF changes must not change emitted machine code.

Therefore ordinary reproducible production output remains independent of changing style profiles.

In "guided" mode, a style profile may change which legal optimizations are discovered within finite optimization budgets.

Therefore:

the exact SSF version and content hash become declared build inputs

for reproducible output.

Two guided builds are byte-reproducible only when they use the same:

source
compiler
target
optimization profile
SSF version
SSF hash

Program semantics remain identical regardless.

---

27.17 PCIR relationship

Unproven stylometric facts do not become semantic PCIR metadata.

They may exist as:

analysis hints

outside the authoritative IR.

A fact may become ordinary PCIR metadata only after a semantic analysis independently proves it.

Thus:

stylometric guess
    ↓
analysis request
    ↓
semantic proof
    ↓
proven PCIR fact

not:

stylometric guess
    ↓
PCIR assumption

---

27.18 SAGE conformance law

The defining SAGE invariant is:

«No program accepted, rejected or semantically transformed solely because of stylometric inference may differ from what the authoritative semantic rules permit.»

---

28. PCIR Completion

PCIR remains Paracine's semantic center.

It receives only verified program meaning.

28.1 Poison and undef

PCIR contains no:

undef
poison
freeze

Every ordinary value is defined.

Unsafe erroneous execution is modeled through violated language guarantees rather than IR poison propagation.

28.2 Provenance

Pointer-producing operations retain known provenance identities:

alloca
global
heap allocation
foreign/raw unknown

"gep" preserves provenance.

Pointer-to-integer conversion loses safe reconstruction authority.

"inttoptr" is unsafe and yields raw/unknown provenance.

Integer inequality alone does not establish nonaliasing.

28.3 Calls and memory effects

Calls carry effect summaries such as:

readnone
readonly(classes)
write(classes)
alloc
free
sync
io
trap
unknown

Foreign calls default conservatively to:

unknown + io + trap

unless a trusted compiler/runtime declaration provides stronger facts.

28.4 Debug locations

Source-originating PCIR instructions retain source spans.

Synthetic instructions inherit meaningful origin information.

Every trap-capable source operation preserves its diagnostic location.

28.5 ABI independence

PCIR does not contain:

RCX
RDX
RSP
XMM0
COFF relocation types
PE RVAs

Those belong to later machine stages.

28.6 Serialization

Serialized PCIR contains:

magic
major version
minor version
type table
layout table
symbol table
functions
semantic metadata
content hash

SAGE advisory state is not serialized into semantic PCIR unless converted into independently proven facts.

---

29. Optimizer Completion

29.1 Determinism

Optimization passes are deterministic for identical:

PCIR
compiler version
target profile
optimization profile
declared guided-analysis inputs

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

no reassociation
no reciprocal approximation
no implicit FMA contraction
no signed-zero elimination
no NaN elimination

29.5 Trap ordering

The optimizer may not turn:

observable effect A
possible trap
observable effect B

into an execution where B becomes observable before that trap.

29.6 SAGE guidance

In advisory mode, stylometry cannot alter optimization output.

In guided mode, SAGE may prioritize optimization investigations.

It may never waive optimizer legality conditions.

---

30. PMIR Completion and Corrections

30.1 Windows unwind model

Tier-1 uses the Windows x64 unwind representation supported by Paracine's frame generator and verifier.

30.2 Leaf functions

A function that:

does not modify nonvolatile registers
does not allocate a stack frame
does not call another function

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

---

31. Native Linker — Complete Specification

The native linker is an essential Paracine component.

It is not an optional foreign-toolchain integration.

Its pipeline is:

object discovery
↓
archive discovery
↓
directive processing
↓
symbol construction
↓
archive extraction
↓
COMDAT resolution
↓
duplicate-definition resolution
↓
reachability graph
↓
section garbage collection
↓
identical-code folding when enabled
↓
section contribution merging
↓
import synthesis
↓
export synthesis
↓
TLS synthesis
↓
unwind synthesis
↓
load-config synthesis
↓
resource synthesis
↓
final section ordering
↓
RVA assignment
↓
file-offset assignment
↓
relocation application
↓
base-relocation synthesis
↓
PE headers
↓
debug directory
↓
checksum/signature reservation
↓
image verification
↓
PE32+

---

32. Link Input Discovery

Inputs may include:

in-memory Paracine object models
COFF .obj
static .lib
import .lib
explicit DLL imports
resource .res
Paracine runtime archives

Input processing is deterministic.

Library search paths come from:

package manifest
target profile
explicit command line
configured SDK locations

The linker does not silently search arbitrary current-directory libraries to satisfy unresolved names.

---

33. Symbol Model

Symbols are classified as:

defined strong
defined weak
COMDAT
undefined
absolute
import
foreign common-compatible

Paracine source itself does not generate C tentative/common definitions.

Foreign objects may contain them.

Duplicate strong definitions produce:

L1001 duplicate strong symbol

Unresolved final symbols produce:

L1002 unresolved external symbol

---

34. Archive Extraction

Static libraries use iterative demand extraction:

build unresolved set
scan archive indexes
extract satisfying members
add their new unresolved references
repeat to fixed point

A member is loaded once.

Whole-archive behavior is available through:

pcn link --whole-archive library.lib

Archive extraction does not depend upon fragile one-pass library ordering.

---

35. COMDAT Resolution

NODUPLICATES

Multiple live candidates are an error.

ANY

Select the deterministic canonical candidate.

Preference:

root package
earlier dependency order
lexicographically smaller object identity

SAME_SIZE

All candidates must have equal byte size.

EXACT_MATCH

Candidate bytes and normalized relocation graphs must match.

ASSOCIATIVE

Contribution lifetime follows its leader.

Used for:

pdata
xdata
debug fragments
associated constant data

LARGEST

Choose the largest candidate.

Deterministic tie-breaking applies.

---

36. Section Garbage Collection

Release profiles enable section GC by default.

Roots include:

entry point
exports
forced includes
TLS structures
TLS callbacks
load-config roots
resource roots
@used symbols
startup hooks
shutdown hooks

Edges include:

relocations
associative COMDAT relationships
unwind references
import thunks
export forwarders
startup tables

Unreachable removable contributions are discarded.

---

37. Identical Code Folding

ICF is optional.

Default release "opt=3" may enable it for eligible nonexported functions.

Two functions may fold only when:

normalized bytes match
relocation topology matches
unwind behavior matches
observable address identity is irrelevant
neither has @unique_address
neither requires distinct export identity

Address-taken functions are not folded by default.

---

38. Final Native Image Sections

A Tier-1 image may contain:

.text
.rdata
.data
.pdata
.xdata
.idata
.edata
.reloc
.tls
.rsrc
.debug

".bss" semantics occupy zero-filled writable virtual storage without requiring matching raw bytes.

Loader semantics are defined by PE structures and directories, not by section names alone.

---

39. Section Permissions

Paracine enforces W^X by default.

Typical final permissions:

.text   read + execute
.rdata  read
.pdata  read
.xdata  read
.idata  loader-compatible read/write as required
.edata  read
.data   read + write
.tls    read + write template semantics
.rsrc   read
.reloc  read + discardable

Ordinary production output does not contain writable-executable sections.

---

40. Final Section Layout

Defaults:

FileAlignment    = 512 bytes
SectionAlignment = 4096 bytes

Final RVAs and file offsets use alignment-aware layout.

"SizeOfRawData" is file-aligned.

"VirtualSize" reflects actual virtual contribution size.

"SizeOfImage" and "SizeOfHeaders" are computed from finalized layout.

---

41. Import Construction

Imports are grouped by DLL.

Paracine emits:

Import Directory
Import Lookup Tables
Import Address Tables
DLL names
Hint/Name records

Each import table is correctly terminated.

Imported named functions use the required hint/name representation.

Ordinal imports use the PE32+ ordinal encoding.

References to:

__imp_symbol

resolve to IAT slots.

Typical direct call:

call qword ptr [rip + IAT-slot]

---

42. Delay Imports

Delay loading is explicit.

It may be requested through:

@delay_import

or package configuration.

The linker emits the required delay-import structures and runtime resolver support.

If unused, the directory is zero.

---

43. Export Construction

Exports contain:

public name
target symbol
optional ordinal
optional NONAME
optional forwarder

Ordinals are unique.

Unassigned ordinals are deterministically assigned.

The linker emits:

Export Directory
Export Address Table
Name Pointer Table
Ordinal Table
strings
DLL identity

Anonymous closure types and private internal-ABI routines cannot be directly exported.

---

44. Resource Construction

Paracine accepts ".res" input and manifest-defined resources.

Resource trees normalize by:

type
name or ID
language

The linker emits the complete ".rsrc" directory tree and payloads.

No resources means a zero Resource data directory.

---

45. TLS Construction

"thread var" items become native TLS template content.

The linker emits:

initialized TLS bytes
zero-fill requirement
TLS index storage
optional callbacks
IMAGE_TLS_DIRECTORY64

Edition-2026 source TLS initialization is const-evaluable and requires no arbitrary user callback.

Runtime TLS callbacks may still exist where the runtime requires them.

---

46. Exception and Unwind Construction

Surviving nonleaf functions receive:

RUNTIME_FUNCTION
UNWIND_INFO

The linker resolves final RVAs, sorts ".pdata", verifies ranges and installs the Exception data directory.

Malformed unwind information is a link failure or internal compiler error depending on source.

---

47. Base Relocation Construction

Paracine uses RIP-relative native code whenever practical.

Remaining image-base-dependent addresses generate base relocations.

Tier-1 primarily uses:

DIR64

Relocations are grouped by 4 KiB page.

Production dynamic-base images contain required relocation information.

---

48. Relocation Application

For relocation evaluation:

S = resolved symbol
A = addend
P = fixup address

Typical Tier-1 rules:

ADDR64   → S + A
ADDR32NB → RVA(S) + A
REL32    → S + A - address_after_fixup
SECTION  → section index
SECREL   → section-relative offset

Overflow is diagnosed.

No silent truncation is allowed.

---

49. PE32+ Header Construction

The final image contains:

DOS header
minimal DOS stub
PE signature
COFF image header
PE32+ optional header
data directories
section table
section data
optional certificate data

Tier-1 machine:

AMD64

Optional header:

PE32+

The writer computes all required image sizing, subsystem, stack, heap and directory fields.

Reserved fields are zero.

---

50. Image Base

Default EXE preferred image base:

0x0000000140000000

DLL image base is linker-selected and correctly aligned.

ASLR means code must not depend upon actual preferred-base loading.

---

51. Subsystem

Applications select:

subsystem = "console"

or:

subsystem = "windows"

Console is the default.

"pcn_start" adapts the ordinary Paracine "main" contract to either process type.

---

52. Default PE Hardening

Production images normally enable:

HIGH_ENTROPY_VA
DYNAMIC_BASE
NX_COMPAT
TERMINAL_SERVER_AWARE

"NO_SEH" is not set by default.

"FORCE_INTEGRITY", "APPCONTAINER" and specialized image characteristics are explicit deployment options.

---

53. Control Flow Guard

CFG is optional production hardening.

The linker sets "GUARD_CF" only after constructing all required guard metadata.

Indirect targets may include:

address-taken routines
ABI thunks
foreign-visible exports
runtime callbacks

Incomplete CFG generation causes link failure rather than a misleading partial image.

---

54. Load Configuration

A load-config directory is synthesized when required by enabled hardening facilities.

Unused fields are zero.

The chosen structure size matches the target-profile version actually emitted.

---

55. Stack and Heap Defaults

Defaults:

stack reserve = 8 MiB
stack commit  = 64 KiB

heap reserve  = 1 MiB
heap commit   = 64 KiB

These are image defaults, not language semantics.

---

56. Entry Point

EXE entry:

pcn_start

DLL startup entry when required:

pcn_dll_start

"pcn_start" performs:

runtime initialization
OS argument acquisition
UTF-8 argument conversion
standard-stream initialization
trap-handler installation
random hash seeding
runtime subsystem initialization
startup hooks
user main
stream flush
process exit

No C runtime is required.

---

57. DLL Startup

DLL process attachment performs only loader-safe work.

General allocation, thread creation and arbitrary application callbacks do not run under loader lock.

Richer initialization uses:

initialize()

or safe lazy initialization.

---

58. Debug Directory

Native debug output may contain a PE debug directory referring to CodeView data.

PDB identity uses deterministic information when reproducible mode is active.

Uncontrolled absolute build paths are not embedded in reproducible mode.

---

59. PDB Completion

The native PDB writer supports:

line information
public symbols
private symbols at -g2
types
modules
source mapping
register/stack variable location ranges

It maintains the necessary MSF/PDB streams.

Conformance is judged by successful consumption by expected Windows debugging tooling and accurate source/symbol resolution.

PDB generation is not required to define language semantics.

---

60. Native Debug Levels

-g0
    no debug information

-g1
    source and line mappings
    public function names

-g2
    full supported type,
    lexical scope,
    local variable,
    and location-range information

Optimized-away values are reported honestly as unavailable or optimized away.

---

61. PE Checksum

Default ordinary EXE/DLL policy:

Checksum = 0

unless explicitly required.

Command:

pcn pe --checksum

may compute it before signing.

---

62. Authenticode and Certificate Table

Signing is a tooling/deployment operation.

It is not part of Paracine semantics.

Commands may include:

pcn sign
pcn verify-signature

Signing occurs after deterministic image construction.

Private keys are never stored directly in normal package manifests.

---

63. Reproducible Native Linking

Identical declared build inputs produce byte-identical native outputs except intentionally external signatures.

Rules include:

zeroed reproducibility-sensitive timestamps
deterministic symbols
deterministic sections
deterministic padding
deterministic PDB identity
normalized source paths
no random binary padding

Runtime randomized hash seeds are generated at process startup and do not affect binary reproducibility.

SAGE advisory mode does not affect emitted code.

SAGE guided mode requires its SSF hash to be treated as a declared build input.

---

64. Build Identity

A build may emit:

program.build.json

containing:

source hashes
package graph
lockfile hash
compiler version
language edition
SAGE version
SAGE mode
SSF version/hash when guided mode is used
PCIR version
PMIR version
optimization profile
target triple
CPU profile
native backend version
COFF writer version
linker version
PE writer version
standard-library version
feature set
reproducibility mode

If the optional C++20 exporter participates, it additionally records:

C++20 exporter version
portable runtime-helper version

If it does not participate, no C++ exporter identity is required.

---

65. Import Library Completion

DLL builds may also emit:

module.lib

Import library names and ordinals are validated against the final DLL export table.

Conventional:

__imp_Name
Name

symbols are provided where required.

---

66. Static Library Completion

"pcn lib" emits deterministic Windows archives containing:

linker members
long-name tables when needed
object members
symbol indexes

The writer supports independent read-back validation.

---

67. Bigobj Resolution

Paracine uses ordinary COFF while legal.

It switches to Windows bigobj when ordinary section/symbol limits are exceeded.

Bigobj is an object-file mechanism only.

It does not alter PE image semantics.

---

68. Native Link Diagnostics

Diagnostic families:

L1000 input/object
L1100 symbol
L1200 relocation
L1300 COMDAT/archive
L1400 import/export
L1500 image layout
L1600 unwind/TLS/load-config
L1700 debug/PDB
L1800 signing/certificate
L1900 internal image verification

Representative diagnostics:

L1001 duplicate strong symbol
L1002 unresolved external
L1104 incompatible symbol ABI
L1204 relocation overflow
L1302 COMDAT exact-match disagreement
L1401 duplicate export name
L1402 duplicate export ordinal
L1501 image exceeds supported small code model
L1601 malformed unwind contribution
L1602 invalid TLS template
L1901 generated PE failed self-validation

---

69. Final Image Verification

Before final output, an independent PE reader reparses the produced image.

It verifies:

signatures
machine type
PE32+ format
header bounds
section-table bounds
file alignment
section alignment
virtual overlap
raw overlap
entry-point location
directory bounds
imports
IAT
exports
pdata sorting
unwind references
TLS
relocations
load config
resources
debug directory
certificate bounds
SizeOfImage
SizeOfHeaders
permissions
reserved fields

Failure deletes the output and raises an internal compiler error.

---

70. Independent Loader Test

Conformance testing launches generated images through the Windows loader and validates:

ASLR
imports
TLS
unwind
console startup
GUI startup
DLL loading
exports
thread creation
trap termination
large stack frames
debug symbolication

---

71. Optional C++20 Exporter

C++20 generation is no longer described as a required backend of Paracine.

It is the:

optional portability/export backend

A conforming native compiler may omit it entirely.

Its pipeline, when installed, is:

optimized PCIR
    ↓
C++20 legalization
    ↓
type lowering
    ↓
control-flow reconstruction
    ↓
ownership/runtime mapping
    ↓
C++20 source emission
    ↓
.cpp

The exporter may also generate:

.hpp
.h

when requested.

Its absence does not affect:

parsing
semantic analysis
SAGE
PCIR
PMIR
machine encoding
COFF generation
native linking
PE generation
native execution

---

72. Optional C++20 Exporter Semantics

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

divisor == 0
MIN / -1

72.6 Float-to-int

Conversion implements:

truncate toward zero
saturate
NaN → 0

without depending on undefined host behavior.

72.7 Layout

ABI-relevant representations receive static layout assertions.

72.8 Atomics

C++20 atomics use explicit corresponding memory orders.

72.9 TLS

"thread var" lowers to suitable "thread_local" storage under the same edition-2026 trivial-destruction constraints.

---

73. Optional Cross-Backend Differential Validation

If the optional C++20 exporter is installed, Paracine should use it as an additional validation oracle.

Then:

Paracine
    ↓
C++20 exporter
    ↓
MSVC/Clang

may be compared against:

Paracine
    ↓
PMIR
    ↓
native PE

and against:

PCIR reference interpreter

This is valuable.

It is not required to prove that the native compiler exists independently.

A native-only conforming compiler does not fail conformance because C++20 differential tests are unavailable.

---

74. Module and Manifest Completion

Example:

[package]
name = "acme.tool"
version = "1.0.0"
edition = "2026"
kind = "app"
root = "src"
no_std = false
no_prelude = false

[target.windows-x86_64]
abi = "msx64"
cpu = "x86-64"
subsystem = "console"
image_base = 0x140000000
stack_reserve = 8388608
stack_commit = 65536
heap_reserve = 1048576
heap_commit = 65536
cfg = true

[sage]
stylometry = "advisory"
profile = ".paracine/semantic-style.pcs"

[profile.debug]
opt = 0
debug = 2
reproducible = true
emit = ["obj", "exe"]

[profile.release]
opt = 3
debug = 1
lto = true
icf = true
gc_sections = true
reproducible = true
emit = ["obj", "exe"]

[profile.portable]
optional_backend = "cxx20"
emit = ["cpp"]

[link]
libraries = ["kernel32.lib"]
search = []
whole_archive = []

[resources]
files = []

[signing]
enabled = false

If the optional C++20 exporter is unavailable, requesting:

emit = ["cpp"]

produces a clear backend-unavailable diagnostic.

It does not make the installation incapable of ordinary native compilation.

---

75. Build-Step Completion

Arbitrary compiler-time dependency scripts remain forbidden.

Explicit root-workspace steps may be declared:

[[step]]
name = "generate_protocol"
tool = "tools.protocol_generator"
inputs = ["schema/protocol.pcn-schema"]
outputs = ["generated/protocol.pcn"]

The build engine:

hashes inputs
hashes the tool
runs it under declared workspace rules
records outputs
rejects undeclared writes in sandboxed mode

Generated source becomes ordinary Paracine source.

---

76. Capability Lookup Freeze

Capability lookup order remains:

1. module defining the subject type;
2. module defining the capability;
3. current module.

Multiple exact candidates at equal precedence produce:

E0340 ambiguous capability member

Compiler-intrinsic relationships such as "callable" bypass ordinary user lookup.

---

77. Generic Coherence

Two visible definitions cannot simultaneously satisfy the same effective capability member signature at equal precedence without ambiguity.

Paracine uses no global impl-block registry.

Ambiguity is resolved or diagnosed at the use site.

---

78. Const Evaluation Completion

Const evaluation supports:

pure arithmetic
records
variants
arrays
bounded loops
pure routine calls
const generics
supported const text operations

It cannot use:

I/O
threads
foreign calls
unsafe memory access
runtime randomness
time
environment state
dynamic libraries
networking
stylometric project state

SAGE stylometric state is never available to const evaluation.

Thus program constants cannot depend on stylistic profiling.

---

79. Static Initialization Completion

There is no hidden user dynamic-global initialization.

Allowed:

const X = const_expr

var static X: T = const_expr

thread var X: T = const_expr

Runtime startup hooks are explicit.

---

80. Startup and Shutdown Hooks

Attributes:

@startup
@shutdown

apply only to:

routine() gives void

Startup order is deterministic.

Shutdown runs in reverse order on normal "main" completion.

Shutdown does not execute after:

trap
process.exit
forced process termination

---

81. Main Routine Completion

Accepted forms:

routine main() gives void

routine main() gives int

routine main(args: view<text>) gives int

routine main() gives result<void, E>
    where E is printable

routine main(args: view<text>) gives result<void, E>
    where E is printable

Windows UTF-16 arguments are converted to valid UTF-8 before user code receives them.

Invalid source UTF-16 input becomes a startup failure.

---

82. No-Std Completion

With:

no_std = true

available facilities include:

primitive types
records
variants
option/result
arrays
views/spans
raw pointers
core arithmetic
PCIR/native code generation
required compiler intrinsics

A freestanding package may provide:

@entry

The native compiler does not require the optional C++ exporter in "no_std" mode or any other mode.

---

83. Unsafe Boundary Completion

Public unsafe-containing routines are classified as either:

Safe encapsulation

Caller has no raw invariant obligation.

No "with unsafe" is exposed.

Unsafe contract

Caller must uphold raw/lifetime/alias/platform requirements.

The declaration includes:

with unsafe

and documentation states the contract.

---

84. FFI ABI Compatibility

C/MS x64-compatible types include:

fixed-width integers
float
double
documented bool representation
raw<T>
routine pointers
@layout(c) compatible records

Paracine-native abstractions such as:

text
view
span
buffer
option
result
variant
string
closure

are not automatically foreign-ABI types.

They require explicit wrappers or thunks.

---

85. Exported Paracine APIs

Ordinary Paracine-to-Paracine exports may use Paracine-native types because interface metadata communicates their ABI.

A foreign-facing:

@export_name

routine must use foreign-compatible types.

Violation:

E0624 foreign export exposes non-FFI type

---

86. Runtime Version Compatibility

Paracine native libraries record:

language edition
public ABI version
standard-library ABI version
target profile

The native linker rejects incompatible combinations.

---

87. PCIR-to-PMIR Machine Verification

The backend verifies:

virtual registers resolved
frame indexes resolved
ABI placement correct
RSP aligned at calls
msx64 shadow space valid
nonvolatile registers restored
direction flag valid
instruction/register combinations legal
branch targets valid
relocations representable
unwind information matches prologue
epilogs satisfy target rules

Failure is an internal compiler error.

---

88. Native Encoder Completion

The integrated encoder covers:

legacy prefixes
REX
VEX
EVEX where allowed
opcode maps
ModRM
SIB
displacements
immediates
rel8
rel32
RIP-relative addressing

A separate decoder validates final instruction bytes.

"pcn asm" displays decoded final machine output rather than merely echoing PMIR intent.

---

89. CPU Feature Safety

The CPU baseline is a declared build input.

The compiler never emits instructions beyond that baseline except inside properly dispatched multiversion bodies.

Deployment to an incompatible CPU is a configuration error, not silent emulation.

---

90. Multiversion Completion

Example:

@multiversion("x86-64", "x86-64-v3")
routine work(...)

may generate:

work$base
work$v3
work$resolver
work$slot

The resolver uses baseline instructions, detects CPU features once and publishes the selected implementation atomically.

Exports retain a stable thunk.

---

91. Runtime Independence

Tier-1 Paracine has no hidden dependency on:

MSVCRT
UCRT
C++ runtime
LLVM runtime
libgcc

unless explicitly introduced by linked foreign code.

The runtime is demand-linked from Paracine native objects.

The optional C++20 exporter does not participate in the normal ".obj" or ".exe" production chain.

---

92. Security Baseline

Production defaults include:

safe-code bounds checks
NX compatibility
ASLR compatibility
high-entropy VA
W^X
randomized default hashing
stack probing
valid unwind metadata
UTF-8 validation
checked allocation-size arithmetic
hardened object parsing
hardened archive parsing
no automatic dependency build scripts

SAGE advisory data cannot weaken safety checking.

Stylometric familiarity is never evidence that unsafe code is safe.

---

93. Compiler Input Hardening

External object/archive/PDB/resource/package inputs are treated as untrusted.

Checked arithmetic is required for:

offset + size
count * element_size
alignment
section ranges
string ranges
relocation indexes
symbol indexes
archive offsets
resource recursion

Malformed inputs produce diagnostics rather than unchecked compiler memory operations.

---

94. Resource Limits

Configurable limits include:

parser nesting
generic instantiation depth
const-eval steps
const recursion
module count
import edges
object sections
symbol count
optimizer growth
diagnostic cascade
SAGE structural-analysis budget
SAGE stylometric-profile size

Limits do not change language semantics.

---

95. Final Conformance Suites

A complete native Paracine 1.0 implementation must pass the following mandatory suites.

Frontend

normalization
layout
grammar
name resolution
modules
overloads
generics
closures
capabilities
effects
const evaluation

SAGE Semantic Core

semantic resolution determinism
SKM correctness
ownership facts
borrow facts
effect facts
closure capture facts
alias facts
structural-model consistency

SAGE Stylometric Boundary

When stylometry is implemented:

advisory facts cannot change semantic validity
advisory mode cannot change emitted code
no stylometric override of type/name/borrow rules
guided transformations require semantic proof
guided reproducibility records SSF hash

Type and ownership

copy/move
partial move
drop flags
shared borrow
exclusive borrow
returned borrow
closure capture regions
thread-scoped borrows
raw boundary
FFI compatibility

Semantics

integer operators
shift semantics
signed division extremes
float conversion
NaNs
signed zero
try/fail/give
cleanup
trap order

PCIR

construction
verification
interpretation
optimization passes
Path Compression
SSA Reduction
fusion
vectorization legality
effect preservation

PMIR

instruction selection
ABI lowering
register allocation
spilling
stack alignment
large frames
unwind
branch relaxation
encoding

COFF

sections
symbols
relocations
COMDAT
bigobj
archives
import libraries
foreign linker interoperability

PE

headers
imports
exports
resources
TLS
pdata/xdata
relocations
ASLR
load config
CFG
DLLs
debug directory
PDB
self-verification
actual Windows loading

Native semantic oracle

Mandatory comparison targets include:

PCIR reference interpreter
direct Paracine native executable

If the optional C++20 exporter is present, an additional optional-backend suite compares:

C++20/MSVC
C++20/Clang
direct native
PCIR interpreter

Failure to ship a C++20 exporter does not fail native language conformance.

---

96. Release Qualification

A native component may be described as production only when:

mandatory conformance tests pass
release-blocking semantic miscompilations are zero
native image verification passes
PCIR/native differential tests pass
ABI matrix passes
unwind matrix passes

If the optional C++20 exporter is shipped as production, it must additionally pass its C++ differential suite.

A compiler may ship:

native production backend
+
experimental or absent C++ exporter

without weakening the status of its native compiler.

---

97. Final Implementation Order

The canonical implementation strategy is native-first.

Phase 1

UTF-8 normalization
lexer
layout engine
parser
typed syntax
basic diagnostics

Phase 2

name resolution
types
generics
capabilities
effects
ownership
closure capture analysis

Phase 3

SAGE Semantic Core
Semantic Knowledge Model
Structural Analysis
optional advisory stylometry

Phase 4

PCIR construction
PCIR verifier
reference interpreter

Phase 5

basic optimizer
SCCP
mem2reg
SROA
DCE
range/BCE

Phase 6

x86-64 PMIR
instruction selection
register allocation
frame layout
machine encoder

Phase 7

COFF writer
object reader
archive reader/writer

Phase 8

native linker
imports
exports
relocations
PE headers
runtime entry
first .exe

Phase 9

unwind
TLS
resources
DLLs
hardening
debug support

Phase 10

advanced optimizer
inlining
specialization
fusion
vectorization
Path Compression
SSA Reduction

Phase 11

full standard library
PDB tooling
SAGE explain/lint refinement
profiling/toolchain polish

Optional Phase 12

C++20 exporter
.hpp/.h generation
foreign C++ integration helpers
C++ differential testing

The native compiler is architecturally complete before Optional Phase 12 begins.

---

98. First Native Milestone Contract

The first architecture-proof compiler supports:

int
bool
text literals
routines
let/var
arithmetic
comparison
if
while
calls
give
minimal output or Windows foreign calls

and directly produces:

hello.pcn
 ↓
semantic frontend
 ↓
PCIR
 ↓
PMIR
 ↓
x86-64
 ↓
COFF/native link
 ↓
hello.exe

without:

C++ generation
external assembler
external linker
LLVM
MSVC compilation
Clang compilation

This is the foundational proof of Paracine's independent architecture.

---

99. Final Standard-Library Tiers

core

std.core
std.mem core
std.num
std.math
std.slice
std.iter
std.atomic
std.hash
std.fmt core
std.text core
std.simd

alloc

std.buffer
std.text.string
std.collections
std.sort
std.mem allocators

std

std.io
std.fs
std.path
std.env
std.time
std.thread
std.sync
std.process
std.random
std.net
std.dynlib
std.win
std.test

No tier silently depends upon a higher tier.

No tier requires C++ source generation.

---

100. Final Philosophy of Memory Safety

Safe Paracine guarantees:

no use after move
no double dispose
no dangling safe borrow
no shared/mutable alias violation
no out-of-bounds memory access
no safe data race
no uninitialized safe read

It does not guarantee:

no leak
no deadlock
no starvation
no logical message-order race
no stack exhaustion
no valid-computation denial of service

Unsafe contract violations create erroneous execution.

Stylometric analysis never weakens this boundary.

---

101. Final Definition of No Undefined Behavior in Safe Code

Safe source has no language-level undefined behavior.

Every operation results in one of:

defined value
recoverable result
defined trap
defined scheduling nondeterminism
documented target-platform behavior

Compiler optimizations cannot invent stronger assumptions.

Erroneous behavior enters only through:

unsafe contract violation
foreign ABI/contract violation
external hardware/OS behavior outside the abstract machine

A SAGE advisory inference is never a source of undefined assumptions.

---

102. Final Production Architecture

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

---

103. DLL and Static-Library Production

DLL production

PCIR
 ↓
PMIR
 ↓
x86-64
 ↓
native linker
 ↓
PE32+ DLL
 ├── module.dll
 └── module.lib

Static library production

PCIR
 ↓
PMIR
 ↓
COFF objects
 ↓
archive writer
 ↓
module.lib

Neither requires C++20.

Optional portable textual export

PCIR
 ↓
optional C++20 exporter
 ↓
module.cpp
module.hpp

---

104. Final One-Language Rule

Paracine's source of truth is:

.pcn

Primary native products include:

.obj
.exe
.dll
.lib

Supporting products include:

.pdb
.h

Optional portability/export products include:

.cpp
.hpp

None are separate language dialects.

They are realizations or supporting artifacts of one Paracine program.

---

105. Final Backend Law

A backend may choose a different representation.

It may not choose a different meaning.

The governing rule is:

«Semantics flow downward. Backends do not flow upward into the language.»

The optional C++20 exporter therefore does not define Paracine semantics.

PCIR and the language definition do.

---

106. Final Analysis Law

Paracine's compiler-analysis hierarchy is:

Semantic Analysis
    What is true?

Structural Analysis
    What computational shape exists?

Stylometric Analysis
    What recurring expression patterns exist?

Their authority is:

Semantic truth
    >
Structural classification
    >
Stylometric suggestion

The final rule is:

«Observe. Classify. Suggest. Prove.»

No advisory system may bypass proof.

---

107. Final Optimization Law

The optimization law remains:

«Resolve what is known. Remove what is unnecessary. Represent what remains directly.»

Resolve includes:

constant evaluation
name resolution
type resolution
capability resolution
ownership resolution
borrow resolution
closure capture resolution
effect resolution
range proof
layout proof
specialization
semantic pattern proof

Remove includes:

dead code
dead storage
dead checks
dead cleanup
dead abstraction
dead iterator layers
dead branches
dead symbols
dead closure fields

Represent directly means:

smallest semantically valid PCIR
then smallest target-appropriate PMIR
then smallest legal machine realization

Stylometry may help the compiler decide where to look.

It may not decide what is true.

---

108. Final Output Hierarchy

Paracine's production hierarchy is:

«Primary native realization: ".pcn → .obj / .exe"»

Additional native deployment realizations include:

.pcn → .lib
.pcn → .dll

The optional portable realization is:

«Optional portability/export realization: ".pcn → .cpp"»

Supporting optional artifacts include:

.hpp
.h
.pdb

The C++20 exporter may be completely absent while Paracine remains a complete independent native language.

---

109. Final Multi-Output Law

«One Paracine semantic program may be realized directly as native object code, native libraries, native DLLs, or complete PE32+ executables, and may optionally be exported as standards-oriented C++20 source, without changing the Paracine language contract.»

Native production is primary.

C++ export is secondary.

Neither changes the source language.

---

110. Final Production Principle

«Write the meaning once. Resolve it once. Prove it once. Optimize it once. Materialize only what deployment requires.»

And for SAGE:

«Stylometry may suggest. Semantics must prove.»

---

111. Final Language Definition

Paracine is a mature statically typed ahead-of-time native programming language with:

explicit data
explicit ownership
inferred nonlexical borrowing
static zero-allocation closures
monomorphized generics
structural capabilities
defined arithmetic
recoverable result semantics
terminal traps
structured native concurrency
authoritative semantic analysis
structural computational analysis
optional advisory stylometric analysis
PCIR semantic optimization
PMIR machine lowering
direct x86-64 machine encoding
direct COFF generation
direct native linking
direct PE32+ construction
no mandatory GC
no mandatory exceptions
no mandatory scheduler
no hidden dynamic initialization
no mandatory external compiler
no mandatory external assembler
no mandatory external linker
no mandatory C++ generation

Its essential Tier-1 machine path is:

.pcn
 ↓
frontend
 ↓
Semantic Analysis
 ↓
SAGE / semantic resolution
 ↓
PCIR
 ↓
PMIR
 ↓
x86-64
 ↓
COFF
 ↓
native link
 ↓
PE32+

Its optional portability/export path is:

optimized PCIR
 ↓
optional C++20 exporter
 ↓
.cpp

The first path defines Paracine's independence.

The second extends Paracine's reach.

---

112. Frozen Edition-2026 Statement

Edition 2026 is frozen when the reference native implementation conforms to the grammar, semantics, ownership rules, SAGE semantic boundary, PCIR contract, PMIR contract and Tier-1 native backend described by the complete 1.0 specification.

The optional C++20 exporter is not a condition of language completeness.

Compatible additions may include:

new optimizations
new diagnostics
new SAGE advisory patterns
new standard-library APIs
new optional exporters
new native target profiles

provided existing semantics remain unchanged.

A new language edition is required for incompatible changes to:

grammar
type semantics
ownership
borrowing
arithmetic
trap semantics
module resolution
capability semantics
closure semantics
safe-code guarantees
ABI-visible language representation not protected by versioning

A new SAGE stylometric heuristic alone does not require a new language edition because stylometry has no semantic authority.

---

113. Final Canonical Summary

PARACINE ".pcn"

Readable source
+
Exact static types
+
Authoritative semantic analysis
+
Structural computational analysis
+
Optional advisory stylometry
+
Explicit ownership
+
Nonlexical borrowing
+
Zero-allocation static closures
+
Recoverable result handling
+
Defined machine arithmetic
+
PCIR semantic optimization
+
Path Compression
+
SSA Reduction
+
Direct PMIR lowering
+
Direct x86-64 machine encoding
+
Direct COFF generation
+
Direct native linking
+
Direct PE32+ generation
────────────────────────────────
Independent Native Program
────────────────────────────────
              ↓
        .obj / .exe

Optional portability exists beside that architecture:

optimized PCIR
     ↓
optional C++20 exporter
     ↓
.cpp

The definitive production hierarchy is:

PRIMARY NATIVE

.pcn → .obj
.pcn → .exe

ADDITIONAL NATIVE

.pcn → .lib
.pcn → .dll

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

## *** ##

# Paracine `text` / `string` Semantic and Native Representation Expansion

## Governing definition

Paracine shall represent `text` and `string` as two distinct language types with distinct semantic meaning, ownership behavior, mutation authority, PCIR identity, and target representation.

The frozen Edition-2026 rule is:

**`text` is an immutable borrowed UTF-8 slice.**

**`string` is its owning counterpart.**

The two types are related, but they are not aliases and must never collapse into a single semantic or IR type.

The intended model is:

```text
text
    immutable
    borrowed
    valid UTF-8
    pointer + byte count
    no ownership of backing storage
    freely share-borrowable
    no mutation operations
    no allocation required merely to view

string
    owning
    mutable
    valid UTF-8 string storage
    owns capacity/storage
    participates in move/drop semantics
    mutation requires exclusive access
    growth may allocate
    converting to text may borrow without copying
```

For truly arbitrary binary data whose contents are not required to remain valid UTF-8, Paracine continues to use:

```text
buffer<ubyte>
view<ubyte>
span<ubyte>
array<ubyte, N>
raw<ubyte>
```

This preserves the specification's separation between human-readable UTF-8 text and general-purpose byte storage.

---

# 1. Add distinct semantic types

The current semantic layer must stop representing both `text` and `string` with `SemanticTypeKind::String`.

Replace the existing conceptual form:

```cpp
enum class SemanticTypeKind {
    Unknown,
    Void,
    Bool,
    Number,
    String,
    Null,
    Custom
};
```

with:

```cpp
enum class SemanticTypeKind {
    Unknown,
    Void,
    Bool,
    Number,
    Text,
    String,
    Null,
    Custom
};
```

`Text` and `String` must remain separate through:

```text
parsing
↓
typed syntax
↓
SAGE
↓
SKM
↓
PCIR
↓
optimization
↓
target legalization
↓
PMIR
↓
x86-64
```

They must not become distinguishable only at PMIR or ABI lowering.

SAGE is the authoritative semantic layer, and PCIR is the authoritative semantic optimization representation, so the distinction must already exist before PCIR construction.

## Semantic constructors

Add:

```cpp
inline SemanticType makeTextType() {
    return {SemanticTypeKind::Text, "text"};
}

inline SemanticType makeStringType() {
    return {SemanticTypeKind::String, "string"};
}
```

Then update:

```cpp
semanticTypeFromName()
semanticTypeName()
isAssignableTo()
isTruthyCompatible()
generic substitution
capability resolution
closure capture typing
SKM serialization
```

so `text` and `string` are never treated as identical merely because both contain UTF-8 data.

For example:

```cpp
inline SemanticType semanticTypeFromName(const std::string& name) {
    if (name.empty()) return makeUnknownType();
    if (name == "void") return makeVoidType();
    if (name == "bool") return makeBoolType();

    if (isNumericTypeName(name))
        return {SemanticTypeKind::Number, name};

    if (name == "text")
        return makeTextType();

    if (name == "string")
        return makeStringType();

    if (name == "null" || name == "nothing")
        return makeNullType();

    return makeCustomType(name);
}
```

There must be no equivalent of:

```cpp
if (name == "text" || name == "string")
    return sameType;
```

after this change.

---

# 2. Add `IRValueKind::Text`

PCIR must preserve the semantic distinction.

The current IR model should be expanded from:

```cpp
enum class IRValueKind {
    Unknown,
    Void,
    Bool,
    Int64,
    String,
    ...
};
```

to:

```cpp
enum class IRValueKind {
    Unknown,
    Void,
    Bool,
    Int64,

    Text,
    String,

    Null,
    Function,
    Aggregate,
    Result,
    Option,
    Closure
};
```

Add separate constructors:

```cpp
inline IRValueType makeIRTextType() {
    return {IRValueKind::Text, "text"};
}

inline IRValueType makeIRStringType() {
    return {IRValueKind::String, "string"};
}
```

and update:

```cpp
irValueTypeName()
irValueTypeFromName()
IR verification
PCIR printer
PCIR interpreter
optimizer
PMIR legalization
C++20 exporter
ABI inspector
```

accordingly.

The type-name conversion becomes:

```cpp
IRValueType irValueTypeFromName(const std::string& name) const {
    if (name == "void") return makeIRVoidType();
    if (name == "bool") return makeIRBoolType();
    if (isNumericTypeName(name)) return makeIRIntType();

    if (name == "text")
        return makeIRTextType();

    if (name == "string")
        return makeIRStringType();

    if (name == "null")
        return makeIRNullType();

    // result / option / closure / aggregate...
}
```

This eliminates the current semantic collapse in which both source types are lowered through `makeIRStringType()`.

---

# 3. String literals produce `text`

A normal quoted Paracine literal:

```pcn
"Hello, world"
```

must have type:

```text
text
```

not:

```text
string
```

This follows directly from the language's borrowed immutable-text model and from the README's native milestone, which explicitly identifies **text literals** as a foundational supported feature.

The AST node may remain named `StringExpr` internally if desired for compatibility, although renaming it to `TextLiteralExpr` would make the compiler architecture clearer.

Semantically:

```cpp
else if constexpr (std::is_same_v<T, StringExpr>) {
    return makeTextType();
}
```

And during PCIR construction:

```cpp
else if constexpr (std::is_same_v<T, StringExpr>) {
    lastValueType_ = makeIRTextType();
    return emitConst(IROpcode::ConstText, node.value);
}
```

The preferred opcode should therefore become:

```cpp
ConstText
```

rather than using `ConstString` for immutable literals.

A compatibility alias may temporarily exist internally:

```cpp
ConstStringLegacy
```

but new PCIR should not use it.

## Literal storage

A literal is immutable static UTF-8 storage.

Conceptually:

```text
"Paracine"

.rdata:
    50 61 72 61 63 69 6E 65
```

and the resulting native `text` value is:

```text
{
    ptr   = address_of_literal,
    count = 8
}
```

The README requires source decoding as UTF-8 and identifies `text` as UTF-8 content.

---

# 4. Define the native representation of `text`

The existing rich-value legalization model should retain `text` as a borrowed two-word slice:

```cpp
struct NativeText {
    const std::uint8_t* ptr;
    std::size_t count;
};
```

On Windows x86-64:

```text
offset 0: pointer       8 bytes
offset 8: byte count    8 bytes

size:      16
alignment: 8
```

The count is a UTF-8 **byte count**, not necessarily a Unicode scalar-value count.

`text` itself does not:

```text
allocate
grow
free storage
mutate bytes
own capacity
```

An empty safe `text` must use the canonical non-null sentinel required by the specification:

```text
ptr   = canonical_non_null_empty_sentinel
count = 0
```

That requirement exists specifically so:

```text
option<text>
```

can use the null pointer as a guaranteed niche.

Therefore:

```text
empty text:
    ptr != null
    count == 0

none option<text>:
    ptr == null
```

must remain an ABI invariant.

---

# 5. Define the native representation of `string`

`string` is the owning counterpart of `text`.

The canonical implementation representation should therefore be an owned UTF-8 buffer:

```cpp
struct NativeString {
    std::uint8_t* ptr;
    std::size_t count;
    std::size_t capacity;
};
```

Conceptually:

```text
offset 0:  owned byte pointer
offset 8:  current UTF-8 byte count
offset 16: allocated byte capacity

size:      24
alignment: 8
```

`string` owns its allocation.

Therefore moving a string transfers:

```text
ptr
count
capacity
```

and invalidates the moved-from ownership state according to normal Paracine move rules.

Destroying a live string releases its owned allocation.

An empty string should use the same safe-empty principles used by other safe buffer-like types where applicable rather than manufacturing dangling pointers.

Because `std.text.string` belongs to the allocation tier, string construction or growth that requires storage belongs to allocation-capable code rather than the allocation-free `core` text layer.

---

# 6. Keep UTF-8 validity as a semantic invariant

`text` is always valid UTF-8.

`string`, as the owning counterpart of `text`, should likewise maintain valid UTF-8 whenever exposed as a normal safe Paracine `string`.

Safe mutation operations may therefore not leave a string permanently containing malformed UTF-8.

For example:

```text
append(text)             safe
append(char)             safe
insert(char)             safe
replace(text, text)      safe
truncate(char-boundary)  safe
```

Operations that work directly with bytes must either:

```text
prove the resulting sequence remains UTF-8
```

or return a checked result such as:

```pcn
result<void, utf8_error>
```

or remain behind an unsafe/raw boundary.

Arbitrary binary payloads therefore belong in:

```pcn
buffer<ubyte>
```

rather than normal `string`.

This is also consistent with the specification's production security baseline, which explicitly includes UTF-8 validation.

---

# 7. Add explicit conversions

No implicit conversion should silently change ownership.

The specification already rejects broad implicit numeric conversion, and the same explicitness principle should govern ownership-changing text/string transformations.

## `string.to_text()`

Borrow an immutable view into an existing owned string:

```pcn
routine to_text(self: string) gives text
```

Conceptually:

```text
string
    ptr ------+
    count ----|----> text { ptr, count }
    capacity  |
```

No allocation is necessary.

No bytes are copied.

The returned `text` borrow may not outlive the source `string`.

SAGE must therefore record returned-borrow provenance from the source string.

Conceptually:

```pcn
routine to_text(self: string) gives text
    @from(self)
```

or the equivalent compiler-intrinsic provenance fact.

The README makes returned-borrow provenance explicit and states that borrow regions are inferred and nonlexical.

## `text.to_string()`

Create owned mutable storage containing the text:

```pcn
routine to_string(self: text) gives string
    with alloc
```

This operation:

```text
allocates capacity
copies UTF-8 bytes
establishes string ownership
preserves UTF-8 validity
```

and therefore contributes:

```text
alloc
```

to the inferred effect set.

Because allocating growth operations are fallible under the language rules, the underlying standard-library API should expose the appropriate fallible form as well, for example:

```pcn
routine try_to_string(self: text)
    gives result<string, alloc_error>
    with alloc
```

with any convenience trapping form contributing the appropriate trap effect.

---

# 8. Add dedicated PCIR conversion operations

Conversions must survive semantic lowering rather than becoming opaque runtime calls too early.

Add PCIR operations such as:

```cpp
TextToString,
StringToText
```

or, more explicitly:

```cpp
TextCloneToString,
StringBorrowAsText
```

Suggested semantics:

```text
string.borrow_text %string
    -> text

text.clone_string %text
    -> string
```

Example PCIR:

```text
v1 = const.text "hello" : text
v2 = text.to_string v1 : string
v3 = string.to_text v2 : text
```

Their semantic behavior is different:

```text
text.to_string
    ownership-producing
    potentially allocating
    effectful

string.to_text
    borrow-producing
    nonallocating
    ownership-preserving
```

The optimizer must not erase that distinction.

---

# 9. Add SAGE ownership rules

`text` and `string` must participate differently in ownership analysis.

## `text`

A `text` value is a borrowed immutable slice.

Reading it creates or extends only a shared borrow.

Multiple simultaneous readers are legal:

```pcn
routine inspect(a: text, b: text)
    ...
```

A `text` cannot authorize mutation of its backing bytes.

Therefore:

```text
shared text borrow + shared text borrow
    legal

shared text borrow + mutation through same storage
    illegal unless semantic ownership proves disjointness/lifetime separation
```

Because `text` contains borrowed storage, SAGE must retain its provenance.

## `string`

A `string` owns mutable storage.

Normal parameter modes apply exactly as specified by Paracine:

```pcn
s: string
```

means:

```text
shared borrow
```

and therefore cannot mutate the string.

```pcn
var s: string
```

means:

```text
exclusive borrow
```

and permits mutation.

```pcn
own s: string
```

means:

```text
ownership transfer
```

and permits consuming/replacing/reallocating operations.

The frozen specification explicitly defines default parameters as shared borrows, `var` as exclusive borrowing, and `own` as ownership transfer.

---

# 10. Enforce mutation authority

String mutation must not merely depend on whether the local declaration used:

```pcn
var
```

It must depend on the actual borrow/ownership state known to SAGE.

For example:

```pcn
routine inspect(s: string)
    s.append("!")       # error
```

The parameter is a shared borrow.

Mutation is illegal.

Correct:

```pcn
routine modify(var s: string)
    s.append("!")
```

or:

```pcn
routine consume(own s: string) gives string
    s.append("!")
    give s
```

SAGE must reject any operation requiring mutable storage when the active authority is only shared.

This follows Paracine's general safety guarantee of no shared/mutable alias violation.

---

# 11. Add string mutation capabilities

Paracine capability satisfaction is structural by name/signature with deterministic lookup.

String operations should use that framework rather than creating a completely separate mutation system.

Conceptually, built-in/library capabilities may describe operations such as:

```pcn
capability appendable<S, T>
    routine append(var self: S, value: T)
```

A `string` can structurally satisfy appropriate capabilities such as:

```text
appendable<string, text>
appendable<string, char>
clearable<string>
resizable<string>
```

The critical rule remains:

**capability satisfaction says the operation exists; borrow authority says whether this particular call may mutate this particular value.**

Those are independent questions.

For example:

```text
string satisfies appendable<string,text>
```

does not make this legal:

```pcn
routine f(s: string)
    s.append("x")
```

because `s` remains only shared.

It becomes legal through:

```pcn
routine f(var s: string)
    s.append("x")
```

---

# 12. Integrate mutation with the effect system

Paracine's effect set is fixed as:

```text
alloc
io
thread
trap
unsafe
```

with `pure` meaning the empty set.

There is **no separate generic `mutate` effect** in the frozen Edition-2026 effect set.

Therefore mutation itself should be governed by borrow authority, while additional consequences contribute existing effects.

Examples:

```pcn
routine clear(var s: string)
```

may remain:

```text
pure
```

if it merely changes count without allocating or trapping.

But:

```pcn
routine append(var s: string, value: text)
```

may require:

```text
alloc
```

because growth can require reallocation.

Likewise:

```text
reserve
grow
insert
replace
to_string
```

may contribute:

```text
alloc
```

when allocation is possible.

An implementation should therefore not invent:

```text
with mutate
```

unless the language specification itself is deliberately changed.

Instead, Paracine should preserve the clean separation:

```text
borrow/ownership system
    answers:
    "may this code mutate this value?"

effect system
    answers:
    "what externally relevant or exceptional operations may this routine perform?"
```

---

# 13. Add nonlexical borrow behavior for `string.to_text()`

This is especially important.

Consider:

```pcn
routine example(var s: string)
    let view = s.to_text()
    inspect(view)

    s.clear()
```

This should be legal.

The immutable borrow created by:

```pcn
s.to_text()
```

ends after the last use of:

```text
view
```

so the later exclusive mutation:

```pcn
s.clear()
```

does not overlap it.

But this is illegal:

```pcn
routine example(var s: string)
    let view = s.to_text()

    s.clear()
    inspect(view)
```

because the shared text borrow remains live through `inspect(view)` and overlaps the attempted exclusive mutation.

The README requires inferred **nonlexical** borrow regions rather than explicit lifetime parameters.

SKM should therefore record something conceptually equivalent to:

```text
borrow region:
    source = s
    kind   = shared
    begin  = string.to_text
    end    = last use of view
```

---

# 14. Make closure capture rules distinguish `text` and `string`

Closure capture must preserve the same distinction.

Reading outer `text`:

```pcn
let message: text = "hello"

let f = routine()
    use(message)
```

creates a shared capture.

Reading an outer `string` without mutation:

```pcn
let name: string = ...

let f = routine()
    inspect(name)
```

also creates a shared capture.

Mutating the string:

```pcn
var name: string = ...

let f = routine()
    name.append("!")
```

requires an exclusive capture.

Moving the string:

```pcn
let f = move routine()
    consume(name)
```

creates an owned capture where legal.

These are exactly the general closure capture categories established by the specification:

```text
read     -> shared
mutate   -> exclusive
consume  -> own
```


---

# 15. Give PCIR distinct text/string operations

PCIR should not force all text and string behavior through generic calls.

Useful semantic operations include:

```text
const.text
text.length
text.compare
text.slice
text.validate
text.to_string

string.new
string.from_text
string.to_text
string.length
string.capacity
string.reserve
string.append
string.insert
string.erase
string.clear
string.truncate
string.dispose
```

Whether all become primitive PCIR opcodes is an optimizer/backend design choice.

At minimum, the following semantic distinctions must be visible:

```text
immutable borrow
owned mutable storage
allocation
reallocation
borrowed projection
ownership transfer
destruction
```

This gives optimization enough information to eliminate unnecessary copies and allocations while still preserving borrow and effect correctness.

---

# 16. Teach the optimizer about `text`

`text` is an excellent optimization type because it is immutable.

Legal transformations include:

```text
constant length folding
constant equality folding
literal deduplication
slice folding
repeated-borrow elimination
CSE of immutable text operations
inlining of text views
elimination of unused text materializations
```

For example:

```pcn
let x = "Paracine"
let n = x.count()
```

may reduce to:

```text
n = 8
```

where semantics permit.

Two identical static literals may share backing storage because neither can mutate it.

---

# 17. Teach the optimizer about `string`

String optimizations require stronger proofs because ownership and mutation matter.

Potential optimizations include:

```text
reserve folding
capacity propagation
allocation sinking
allocation elimination
move elision
string-to-text borrow forwarding
copy elimination
SROA of temporary strings
dead string construction elimination
```

For example:

```pcn
let owned = "hello".to_string()
let view = owned.to_text()
consume(view)
```

may be optimized aggressively if SAGE proves:

```text
owned does not escape
no mutation occurs
no API requires ownership
```

Potentially the owned `string` allocation may disappear entirely and `view` may reference the literal directly.

But that transformation is legal only when semantic proof confirms no observable ownership or mutation difference.

Paracine's optimizer must independently prove every transformation; advisory SAGE/stylometric information may never manufacture semantic authority.

---

# 18. Extend rich-value ABI legalization

The ABI layer must now maintain two separate representations.

## `text`

```text
ABI class: borrowed rich slice
size:      16
align:     8
fields:
    ptr
    count
ownership:
    none
drop:
    none
mutation:
    impossible through text
```

## `string`

```text
ABI class: owned mutable buffer
size:      24
align:     8
fields:
    ptr
    count
    capacity
ownership:
    owning
drop:
    releases allocation
mutation:
    requires exclusive/owned authority
```

Both are native Paracine types and therefore are not automatically foreign-ABI-compatible. The README explicitly lists both `text` and `string` among the types requiring explicit foreign wrappers or thunks.

---

# 19. Add PMIR operations

Target legalization can lower semantic PCIR into PMIR forms such as:

```text
PMIR_TEXT_ADDR
PMIR_TEXT_COUNT

PMIR_STRING_ADDR
PMIR_STRING_COUNT
PMIR_STRING_CAPACITY

PMIR_STRING_BORROW_TEXT
PMIR_STRING_MOVE
PMIR_STRING_DROP
PMIR_STRING_GROW
```

For:

```pcn
let t = s.to_text()
```

PMIR requires only projection of:

```text
s.ptr
s.count
```

There is no allocation and no byte copy.

For:

```pcn
let s = t.to_string()
```

PMIR must perform or call the allocation path, copy the byte sequence, and materialize:

```text
ptr
count
capacity
```

in the new owned object.

---

# 20. Define assignability explicitly

The types should not be implicitly interchangeable.

These should be rejected without an explicit conversion:

```pcn
let s: string = "hello"
```

if ordinary literal coercion is not specifically defined to allocate.

The fully explicit form is:

```pcn
let s: string = "hello".to_string()
```

Likewise:

```pcn
routine read(value: text)

var s: string = ...
read(s)
```

should either require:

```pcn
read(s.to_text())
```

or, if Paracine defines a contextual borrow coercion, that coercion must be:

```text
nonallocating
borrow-producing
lifetime tracked
never ownership-transferring
```

It must not be implemented as general type equality.

The compiler must never make this true:

```text
text == string
```

at the semantic-type level.

---

# 21. Add diagnostics

Introduce precise diagnostics rather than generic type mismatch errors.

Examples:

```text
E04xx cannot mutate string through shared borrow

E04xx mutable string borrow overlaps live text view

E03xx string growth requires alloc effect

E03xx text-to-string conversion may allocate

E02xx string value required; found borrowed text

E02xx text value required; explicit string.to_text() conversion available

E04xx use of string after ownership transfer

E04xx returned text borrow outlives source string

E05xx invalid UTF-8 cannot construct safe text

E05xx byte mutation would violate string UTF-8 invariant
```

Exact numeric assignments can be selected to fit Paracine's final diagnostic registry.

---

# 22. Update SKM

The Semantic Knowledge Model should record enough information to make text/string ownership explicit.

Suggested facts include:

```text
SKMTypeFact
    kind = Text | String

SKMBorrowRegionFact
    sourceSymbol
    borrowKind
    beginPoint
    endPoint
    projection

SKMOwnershipFact
    symbol
    owned
    moved
    disposed

SKMTextProvenanceFact
    textValue
    backingSymbol
    staticLiteral
    byteRange

SKMStringMutationFact
    stringSymbol
    operation
    requiresExclusive
    mayAllocate
```

This prevents PCIR, optimizer, and PMIR from independently reconstructing ownership facts.

---

# 23. Preserve distinctions in `result`, `option`, aggregates, and closures

The distinction must survive nesting.

These are all separate layouts and semantics:

```pcn
option<text>
option<string>

result<text, E>
result<string, E>

record A
    name: text

record B
    name: string

closure capturing text
closure owning string
```

`option<text>` receives the guaranteed null-niche representation because safe empty text uses a non-null sentinel.

`option<string>` must use whatever representation the compiler proves valid for the owning string representation; it must not simply inherit the text niche without proving the required invariant.

---

# 24. Update standard-library layering

The README already separates:

```text
std.text core
```

from:

```text
std.text.string
```

in the allocation tier.

That should become an architectural boundary.

`std.text` should contain allocation-free immutable operations such as:

```text
length
is_empty
compare
starts_with
ends_with
find
slice
UTF-8 traversal
Unicode scalar iteration
validation
```

`std.text.string` should contain owned/mutable operations such as:

```text
new
with_capacity
from_text
try_from_text
reserve
append
insert
erase
replace
clear
truncate
shrink
into_buffer
borrow_text
```

Operations that may grow storage contribute `alloc`.

---

# 25. Update the C++20 exporter

The optional exporter must preserve the same distinction.

A plausible representation is:

```cpp
struct pcn_text {
    const std::uint8_t* ptr;
    std::size_t count;
};

struct pcn_string {
    std::uint8_t* ptr;
    std::size_t count;
    std::size_t capacity;
};
```

The exporter must not reduce both to:

```cpp
std::string
```

because that destroys:

```text
borrow provenance
nonownership of text
allocation visibility
move semantics
capacity semantics
ABI equivalence
```

The C++20 exporter is a semantic-equivalent secondary backend, not permission to weaken Paracine's type model.

---

# 26. Required conformance tests

The implementation should add a dedicated text/string conformance matrix.

Compile-success cases should include:

```pcn
let t: text = "hello"
```

```pcn
let s = "hello".to_string()
let t = s.to_text()
```

```pcn
routine mutate(var s: string)
    s.append("!")
```

```pcn
routine consume(own s: string)
    ...
```

```pcn
routine many_readers(a: text, b: text)
    ...
```

NLL success:

```pcn
routine test(var s: string)
    let t = s.to_text()
    inspect(t)

    s.clear()
```

Expected failures should include:

```pcn
routine bad(s: string)
    s.clear()
```

because `s` is shared.

And:

```pcn
routine bad(var s: string)
    let t = s.to_text()

    s.clear()
    inspect(t)
```

because the immutable text borrow remains live.

And use-after-move:

```pcn
routine bad(own s: string)
    consume(s)
    inspect(s)
```

The mandatory Paracine conformance suites already require ownership facts, borrow facts, effect facts, PCIR verification, ABI lowering, and PCIR/native differential testing, so text/string tests must participate in all of those layers rather than only parser tests.

---

# 27. Final implementation rule

The compiler must never again use one internal "String" category to mean both borrowed text and owned mutable string storage.

The permanent model is:

```text
SOURCE

"hello"
   │
   ▼
 text
 immutable borrowed UTF-8
   │
   ├────────────── string.to_text() ◄──────────────┐
   │                                               │
   │                                               │
   └── text.to_string() ── alloc/copy ──► string  │
                                          │        │
                                          │ owned  │
                                          │ mutable│
                                          └────────┘
```

Semantically:

```text
text   != string
```

Operationally:

```text
text   = immutable borrow
string = owned mutable UTF-8 storage
```

For arbitrary binary storage:

```text
buffer<ubyte>
view<ubyte>
span<ubyte>
```

remains the canonical byte-oriented family.

This keeps Paracine aligned with its frozen Edition-2026 rules:

```text
text is immutable and borrowed
string owns the storage
default parameters are shared
var means exclusive borrowing
own transfers ownership
borrow regions are inferred and nonlexical
allocation contributes alloc
safe empty text has a canonical non-null representation
option<text> therefore has a guaranteed null niche
```

and carries that distinction intact from SAGE through PCIR, optimization, PMIR, x86-64, COFF, PE32+, and the optional C++20 exporter.

## *** ##

Paracine native backend upgrade — implementation and verification status
Status: An executable native backend for a verified, substantial subset of the supplied frontend, NOT a production-ready, complete implementation of every accepted language construct.

Source of patch: the user's exact 7,204-line CRLF attachment Pasted text(20261028-224523).txt (byte-exact after patch application).

Output source: Paracine_Backend_Expanded.cpp, 7,394 physical lines. C++23.
Changes relative to attached source
1. Rebased the previously developed rich-value codegen spine onto this exact attachment, replacing the older scalar-only PMIR/PCIR-oracle paths. Native values include records, text, results/options, and monomorphic closure environments. The ABI model includes hidden sret storage, Windows x64 argument lanes and rich-value stack storage.
2. Corrected choose from an unconditional pseudo-phi into a real sequence of conditional branches with lazily evaluated arms. A choose subject compares the subject against each when condition; subjectless choose requires bool conditions. Non-exhaustive choose expressions are rejected unless an otherwise arm exists.
3. Made defer scope-exit based, with LIFO expression execution on normal exits, give, return, fail, break, and continue. Return operands are snapshotted before running deferred cleanup. A corner case remains: early-return through the existing native result.try opcode has not been integrated with deferred cleanup.
4. Fixed an actual segmentation fault during lowering of for loops with break/`continue` by keeping the defer scope-depth stack synchronized with loop targets.
5. Added a reachable-path verifier that rejects non-void functions that fall through without a return, and explicit void returns at normal routine exit.
6. Changed the PMIR opcode mapper to fail closed on unhandled instructions instead of silently converting them to NOP.
7. The optional C++20 emitter now rejects rich-value instructions and print/println it cannot faithfully implement, instead of generating comments/no-op code that appears runnable.
8. Added native machine execution to the compiler's own test subcommand: on Windows, generate and run a PE through the Windows loader; on Linux x86-64, map emitted self-contained .text as RX and invoke with the Microsoft x64 ABI. On Linux this is direct machine-code execution, not PE loader emulation.
9. run likewise invokes the emitted Microsoft-x64 machine code on Linux x86-64 for self-contained programs. Native Win32 API calls are not supported in this execution mode.
Verification performed in this session
- GCC g++ -std=c++23 -O0: compile success.
- Clang clang++ -std=c++23 -O0: compile success.
- Actual emitted x86-64 executions compared against PCIR oracle: 5 programs × O0/O2/O3 = 15/15 passed: choose bool cases (17), choose subject (42), record passing (17), basic defer (17), loop break/continue (18).
- Additional choose lazy evaluation test: 4/4 passes at O0/O1/O2/O3; untaken arm divides by zero, but is never executed; result 23.
- Additional generated machine-code tests using compiler test command: concrete closure capture (17), result/try propagation (17) passed.
- Direct Windows-x64 ABI in-memory native call: string.to_text() returned matching source pointer/count and sret pointer; deferred string.clear() mutated count to zero without touching capacity.
- file and GNU objdump recognize generated executable as Windows PE32+ x86-64, six sections, with .pdata and .xdata unwind information.
- C++20 scalar choose export compiled and exited 17; rich C++20 export rejected explicitly.
- Missing return was rejected; implicit text-to-string assignment was rejected.
- Unified patch dry-run applied to a copy of the exact attached source; full patch application yielded a byte-for-byte copy of updated compiler.
What this build still does not support
Do not describe this build as executing the entire frontend or as industrially production-ready. Important unsupported/incomplete areas include:
- Owned text.to_string() allocation, growth, deallocation/destruction and allocation failure handling; the direct Windows native emitter correctly rejects it, rather than fabricating ownership of literal memory.
- Win32 import table, external C/foreign ABIs, import libraries, and native print/println I/O paths.
- Non-range iterators, multi-binding loops, structural pattern matching/exhaustiveness beyond the implemented choose form, labeled break/`continue`.
- Complete module/import/linking semantics, constant evaluation, generic container runtime, full array/buffer/view/span operations and variants/enums.
- Complete native/SAGE borrow and move proofs under every program shape, scope-exit cleanup when result.try early-returns, destructor/drop glue.
- Complete C++20 rich-value emitter; formatter/package/profile subsystems.
- A full Windows loader run or MSVC 2022 test was not performed in this Linux environment. Windows-specific loader/unwind/ASLR testing remains necessary.
- Formal conformance, fuzzing, security hardening, stress tests, stable diagnostics/ABI and release qualification have not been completed.
This artifact is a validated working backend upgrade and reproducible engineering milestone, not a claim that a many-subsystem language implementation is complete.
Build and test
g++ -std=c++23 -O0 Paracine_Backend_Expanded.cpp -o paracine
./paracine check tests/test_rich.pcn
./paracine test tests/test_rich.pcn
./paracine run tests/test_choose.pcn
./paracine build tests/test_rich.pcn -o result.exe
On Windows, add Paracine_Backend_Expanded.cpp to an x64 C++23 Visual Studio project, set your .pcn source as the command argument and invoke build, test, or run.

## *** ##

