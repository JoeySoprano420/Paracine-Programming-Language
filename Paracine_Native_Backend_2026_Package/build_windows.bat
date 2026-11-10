@echo off
setlocal
where cl >NUL 2>NUL
if errorlevel 1 (
  echo Microsoft Visual C++ compiler not found. Open an x64 Native Tools Command Prompt for Visual Studio.
  exit /b 1
)
cl /nologo /std:c++latest /EHsc /utf-8 /O2 /bigobj /W3 ParacineCompiler_2026_Native_Backend_Optimized.cpp /Fe:paracine.exe
if errorlevel 1 exit /b 1
paracine.exe selftest
if errorlevel 1 exit /b 1
paracine.exe test tests\arithmetic_and_branches.pcn -O2
if errorlevel 1 exit /b 1
paracine.exe test tests\memory_and_calls.pcn -O2
if errorlevel 1 exit /b 1
echo Build and tests completed.
