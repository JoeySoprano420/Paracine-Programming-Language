PARACINE — ".pcn"

Final Normative Resolution and Production Completion Standard

Edition: Ultimate Multi-Output Production Standard
Language edition: 2026
Specification level: 1.0 Final
Status: Frozen production definition
Canonical compiler implementation: C++23
Canonical textual output: C++20
Canonical native object output: COFF ".obj"
Canonical native executable output: PE32+ ".exe"
Canonical Tier-1 target: Windows x86-64
Canonical external ABI: Microsoft x64 ABI
Canonical semantic IR: PCIR
Canonical machine IR: PMIR

This document completes and freezes the Paracine Language Specification and Paracine Backend Specifications.

Where this document conflicts with an earlier "[Decision]", "[Verify]", open issue, tentative statement, or incomplete backend clause, this document is authoritative.

All earlier provisions not changed here remain in force.

No "[Decision]", "[Verify]", “open issue,” “future specification,” or unspecified mandatory production component remains in Paracine 1.0.

---

1. Final Language Identity

Paracine is a statically typed, ahead-of-time compiled, native general-purpose and systems programming language built around one semantic program and multiple exact realizations.

The canonical production equation is:

                    .pcn
                      ↓
           normalization + frontend
                      ↓
            semantic resolution
                      ↓
           ownership/effect analysis
                      ↓
                    PCIR
                      ↓
           semantic optimization
                      ↓
          ┌───────────┼───────────┐
          ↓           ↓           ↓
       C++20         PMIR        PMIR
          ↓           ↓           ↓
        .cpp       x86-64       x86-64
                      ↓           ↓
                    COFF        native link
                      ↓           ↓
                    .obj        PE32+
                                  ↓
                                 .exe

The following statements are final:

C++23  = implementation language of the Paracine compiler
C++20  = portable textual output
PCIR   = semantic optimization representation
PMIR   = machine representation
COFF   = canonical native object format
PE32+  = canonical native executable format

C++20 is never a mandatory intermediate representation for native compilation.

The direct-native compiler does not depend upon:

MSVC
Clang
GCC
LLVM
an external assembler
an external linker

for Tier-1 ".obj" or ".exe" production.

---

2. Final Resolution Ledger

Every former design question is resolved as follows.

ID| Final resolution
R1| Indentation is exactly four spaces. Tabs remain forbidden for structural indentation.
R2| Layout tokenization is purely lexical and no longer depends on parser knowledge.
R3| "text" is an immutable borrowed UTF-8 slice. "string" is its owning counterpart.
R4| Paracine 1.0 does support zero-allocation capturing closures, but only as statically typed, monomorphized closure values. There is no managed closure object or dynamic closure runtime.
R5| Ordinary routine pointers remain environment-free. A noncapturing closure may coerce to a routine pointer.
R6| There are no implicit numeric conversions. Contextual literal fitting remains allowed.
R7| Capability satisfaction remains structural by name/signature according to deterministic lookup. Associated types are not part of edition 2026.
R8| Record fields remain declaration ordered. The compiler never silently reorders them.
R9| Integer "+", "-", "*", and unary negation use defined modular two’s-complement semantics.
R10| Shift counts are reduced modulo the operand width.
R11| Signed "MIN / -1 = MIN"; "MIN % -1 = 0".
R12| Default routine parameters are shared borrows; "var" means exclusive borrow; "own" transfers ownership.
R13| Explicit lifetime parameters do not exist in edition 2026. Borrow regions are inferred and nonlexical.
R14| "@from(...)" remains the explicit returned-borrow disambiguator.
R15| Traps terminate the current process. They do not perform language cleanup and are not catchable exceptions.
R16| Thread traps obey the same rule. The former recoverable "thread_panic" model is removed.
R17| "give" automatically wraps values in "ok" or "some" when required by the declared result type.
R18| "fail" automatically produces "err".
R19| Floating contraction and reassociation remain disabled except in "@fastmath".
R20| Default hash-map construction is now hardened against HashDoS with a process-randomized key. Stable deterministic hashing remains separately available.
R21| Allocating growth operations are fallible. Convenience constructors may trap on OOM and retain "try_" alternatives.
R22| The import graph remains acyclic.
R23| Visibility remains private / public / export.
R24| "vec<T,N>" remains a compiler-defined operator-capable built-in exception to the no-user-operator rule.
R25| One source file defines one module and its path must match the module path.
R26| General textual/preprocessor macros do not exist in edition 2026.
R27| User procedural macros do not exist in edition 2026. Built-in "@derive" remains the only declaration generator.
R28| Async/await is not a language construct in edition 2026. Structured native threads, channels and synchronization are the concurrency model.
R29| Windows x86-64 is the only mandatory direct-native 1.0 target. Other platforms are reached through C++20 unless and until a native Paracine target profile is implemented.
R30| Index disjointness remains conservative unless proved by static range analysis or established by a library primitive such as "split_at".
R31| Capability associated types are deliberately excluded rather than left undecided. Multi-parameter capabilities are canonical.
R32| Trap cleanup is deliberately absent rather than unspecified. Recoverable failures use "result<T,E>".
R33| Build-time metaprogramming uses ordinary const evaluation and explicit build-tool steps, not compiler-executed arbitrary scripts.
R34| Nullable "raw<T>" is not niche-optimized as "option<raw<T>>".
R35| Safe "view", "span", "text", and empty "buffer" representations use a canonical non-null sentinel, permitting their guaranteed null niche for "option".
R36| "raw" as a parameter mode is legal only where raw authority actually exists in the declared parameter type.
R37| Native Tier-1 unwind metadata uses the classic Windows x64 unwind representation supported by the Paracine frame generator.
R38| PDB emission is a canonical toolchain feature but not part of language semantic equivalence.
R39| Default executable hardening includes ASLR-compatible relocations, high-entropy VA support and NX compatibility.
R40| Control Flow Guard is emitted when enabled by the selected production profile and only when complete guard metadata is generated.
R41| Reproducible builds contain no wall-clock timestamps unless the user explicitly requests stamped output.
R42| Native executable linking is complete inside Paracine; no unresolved linker stage remains.

These resolutions are frozen for the 2026 edition.

---

3. Corrected Lexical and Layout Definition

3.1 Source normalization

Before lexical analysis:

1. input is decoded as UTF-8;
2. a leading UTF-8 BOM is discarded;
3. CRLF and CR become LF;
4. Unicode is normalized to NFC outside raw byte-oriented compiler internals;
5. a missing final LF is conceptually appended;
6. trailing horizontal whitespace outside literals is ignored.

Tabs remain legal inside:

text literals
raw text
character literals where escaped
comments

Tabs are illegal in indentation.

---

4. Final Layout Algorithm

The old requirement that the lexer know whether the previous line was a grammatical “block opener” is removed.

That requirement improperly mixed lexical and syntactic phases.

The final algorithm is purely lexical.

Let the current indentation stack initially contain:

[0]

For each significant physical line not enclosed by "()", "[]", or "{}":

indent = number of leading ASCII space characters

The indent count must be divisible by four.

Otherwise:

E0100 invalid indentation width

Let:

current = top(indent_stack)

Then:

Equal indentation

If:

indent == current

emit:

NEWLINE

before the next line's first token.

One-level or multi-level increase

If:

indent > current

then the increase must be exactly four spaces.

A source line may not jump by more than one indentation level.

Therefore:

indent == current + 4

is required.

Emit:

NEWLINE
INDENT

and push "indent".

Anything larger is:

E0101 indentation skips a structural level

Whether the parser legally expected a block at that point is decided during parsing.

An unexpected "INDENT" becomes:

E0201 unexpected block

Decrease

If:

indent < current

the value must already occur in the indentation stack.

Emit:

NEWLINE
DEDENT...

until it becomes the top value.

If no stack entry equals it:

E0102 inconsistent dedent

Continuations

Inside:

(...)
[...]
{...}

physical newlines are whitespace.

A physical line beginning with "->" after whitespace continues the preceding pipeline expression and does not start a new statement.

No other implicit backslash continuation exists.

At EOF, emit one final "NEWLINE" when required, then enough "DEDENT" tokens to return to zero.

