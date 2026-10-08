# PARACINE — `.pcn`

## Mature Industrial Native Programming Language Specification

**Status:** Production design standard  
**Language class:** Native general-purpose, systems, performance, application, numerical, and infrastructure language  
**Compilation:** Ahead-of-time native compilation  
**Canonical implementation language:** C++23  
**Canonical portable backend:** Generated low-level C++23  
**Optional future backend:** Direct native code generation  
**Structural model:** Deterministic indentation  
**Default indentation:** 4 spaces per level  
**Runtime:** Minimal, feature-linked, non-managed  
**Mandatory VM:** None  
**Mandatory GC:** None  
**Mandatory exception runtime:** None  
**Mandatory async runtime:** None  
**Mandatory custom assembler/linker:** None  

### Governing principle

**«Make the program obvious to the reader and unsurprising to the machine.»**

### Optimization law

**«Resolve what is known. Remove what is unnecessary. Represent what remains directly.»**

### Implementation law

**«No language feature belongs in Paracine unless one person can explain and implement its lowering completely.»**

### Official motto

# PARACINE

**Clear to people. Obvious to machines.**

---

# 1. Definition

Paracine is a statically typed, ahead-of-time compiled native programming language designed around a strict engineering objective:

> A human-readable program should already have nearly the shape an optimizing compiler wants.

Paracine does not depend upon an enormous compiler architecture to recover simple meaning from complicated language machinery.

Instead, Paracine makes useful program information explicit while keeping the source familiar.

Its defining characteristics are:

- simple native types;
- familiar `int`-based declarations;
- one callable abstraction;
- static typing;
- predictable control flow;
- explicit mutation;
- explicit failure;
- explicit memory;
- early generic specialization;
- value-oriented programming;
- aggressive abstraction removal;
- simple compiler lowering;
- C++23 representability;
- native execution.

The language incorporates selected strengths from Volt, Skyz, Starstruck, and Instance while deliberately reducing their compiler complexity.

---

# 2. Canonical Integer Vocabulary

Paracine uses familiar integer names as the **canonical source spelling**.

The signed family is:

```text
byte
short
int
long
```

The unsigned family is:

```text
ubyte
ushort
uint
ulong
```

Their meanings are fixed.

| Paracine type | Width | Signedness |
|---|---:|---|
| `byte` | 8 bits | signed |
| `ubyte` | 8 bits | unsigned |
| `short` | 16 bits | signed |
| `ushort` | 16 bits | unsigned |
| `int` | 32 bits | signed |
| `uint` | 32 bits | unsigned |
| `long` | 64 bits | signed |
| `ulong` | 64 bits | unsigned |

Therefore:

```text
int
```

always means a signed 32-bit integer.

It does not vary according to operating system, compiler, or ABI.

Likewise:

```text
long
```

always means a signed 64-bit integer.

This intentionally avoids the platform-dependent integer widths historically associated with C and C++.

---

# 3. Canonical Routine Syntax

Paracine uses the familiar parameter style:

```text
name: type
```

The canonical integer routine is:

```text
routine add(a: int, b: int) gives int
    give a + b
```

Calling it:

```text
let total = add(a, b)
```

Returning the result of another call:

```text
give add(a, b)
```

This syntax is now authoritative Paracine style.

Paracine therefore reads:

```text
routine multiply(a: long, b: long) gives long
    give a * b
```

rather than:

```text
routine multiply(a: i64, b: i64) gives i64
```

The latter is not canonical Paracine source.

---

# 4. Primitive Types

The standard primitive family is:

```text
bool

byte
ubyte

short
ushort

int
uint

long
ulong

float
double

char
text

size
ssize

void
```

Canonical numerical meaning:

```text
byte    = signed 8-bit
ubyte   = unsigned 8-bit

short   = signed 16-bit
ushort  = unsigned 16-bit

int     = signed 32-bit
uint    = unsigned 32-bit

long    = signed 64-bit
ulong   = unsigned 64-bit

float   = IEEE-oriented 32-bit floating point
double  = IEEE-oriented 64-bit floating point
```

`size` is the unsigned native address-size integer used for sizes, counts, indexing, and allocation extents.

`ssize` is its signed counterpart.

On a 64-bit target they are normally 64 bits.

On a 32-bit target they are normally 32 bits.

Unlike the ordinary integer family, their width intentionally follows the target address model.

---

# 5. Optional Extended Integers

Implementations may support:

```text
int128
uint128
```

as standardized extended primitive types when the backend can preserve their defined semantics.

They are not replacements for:

```text
byte
short
int
long
```

The ordinary integer family remains the preferred source vocabulary.

---

# 6. Why Paracine Uses Familiar Type Names

Paracine prioritizes immediate readability.

This:

