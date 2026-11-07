@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist "build-win64" mkdir "build-win64"
set "STAGE0="
for /f "delims=" %%I in ('where Paracine.exe 2^>nul') do if not defined STAGE0 set "STAGE0=%%I"
if not defined STAGE0 (
    echo ERROR: Paracine.exe not found on PATH. Run Build-And-Test.cmd instead.
    exit /b 1
)
set "STAGE1=%CD%\build-win64\ParacineCompiler.exe"
set "STRICT=%CD%\build-win64\strict_feature.exe"
set "FULL=%CD%\build-win64\full_feature.exe"
echo Using existing Stage 0: %STAGE0%
echo Requires std.process.run, std.fs.read_text and std.fs.write_text.
"%STAGE0%" check "ParacineCompiler.pcn"
if errorlevel 1 exit /b 1
"%STAGE0%" pe "ParacineCompiler.pcn" -O3 -o "%STAGE1%"
if errorlevel 1 exit /b 1
"%STAGE1%" run "examples\feature_02.pcn" -o "%STRICT%" --stage0 "%STAGE0%"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="42" (
    echo ERROR: strict example returned %RESULT%; expected 42.
    exit /b 1
)
"%STAGE1%" run "examples\advanced_record_array.pcn" -o "%FULL%" --stage0-full --stage0 "%STAGE0%"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="42" (
    echo ERROR: Stage-0-compatible example returned %RESULT%; expected 42.
    exit /b 1
)
echo PASS: Both compilation modes produced running native executables.
exit /b 0