---

5. Final Reserved Vocabulary

The complete keyword set for edition 2026 is:

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

"return_" and "yield_" remain reserved for future editions.

They have no edition-2026 semantics.

The following are predeclared identifiers rather than keywords:

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

The old "import std.prelude as nothing" mechanism is removed.

Prelude suppression is controlled by:

no_prelude = true

in the package manifest or by "no_std = true".

---

6. Completed Punctuation

Edition 2026 recognizes:

+ - * / % & | ^ ~ << >>
== != < <= > >=
= += -= *= /= %= &= |= ^= <<= >>=
-> => ... . .. ..=
, : :: ? @
( ) [ ] { }

"..." is legal only:

1. in a "foreign" variadic declaration;
2. in future-edition-reserved parser positions.

Using it elsewhere is E0208.

---

7. Corrected Statement Grammar

The former statement grammar mixed newline-consuming simple statements with block-bearing compound statements.

The final distinction is:

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

A labeled jump therefore uses:

break @outer
continue @scan

rather than an unadorned identifier.

---

8. Completed Top-Level Grammar

The final top-level item set is:

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

A "thread var" value:

1. has one instance per native thread;
2. must have a const-evaluable initializer;
3. must be trivially disposable in edition 2026;
4. may be read and written in safe code because it cannot be concurrently accessed by a different thread through the same instance;
5. may not be borrowed across a thread boundary;
6. may not appear in a DLL ABI-visible layout.

Nontrivial thread-local destructors are deliberately excluded from edition 2026.

This keeps native TLS initialization deterministic and prevents hidden destruction order.

---

9. Final Closure Model

The former “no closures” rule is replaced.

Paracine supports zero-runtime, statically typed closures.

There is still:

no GC closure
no boxed closure by default
no dynamic dispatch
no hidden heap allocation

9.1 Syntax

A closure expression is:

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

Examples:

let twice = routine(x: int) => x * 2

let threshold = 10
let over = routine(x: int) => x > threshold

Parameter types may be inferred only when the expected callable type supplies them.

Otherwise they must be written.

9.2 Capture semantics

Captures are inferred.

If a closure reads an outer place:

shared capture

If it mutates an outer "var" place:

exclusive capture

If it consumes a move-only outer value:

own capture

Explicit capture forcing is available:

move routine(x: int) => x + base

which captures every used move-capable outer value by ownership where possible.

9.3 Representation

A capturing closure becomes an anonymous compiler-generated record:

closure$42 {
    captured fields...
}

plus a statically known call body.

A closure value is therefore an ordinary value.

It can:

live on the stack
be moved
be stored in another monomorphic aggregate
be specialized into a generic routine
be passed to another thread when its captures satisfy sendable

It never needs a managed environment.

9.4 Callable capability

The compiler defines:

capability callable<F, A, R>

conceptually meaning:

F can be invoked with arguments A and produces R

This capability is intrinsic because tuple-shaped argument packs are compiler-level.

User declarations cannot forge "callable".

9.5 Routine-pointer conversion

A noncapturing closure may implicitly convert to the matching:

routine(...) gives ...

pointer type.

A capturing closure cannot.

9.6 Escaping

Capturing closures may escape their defining lexical block only if every capture is owned by value.

A closure containing a borrow may not outlive that borrow.

The normal region checker enforces this.

No special closure lifetime system is added.

9.7 Public ABI

Anonymous closure types may not occur directly in:

public signatures
export signatures
foreign declarations

Public generic routines may accept a type parameter constrained by "callable".

This preserves a stable external ABI.

---

10. Standard Iterators Updated for Closures

The standard adapters are generalized.

Conceptually:

routine map<I, T, U, F>(
    own it: I,
    own f: F
) gives map_iter<I, T, U, F>
    where I is iterator<T>,
          F is callable<(T), U>

Similarly:

filter
fold
any
all
find
sort_by_key
parallel_for

accept generic callable values.

Routine pointers remain accepted because they satisfy "callable".

This resolves the original ergonomics problem without introducing managed closures.

Pipeline fusion remains mandatory for the same built-in cases.

After inlining and specialization, closure environments are scalar-replaced where possible.

---

11. Associated Types Resolution

Associated types do not exist in edition 2026.

This is final.

The iterator model remains:

iterator<I,T>

rather than:

I::Item

Reasons:

simpler name resolution
simpler interface serialization
simpler specialization
no projection-normalization system
fewer cyclic type constraints
straightforward PCIR monomorphization

A future edition may add associated types, but no 1.0 program depends upon them.

---

12. Type-System Representation Corrections

12.1 "never"

"never" is a predeclared uninhabited type.

It has no runtime representation.

These expressions have type "never":

trap(...)
unreachable()
process.exit(...)
a routine declared gives never

"never" coerces to any expected type.

12.2 "ref<T>" and "mut<T>"

They are aliases:

ref<T> = view<T> constrained to count == 1
mut<T> = span<T> constrained to count == 1

They do not create a distinct ownership category.

12.3 Pointer niches

"raw<T>" is nullable.

Therefore:

option<raw<T>>

is not guaranteed to use a null-pointer niche.

It uses an explicit tag unless some different layout niche is proven and recorded by the compiler.

The source semantics never expose which choice was made.

12.4 Canonical safe empty pointer

Safe nonowning slices use a non-null canonical aligned dangling sentinel when count is zero.

Therefore:

view<T>
span<T>
text

always have a non-null pointer in safe well-formed values.

An empty buffer likewise stores:

ptr = canonical aligned sentinel
count = 0
capacity = 0

rather than "null".

This permits the following guaranteed niche representations:

option<view<T>>
option<span<T>>
option<text>
option<buffer<T>>

where null in the representation denotes "none".

Raw construction may violate the sentinel convention only inside unsafe code and must normalize before creating a safe value.

12.5 "option<bool>"

The stored byte forms are:

0 = false
1 = true
other compiler-selected invalid byte = none

The compiler fixes the actual none value in the ABI profile.

For edition-2026 Tier-1 it is:

2

12.6 "option<char>"

Any invalid Unicode scalar representation may form the niche.

Tier-1 uses:

0x00110000

for "none".

---

13. "raw" Parameter Mode Resolution

The syntax:

raw x: T

means the callee receives raw authority associated with "T".

It is valid only if "T" is one of:

raw<U>
a record explicitly marked @raw_authority
a tuple/aggregate structurally containing raw authority and marked @raw_authority

Thus:

raw p: raw<int>

is valid.

This is not:

raw x: int

because there is no raw authority to grant.

Violation:

E0408 meaningless raw parameter mode

The separate "raw<T>" type remains the machine pointer type.

---

14. Final Effect System

Effects are now fully defined.

The effect set is:

alloc
io
thread
trap
unsafe

"pure" denotes the empty set.

It is not an additional effect.

A declaration may write:

with io, alloc

or:

with pure

but "pure" may not be combined with another effect.

14.1 Inference

Every routine body has its minimum effect set inferred.

Operations contribute:

allocation                    alloc
file/console/network/process  io
thread creation/sync blocking thread
possible defined trap         trap
unsafe operations             unsafe

Calls union the callee effects.

14.2 Explicit annotation

An explicit "with" clause is an allowed upper bound.

The inferred effect set must be a subset.

Otherwise:

E0360 undeclared effect

No "with" clause means:

infer and record the exact effect set

for private routines.

For public and export routines, the compiler writes the inferred set into the interface.

A user may explicitly pin it to make API evolution intentional.

14.3 "unsafe"

A routine containing unsafe operations may encapsulate them.

If it has a safe public contract, its implementation may contain an "unsafe" block without exposing "with unsafe".

However, a routine that accepts raw authority or requires the caller to uphold an unsafe precondition must declare:

with unsafe

The compiler cannot prove semantic encapsulation, so this distinction is part of API authorship.

14.4 Trap effect

Ordinary bounds checking, division by zero checking and explicit assertions contribute "trap".

An optimizer proving the trapping condition impossible may eliminate the operation and its local trap effect internally.

The public effect of a routine is determined before optimization.

---

15. Final Failure Model

Paracine has exactly two failure classes.

