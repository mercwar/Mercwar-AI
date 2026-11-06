:: ============================================================================
:: AVIS :: AIFVS-ARTIFACT
:: Filename: build_mercwarai.bat
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
echo          COMPILING RESOURCE SCRIPT (mercwarai.rc)
echo ============================================================
if exist "mercwarai.rc" (
    echo [INFO] Compiling resource script mercwarai.rc...
    rc mercwarai.rc
    if !errorlevel! neq 0 (
        echo [ERROR] Resource compilation failed!
        goto error_exit
    )
) else (
    echo [WARN] mercwarai.rc not found. Skipping resource compilation.
)

echo.
echo ============================================================
echo          BUILDING CLI SIGNING ENGINE (mercwarai.exe)
echo ============================================================
if exist "mercwarai.c" (
    echo [INFO] Compiling mercwarai.c...
    if exist "mercwarai.res" (
        cl /O2 mercwarai.c mercwarai.res /Fe:mercwarai.exe advapi32.lib shell32.lib crypt32.lib user32.lib gdi32.lib
    ) else (
        cl /O2 mercwarai.c /Fe:mercwarai.exe advapi32.lib shell32.lib crypt32.lib
    )
    
    if !errorlevel! neq 0 (
        echo [ERROR] Compilation of mercwarai.exe failed!
        goto error_exit
    )
    echo [SUCCESS] Compiled mercwarai.exe successfully.
) else (
    echo [ERROR] mercwarai.c not found on disk.
    goto error_exit
)

echo.
echo ============================================================
echo          BUILD COMPLETE :: mercwarai.exe READY
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