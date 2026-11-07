# Paracine Stage-0 — Windows `small` macro / MSVC C3530 repair

## Root cause (confirmed)

The Windows headers included via `<windows.h>` can define `small` as a macro expanding to `char` (as used by Windows RPC headers). That macro replaces the local identifier `small` in three Windows x64 PMIR code-generator cases. For example:

```cpp
auto small = emitLocalJcc(b, 0x86);
patchLocal(b, small, b.size());
```

After macro expansion this becomes invalid `auto char = ...` and `patchLocal(b, char, b.size())`. This matches the user's C3530 / C2513 / C2062 failures.

In this release, all three local identifiers are renamed `smallChunkBranch` at the code-generator locations near lines **10802**, **10949–10950**, **11004–11005**. This is a surgical rename with no change to generated machine instructions or intended semantics. The earlier C4996 `std::filesystem::u8path` fix is retained; there are no deprecated calls in this file.

## Installation in existing Visual Studio project

1. Save a backup of `C:\Users\420up\source\repos\Paracine\main.cpp`.
2. Copy the package's **main.cpp** to that precise project path, overwriting the old copy (or apply `MSVC_small_macro.patch` to the previous *MSVC compatibility fix* edition).
3. In Visual Studio select **Build > Clean Solution** and then **Build > Rebuild Solution** with **Release | x64** and C++23 mode.
4. Run the newly built executable by full path, for example `x64\Release\Paracine.exe version`, and confirm identifier `stage0-msvc-fix-20261106.8`.
5. Run `x64\Release\Paracine.exe selftest`. 89 passing tests is expected, but the MSVC build and Windows run must be confirmed on your machine.

Alternatively, from an **x64 Native Tools Command Prompt for Visual Studio**, in the extracted package folder run `Build-And-Test.cmd`. This builds a separate `build-win64\paracine_stage0.exe` and runs its Windows regression pipeline, without modifying the Visual Studio project's original executable.

### Minimal manual patch, if you are keeping other local edits

In the three local Windows code-generation blocks, rename the **identifier** `small` to `smallChunkBranch` in both the declaration and nearby `patchLocal` call. Do not globally replace the English word `small` in strings or unrelated comments.

Example:

```cpp
auto smallChunkBranch = emitLocalJcc(b, 0x86);
movReg(b, 8, 10);
patchLocal(b, smallChunkBranch, b.size());
```

You do not need to disable `small`, suppress warnings, or change Paracine syntax.

## Validation

- Original source with `-Dsmall=char`: reproduced compiler errors at all three declarations and their uses.
- Patched source with `-Dsmall=char`: passed Clang C++23 syntax-only compilation.
- Patched source: built with GCC C++23 `-O0` and `-Werror=deprecated-declarations`.
- GCC-built Paracine Stage-0: **89 passed, 0 failed** in `selftest`.
- **MSVC build and real Windows execution remain unverified here**; those are the final local checks.

This is a focused MSVC compatibility fix, not a new expansion of language coverage.