Recoverable failure

Represented by:

result<T,E>
option<T>

and manipulated with:

try
fail
choose

Cleanup executes normally.

Terminal trap

Represented by:

trap
bounds violation
division by zero
assertion failure
explicit unreachable
stack exhaustion
configured OOM trap

A trap:

does not unwind Paracine scopes
does not execute defer
does not execute dispose
does not become result
does not become thread_panic
does not resume

The trap handler is:

routine(trap_code, text) gives never

and cannot return.

This rule is final.

---

16. Thread Trap Resolution

The former recoverable thread-trap concept is deleted.

There is no:

std.thread.on_trap(recover)

and no trap-derived "thread_panic".

If any thread traps under the normal runtime:

the process terminates

just as if the main thread trapped.

"join" therefore becomes:

routine join<R>(own h: join_handle<R>)
    gives result<R, thread_error>

"thread_error" represents operating-system/runtime failures such as an invalid native wait or thread start failure.

It never represents a language trap.

---

17. Structured Thread Scope Resolution

The old capture-free scope callback could not access parent data.

The canonical API is now generic over a closure:

routine scope<F>(own f: F)
    where F is callable<(var thread_scope), void>

Because closures may carry inferred borrows, this directly supports:

var values = ...
scope(routine(var s)
    spawn_scoped(var s, routine()
        use(values)
    )
)

The compiler knows every borrow captured by "f".

"scope" does not return until all spawned scoped threads finish.

Therefore every captured borrow dies before the enclosing call returns.

"spawn_scoped" accepts a statically typed callable and applies the normal "sendable" rules to moved captures and the specialized scoped rules to borrowed captures.

This resolves the concurrency/closure mismatch.

---

18. Interior Mutability Resolution

Safe interior mutability exists only in compiler-known or standard-library types whose implementation internally uses unsafe authority.

Edition 2026 recognizes these safe interior-mutability families:

atomic<T>
mutex<T>
rwlock<T>
once<T>
channel<T> internal state
thread-safe runtime handles explicitly marked by std

Ordinary user records cannot mutate through a shared borrow.

A future user-defined interior-mutability capability is not part of 1.0.

This preserves the simple alias rule.

---

19. Index Disjointness Resolution

The borrow checker treats:

a[i]
a[j]

as potentially overlapping unless it proves disjointness.

The checker may prove:

i != j

using exact constants, normalized ranges, induction-variable facts and dominating comparisons.

This analysis is optional for acceptance only when the proof succeeds.

Programs requiring guaranteed independent mutable regions must use:

split_at
chunks_mut
split_first_mut

or another standard disjointness primitive.

Failure to prove arbitrary dynamic indices disjoint is not a compiler deficiency.

It is the specified conservative model.

---

20. Macro and Compile-Time Generation Resolution

Paracine edition 2026 has:

no textual macros
no token macros
no hygienic syntax macros
no procedural compiler plugins
no source-generating compiler callbacks

It does have:

const evaluation
const generics
generic specialization
@derive
conditional @feature items
explicit build-tool pipeline steps

"@derive" is limited to compiler-defined capabilities.

Edition 2026 built-ins are:

equatable
ordered
hashable
printable
defaultable

The absence of a macro language is deliberate.

---

21. Async Resolution

Edition 2026 has no:

async
await
coroutine syntax
hidden scheduler
mandatory event loop

Asynchronous architectures are built with:

threads
nonblocking OS APIs
channels
callbacks
state machines
user libraries

A later edition may define coroutine lowering.

No 1.0 ABI reserves coroutine frame machinery.

---

22. Other Native Targets Resolution

Only this direct-native profile is mandatory:

windows-x86_64-msx64

The C++20 backend is the official portability backend.

A future direct target must provide a complete target profile containing:

primitive widths
endianness
pointer width
ABI
object format
executable format
relocation model
unwind model
thread/TLS model
atomics lowering
OS runtime bindings
target-specific std realization

No native target is considered supported merely because PMIR can express it.

Support requires the entire profile.

---

23. Hardened Hashing Resolution

The old fixed-key default map has been replaced.

General map/set

The default:

map<K,V>
set<T>

uses a process-randomized SipHash-family key obtained from "os_rng".

This protects untrusted hash tables against adversarial collision flooding.

Iteration order is therefore unspecified and may differ between runs.

Programs must never rely upon it.

Stable hashing

The following remain deterministic:

hash_of<T>
fnv1a
crc32
xxh3_64
stable_hasher

"stable_hasher" has a specified fixed initialization and is intended for:

compiler caches
serialization fingerprints
reproducible tooling
tests

A deterministic table may be constructed explicitly:

map_with_hasher(stable_hasher())

This resolves security without removing deterministic tooling.

---

24. Standard-Library Network Completion

The "std" tier now includes:

std.net

Its Tier-1 implementation uses Winsock through "std.win.ws2_32".

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

routine send_to(var socket: udp_socket, data: view<ubyte>, addr: socket_addr)
    gives result<size, socket_error>

routine recv_from(var socket: udp_socket, dst: span<ubyte>)
    gives result<(size, socket_addr), socket_error>

Socket objects are disposable.

Native initialization of Winsock is demand-linked and performed exactly once by the runtime support object.

No network runtime exists in "no_std".

---

25. Dynamic-Library Completion

"std.dynlib" is added to the "std" tier.

Tier-1 API:

routine open(path: text)
    gives result<library, io_error>

routine symbol<T>(self: library, name: text)
    gives result<raw<T>, io_error>
    with unsafe

routine close(own lib: library)

The Windows realization uses the platform loader.

Typed function-pointer conversion from a raw symbol requires "unsafe".

This does not alter static linking.

---

26. Global Allocator Completion

The global allocator is initialized before "main".

Replacing it is permitted only through a startup hook:

@startup
routine configure_allocator()

that executes before any user allocation and before user thread creation.

Once the first allocation occurs, global allocator replacement is permanently locked.

Attempting replacement afterward traps "Assertion".

Dependencies cannot silently replace the global allocator.

Exactly one startup allocator provider may be selected by the root package.

---

27. PCIR Completion

The following rules close remaining semantic gaps in PCIR.

27.1 Poison and undef

PCIR contains neither:

undef
poison
freeze

Every value is defined.

Unsafe invalid execution is modeled by loss of language guarantees, not by propagating an IR poison value.

This prevents optimizer behavior from silently exceeding Paracine semantics.

27.2 Provenance

Every pointer-producing operation carries a provenance identity where known:

alloca
global
heap allocation
foreign/raw unknown

"gep" preserves provenance.

Converting a pointer to an integer loses usable provenance for safe reconstruction.

"inttoptr" is unsafe and produces raw/unknown provenance.

The optimizer cannot infer nonaliasing merely because integer pointer values differ.

27.3 Calls and memory effects

Every call carries an effect summary:

readnone
readonly(classes)
write(classes)
alloc
free
sync
io
trap
unknown

A foreign call defaults to:

unknown + io + trap

unless its declaration supplies a trusted standard-library intrinsic summary.

User attributes cannot lie about a foreign routine's memory effects in safe code.

27.4 Debug locations

Every PCIR instruction originating from source carries a source span.

Synthetic optimizer instructions inherit the narrowest dominating origin that accurately describes them.

Trap sites always retain the original source operation location.

27.5 ABI independence

PCIR never contains:

RCX
RDX
RSP
XMM0
COFF relocation types
PE RVAs

Those are strictly backend concepts.

27.6 Serialization identity

A serialized PCIR module contains:

magic
major version
minor version
target-independent type table
layout table
symbol table
functions
metadata
content hash

The content hash excludes debug-path normalization differences unless debug identity is requested.

---

28. Optimizer Completion

28.1 Pass determinism

Every pass is deterministic for identical:

PCIR input
compiler version
target profile
optimization profile

No pass uses unordered container iteration to make transformation choices.

28.2 Code-size budgets

Inlining, specialization, path compression and unrolling each have separate budgets.

They may not recursively amplify one another without a global growth limit.

Default "opt=3" global post-inlining growth limit:

300% of pre-inline PCIR instruction count per module

unless profile-guided optimization overrides it.