```text
routine calculate(count: int, total: long) gives long
```

is easier for most programmers and non-programmers to approach than:

```text
routine calculate(count: i32, total: i64) gives i64
```

while Paracine still retains the crucial systems-language property that the widths are formally fixed.

The language therefore combines:

```text
familiar spelling
+
fixed representation
```

rather than forcing a choice between them.

---

# 7. One Callable Abstraction

Paracine has one normal callable abstraction:

```text
routine
```

Example:

```text
routine add(a: int, b: int) gives int
    give a + b
```

Procedure:

```text
routine print_name(name: text)
    print(name)
```

Systems routine:

```text
routine clear(memory: span<ubyte>)
    each index, value in memory
        memory[index] = 0
```

There are no separate fundamental source constructs for:

```text
function
task
process
node
solve
sequence
```

This dramatically reduces implementation complexity.

---

# 8. Program Entry

Canonical entry:

```text
routine main() gives int
    give 0
```

Arguments:

```text
routine main(args: view<text>) gives int
    print("Hello")
    give 0
```

The target platform adapter maps the routine onto the operating system's actual process entry requirements.

---

# 9. Source Layout

Paracine uses deterministic indentation.

Exactly:

```text
4 spaces = one structural level
```

Example:

```text
routine main() gives int
    let x = 10

    if x > 5
        print(x)

    give 0
```

Ordinary blocks use no braces.

Ordinary statements use no semicolons.

Tabs normalize to four spaces before indentation structure is interpreted.

The lexer emits:

```text
NEWLINE
INDENT
DEDENT
EOF
```

---

# 10. Bindings

Immutable binding:

```text
let count = 25
```

Explicit type:

```text
let count: int = 25
```

Other examples:

```text
let small: byte = 10
let population: long = 8_000_000_000
let flags: uint = 0
```

Mutable binding:

```text
var count: int = 0

count += 1
```

Immutability is easier to write than mutability.

This benefits both human reasoning and compiler optimization.

---

# 11. Assignment

Assignment remains statement-oriented:

```text
count = 10
count += 1
count -= 1
count *= scale
count /= divisor
```

Equality comparison is:

```text
count == 10
```

Assignment is not an expression.

Therefore this is invalid:

```text
if x = y
```

and this is correct:

```text
if x == y
```

---

# 12. Literal Inference

Integer literals initially have context-sensitive integer-literal semantics.

Example:

```text
let a: byte = 10
let b: short = 10
let c: int = 10
let d: long = 10
```

No runtime conversion is required.

Without a constraining context:

```text
let count = 10
```

the default type is:

```text
int
```

Likewise:

```text
let value = 3.14
```

defaults to:

```text
double
```

Explicit suffixes may be provided where necessary:

```text
10b
10ub
10s
10us
10u
10l
10ul

3.5f
3.5d
```

The normal style relies on contextual inference rather than suffix-heavy source.

---

# 13. Numerical Examples

```text
let age: byte = 31
let year: short = 2026

let score: int = 95000
let flags: uint = 0xFF

let population: long = 8_400_000_000
let address_bits: ulong = 0xFFFF_FFFF_FFFF_FFFF
```

The source spelling itself remains easy to recognize.

---

# 14. Conversion

Paracine permits only conservative implicit numeric conversion.

Potentially narrowing operations require explicit conversion.

Example:

```text
let large: long = count as long
```

Narrowing:

```text
let small = value as short
```

Unsigned conversion:

```text
let bits = value as uint
```

Floating conversion:

```text
let ratio = count as double
```

Bit reinterpretation remains distinct:

```text
let bits = bitcast<uint>(value)
```

This prevents numeric conversion and raw representation reinterpretation from being confused.

---

# 15. Records

```text
record Point
    x: float
    y: float
```

Construction:

```text
let point = Point(
    x = 10.0,
    y = 20.0
)
```

Another:

```text
record User
    id: ulong
    age: byte
    active: bool
    name: text
```

Paracine records contain data.

They do not imply:

- inheritance;
- hidden allocation;
- runtime object identity;
- virtual dispatch;
- runtime metadata.

---

# 16. Enumerations

```text
enum State
    idle
    running
    paused
    finished
```

Explicit underlying representation:

```text
enum State: ubyte
    idle = 0
    running = 1
    paused = 2
    finished = 3
```

Or:

```text
enum ErrorCode: int
    success = 0
    invalid = 1
    failed = 2
```

---

# 17. Native Unions

```text
union Number
    integer: long
    floating: double
```

Native unions are intended for low-level representation work.

Tagged alternatives use `variant`.

---

# 18. Variants

```text
variant Token
    number(long)
    name(text)
    symbol(char)
```

A variant compiles into a concrete discriminated representation.

No dynamic object model is required.

