# Feature coverage — Paracine-written Stage-1 compiler v0.4.1

**Two distinct compilation modes exist.** This distinction matters to correctness and self-hosting progress.

| Capability | Strict Stage 1 (native Paracine frontend) | `--stage0-full` (Stage-0 delegation) |
|---|---|---|
| Compiler CLI and file I/O written in Paracine | Yes | Yes |
| Lexical validation | Ported Stage-1 lexer | Stage-0 original lexer |
| Syntax and local type checks | Stage-1 subset parser | Stage-0 parser/SAGE |
| `module`, imports, multiple routines, forward calls | Yes | Yes |
| `int`, `bool`, `long`, signed/unsigned integer spellings | Yes, width checks delegated | Stage-0 rules |
| `float`, `double`, `char` | Yes, subset | Stage-0 rules |
| `text`, owned `string`, common text methods | Yes, subset | Stage-0 rules |
| Typed returns and simple routine calls | Yes, static return lookup | Stage-0 rules |
| Explicit `with` effect clauses, preserved in generated source | Yes | Stage-0 rules |
| `let`, `var`, assignment and compound assignments | Yes, including integer bitwise variants | Stage-0 rules |
| Conditional expressions, nested `if`/`else`, `while` | Yes | Stage-0 rules |
| `from ... to`, `continue until`, `break`, `continue` | Yes | Stage-0 rules |
| `for x in [integer, literals]` | Yes | Stage-0 rules, including other iterables |
| `std.io.print` / `println` with typed text expressions | Yes | Stage-0 rules |
| `std.fs.exists/read_text/write_text`, `std.io.read_all` | Limited expressions and native runtime calls | Stage-0 rules |
| Record declarations and construction | **No** | Yes where Stage 0 implements them |
| Fixed arrays, full views and indexing | **No**, except fixed literal-array `for` | Yes where Stage 0 implements them |
| Variants, enums, unions, generics, capabilities | **No** | Only the subset Stage 0 implements |
| Ownership/borrowing, effect checking, FFI, async | **No** independent implementation | Only the subset Stage 0 implements |
| PCIR, PMIR, optimizer, x64 machine-code generation | Delegated to Stage 0 | Delegated to Stage 0 |
| COFF/PE32+ writing and linking | Delegated to Stage 0 | Delegated to Stage 0 |
| Independent Stage-1 self-hosting | **No** | **No** |

Strict Stage 1 retains its 32-active-binding limit and intentionally simple frontend symbol table. It distinguishes `text` and `string` but relies on Stage 0 for full ownership, numeric width checks, function arity, inferred effects, and complete SAGE/SKM rules. Its current `.pcn` output is canonicalized source, not its own machine code.

In `--stage0-full`, Stage 1 **does not run its strict parser**. It forwards the UTF-8 source to Stage 0 for semantic checking and backend compilation, thereby letting the single CLI handle much more of the currently implemented Paracine language. This mode is explicitly a *delegation workflow*, not a claim that Stage 1 has independently implemented those constructs or that the 2026 Paracine specification is complete.