This is a compile-time/resource guard, not a semantic rule.

28.3 Recursion

Direct and mutual recursion are permitted.

The inliner never recursively expands a cyclic call edge unless finite specialization proves a terminating depth and the resulting expansion fits its budget.

28.4 Floating semantics

Outside "@fastmath":

no reassociation
no reciprocal approximation
no implicit FMA contraction
no signed-zero elimination
no NaN elimination

Vectorization must preserve these rules.

28.5 Trap ordering

A pass may not transform:

effect A
possible trap
effect B

into any arrangement where B becomes externally observable before that trap.

This is checked by the effect scheduler verifier.

---

29. PMIR Completion and Corrections

29.1 Canonical Windows unwind version

Tier-1 emits the classic Windows x64 unwind representation used by Paracine's frame generator.

Paracine 1.0 does not emit newer optional unwind encodings merely because the host operating system may recognize them.

A later backend revision may add them behind a target-feature flag without changing language semantics.

29.2 Leaf functions

A function that:

does not modify nonvolatile registers
does not allocate a stack frame
does not make calls

may omit ".pdata/.xdata".

Every nonleaf native function receives unwind metadata.

29.3 Stack probes

When fixed stack allocation crosses the target profile's guard-page probe threshold, the backend emits a probing sequence compatible with Windows stack growth.

The Tier-1 profile uses the standard Windows-compatible "__chkstk" runtime helper for large fixed frames.

Dynamic stack allocation is not available in ordinary safe Paracine source.

Only compiler/runtime intrinsics may request it in edition 2026.

29.4 "INC"/"DEC"

Machine selection may use "INC" or "DEC" when flag semantics and scheduling costs permit.

They are not semantically forbidden.

The encoder's cost model chooses between them and "ADD/SUB 1".

29.5 Red zone

There is no Windows x64 red zone.

Generated Tier-1 code never stores below current "RSP".

29.6 Internal ABI

The private "pcn" convention remains legal only for routines whose ABI is entirely compiler-controlled.

An address-taken routine, exported routine, foreign-callable routine or routine stored in a public routine-pointer value uses the declared external convention.

Cross-convention thunks are emitted automatically.

---

30. Native Linker — Complete Specification

The native linker is now fully defined.

The link pipeline is:

object discovery
↓
archive discovery
↓
directive processing
↓
symbol-table construction
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
identical-code folding where enabled
↓
section contribution merging
↓
import synthesis
↓
export synthesis
↓
TLS synthesis
↓
unwind table synthesis
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
headers
↓
debug directory
↓
checksum/signature reservation
↓
image verification
↓
PE32+

---

31. Link Input Discovery

Inputs are processed in deterministic command-line order.

Inputs may be:

Paracine in-memory object models
COFF .obj
static .lib
import .lib
explicit DLL import declarations
resource .res files
Paracine runtime archives

The linker never searches the current directory implicitly for libraries merely because a symbol is unresolved.

Search directories come from:

package manifest
target profile
explicit command line
configured SDK location

in that precedence.

---

32. Symbol Model

Every link symbol is one of:

defined strong
defined weak
COMDAT
undefined
absolute
import
common-compatible foreign

Paracine source does not generate C-style tentative common definitions.

The linker can consume foreign objects that do.

A strong duplicate definition is:

L1001 duplicate strong symbol

unless one definition was eliminated by valid COMDAT selection.

Undefined final symbols are:

L1002 unresolved external symbol

with all referencing objects listed.

---

33. Archive Extraction

Static libraries use demand extraction.

Algorithm:

build unresolved-symbol set
scan archive index
extract members satisfying unresolved strong references
add their undefined references
repeat until fixed point

An archive member is loaded at most once.

"/WHOLEARCHIVE" equivalent behavior is available as:

pcn link --whole-archive library.lib

and in the manifest.

Archive processing is deterministic.

Cyclic static-library dependency does not depend on command ordering because Paracine repeats extraction across the complete selected archive set until no new member is loaded.

---

34. COMDAT Resolution

Selections are processed as follows:

NODUPLICATES

More than one live definition is an error.

ANY

Choose the deterministic canonical instance.

Preference order:

root package
earlier dependency order
lexicographically smaller object identity

SAME_SIZE

All candidates must have identical byte size.

Otherwise link error.

Choose canonically as above.

EXACT_MATCH

All candidate bytes and relocation graphs must match after symbol normalization.

Otherwise link error.

ASSOCIATIVE

The contribution lives exactly when its leader lives.

Used for:

pdata
xdata
debug fragments
auxiliary constant data

LARGEST

Choose the largest contribution.

Ties use deterministic canonical ordering.

---

35. Section Garbage Collection

Release profiles enable section GC by default.

Roots include:

entry point
exports
forced-include symbols
TLS structures
TLS callbacks
load-config referenced tables
resource roots
debug-required anchors when debugging is enabled
symbols marked @used
startup hooks
shutdown hooks

Edges include:

relocations
associative COMDAT links
unwind references
import thunks
export forwarders
startup tables

Every unreachable removable contribution is discarded.

Debug information for discarded code is discarded with it.

---

36. Identical Code Folding

ICF is optional and enabled at:

opt=3 release

for nonexported functions unless disabled.

Two functions may fold only if:

machine bytes match after relocation normalization
relocation topology matches
unwind behavior matches
address identity is not semantically observable
neither function carries @unique_address
neither is an export with distinct required address

Routine-pointer equality can observe addresses.

Therefore functions whose addresses escape into program-visible values are not folded unless the language program cannot distinguish them through a guaranteed semantic comparison.

Default policy:

address-taken function → no ICF

This favors semantic simplicity.

---

37. Final Native Image Sections

The canonical Tier-1 image may contain:

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

The linker may merge compatible object contributions into those final image sections.

".bss" occupies virtual space in ".data"-class writable memory but does not require raw file bytes for the zero-filled portion.

Section names are organizational.

Loader semantics are defined by headers and data directories, not by assuming a table must reside in a same-named section.

---

38. Section Permissions

The linker enforces W^X by default.

Canonical final permissions:

.text   execute + read
.rdata  read
.pdata  read
.xdata  read
.idata  read/write only where loader IAT mutation requires it
.edata  read
.data   read + write
.tls    read + write template semantics
.rsrc   read
.reloc  read + discardable

No section is both writable and executable in a standard production image.

A package requesting writable executable memory requires an explicit unsafe platform feature and does not change static section permissions automatically.

---

39. Final Section Layout

Defaults:

FileAlignment    = 512 bytes
SectionAlignment = 4096 bytes

unless a target profile requires otherwise.

The linker computes:

VirtualAddress(section[i])
    = align_up(previous.VirtualAddress + previous.VirtualSize,
               SectionAlignment)

PointerToRawData(section[i])
    = align_up(previous file end,
               FileAlignment)

"SizeOfRawData" is file-aligned.

"VirtualSize" is the actual in-memory contribution size before section alignment.

"SizeOfImage" is the end of the final virtual section rounded to "SectionAlignment".

"SizeOfHeaders" is the complete headers region rounded to "FileAlignment".

---

40. Import Construction

Imports are grouped by DLL name.

DLL grouping is case-insensitive for canonicalization but preserves the first explicitly supplied spelling for diagnostics.

Within each DLL, imports are sorted deterministically by:

name imports lexicographically
then ordinal imports numerically

unless preserving foreign object order is required for compatibility.

For PE32+ each DLL receives:

Import Directory entry
Import Lookup Table
Import Address Table
DLL name string
Hint/Name records

The Import Directory ends with one zeroed entry.

The lookup table and IAT each end with a zero entry.

Name imports use a 16-bit hint followed by a NUL-terminated ASCII export name and alignment padding.

Ordinal imports set the high PE32+ ordinal flag and contain the 16-bit ordinal.

Before loading, ILT and IAT contain equivalent thunk values.

The loader rewrites the IAT with resolved addresses.

References to:

__imp_symbol

resolve directly to the IAT slot.

A direct imported call normally becomes:

call qword ptr [rip + IAT-slot]

unless a foreign object explicitly references an import thunk symbol.

---

41. Delay Imports

Delay loading is supported as an explicit link feature:

