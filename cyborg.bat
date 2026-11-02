:: ============================================================================
:: AVIS :: AIFVS-ARTIFACT
:: Filename: build_cyborg.bat
:: ============================================================================
@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================================
echo        INITIALIZING MSVC BUILD ENVIRONMENT (x64)
echo ============================================================

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
        echo [ERROR] Visual Studio vcvarsall.bat environment initializer script not found.
        goto error_exit
    )
)

echo.
echo ============================================================
echo          BUILDING WIN64 WINDOW WRAPPER (cyborg.exe)
echo ============================================================

:: Verify required source modules exist on disk
if not exist "cyborg.c" (
    echo [ERROR] cyborg.c not found on disk.
    goto error_exit
)
if not exist "cyborg_proc.c" (
    echo [ERROR] cyborg_proc.c not found on disk.
    goto error_exit
)

echo [INFO] Compiling and building source targets...

:: 1. Compile resource script if it exists
set RES_OBJ=
if exist "cyborg.rc" (
    echo [INFO] Compiling resource script cyborg.rc...
    rc.exe /fo cyborg.res cyborg.rc
    set RES_OBJ=cyborg.res
) else (
    echo [WARN] cyborg.rc not found. Proceeding without embedded application assets.
)

:: 2. Compile both C modules into the single target application package
cl.exe /O2 cyborg.c cyborg_proc.c %RES_OBJ% /Fe:cyborg.exe User32.lib Gdi32.lib Comctl32.lib Shell32.lib Ole32.lib Advapi32.lib

if !errorlevel! neq 0 (
    echo [ERROR] Compilation of cyborg.exe failed!
    goto error_exit
)

echo [SUCCESS] Compiled cyborg.exe successfully.

echo.
echo ============================================================
echo          BUILD COMPLETE :: cyborg.exe READY
echo ============================================================
goto end

:error_exit
echo.
echo ============================================================
echo [FAILED] Build process encountered errors.
echo ============================================================
pause
exit /b %errorlevel%

:end
pause