---

# 19. Result Types

Recoverable failure uses:

```text
result<T, E>
```

Example:

```text
enum DivideError
    zero

routine divide(a: int, b: int) gives result<int, DivideError>
    if b == 0
        fail DivideError.zero

    give a / b
```

Successful return:

```text
give value
```

Failure:

```text
fail error
```

---

# 20. Error Propagation

```text
routine calculate(a: int, b: int) gives result<int, DivideError>
    let quotient = try divide(a, b)

    give quotient * 10
```

`try` expands semantically into:

```text
evaluate result
if failure
    propagate failure
otherwise
    extract successful value
```

No exception unwinder is necessary.

---

# 21. Options

```text
option<T>
```

Example:

```text
routine find_user(id: ulong) gives option<User>
```

Values:

```text
some(user)
none
```

Representation may specialize according to `T`.

---

# 22. Fixed Arrays

```text
array<int, 64>
```

Example:

```text
let values: array<int, 64>
```

Byte-oriented storage:

```text
let packet: array<ubyte, 1500>
```

The extent forms part of the type.

---

# 23. Views

Read-only contiguous view:

```text
view<T>
```

Example:

```text
routine sum(values: view<int>) gives long
```

Conceptually it contains:

```text
pointer
count
```

without ownership.

---

# 24. Spans

Mutable contiguous view:

```text
span<T>
```

Example:

```text
routine clear(values: span<ubyte>)
    each index, value in values
        values[index] = 0
```

A span does not own the referenced storage.

---

# 25. Buffers

Owned dynamic contiguous storage:

```text
buffer<T>
```

Example:

```text
let data = buffer<ubyte>(4096)
```

A buffer owns its backing storage.

It does not use mandatory garbage collection or reference counting.

---

# 26. Raw Pointers

```text
ptr<T>
```

Example:

```text
let address: ptr<int>
```

Address:

```text
let address = &value
```

Dereference:

```text
let result = *address
```

Pointer arithmetic is available in low-level contexts:

```text
address += 1
```

Raw pointer misuse lies outside checked Paracine guarantees.

---

# 27. Text

`text` is an immutable UTF-8 view.

```text
let name: text = "Paracine"
```

It does not imply heap allocation.

Owned mutable textual storage belongs to the standard-library `string` type.

---

# 28. Conditional Control

```text
if score >= 90
    print("excellent")
else if score >= 70
    print("passing")
else
    print("retry")
```

Negative condition:

```text
unless ready
    initialize()
```

`unless x` is equivalent to `if not x`.

---

# 29. `choose`

Paracine provides a readable decision expression:

```text
let discount = choose
    when customer.premium
        0.20

    when customer.member
        0.05

    otherwise
        0.00
```

Every reachable branch must resolve to a compatible type.

The compiler may lower `choose` into:

- branches;
- conditional moves;
- selects;
- switches;
- lookup tables.

There is no runtime `choose` structure.

---

# 30. Subject Selection

```text
let message = choose code
    case 0
        "success"

    case 1
        "retry"

    otherwise
        "failure"
```

Enum example:

```text
let text = choose state
    case State.idle
        "idle"

    case State.running
        "running"

    case State.paused
        "paused"

    case State.finished
        "finished"
```

---

# 31. Variant Matching

```text
let description = choose token
    case number(value)
        format(value)

    case name(value)
        value

    case symbol(value)
        char_text(value)
```

Variant selection is exhaustiveness-checked.

---

# 32. Loops

While:

```text
while running
    update()
```

Each:

```text
each item in values
    process(item)
```

Indexed:

```text
each index, item in values
    output[index] = transform(item)
```

The compiler receives explicit iteration domains suitable for range and bounds analysis.

---

# 33. Ranges

Half-open:

```text
0..<10
```

means:

```text
0 through 9
```

Inclusive:

```text
0..10
```

means:

```text
0 through 10
```

Stepped:

```text
0..<100 by 4
```

Descending:

```text
100..0 by -1
```

The half-open form is preferred for array indexing.

---

# 34. Pipeline Operator

Paracine preserves:

```text
->
```

as a readable value-flow operator.

Example:

```text
data -> decode -> normalize -> encode -> emit
```

With arguments:

```text
value -> scale(4) -> clamp(0, 255)
```

This:

```text
value -> scale(4)
```

means:

```text
scale(value, 4)
```

Pipelines are compile-time syntax.

They do not create runtime pipeline objects.

---

# 35. Pipeline Return

A routine can return a pipeline directly:

```text
routine prepare(input: view<ubyte>) gives Packet
    give input
        -> decode
        -> normalize
        -> verify
```

Likewise ordinary routine composition remains simple:

```text
routine total(a: int, b: int) gives int
    give add(a, b)
```