@delay_import

or manifest configuration.

It is never inserted automatically.

Delay-import construction generates:

delay import descriptors
delay IAT
delay name table
module-handle storage
resolver thunks

using Paracine runtime support.

If no delay imports exist, the Delay Import data directory is zero.

---

42. Export Construction

An export record contains:

public export name
target symbol
optional ordinal
optional NONAME
optional forwarder

Explicit ordinals must be unique.

Unassigned ordinals are allocated deterministically from the lowest available ordinal beginning at 1.

The linker emits:

Export Directory
Export Address Table
Name Pointer Table
Ordinal Table
export strings
DLL name

Name pointers are sorted lexicographically as required for efficient loader lookup.

A forwarder is represented by an RVA pointing to a string inside the export directory range, such as:

KERNEL32.Sleep
OTHERDLL.#27

An EXE ordinarily has no exports unless requested.

A DLL may export:

functions
data
foreign-compatible ABI thunks

Anonymous closure types and private internal-ABI routines cannot be exported.

---

43. Resource Construction

Paracine accepts:

.res

resource input and may also construct resources from manifest declarations.

Resource trees are normalized by:

type
name/id
language

Name entries precede numeric-ID entries at each level and are sorted in canonical order.

The linker emits:

resource directory tables
directory entries
UTF-16 resource names
resource data entries
raw resource blobs

into ".rsrc".

If no resources exist, the Resource data directory is zero.

---

44. TLS Construction

"thread var" items are collected into a native TLS template.

The linker emits:

initialized TLS bytes
zero-fill size
TLS index storage
optional callback array
IMAGE_TLS_DIRECTORY64

The directory fields containing addresses use image VAs where the PE format requires VAs and therefore receive base relocations.

The callback array is NUL terminated.

Edition 2026 source-level "thread var" does not require callbacks because initialization is const-evaluable and values are trivially disposable.

Runtime/internal TLS callbacks may still be emitted when the runtime needs them.

Generated application code does not hard-code undocumented TEB offsets.

TLS addressing is implemented by the Tier-1 lowering/runtime contract.

---

45. Exception/Unwind Table Construction

All surviving nonleaf functions contribute final:

RUNTIME_FUNCTION
UNWIND_INFO

records.

The linker:

1. applies final function RVAs;
2. resolves ".xdata" RVAs;
3. sorts final ".pdata" records by "BeginAddress";
4. verifies nonoverlapping function ranges;
5. writes the Exception data-directory RVA and size.

Associative unwind data disappears when its function is garbage-collected.

A final image with malformed or unsorted exception entries is rejected by the image verifier.

---

46. Base Relocation Construction

Paracine emits position-independent RIP-relative code whenever practical.

Any remaining absolute image address requiring rebasing is recorded.

For Tier-1 PE32+ the normal base-relocation type is:

DIR64

Relocations are grouped by 4 KiB page.

Each block contains:

PageRVA
BlockSize
TypeOffset entries

Entries are sorted by offset.

An "ABSOLUTE" relocation entry may be inserted only as padding to preserve required alignment.

If the image contains no absolute relocatable addresses, the relocation directory may be empty.

Production dynamic-base profiles ordinarily retain a valid relocation table whenever required by the image.

---

47. Relocation Application

For every COFF relocation:

S = final symbol value
A = encoded addend
P = final fixup location

The linker computes the architecture-defined result.

For Tier-1:

ADDR64   → S + A
ADDR32NB → RVA(S) + A
REL32    → S + A - address_after_fixup
SECTION  → target section index
SECREL   → target offset within final section contribution

Overflow is always diagnosed.

There is no silent truncation.

Example:

L1204 REL32 target exceeds signed 32-bit displacement

The small image/code model means such an overflow normally indicates an unsupported oversized image or malformed foreign object.

---

48. PE32+ Header Construction

A canonical executable contains:

DOS header
minimal DOS stub
PE signature
COFF image header
PE32+ optional header
data directories
section table
section data
optional certificate data

The Tier-1 machine field is:

AMD64

The PE optional-header magic is:

PE32+

The linker fills:

AddressOfEntryPoint
BaseOfCode
ImageBase
SectionAlignment
FileAlignment
OS version policy
Image version
Subsystem version
SizeOfImage
SizeOfHeaders
Checksum
Subsystem
DllCharacteristics
StackReserve
StackCommit
HeapReserve
HeapCommit
LoaderFlags = 0
NumberOfRvaAndSizes

Reserved fields are zero.

---

49. Image Base

Default EXE preferred image base:

0x0000000140000000

Default DLL preferred image base is linker-selected and alignment-correct.

The user may override the preferred base.

Because ASLR is enabled, code must not depend upon actually loading at the preferred base.

The image base is 64 KiB aligned.

---

50. Subsystem

Application packages choose:

subsystem = "console"

or:

subsystem = "windows"

Default:

console

unless the root package declares GUI application intent.

The native subsystem field is set accordingly.

A GUI package may define the normal Paracine "main"; "pcn_start" adapts it to the Windows GUI process startup environment.

---

51. Default PE Hardening

Production Tier-1 images enable:

HIGH_ENTROPY_VA
DYNAMIC_BASE
NX_COMPAT
TERMINAL_SERVER_AWARE

unless the selected image category makes a flag inappropriate.

Paracine does not set:

NO_SEH

by default because the runtime and operating system must remain able to participate in Windows exception/unwind machinery.

"FORCE_INTEGRITY" and "APPCONTAINER" are opt-in deployment properties.

"WDM_DRIVER" is never set for ordinary application images.

---

52. Control Flow Guard

CFG is a production hardening option.

When enabled, the compiler/linker must generate the complete load-configuration and guard target metadata required by the selected Windows target profile.

Only then is:

GUARD_CF

set.

Indirect-call targets include:

address-taken routines
external ABI thunks
exports that may be called indirectly
required runtime callbacks

Internal functions proven never to be indirect targets need not appear.

A partially generated CFG table is forbidden.

If CFG metadata generation fails, linking fails rather than silently setting an invalid hardening flag.

---

53. Load Configuration

The linker synthesizes a load-configuration directory when required by enabled hardening features.

Fields not used by the selected target profile are zero.

Potential consumers include:

Control Flow Guard
security cookie integration where enabled
guard tables
future target-profile hardening

The emitted structure size matches the exact layout version selected by the target profile.

Paracine never emits a larger structure while leaving fields semantically uninitialized.

---

54. Stack and Heap Header Defaults

Canonical defaults:

stack reserve = 8 MiB
stack commit  = 64 KiB

heap reserve  = 1 MiB
heap commit   = 64 KiB

These are linker defaults, not language guarantees.

They may be overridden per package.

The runtime allocator is not restricted to the process default heap merely because these PE heap fields exist.

---

55. Entry Point

EXE entry:

pcn_start

DLL entry when runtime initialization is needed:

pcn_dll_start

"pcn_start" performs:

runtime state initialization
OS argument acquisition
UTF-8 argument construction
standard stream initialization
trap-handler installation
random hash seed initialization
runtime one-time subsystem setup
startup hooks
user main
standard stream flush
process exit

There is no C runtime dependency.

User "main" is never the literal PE entry point.

---

56. DLL Startup

A Paracine DLL avoids complex work under loader lock.

The DLL entry point performs only operations safe for loader initialization.

General allocation, thread creation and arbitrary user callbacks are not performed from process attach.

Library-level initialization requiring richer work uses explicit:

initialize()

APIs or safe lazy-once initialization after load.

This avoids hidden loader-lock hazards.

---

57. Debug Directory

When native debug information is enabled, the final image may contain a PE debug-directory entry pointing to CodeView information.

For PDB mode, the CodeView record uses an RSDS-style identity containing:

PDB identity
age
path or normalized file name

Reproducible mode does not embed an uncontrolled absolute build path.

---

58. PDB Completion

Paracine's canonical native PDB writer produces a modern MSF-based PDB sufficient for:

line mapping
public symbols
private symbols at -g2
type information
module information
source-file mapping
register/stack variable location ranges

The writer maintains:

PDB information stream
TPI
IPI
DBI
module streams
global/public symbol indexes
section contribution information
line tables
string table

