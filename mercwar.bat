:: AVIS 
:: AIFVS-ARTIFACT
:: Filename: build_and_run.bat
@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not defined VCINSTALLDIR (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
    ) else (
        echo ERROR: Visual Studio vcvarsall.bat environment initializer script not found.
        goto end
    )
)

:: Compile resource script if present
if exist "mercwarai.rc" (
    echo [INFO] Compiling resource script mercwarai.rc...
    rc mercwarai.rc
    if !errorlevel! neq 0 (
        echo [ERROR] Resource compilation failed!
        goto end
    )
)

echo [INFO] Compiling mercwarai.c with resource linkage...
if exist "mercwarai.res" (
    cl /O2 /W3 /D_CRT_SECURE_NO_WARNINGS mercwarai.c mercwarai.res /link Shell32.lib User32.lib Advapi32.lib /out:mercwarai.exe
) else (
    cl /O2 /W3 /D_CRT_SECURE_NO_WARNINGS mercwarai.c /link Shell32.lib User32.lib Advapi32.lib /out:mercwarai.exe
)
	
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %errorlevel%
)

echo [SUCCESS] Compiled mercwarai.exe successfully.
echo.
echo [INFO] Executing mercwarai.exe --config config.json
rem mercwarai.exe --config config.json

:end
pause