@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist "build-win64" mkdir "build-win64"
where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: Use an x64 Native Tools Command Prompt for Visual Studio.
    exit /b 1
)
set "STAGE0=%CD%\build-win64\paracine_stage0.exe"
set "STAGE1=%CD%\build-win64\ParacineCompiler.exe"
set "STRICT=%CD%\build-win64\strict_feature.exe"
set "FULL=%CD%\build-win64\full_feature.exe"
echo [1/9] Build Stage 0 with MSVC C++23...
cl /nologo /std:c++latest /EHsc /O2 /W3 /utf-8 "stage0\main.cpp" /Fe:"%STAGE0%"
if errorlevel 1 exit /b 1
echo [2/9] Run Stage-0 regression tests...
"%STAGE0%" selftest
if errorlevel 1 exit /b 1
echo [3/9] Validate Stage-1 Paracine source...
"%STAGE0%" check "ParacineCompiler.pcn"
if errorlevel 1 exit /b 1
echo [4/9] Build Paracine-written Stage 1 to PE32+...
"%STAGE0%" pe "ParacineCompiler.pcn" -O3 -o "%STAGE1%"
if errorlevel 1 exit /b 1
"%STAGE0%" verify-bin "%STAGE1%"
if errorlevel 1 exit /b 1
echo [5/9] Validate the expanded strict subset...
"%STAGE1%" check "examples\feature_02.pcn"
if errorlevel 1 exit /b 1
echo [6/9] Build and run the strict-subset example...
"%STAGE1%" build "examples\feature_02.pcn" -o "%STRICT%" --stage0 "%STAGE0%" -O3
if errorlevel 1 exit /b 1
"%STAGE0%" verify-bin "%STRICT%"
if errorlevel 1 exit /b 1
"%STAGE1%" run "examples\feature_02.pcn" -o "%STRICT%" --stage0 "%STAGE0%" -O3
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="42" (
    echo ERROR: strict-subset executable returned %RESULT%; expected 42.
    exit /b 1
)
echo [7/9] Verify full-grammar delegation with a record and array...
"%STAGE1%" check "examples\advanced_record_array.pcn" --stage0-full --stage0 "%STAGE0%"
if errorlevel 1 exit /b 1
echo [8/9] Build and run the Stage-0-compatible example...
"%STAGE1%" run "examples\advanced_record_array.pcn" -o "%FULL%" --stage0-full --stage0 "%STAGE0%" -O3
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="42" (
    echo ERROR: full-grammar delegated executable returned %RESULT%; expected 42.
    exit /b 1
)
echo [9/9] Verify final PE32+ output...
"%STAGE0%" verify-bin "%FULL%"
if errorlevel 1 exit /b 1
echo.
echo PASS: Stage-1 frontend, Stage-0 compatibility mode, PE32+, and Windows execution.
exit /b 0