The exact binary container implementation is a toolchain-format concern, not part of Paracine language semantics.

Conformance is judged by:

successful DIA/WinDbg consumption
correct source-line resolution
correct public-symbol resolution
correct type identity
correct variable-location ranges where emitted

In reproducible mode, PDB identity is derived deterministically from the debug content hash rather than wall-clock randomness.

PDB failure does not alter generated executable semantics, but a requested "-g2" build fails if its PDB cannot be produced.

---

59. Native Debug Levels

-g0  no debug information

-g1  source files + line mappings + public function names

-g2  full CodeView/PDB type, local variable, lexical scope and location-range data

Optimized builds may report a variable as:

optimized away
constant
split across locations
unavailable for part of its source range

rather than inventing a misleading location.

---

60. PE Checksum

For ordinary EXE/DLL output the checksum field is deterministic.

Default application policy:

Checksum = 0

unless the selected image profile requires or requests a computed PE checksum.

"pcn pe --checksum" computes it after final layout and before signing.

Profiles such as kernel/driver targets, if added in a future target profile, may require a nonzero checksum.

---

61. Authenticode and Certificate Table

Code signing is not part of program semantics.

The PE writer reserves correct certificate-table behavior.

A signed image stores certificates outside the normal mapped section data and updates the Certificate data-directory entry with the required file offset and size.

The image digest excludes fields required by the PE/Authenticode hashing convention.

Paracine provides:

pcn sign
pcn verify-signature

as toolchain commands when signing support is installed.

Signing happens after deterministic image construction.

Unsigned and signed forms represent the same Paracine program.

A signing private key is never stored in a package manifest.

---

62. Reproducible Native Linking

With reproducible mode enabled, identical inputs produce byte-identical:

.obj
.lib
.exe
.dll

except intentionally external signatures.

Rules:

COFF timestamps = 0
PE COFF timestamp = 0
export timestamp = 0
load-config timestamp = 0
archive member timestamps = 0
deterministic symbol ordering
deterministic section ordering
deterministic padding
deterministic PDB identity
normalized source paths
no random image padding

Randomized runtime map hash keys are generated when the program starts and therefore do not affect binary reproducibility.

---

63. Build Identity

Every build can emit:

program.build.json

containing:

source hashes
package graph
lockfile hash
compiler version
language edition
PCIR version
PMIR version
optimization profile
target triple
CPU profile
C++ emitter version
native backend version
COFF writer version
linker version
PE writer version
standard library version
feature set
build reproducibility mode

The manifest is not embedded into the executable unless requested.

---

64. Import Library Completion

When producing a DLL, Paracine may also produce:

module.lib

containing import descriptors for every selected export.

Both:

__imp_Name
Name

forms are represented where required for conventional Windows linking.

Export ordinals and names match the DLL's final export table.

An import library generated in the same build is validated against that table before completion.

---

65. Static Library Completion

"pcn lib" produces a deterministic Windows archive.

It emits:

first linker member
second linker member
long-name table when needed
object members

Global symbol indexes refer to the correct archive member offsets after final serialization.

The writer performs a read-back validation pass in debug/CI mode.

---

66. Bigobj Resolution

The normal COFF representation is used whenever legal.

When section or symbol numbering exceeds normal COFF limits, the writer switches to the Windows bigobj representation.

Bigobj is an object-level compatibility mechanism only.

Final PE images do not contain “bigobj sections.”

Objects emitted as bigobj must be accepted by:

Paracine linker
supported contemporary Microsoft linker
supported contemporary LLVM linker

in interoperability testing.

---

67. Native Link Diagnostics

Canonical linker families:

L1000 input/object errors
L1100 symbol errors
L1200 relocation errors
L1300 COMDAT/archive errors
L1400 import/export errors
L1500 image-layout errors
L1600 unwind/TLS/load-config errors
L1700 debug/PDB errors
L1800 signing/certificate errors
L1900 internal image verification

Examples:

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

68. Final Image Verification

Before an ".exe" or ".dll" is committed to disk, the PE writer reparses the completed byte image using an independent reader.

It verifies:

DOS/PE signatures
machine type
PE32+ magic
header sizes
section-table bounds
section alignment
file alignment
nonoverlapping raw ranges
nonoverlapping virtual ranges
entry point inside executable section
data-directory bounds
import termination
IAT bounds
export-name sort order
export ordinal validity
pdata sort order
unwind references
TLS directory validity
relocation block boundaries
base-relocation target bounds
load-config bounds
resource tree bounds
debug-directory bounds
certificate-table file bounds
SizeOfImage
SizeOfHeaders
section permissions
reserved fields

Any failure is an internal compiler error and the output image is deleted.

---

69. Independent Loader Test

The conformance suite additionally launches generated executables through the actual Windows loader.

It validates:

ASLR relocation
imports
TLS
unwind
console startup
GUI startup where applicable
DLL loading
exports
thread creation
trap termination
large stack frames
PDB symbolication

A file merely passing Paracine's parser is not enough to claim PE conformance.

---

70. C++20 Backend Completion

The textual backend must represent all edition-2026 language features.

70.1 Closures

A monomorphized capturing closure becomes a generated aggregate:

struct closure_42 {
    ...
    Result operator()(...) noexcept;
};

or equivalent ordinary helper functions.

No "std::function" is used.

No heap allocation is introduced solely for closure representation.

70.2 Sequencing

Because Paracine defines left-to-right evaluation, generated C++ introduces temporaries where needed instead of relying upon a C++ expression whose operand evaluation rules could differ.

70.3 Arithmetic

Signed modular operations are expressed using unsigned-domain helpers and explicit conversion.

The backend never relies on C++ signed overflow.

70.4 Shifts

Shift counts are explicitly normalized to Paracine width semantics where C++ would otherwise have undefined behavior.

70.5 Division

Generated code explicitly handles:

divisor == 0
MIN / -1

before using host division where required.

70.6 Float-to-int

The emitted helper implements:

truncate toward zero
saturate to destination range
NaN → 0

without invoking C++ undefined behavior.

70.7 Layout

Every ABI-relevant generated struct includes static compile-time checks for:

sizeof
alignof
offsetof

against PCIR layout.

70.8 Atomics

C++ output uses standard atomic operations with explicit memory orders corresponding exactly to Paracine order values.

70.9 Thread-local storage

"thread var" lowers to suitable C++20 "thread_local" storage in portability mode, subject to the same trivial-destruction edition-2026 restriction.

---

71. Differential Backend Equivalence

For programs not intentionally dependent upon target-specific properties:

Paracine → C++20 → host compiler

and:

Paracine → PMIR → native PE

must agree on Paracine-observable behavior.

Differences caused solely by:

thread scheduling
native OS error codes
addresses
timing
supported platform facilities

are outside strict equality where documented.

The language semantics themselves do not diverge.

---

72. Module/Manifest Completion

The package manifest gains these final production fields.

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
emit = ["cpp"]

[link]
libraries = ["kernel32.lib"]
search = []
whole_archive = []

[resources]
files = []

[signing]
enabled = false

Dependency manifests cannot override root executable hardening or signing policy.

---

73. Build-Step Completion

Arbitrary compiler-time code execution remains forbidden.

Explicit build steps are permitted only as root-workspace configuration.

A dependency package cannot automatically execute a build step merely by being depended upon.

Canonical form:

[[step]]
name = "generate_protocol"
tool = "tools.protocol_generator"
inputs = ["schema/protocol.pcn-schema"]
outputs = ["generated/protocol.pcn"]

The tool must itself be a declared executable package.

The build system:

hashes inputs
hashes tool binary
runs in declared workspace sandbox
records outputs
rejects undeclared output writes in sandboxed mode

Generated source becomes an ordinary compiler input.

This gives controlled generation without a hidden build-script language.

---

74. Capability Lookup Freeze

Capability satisfaction lookup remains:

1. module defining the subject type;
2. module defining the capability;
3. current module.

All viable candidates at the same precedence level are considered together.

More than one exact candidate is:

E0340 ambiguous capability member

Imported wildcard modules do not receive special capability precedence.