That form is canonical Paracine syntax.

---

# 36. Path Compression

Paracine aggressively removes unnecessary pipeline structure.

Source:

```text
input
    -> decode
    -> normalize
    -> transform
    -> encode
```

may become:

```text
load
combined transformation
store
```

after:

- inlining;
- temporary elimination;
- scalar replacement;
- loop fusion;
- dead-code elimination.

Paracine calls this:

**Path Compression.**

---

# 37. Generics

```text
routine maximum<T>(a: T, b: T) gives T
    where T is ordered

    if a > b
        give a

    give b
```

Uses such as:

```text
maximum<int>
maximum<long>
maximum<double>
```

produce concrete specialized implementations.

No runtime generic dispatch is required.

---

# 38. Generic Records

```text
record Pair<A, B>
    first: A
    second: B
```

Example:

```text
let pair: Pair<int, double>
```

Every instantiated generic receives a concrete representation.

---

# 39. Generic Constraints

Initial built-in constraints include:

```text
integer
signed
unsigned
floating
number
ordered
copyable
```

Example:

```text
routine absolute<T>(value: T) gives T
    where T is signed

    if value < 0
        give -value

    give value
```

The initial language deliberately avoids an enormous trait or concept system.

---

# 40. No General Operator Overloading

Operators retain predictable built-in meaning.

```text
a + b
```

does not trigger arbitrary user-defined overload search.

This makes:

- parsing easier;
- semantic analysis smaller;
- diagnostics clearer;
- compilation faster;
- source easier to understand.

---

# 41. Restricted Routine Overloading

Paracine does not depend upon complex C++-style overload resolution.

The preferred model is:

```text
one routine name
+
optional generic specialization
```

rather than large unrelated overload sets.

This keeps routine resolution deterministic and easy to implement.

---

# 42. Constants

```text
const maximum = 4096
const gravity: double = 9.80665
```

Integer constants infer `int` where their value fits unless context determines another type.

Example:

```text
const max_packet: ushort = 65535
```

---

# 43. Compile Routines

```text
compile routine mask(bits: int) gives ulong
    give (1 as ulong << bits) - 1
```

Usage:

```text
const permissions = mask(12)
```

Compile routines use ordinary Paracine semantics under restricted effects.

There is no separate metaprogramming language.

---

# 44. Arithmetic

Canonical arithmetic operates over the familiar primitive family:

```text
int a
long b
uint flags
double ratio
```

Unsigned arithmetic wraps modulo its width.

Signed integer arithmetic also has deterministic two's-complement wrapping semantics.

This means:

```text
int
long
short
byte
```

do not inherit C++ signed-overflow undefined behavior.

The backend must preserve Paracine semantics.

---

# 45. Checked Arithmetic

Explicit checked operations use standard library/intrinsic operations:

```text
let total = math.checked_add(a, b)
```

For `int` operands this returns conceptually:

```text
result<int, ArithmeticError>
```

Likewise:

```text
math.checked_mul
math.checked_sub
```

---

# 46. Floating Point

Canonical floating types are:

```text
float
double
```

Example:

```text
routine kinetic_energy(mass: double, velocity: double) gives double
    give 0.5 * mass * velocity * velocity
```

This keeps mathematical source familiar.

---

# 47. Bounds

Containers such as:

```text
array
view
span
buffer
```

carry sufficient extent information for safe indexing.

Example:

```text
routine total(values: view<int>) gives long
    var result: long = 0

    each value in values
        result += value

    give result
```

No manual integer-width syntax distracts from the algorithm.

---

# 48. Unsafe Regions

```text
unsafe
    *address = 10
```

Unsafe regions permit operations such as:

- unchecked pointer access;
- raw representation manipulation;
- integer-to-address construction;
- target intrinsics.

They do not disable ordinary type or syntax checking.

---

# 49. Memory Model

Core native storage vocabulary remains:

```text
value
array<T,N>
view<T>
span<T>
buffer<T>
ptr<T>
```

Example:

```text
let packet: array<ubyte, 1500>
let pixels: span<uint>
let source: view<float>
let memory: ptr<ubyte>
```

These familiar primitive spellings now propagate consistently through the entire memory system.

---

# 50. Deterministic Cleanup

Owned resources clean up lexically.

```text
let file = file.open(path)
```

The associated resource ends when its owning scope ends unless ownership is moved elsewhere.

Procedural cleanup may use:

```text
defer close(handle)
```

No garbage collector is required.

---

# 51. Modules

```text
module image.processing

use math
use image.pixel
```

Alias:

```text
use platform.windows as win
```

Definitions are private by default.

---

# 52. Public Routines

```text
public routine calculate(value: int) gives int
    give value * 2
```

Public C-compatible export:

```text
export c routine pcn_add(a: int, b: int) gives int
    give a + b
```

---

# 53. C Interoperability

```text
foreign c routine puts(value: ptr<char>) gives int
```

Another:

```text
foreign c routine write(
    fd: int,
    data: ptr<ubyte>,
    count: size
) gives ssize
```

C ABI layout:

```text
record Header
    layout c

    magic: uint
    size: uint
```

Paracine type names retain fixed Paracine meaning even when crossing C boundaries.

The compiler maps them onto the ABI-compatible C++23 representation.

---

# 54. C++23 Primitive Mapping

Canonical lowering is:

| Paracine | C++23 representation |
|---|---|
| `byte` | `std::int8_t` |
| `ubyte` | `std::uint8_t` |
| `short` | `std::int16_t` |
| `ushort` | `std::uint16_t` |
| `int` | `std::int32_t` |
| `uint` | `std::uint32_t` |
| `long` | `std::int64_t` |
| `ulong` | `std::uint64_t` |
| `float` | suitable 32-bit floating type |
| `double` | suitable 64-bit floating type |
| `size` | `std::size_t` |
| `ssize` | matching signed address-size integer |
| `bool` | defined boolean representation |
| `char` | Paracine character representation |

This guarantees that familiar source spelling does not introduce C/C++ width ambiguity.

---

# 55. Explicit Layout

```text
record Header
    layout c

    type: ushort
    flags: ushort
    length: uint
```

Explicit alignment:

```text
record CacheLine
    align 64

    value: ulong
```

Packed:

```text
record PacketHeader
    layout packed

    kind: ubyte
    flags: ubyte
    size: ushort
```

---

# 56. Concurrency

Paracine does not require a language scheduler.

Concurrency is provided through native library facilities such as:

```text
thread
atomic
mutex
semaphore
barrier
channel
```

This keeps the language core small.

---

# 57. Atomics

```text
let count: atomic<ulong> = 0
```

Operation:

```text
count.add(1, relaxed)
```

Memory orders:

```text
relaxed
acquire
release
acq_rel
seq_cst
```

The semantics map directly onto appropriate C++23/native atomic operations.

---

# 58. PCIR

Paracine uses one internal compiler representation:

**PCIR — Paracine Intermediate Representation**

PCIR is:

- typed;
- SSA-oriented;
- block-based;
- target-neutral;
- easy to verify;
- easy to print;
- easy to lower to C++23.

Canonical integer PCIR types may internally use explicit widths such as:

```text
s8
u8
s16
u16
s32
u32
s64
u64
```

That internal notation is not Paracine source syntax.

For example:

```text
int
```

may lower internally to:

```text
s32
```

and:

```text
ulong
```

to:

```text
u64
```

This cleanly separates:

```text
human-facing language
```

from:

```text
compiler-facing representation
```

---

# 59. PCIR Operations

A compact PCIR requires operations approximately equivalent to:

```text
const
copy

add
sub
mul
div
rem

and
or
xor
shift

compare
cast
bitcast

aggregate.make
aggregate.get

load
store
address

call

branch
branch_if
switch

bounds_check
trap

return
```

High-level syntax disappears before final code generation.

---

# 60. Source-to-PCIR Example

Source:

```text
routine add(a: int, b: int) gives int
    give a + b
```

Possible PCIR:

```text
routine add(a:s32, b:s32) -> s32
entry:
    %result = add.wrap.s32 a, b
    return %result
```

The compiler's explicit-width representation never needs to leak into ordinary `.pcn` source.

---

# 61. Calling Example

Source:

```text
routine calculate(a: int, b: int) gives int
    give add(a, b)
```

After inlining:

```text
routine calculate(a:s32, b:s32) -> s32
entry:
    %result = add.wrap.s32 a, b
    return %result
```

If `calculate` is itself inlined, even that routine boundary may disappear.

---

# 62. Optimization Pipeline

The canonical compact optimization pipeline includes:

```text
constant propagation
constant folding
CFG simplification
dead-code elimination
routine inlining
monomorphization cleanup
scalar replacement
copy propagation
simple range propagation
bounds-check elimination
loop canonicalization
path compression
common-expression elimination
dead-store elimination
final simplification
```

The backend compiler then performs target-level optimization.

---

# 63. Generated C++23

Paracine source:

```text
routine add(a: int, b: int) gives int
    give a + b
```

may conceptually become:

```cpp
std::int32_t pcn_add(
    std::int32_t a,
    std::int32_t b
) noexcept {
    return pcn_add_wrap_i32(a, b);
}
```

After native optimization, the wrapper implementing Paracine's defined wrapping semantics can normally collapse directly into the target addition instruction.

The generated C++ is compiler-owned.

Users program Paracine, not the generated implementation.

---

