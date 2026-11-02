@echo off
setlocal EnableDelayedExpansion
title Windows INI-Driven Automated Signing Engine
cd /d "%~dp0"

:: ============================================================
:: CONFIGURABLE FILE AND PATH VARIABLES
:: ============================================================
set "CDF_FILE=mercwarai.cdf"
set "CAT_FILE=mercwarai.cat"
set "INI_FILE=mercwarai2.ini"
set "CERT_FILE=mercwarai.pfx"
set "CERT_SUBJECT=CN=Mercwar AI, E=mercwar01@gmail.com"
set "CERT_PASSWORD=ofg48qpo5834gp56qg435dch8qp23hn45q8932p4"
set "TIMESTAMP_URL=http://timestamp.digicert.com"

:: Check for Administrative Elevation (Required to register Trusted Roots)
fsutil dirty query %systemdrive% >nul 2>&1
if %errorlevel% neq 0 (
    echo ============================================================
    echo [INFO] Requesting Administrative Elevation...
    echo ============================================================
    powershell -Command "Start-Process -FilePath '%0' -Verb RunAs"
    exit /b
)

echo ============================================================
echo        WINDOWS INI-DRIVEN AUTOMATED SIGNING ENGINE
echo ============================================================
echo.

:: 1. Auto-locate makecat.exe across standard Windows SDK paths
set "MAKECAT="
where makecat.exe >nul 2>&1
if !errorlevel! equ 0 (
    set "MAKECAT=makecat.exe"
) else (
    for /d %%D in ("C:\Program Files (x86)\Windows Kits\10\bin\*") do (
        if exist "%%~D\x64\makecat.exe" set "MAKECAT=%%~D\x64\makecat.exe"
    )
)

if not defined MAKECAT (
    echo [ERROR] makecat.exe could not be found. Please install the Windows SDK.
    pause
    exit /b 1
)

:: 2. Verify and load the existing catalog definition file (.cdf)
if not exist "%CDF_FILE%" (
    echo [ERROR] %CDF_FILE% file was not found in this directory.
    pause
    exit /b 1
)

echo [*] Compiling security catalog via loaded definition file: %CDF_FILE%
"!MAKECAT!" -v "%CDF_FILE%"
set "MAKECAT_EXIT=!errorlevel!"

if !MAKECAT_EXIT! neq 0 (
    echo [ERROR] Catalog compilation failed.
    pause
    exit /b 1
)
echo [SUCCESS] %CAT_FILE% generated successfully.
echo.

:: 3. Verify configuration mapping exists
if not exist "%INI_FILE%" (
    echo [ERROR] %INI_FILE% file was not found in this directory.
    pause
    exit /b 1
)

:: 4. Auto-locate signtool.exe across standard Windows SDK paths
set "SIGNTOOL="
where signtool.exe >nul 2>&1
if !errorlevel! equ 0 (
    set "SIGNTOOL=signtool.exe"
) else (
    for /d %%D in ("C:\Program Files (x86)\Windows Kits\10\bin\*") do (
        if exist "%%~D\x64\signtool.exe" set "SIGNTOOL=%%~D\x64\signtool.exe"
    )
)

if not defined SIGNTOOL (
    echo [ERROR] signtool.exe could not be found. 
    pause
    exit /b 1
)
echo [SUCCESS] Located SignTool: !SIGNTOOL!

:: 5. Force-generate a fresh local test certificate with your updated email every run
if exist "%CERT_FILE%" (
    del /f /q "%CERT_FILE%" >nul 2>&1
)

echo [INFO] Generating fresh test certificate with updated metadata...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject '%CERT_SUBJECT%' -CertStoreLocation 'Cert:\CurrentUser\My'; " ^
    "$pwd = ConvertTo-SecureString '%CERT_PASSWORD%' -AsPlainText -Force; " ^
    "Export-PfxCertificate -Cert $cert -FilePath '.\%CERT_FILE%' -Password $pwd"

if !errorlevel! neq 0 (
    echo [ERROR] Failed to automatically create local test certificate via PowerShell.
    pause
    exit /b !errorlevel!
)
echo [SUCCESS] Fresh %CERT_FILE% created successfully.

echo [*] Registering test certificate into Trusted Root Authority storage...
certutil -f -p "%CERT_PASSWORD%" -importpfx Root "%CERT_FILE%" >nul 2>&1
certutil -f -p "%CERT_PASSWORD%" -importpfx My "%CERT_FILE%" >nul 2>&1
echo [SUCCESS] Certificate completely trusted on this workstation system.
echo.