Closure compiler capabilities such as "callable" are intrinsic and bypass ordinary lookup.

---

75. Generic Coherence

Two different visible definitions cannot both claim the same effective capability member signature for the same subject at equal precedence.

Cross-package ambiguity is diagnosed at the use site.

Paracine does not have orphan impl blocks, so no global coherence registry is required.

This completes the structural capability model.

---

76. Const Evaluation Completion

Const execution may allocate only compiler-temporary evaluator storage.

It cannot perform semantic runtime allocation.

Const code may use:

pure arithmetic
records
variants
arrays
bounded loops
pure routines
const generic operations
string/text literal manipulation supported by const std

It cannot perform:

I/O
threads
foreign calls
unsafe memory access
runtime random
time
environment access
dynamic library access
network access

Const results are serialized into ordinary constants.

The step limit and recursion-depth limit are deterministic diagnostics.

---

77. Static Initialization Completion

There is no user-visible dynamic global initialization.

Allowed:

const X = const_expr
var static X: T = const_expr
thread var X: T = const_expr

Initialization bytes are established by the image.

Startup hooks are explicit routines and are not equivalent to hidden constructors.

This prevents initialization-order fiascos.

---

78. Startup and Shutdown Hooks

Edition 2026 supports compiler attributes:

@startup
@shutdown

only on:

routine() gives void

with no parameters.

Startup ordering:

root package first
then dependencies in topological package order
then source-order within a module

except allocator configuration, which has its own earliest reserved phase.

Shutdown hooks run in exact reverse order only on normal process return from "main".

They do not run on:

trap
process.exit
forced OS termination

This rule mirrors "defer"'s non-trap behavior.

---

79. Main Routine Completion

The accepted root signatures are:

routine main() gives void

routine main() gives int

routine main(args: view<text>) gives int

routine main() gives result<void, E>
    where E is printable

Additionally:

routine main(args: view<text>) gives result<void, E>
    where E is printable

is now accepted.

Argument strings are valid UTF-8.

Windows UTF-16 command-line arguments are converted with replacement-free validation; malformed surrogate sequences become a startup error before user "main".

A startup conversion failure prints a runtime diagnostic and exits nonzero.

---

80. No-Std Completion

With:

no_std = true

available facilities are:

primitive types
records
variants
option/result
arrays
views/spans
raw pointers
core arithmetic
PCIR/native code generation
compiler intrinsics required for freestanding semantics

The package supplies its own entry point contract through:

@entry

when producing a freestanding image.

Ordinary Windows app startup is a "std" facility.

A "no_std" package can still produce ".obj" for kernels, firmware loaders or custom runtimes, but such environments are outside the default Windows application profile.

---

81. Unsafe Boundary Completion

Every public routine containing unsafe internals must satisfy one of two classifications.

Safe encapsulation

Signature exposes no requirement for caller-maintained raw invariants.

No "with unsafe".

Unsafe contract

Correct calling requires the caller to uphold raw/lifetime/alias/platform invariants.

Declaration includes:

with unsafe

and documentation must state the contract.

The compiler enforces call-site unsafe context for the second class.

---

82. FFI ABI Compatibility

A type is C/MS x64 FFI-compatible if it is one of:

fixed-width integer
float
double
bool with documented uint8 representation
raw<T>
routine pointer
@layout(c) record whose fields are recursively compatible

Paracine-native:

text
view
span
buffer
option
result
variant
string
closure

are not automatically C ABI types.

They require:

explicit wrapper records
or exported ABI thunks

This prevents accidental representation coupling.

---

83. Exported Paracine APIs

An ordinary "export" routine may use Paracine types when consumed by another Paracine module/package because ".pci" communicates the canonical ABI contract.

An "@export_name" routine intended for arbitrary foreign code must use FFI-compatible parameter/result types.

Violation:

E0624 foreign export exposes non-FFI type

---

84. Runtime Version Compatibility

Every Paracine native library records:

language edition
public ABI version
std ABI version
target profile

in its interface metadata.

The linker rejects incompatible Paracine-native library ABI combinations before producing an image.

Pure C ABI libraries remain governed by their own symbols/contracts.

---

85. PCIR-to-PMIR Machine Verification

After final PMIR lowering, the backend verifies:

all virtual registers resolved
all frame indexes resolved
all calls satisfy ABI placement
RSP alignment correct at every call
shadow space present for msx64 calls
nonvolatile registers restored
DF clear at external boundaries
no illegal high-byte/REX combinations
branch targets valid
all relocations representable
unwind model matches actual prologue
epilog shapes recognized by target policy

Failure is an internal compiler error.

---

86. Native Encoder Completion

Every emitted instruction is produced from a declarative encoding record.

The encoder covers:

legacy prefixes
REX
VEX
EVEX when target allows
opcode maps
ModRM
SIB
displacements
immediates
rel8
rel32
RIP-relative addressing

The separate decoder round-trips final bytes during conformance tests.

The compiler's assembly listing is generated from decoded final bytes, not merely PMIR intent.

---

87. CPU Feature Safety

The selected CPU baseline is part of the build identity.

The compiler never emit instructions unavailable in that baseline unless:

the routine is inside a valid multiversion body

with dispatch.

A v3 executable is not silently made compatible with baseline x86-64.

The loader cannot detect this on Paracine's behalf.

It is an explicit deployment contract.

---

88. Multiversion Completion

Syntax:

@multiversion("x86-64", "x86-64-v3")
routine work(...)

The compiler generates:

work$base
work$v3
work$resolver
work$slot

The resolver performs one-time CPU feature detection.

The slot is published atomically.

Subsequent calls dispatch through the resolved target.

The resolver itself uses only baseline instructions.

Exported multiversion routines expose one stable external thunk.

---

89. Runtime Independence

Tier-1 Paracine requires only Windows system facilities selected by the program.

There is no hidden requirement for:

MSVCRT
UCRT
C++ runtime
LLVM runtime
libgcc

unless the user links a foreign library that itself requires one.

The Paracine runtime is demand-linked from native object modules.

---

90. Security Baseline

Production compiler defaults include:

bounds checks in safe code
NX-compatible image
ASLR-compatible image
high-entropy VA
W^X sections
randomized default hash-table keys
stack probing
valid unwind metadata
UTF-8 validation
checked allocation size arithmetic
COFF/PE parser bounds checking
archive parser bounds checking
no automatic dependency build-script execution

Unsafe code can deliberately step outside safe-language guarantees but does not disable toolchain input validation.

---

91. Compiler Input Hardening

Object, archive, PDB, resource and package parsers treat all external binary input as untrusted.

They perform checked arithmetic for:

offset + size
count * element_size
alignment
section spans
string table spans
relocation indexes
symbol indexes
archive member offsets
resource recursion

Malformed binary input produces a diagnostic.

It never becomes unchecked pointer arithmetic in the compiler.

---

92. Resource Limits

The compiler implements configurable resource ceilings for adversarial or accidental pathological source:

maximum parser nesting
maximum generic instantiation depth
maximum const-eval steps
maximum const recursion depth
maximum module count
maximum import graph edges
maximum object sections
maximum symbol count
maximum optimizer growth
maximum diagnostic cascade

Reasonable defaults are implementation-defined and reported with the relevant limit diagnostic.

A user may raise them.

They do not alter program semantics.

---

93. Final Conformance Suites

A complete Paracine 1.0 implementation must pass:

Frontend

lexical normalization
indentation/layout
grammar
name resolution
modules
overloads
generics
closures
capabilities
effects
const evaluation

Type/ownership

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

all integer operations
all shift counts
signed division extremes
float-to-int saturation
NaNs
signed zero
try/fail/give
defer/dispose
trap ordering

PCIR

construction
verification
reference interpretation
every optimization pass
path compression
SSA reduction
fusion
vectorization legality
effect preservation

PMIR

instruction selection
ABI lowering
register allocation
spill behavior
stack alignment
large frames
unwind
branch relaxation
machine encoding

COFF

sections
symbols
relocations
COMDAT
bigobj
archives
import libraries
foreign-linker compatibility

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
CFG when enabled
DLLs
debug directory
PDB
image verification
real Windows loader execution

Cross-backend