# 64. Monomorphization

Generic Paracine:

```text
routine square<T>(value: T) gives T
    give value * value
```

Uses:

```text
square<int>
square<long>
square<double>
```

produce concrete PCIR implementations corresponding to:

```text
int
long
double
```

before C++23 emission.

No Paracine generic semantics are delegated to C++ templates.

---

# 65. Beginner Example

```text
routine main() gives int
    let name = "Mira"
    let score = 87

    if score >= 70
        print(name, " passed")
    else
        print(name, " should retry")

    give 0
```

A beginner sees familiar concepts:

```text
routine
int
let
if
else
give
```

without systems-oriented width notation being forced into ordinary source.

---

# 66. Arithmetic Routine

```text
routine add(a: int, b: int) gives int
    give a + b
```

Composition:

```text
routine add_three(a: int, b: int, c: int) gives int
    give add(a, b) + c
```

Direct forwarding:

```text
routine sum_pair(a: int, b: int) gives int
    give add(a, b)
```

These forms now define the canonical function-writing character of Paracine.

---

# 67. Long Integer Example

```text
routine population_after(
    current: long,
    increase: int
) gives long
    give current + increase
```

The compiler performs the safe widening necessary for the addition.

---

# 68. Unsigned Example

```text
routine set_flag(flags: uint, mask: uint) gives uint
    give flags | mask
```

64-bit unsigned:

```text
routine mix(value: ulong, key: ulong) gives ulong
    give value ^ key
```

---

# 69. Small Integer Example

```text
routine pack(red: ubyte, green: ubyte, blue: ubyte) gives uint
    give
        (red as uint << 16)
        | (green as uint << 8)
        | blue as uint
```

This makes width-sensitive source readable while remaining explicit.

---

# 70. Record Example

```text
record Person
    id: ulong
    age: ubyte
    score: int
    name: text

routine describe(person: Person)
    print(
        person.name,
        " age ",
        person.age,
        " score ",
        person.score
    )
```

---

# 71. Numerical Example

```text
record Vector3
    x: double
    y: double
    z: double

routine magnitude(value: Vector3) gives double
    let squared =
        value.x * value.x
        + value.y * value.y
        + value.z * value.z

    give math.sqrt(squared)
```

---

# 72. Decision Example

```text
routine access_for(user: User, resource: Resource) gives Access
    give choose
        when user.id == resource.owner
            Access.full

        when user.member and user.verified
            Access.standard

        when user.member
            Access.restricted

        otherwise
            Access.read_only
```

---

# 73. Pipeline Example

```text
routine prepare(input: view<ubyte>) gives Packet
    give input
        -> decode
        -> normalize
        -> verify
```

Byte-oriented programs naturally use:

```text
ubyte
```

rather than a less familiar `u8` spelling.

---

# 74. Systems Example

```text
foreign c routine write(
    fd: int,
    data: ptr<ubyte>,
    count: size
) gives ssize

routine send(data: view<ubyte>) gives result<size, IOError>
    let written = write(
        1,
        data.ptr,
        data.count
    )

    if written < 0
        fail IOError.write_failed

    give written as size
```

---

# 75. SIMD-Friendly Example

```text
routine add(
    output: span<float>,
    left: view<float>,
    right: view<float>
)
    each i in 0..<output.count
        output[i] = left[i] + right[i]
```

Nothing in Paracine's human-readable primitive spelling interferes with vectorization.

The compiler still knows these are exact 32-bit floating-point values.

---

# 76. Compile-Time Example

```text
compile routine bit_mask(bits: int) gives ulong
    give (1 as ulong << bits) - 1

const permissions = bit_mask(12)
```

The result may be completely resolved during compilation.

---

# 77. Type-System Readability

Paracine intentionally prefers:

```text
record Header
    version: ushort
    flags: ushort
    size: uint
    timestamp: ulong
```

over:

```text
record Header
    version: u16
    flags: u16
    size: u32
    timestamp: u64
```

Both describe the same machine concepts.

Paracine chooses the former because the language is explicitly intended to remain approachable to people who are not already systems programmers.

---

# 78. Compiler Friendliness Is Preserved

The source spelling changes nothing about compiler precision.

The lexer can map primitive keywords immediately:

```text
byte   → S8
ubyte  → U8

short  → S16
ushort → U16

int    → S32
uint   → U32

long   → S64
ulong  → U64
```

Semantic analysis therefore works with exact representations internally.

The human gets familiar terminology.

The compiler gets exact widths.

Both win.

---

# 79. Parser Simplicity

Routine parameters consistently use:

```text
identifier : type
```

Example:

```text
routine calculate(
    count: int,
    total: long,
    ratio: double
) gives double
```

The grammar is straightforward:

```text
parameter
    = identifier,
      ":",
      type
    ;
```

Return type:

```text
return_clause
    = "gives",
      type
    ;
```

This is easy for a hand-written recursive-descent parser.

---

# 80. Routine Grammar

Canonical EBNF:

```text
routine_decl
    = [ "public" ],
      [ "compile" ],
      "routine",
      identifier,
      "(",
      [ parameter_list ],
      ")",
      [ "gives", type ],
      block
    ;

parameter_list
    = parameter,
      { ",", parameter }
    ;

parameter
    = identifier,
      ":",
      type
    ;
```

Example accepted source:

```text
routine add(a: int, b: int) gives int
    give a + b
```

---

# 81. Primitive Grammar

```text
primitive_type
    = "bool"

    | "byte"
    | "ubyte"

    | "short"
    | "ushort"

    | "int"
    | "uint"

    | "long"
    | "ulong"

    | "float"
    | "double"

    | "char"
    | "text"

    | "size"
    | "ssize"

    | "void"
    ;
```

This grammar is now canonical.

---

# 82. Type Grammar

```text
type
    = primitive_type
    | named_type
    | generic_type
    ;

generic_type
    = identifier,
      "<",
      type,
      { ",", type },
      ">"
    ;
```

Examples:

```text
view<int>
span<float>
buffer<ubyte>
ptr<long>
result<int, ParseError>
option<User>
array<uint, 64>
```

---

# 83. Variable Grammar

```text
let_stmt
    = "let",
      identifier,
      [ ":", type ],
      "=",
      expression
    ;

var_stmt
    = "var",
      identifier,
      [ ":", type ],
      [ "=", expression ]
    ;
```

Example:

```text
let count: int = 10
var total: long = 0
```

---

# 84. Expression Style

Paracine remains expression-friendly without making everything an expression.

Examples:

```text
a + b
a * b
count as long
values[index]
object.member
routine_call(a, b)
```

Return:

```text
give add(a, b)
```

Assignment remains statement-only.

---

# 85. No C Integer Ambiguity

Although Paracine deliberately adopts familiar names such as:

```text
short
int
long
```

it does **not** inherit C's platform-dependent width model.

For example, on Windows:

```text
long
```

in C++ may be 32 bits.

In Paracine:

```text
long
```

is always 64 bits.

The generated backend therefore uses:

```cpp
std::int64_t
```

rather than C++ `long`.

This distinction is fundamental.

---

# 86. No Signedness Ambiguity

In Paracine:

```text
byte
```

means signed 8-bit.

```text
ubyte
```

means unsigned 8-bit.

There is no platform-dependent `char` signedness problem.

Likewise:

```text
short
int
long
```

are always signed.

Their `u`-prefixed forms are always unsigned.

---

# 87. C++23 Representability

Every canonical primitive maps cleanly and completely to C++23.

Thus a solo compiler implementation can represent source types approximately as:

```cpp
enum class PrimitiveType {
    Bool,

    Byte,
    UByte,

    Short,
    UShort,

    Int,
    UInt,

    Long,
    ULong,

    Float,
    Double,

    Char,
    Text,

    Size,
    SSize,

    Void
};
```

Lowering can then select exact-width C++ representations.

No sophisticated type backend is required.

---

# 88. Canonical Style

Preferred Paracine:

```text
routine distance(
    speed: double,
    time: double
) gives double
    give speed * time
```

Integer:

```text
routine clamp_score(score: int) gives int
    if score < 0
        give 0

    if score > 100
        give 100

    give score
```

Systems:

```text
routine checksum(data: view<ubyte>) gives uint
    var result: uint = 0

    each value in data
        result += value

    give result
```

---

# 89. What Paracine Deliberately Does Not Do

The language still intentionally excludes:

- inheritance;
- mandatory virtual dispatch;
- dynamic typing;
- mandatory reflection;
- mandatory GC;
- mandatory reference counting;
- stack-unwinding exceptions;
- language-level schedulers;
- Vthreads;
- arbitrary optimizer directives;
- explicit register binding;
- custom mandatory linker;
- custom mandatory assembler;
- complex operator overloading;
- giant overload-resolution systems;
- giant trait systems;
- mandatory macro metaprogramming.

The integer spelling update does not change Paracine's architectural restraint.

---

# 90. The One-Person Test

Every feature must still satisfy:

1. Can its syntax be parsed deterministically?
2. Can its static semantics be described simply?
3. Can it lower into existing PCIR?
4. Can it be represented completely using C++23?
5. Does it avoid unnecessary mandatory runtime machinery?
6. Can a solo compiler developer realistically test it?
7. Is its benefit greater than its implementation cost?

The canonical integer model scores especially well here.

Eight ordinary keywords map onto eight exact integer representations.

