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
if exist "cyborg.c" (
    echo [INFO] Compiling cyborg.c...
:: 1. Compile resource script into a binary resource object
rc.exe /fo cyborg.res cyborg.rc

:: 2. Compile cyborg.c and link it with the compiled resource and required libraries
cl.exe /O2 cyborg.c cyborg.res /Fe:cyborg.exe User32.lib Gdi32.lib Comctl32.lib Shell32.lib Ole32.lib Advapi32.lib
    
    if !errorlevel! neq 0 (
        echo [ERROR] Compilation of cyborg.exe failed!
        goto error_exit
    )
    echo [SUCCESS] Compiled cyborg.exe successfully.
) else (
    echo [ERROR] cyborg.c not found on disk.
    goto error_exit
)

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