C++20/MSVC
C++20/Clang
direct Paracine native
PCIR interpreter

are compared over the portable semantic corpus.

---

94. Release Qualification

A compiler release cannot describe a component as “production” unless:

its required conformance suite passes
known semantic miscompilations are zero in the release blocker set
the native image verifier passes
the cross-backend differential corpus passes
the ABI matrix passes
the unwinder matrix passes

An experimental backend can coexist with the production compiler but must be labeled experimental.

---

95. Final Implementation Order

The canonical implementation strategy is:

Phase 1
UTF-8 normalization
lexer
layout engine
parser
typed AST
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
PCIR construction
PCIR verifier
reference interpreter

Phase 4
basic optimizer
SCCP
mem2reg
SROA
DCE
range/BCE

Phase 5
x86-64 PMIR
instruction selection
register allocation
frame layout
encoder

Phase 6
COFF writer
object reader
archive reader/writer

Phase 7
native linker
imports
exports
relocations
PE headers
runtime entry
first .exe

Phase 8
unwind
TLS
resources
DLL
hardening
debug

Phase 9
advanced optimizer
inlining
specialization
fusion
vectorization
Path Compression
SSA Reduction

Phase 10
C++20 backend
differential validation
full standard library
PDB/tooling

The first architecture-proof milestone is:

hello.pcn
↓
PCIR
↓
PMIR
↓
x86-64
↓
COFF/link
↓
hello.exe

without an external assembler, C++ compiler or linker.

---

96. First Native Milestone Contract

The minimum self-contained native compiler must support:

int
bool
text literals
routines
local let/var
arithmetic
comparison
if
while
calls
give
foreign Windows calls or minimal std output

and generate a valid direct PE32+ executable.

This milestone is not language 1.0 completion by itself.

It proves the architecture.

---

97. Final Standard-Library Tiers

The canonical tiers become:

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

No tier silently pulls a higher tier.

---

98. Final Philosophy of Memory Safety

Safe Paracine guarantees:

no use after move
no double dispose
no dangling safe borrow
no mutable/shared alias violation
no out-of-bounds memory access
no safe data race
no uninitialized safe read

It does not guarantee:

no memory leak
no deadlock
no logical race through message ordering
no starvation
no stack exhaustion
no denial-of-service through valid huge work

Unsafe code may violate memory-safety assumptions.

Such violation is erroneous execution and invalidates language guarantees from that point.

---

99. Final Definition of “No Undefined Behavior in Safe Code”

Safe source has no language-level undefined behavior.

An operation is instead one of:

defined value
recoverable result
defined trap
defined nondeterministic scheduling result
target-documented platform behavior

Compiler optimizations cannot manufacture assumptions beyond these rules.

Undefined or erroneous execution enters only through:

unsafe contract violation
foreign code violating its declared ABI/contract
external hardware/OS failure outside the abstract machine

---

100. Final Production Architecture

The completed Paracine toolchain is:

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
                    name resolution
                            │
                            ▼
                generics + capabilities
                            │
                            ▼
             ownership + effects + closure
                      resolution
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
          ┌─────────────────┼─────────────────┐
          │                 │                 │
          ▼                 ▼                 ▼
     C++20 legalizer    target legalizer  target legalizer
          │                 │                 │
          ▼                 ▼                 ▼
     C++20 emitter          PMIR              PMIR
          │                 │                 │
          ▼                 ▼                 ▼
        .cpp          x86-64 encoding    x86-64 encoding
                            │                 │
                            ▼                 ▼
                       object model      object model
                            │                 │
                            ▼                 ▼
                      COFF writer         native linker
                            │                 │
                            ▼                 ▼
                           .obj            PE32+
                                              │
                                              ▼
                                             .exe

For DLL production:

PCIR
 ↓
PMIR
 ↓
machine code
 ↓
native linker
 ↓
PE32+ DLL
 ├──→ module.dll
 └──→ module.lib

For static-library production:

PCIR
 ↓
PMIR
 ↓
COFF objects
 ↓
archive writer
 ↓
module.lib

For portable textual production:

PCIR
 ↓
C++20 backend
 ↓
module.cpp
module.hpp

---

101. Final One-Language Rule

All realizations derive from one resolved semantic program.

Therefore:

.cpp
.obj
.exe
.dll
.lib

are deployment products.

They are not separate Paracine dialects.

The source of truth remains:

.pcn

---

102. Final Backend Law

A backend may choose a different representation.

It may not choose a different meaning.

The definitive rule is:

«Semantics flow downward. Backends do not flow upward into the language.»

---

103. Final Optimization Law

The original law remains unchanged and is now given its exact operational interpretation:

«Resolve what is known. Remove what is unnecessary. Represent what remains directly.»

“Resolve” means:

constant evaluation
specialization
range proof
capability resolution
layout resolution
capture resolution
ownership resolution

“Remove” means:

dead code
dead storage
dead checks
dead cleanup
dead abstractions
dead iterator layers
dead branches
dead symbols

“Represent directly” means:

the smallest semantically valid PCIR
then the smallest target-appropriate PMIR/machine form
without introducing runtime structure the program does not need

---

104. Final Multi-Output Law

«One Paracine program may be materialized as portable C++20 text, native COFF objects, native static libraries, PE DLLs, or finished PE executables without changing its language semantics.»

The canonical three primary outputs remain:

.pcn → .cpp
.pcn → .obj
.pcn → .exe

while:

.lib
.dll
.hpp
.h
.pdb

are canonical supporting production artifacts.

---

105. Final Production Principle

«Write the meaning once. Resolve it once. Optimize it once. Materialize only what deployment requires.»

---

106. Final Language Definition

Paracine is a mature statically typed ahead-of-time native programming language with:

explicit data
explicit ownership
inferred nonlexical borrowing
static closures
monomorphized generics
structural capabilities
defined arithmetic
recoverable result semantics
terminal traps
structured native concurrency
no mandatory GC
no mandatory exceptions
no mandatory scheduler
no hidden dynamic initialization
no mandatory external compiler
no mandatory external assembler
no mandatory external linker

Its canonical semantic center is PCIR.

Its canonical Tier-1 machine path is:

.pcn
 ↓
PCIR
 ↓
PMIR
 ↓
x86-64
 ↓
COFF/link
 ↓
PE32+

Its canonical portability path is:

.pcn
 ↓
PCIR
 ↓
C++20
 ↓
.cpp

Both paths implement the same program.

---

107. Frozen Edition-2026 Statement

The edition-2026 language is considered frozen when the reference implementation accepts the grammar and semantics in the complete 1.0 specification plus this resolution document and no longer contains unresolved semantic placeholders.

New optimizations may be added without an edition change when they preserve semantics.

New library APIs may be added compatibly.

A change to any of these requires a new language edition:

grammar incompatibility
type-system incompatibility
ownership incompatibility
arithmetic semantic change
trap semantic change
module-resolution incompatibility
ABI-visible language representation change not protected by versioning

---

108. Final Canonical Summary

PARACINE ".pcn"

Readable source
+
Exact static types
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
+
C++20 portability output
────────────────────────────────
One semantic program
────────────────────────────────
       ↓
 C++20 / COFF / PE

And the definitive production rule is:

«One language. One semantic truth. Multiple exact realizations.»

PARACINE

.pcn → .cpp
.pcn → .obj
.pcn → .exe

Clear to people. Obvious to machines.A couple of the backend decisions above are deliberately conservative where Windows gives the compiler several legal possibilities. For example, Paracine now commits to conventional x64 unwind metadata it can generate and verify consistently; Microsoft documents the restricted x64 prolog/epilog and unwind model that makes this necessary.  The completed linker rules also match the documented PE32+ model: imports use directory/lookup/IAT structures, exports use address/name/ordinal tables, TLS contains loader-initialized template/index/callback information, and 64-bit images use page-grouped base relocations such as DIR64. 

The most consequential design change is the closure resolution: Paracine now gets the ergonomics of lambdas and iterator callbacks without adding GC, std::function, heap closure objects, or dynamic dispatch. That also fixes the earlier thread.scope design cleanly rather than papering over the fact that a capture-free callback could not actually borrow the caller's data.