That is nearly ideal compiler engineering.

---

# 91. Compiler Architecture

```text
.pcn source
     ↓
normalizer
     ↓
lexer
     ↓
recursive-descent parser
     +
Pratt expression parser
     ↓
typed syntax tree
     ↓
semantic resolution
     ↓
generic monomorphization
     ↓
PCIR
     ↓
Paracine optimization
     ↓
low-level C++23
     ↓
Clang / GCC / MSVC
     ↓
native object
     ↓
platform linker
     ↓
native executable
```

No change to this architecture is required for the new type syntax.

---

# 92. Implementation Simplicity

The front end merely interns primitive keywords.

Conceptually:

```text
"byte"   → TypeId::Byte
"ubyte"  → TypeId::UByte

"short"  → TypeId::Short
"ushort" → TypeId::UShort

"int"    → TypeId::Int
"uint"   → TypeId::UInt

"long"   → TypeId::Long
"ulong"  → TypeId::ULong
```

Their width and signedness can be stored in a tiny static table.

Example:

```text
Type          Bits     Signed
byte          8        yes
ubyte         8        no
short         16       yes
ushort        16       no
int           32       yes
uint          32       no
long          64       yes
ulong         64       no
```

That is extremely friendly to a one-person compiler.

---

# 93. Definitive Technical Profile

| Property | Paracine |
|---|---|
| Language | Paracine |
| Extension | `.pcn` |
| Category | Native general-purpose / systems / performance |
| Compilation | Ahead-of-time |
| Compiler implementation | C++23 |
| Canonical backend | Low-level generated C++23 |
| Integer syntax | Familiar fixed-width names |
| Signed integers | `byte`, `short`, `int`, `long` |
| Unsigned integers | `ubyte`, `ushort`, `uint`, `ulong` |
| Default integer | `int` |
| Floating types | `float`, `double` |
| Pointer-sized integers | `size`, `ssize` |
| Callable | `routine` |
| Parameter form | `name: type` |
| Return declaration | `gives type` |
| Return operation | `give` |
| Example | `routine add(a: int, b: int) gives int` |
| Call/forward example | `give add(a, b)` |
| Bindings | `let`, `var` |
| Failure | `result<T,E>`, `fail`, `try` |
| Decisions | `if`, `unless`, `choose` |
| Pipelines | `->` |
| Blocks | 4-space indentation |
| Generics | Monomorphized |
| Records | Native value aggregates |
| Arrays | Fixed native |
| Views | `view<T>` |
| Mutable views | `span<T>` |
| Owned arrays | `buffer<T>` |
| Raw pointer | `ptr<T>` |
| C FFI | First-class |
| GC | None mandatory |
| VM | None |
| Exception runtime | None |
| Scheduler | None mandatory |
| IR | PCIR |
| Final target optimization | Native C++23 toolchain |
| Primary compiler advantage | Small, exact semantic model |
| Primary runtime advantage | Minimal surviving work |

---

# 94. Final Canonical Syntax Identity

The characteristic Paracine routine is now:

```text
routine add(a: int, b: int) gives int
    give a + b
```

A routine may directly return another call:

```text
routine combine(a: int, b: int) gives int
    give add(a, b)
```

The canonical signed integer ladder is:

```text
byte
short
int
long
```

The canonical unsigned ladder is:

```text
ubyte
ushort
uint
ulong
```

This gives Paracine a source language that feels immediately recognizable while remaining far more deterministic than C's historical primitive-width model.

---

# 95. Final Definition

Paracine is a statically typed, ahead-of-time compiled native programming language built around extremely direct translation from readable source semantics to efficient machine behavior.

Its `.pcn` source uses familiar programming vocabulary without surrendering representation precision.

The programmer writes:

```text
routine add(a: int, b: int) gives int
    give a + b
```

The compiler knows immediately that:

```text
a
b
result
```

are signed 32-bit values.

The programmer writes:

```text
long
```

and the compiler knows that it means signed 64-bit.

The programmer writes:

```text
ulong
```

and the compiler knows that it means unsigned 64-bit.

There is no platform ambiguity.

There is no C-style uncertainty over the width of `long`.

There is no need for `i32`, `u32`, `i64`, and `u64` to dominate normal source code.

Paracine's type vocabulary is therefore:

```text
Human familiarity
+
Fixed machine representation
+
Simple compiler classification
──────────────────────────────
Readable native typing
```

Its mature compilation philosophy remains:

```text
Clear Meaning
+ Concrete Types
+ Explicit Data
+ Simple Control
+ Early Specialization
+ Path Compression
+ Mature Native Optimization
────────────────────────────
Minimum Necessary Machine Work
```

And its permanent identity remains:

# PARACINE

**Clear to people. Obvious to machines.**