:: 6. Parse [SigningAssets] section safely
set "INI_CERT_FILE="
set "INI_CERT_PWD="

for /f "tokens=1,* delims==" %%A in (%INI_FILE%) do (
    set "key=%%A"
    set "val=%%B"
    if /i "!key!"=="CertificateFile" set "INI_CERT_FILE=!val!"
    if /i "!key!"=="CertificatePassword" set "INI_CERT_PWD=!val!"
)

:: Strip surrounding whitespace if present
for /f "tokens=*" %%a in ("!INI_CERT_FILE!") do set "INI_CERT_FILE=%%a"
for /f "tokens=*" %%a in ("!INI_CERT_PWD!") do set "INI_CERT_PWD=%%a"

if not "!INI_CERT_FILE!"=="" set "CERT_FILE=!INI_CERT_FILE!"
if not "!INI_CERT_PWD!"=="" set "CERT_PASSWORD=!INI_CERT_PWD!"

if not exist "%CERT_FILE%" (
    if exist "Eclipse\%CERT_FILE%" set "CERT_FILE=Eclipse\%CERT_FILE%"
)

if not exist "%CERT_FILE%" (
    echo [ERROR] Certificate file path specified does not exist: %CERT_FILE%
    pause
    exit /b 1
)

echo [SUCCESS] Located Sign Asset: %CERT_FILE%
echo ------------------------------------------------------------
echo Processing target files listed under [TargetFiles] section...
echo.

:: 7. Parse items inside the target block and sign using configured timestamp endpoint
set /a SUCCESS_COUNT=0
set "IN_TARGET_SECTION=0"

for /f "usebackq delims=" %%L in ("%INI_FILE%") do (
    set "line=%%L"
    if "!line:~0,1!"=="[" (
        if /i "!line!"=="[TargetFiles]" (
            set "IN_TARGET_SECTION=1"
        ) else (
            set "IN_TARGET_SECTION=0"
        )
    ) else (
        if "!IN_TARGET_SECTION!"=="1" (
            if not "!line:~0,1!"==";" (
                for /f "tokens=1,* delims==" %%X in ("!line!") do (
                    set "file_val=%%Y"
                    if not "!file_val!"=="" (
                        for /f "tokens=*" %%a in ("!file_val!") do set "file_val=%%a"
                        
                        set "TARGET_PATH="
                        if exist "!file_val!" (
                            set "TARGET_PATH=!file_val!"
                        ) else (
                            if exist "Eclipse\!file_val!" (
                                set "TARGET_PATH=Eclipse\!file_val!"
                            )
                        )
                        
                        if defined TARGET_PATH (
                            echo [SIGNING] Applying SHA-256 signature + RFC-3161 timestamp to: !TARGET_PATH!
                            "!SIGNTOOL!" sign /f "%CERT_FILE%" /p "%CERT_PASSWORD%" /fd sha256 /tr %TIMESTAMP_URL% /td sha256 "!TARGET_PATH!"

                            if !errorlevel! equ 0 (
                                echo [SUCCESS] Successfully signed: !TARGET_PATH!
                                set /a SUCCESS_COUNT+=1
                            ) else (
                                echo [FAILED] Failed execution pass on !TARGET_PATH!
                            )
                        ) else (
                            echo [SKIP] Declared target file does not exist: !file_val!
                        )
                        echo.
                    )
                )
            )
        )
    )
)

:: Explicitly sign the compiled security catalog itself using variables
if exist "%CAT_FILE%" (
    echo [SIGNING] Applying SHA-256 signature + RFC-3161 timestamp to catalog: %CAT_FILE%
    "!SIGNTOOL!" sign /f "%CERT_FILE%" /p "%CERT_PASSWORD%" /fd sha256 /tr %TIMESTAMP_URL% /td sha256 "%CAT_FILE%"
    if !errorlevel! equ 0 (
        echo [SUCCESS] Successfully signed: %CAT_FILE%
        set /a SUCCESS_COUNT+=1
    ) else (
        echo [FAILED] Failed execution pass on catalog: %CAT_FILE%
    )
    echo.
)

echo ============================================================
echo OPERATION COMPLETE: Cleanly signed !SUCCESS_COUNT! file assets via pipeline.
echo ============================================================
pause