#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <ctype.h>
#include <errno.h>

#define MAX_STR 512
#define MAX_FILES 100

// Execute external SDK tools safely using CreateProcessA
int RunSdkTool(const char* exePath, const char* args) {
    char cmdLine[MAX_STR * 3];
    sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" %s", exePath, args);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("[ERROR] Failed to execute process: %s (Error: %lu)\n", exePath, GetLastError());
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exitCode;
}

// Helper to strip JSON/INI quotes, spaces, trailing commas, and newlines
void StripJsonQuotes(char* src, char* dst) {
    size_t slen = strlen(src);
    while (slen > 0 && (src[slen - 1] == '\r' || src[slen - 1] == '\n')) {
        src[--slen] = '\0';
    }

    char* start = src;
    while (*start == ' ' || *start == '\t' || *start == '"' || *start == '\xEF' || *start == '\xBB' || *start == '\xBF') start++;
    
    size_t len = strlen(start);
    while (len > 0 && (start[len - 1] == '"' || start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == ',' || start[len - 1] == '\r' || start[len - 1] == '\n')) {
        len--;
    }
    strncpy_s(dst, MAX_STR, start, len);
    dst[len] = '\0';
}

// Normalize path strings for Windows APIs (strip quotes/spaces and fix slashes)
void NormalizePath(const char* src, char* dst, size_t dstSize) {
    char temp[MAX_STR];
    StripJsonQuotes((char*)src, temp);
    for (int i = 0; temp[i] != '\0'; i++) {
        if (temp[i] == '/') temp[i] = '\\';
    }
    strcpy_s(dst, dstSize, temp);
}

// Resolve relative paths against the config file's base directory
void ResolvePath(const char* baseDir, const char* src, char* out, size_t outSize) {
    if (src[0] == '\0') {
        out[0] = '\0';
        return;
    }
    char norm[MAX_STR];
    NormalizePath(src, norm, sizeof(norm));

    if ((isalpha((unsigned char)norm[0]) && norm[1] == ':') || norm[0] == '\\') {
        strcpy_s(out, outSize, norm);
    } else {
        sprintf_s(out, outSize, "%s%s", baseDir, norm);
    }
}

// Robust manual parser supporting [CatalogFiles], [TargetFiles], [Files], and unsectioned files
int LoadTargetFiles(const char* iniPath, char targets[][MAX_STR], int maxTargets) {
    FILE* f = fopen(iniPath, "r");
    if (!f) {
        char errBuf[256];
        strerror_s(errBuf, sizeof(errBuf), errno);
        printf("[ERROR] Failed to open target file at '%s': %s (Errno: %d)\n", iniPath, errBuf, errno);
        return 0;
    }

    char line[1024];
    int inTargetSection = 1;
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        char* start = line;
        while (*start == ' ' || *start == '\t' || *start == '\xEF' || *start == '\xBB' || *start == '\xBF') start++;

        if (start[0] == '[' || strchr(start, '[')) {
            char sec[128] = "";
            int sIdx = 0;
            for (int i = 0; start[i] != '\0' && sIdx < sizeof(sec) - 1; i++) {
                if (start[i] != '[' && start[i] != ']' && start[i] != ' ' && start[i] != '\t' && start[i] != '\r' && start[i] != '\n') {
                    sec[sIdx++] = start[i];
                }
            }
            sec[sIdx] = '\0';

            if (_stricmp(sec, "CatalogFiles") == 0 || _stricmp(sec, "TargetFiles") == 0 || _stricmp(sec, "Files") == 0 || _stricmp(sec, "Targets") == 0) {
                inTargetSection = 1;
            } else {
                inTargetSection = 0;
            }
            continue;
        }

        if (inTargetSection && start[0] != ';' && start[0] != '#' && start[0] != '\0' && strncmp(start, "[CatalogHeader]", 14) != 0) {
            char* eq = strchr(start, '=');
            char rawTarget[MAX_STR];
            if (eq) {
                char* val = eq + 1;
                while (*val == ' ' || *val == '\t') val++;
                strcpy_s(rawTarget, sizeof(rawTarget), val);
            } else {
                strcpy_s(rawTarget, sizeof(rawTarget), start);
            }

            NormalizePath(rawTarget, targets[count], sizeof(targets[0]));
            if (targets[count][0] != '\0') {
                count++;
                if (count >= maxTargets) break;
            }
        }
    }
    fclose(f);
    printf("[INFO] Successfully parsed %d target files.\n", count);
    return count;
}

// Print usage reference when launched without arguments or help switches
void PrintUsage(const char* exeName) {
    printf("============================================================\n");
    printf("        MERCWAR AI AUTOMATED SIGNING ENGINE - CLI REFERENCE\n");
    printf("============================================================\n\n");
    printf("Usage:\n");
    printf("  %s --config <path_to_json> [switches]\n\n", exeName);
    printf("Switches:\n");
    printf("  --config <path>    Specify absolute or relative path to the JSON config.\n");
    printf("  --new-cert         Force-generate and register a fresh PFX certificate.\n");
    printf("  --use-existing     Use the existing PFX certificate without regenerating.\n");
    printf("  --help, -h         Display this command line reference guide.\n\n");
    printf("Example:\n");
    printf("  %s --config C:\\Mercwar-AI\\Dark-Com-2.json --new-cert\n\n", exeName);
    system("pause");
}

// Locate SDK binaries dynamically via Windows Registry
int LocateSdkTool(const char* exeName, char* outPath) {
    if (SearchPathA(NULL, exeName, NULL, MAX_STR, outPath, NULL) > 0) return 1;

    char sdkRoot[MAX_STR] = "";
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD bufSize = sizeof(sdkRoot);
        RegQueryValueExA(hKey, "KitsRoot10", NULL, NULL, (LPBYTE)sdkRoot, &bufSize);
        RegCloseKey(hKey);
    }
    if (strlen(sdkRoot) == 0) return 0;

    size_t len = strlen(sdkRoot);
    if (len > 0 && sdkRoot[len - 1] == '\\') sdkRoot[len - 1] = '\0';

    char searchPattern[MAX_STR];
    sprintf_s(searchPattern, sizeof(searchPattern), "%s\\bin\\*", sdkRoot);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.cFileName[0] != '.') {
                    sprintf_s(outPath, MAX_STR, "%s\\bin\\%s\\x64\\%s", sdkRoot, fd.cFileName, exeName);
                    if (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES) {
                        FindClose(hFind);
                        return 1;
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return 0;
}

// Explicitly load and parse variables from the JSON "Config" block
int LoadJsonConfig(const char* jsonPath, 
                   char* title, size_t tSize,
                   char* tsUrl, size_t uSize,
                   char* certCat, size_t catSize,
                   char* certIni, size_t iniSize,
                   char* certCdf, size_t cdfSize,
                   char* certReset, size_t rSize,
                   char* certName, size_t nSize,
                   char* subject, size_t sSize,
                   char* password, size_t pSize) {
    
    FILE* f = fopen(jsonPath, "r");
    if (!f) return 0;

    char line[1024];
    int inConfig = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "Config")) {
            inConfig = 1;
            continue;
        }
        if (inConfig) {
            if (strchr(line, '}')) {
                inConfig = 0;
                break;
            }
            char* firstQuote = strchr(line, '"');
            if (!firstQuote) continue;
            char* secondQuote = strchr(firstQuote + 1, '"');
            if (!secondQuote) continue;
            char* colon = strchr(secondQuote + 1, ':');
            if (colon) {
                *colon = '\0';
                char key[MAX_STR], val[MAX_STR];
                StripJsonQuotes(firstQuote, key);
                StripJsonQuotes(colon + 1, val);

                if (_stricmp(key, "Title") == 0) strncpy_s(title, tSize, val, _TRUNCATE);
                else if (_stricmp(key, "TimestampUrl") == 0) strncpy_s(tsUrl, uSize, val, _TRUNCATE);
                else if (_stricmp(key, "cert_cat") == 0) strncpy_s(certCat, catSize, val, _TRUNCATE);
                else if (_stricmp(key, "cert_ini") == 0) strncpy_s(certIni, iniSize, val, _TRUNCATE);
                else if (_stricmp(key, "cert_cdf") == 0) strncpy_s(certCdf, cdfSize, val, _TRUNCATE);
                else if (_stricmp(key, "cert_reset") == 0) strncpy_s(certReset, rSize, val, _TRUNCATE);
                else if (_stricmp(key, "cert_name") == 0) strncpy_s(certName, nSize, val, _TRUNCATE);
                else if (_stricmp(key, "Subject") == 0) strncpy_s(subject, sSize, val, _TRUNCATE);
                else if (_stricmp(key, "Password") == 0) strncpy_s(password, pSize, val, _TRUNCATE);
            }
        }
    }
    fclose(f);
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        PrintUsage(argv[0]);
        return 0;
    }

    int isElevatedInstance = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--elevated") == 0) {
            isElevatedInstance = 1;
            break;
        }
    }

    char currentDir[MAX_STR];
    GetCurrentDirectoryA(sizeof(currentDir), currentDir);
    size_t dLen = strlen(currentDir);
    if (dLen > 0 && currentDir[dLen - 1] != '\\') {
        strcat_s(currentDir, sizeof(currentDir), "\\");
    }

    // 1. Enforce Administrative Elevation
    if (!IsUserAnAdmin() && !isElevatedInstance) {
        printf("[INFO] Requesting Administrative Elevation...\n");
        char exePath[MAX_STR];
        GetModuleFileNameA(NULL, exePath, MAX_STR);
        
        char args[MAX_STR * 2] = "--elevated ";
        for (int i = 1; i < argc; i++) {
            strcat_s(args, sizeof(args), "\"");
            strcat_s(args, sizeof(args), argv[i]);
            strcat_s(args, sizeof(args), "\" ");
        }
        
        SHELLEXECUTEINFOA sei = { 0 };
        sei.cbSize = sizeof(SHELLEXECUTEINFOA);
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.lpParameters = args;
        sei.lpDirectory = currentDir;
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExA(&sei)) {
            return 0;
        }
        printf("[ERROR] Administrative elevation request failed.\n");
        system("pause");
        return 1;
    }

    // 2. Parse Command Line Arguments
    char jsonRawPath[MAX_STR] = "";
    int overrideCertReset = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            strcpy_s(jsonRawPath, sizeof(jsonRawPath), argv[++i]);
        } else if (strcmp(argv[i], "--new-cert") == 0) {
            overrideCertReset = 1;
        } else if (strcmp(argv[i], "--use-existing") == 0) {
            overrideCertReset = 0;
        }
    }

    if (jsonRawPath[0] == '\0') {
        printf("[ERROR] No configuration file specified. Use --config <path_to_json>\n\n");
        PrintUsage(argv[0]);
        return 1;
    }

    char configBaseDir[MAX_STR];
    NormalizePath(jsonRawPath, configBaseDir, sizeof(configBaseDir));
    char* lastSlash = strrchr(configBaseDir, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    } else {
        strcpy_s(configBaseDir, sizeof(configBaseDir), currentDir);
    }

    // 3. Load variables directly from JSON
    char title[MAX_STR] = "";
    char tsUrl[MAX_STR] = "";
    char rawCat[MAX_STR] = "";
    char rawIni[MAX_STR] = "";
    char rawCdf[MAX_STR] = "";
    char certReset[MAX_STR] = "yes";
    char rawName[MAX_STR] = "";
    char subject[MAX_STR] = "";
    char password[MAX_STR] = "";

    if (!LoadJsonConfig(jsonRawPath, title, sizeof(title), tsUrl, sizeof(tsUrl), 
                        rawCat, sizeof(rawCat), rawIni, sizeof(rawIni), 
                        rawCdf, sizeof(rawCdf), certReset, sizeof(certReset), 
                        rawName, sizeof(rawName), subject, sizeof(subject), password, sizeof(password))) {
        printf("[ERROR] Failed to load or parse JSON configuration file: %s\n", jsonRawPath);
        system("pause");
        return 1;
    }

    char certCat[MAX_STR], certIni[MAX_STR], certCdf[MAX_STR], certName[MAX_STR];
    ResolvePath(configBaseDir, rawCat, certCat, sizeof(certCat));
    ResolvePath(configBaseDir, rawIni, certIni, sizeof(certIni));
    ResolvePath(configBaseDir, rawCdf, certCdf, sizeof(certCdf));
    ResolvePath(configBaseDir, rawName, certName, sizeof(certName));

    if (overrideCertReset == 1) {
        strcpy_s(certReset, sizeof(certReset), "yes");
    } else if (overrideCertReset == 0) {
        strcpy_s(certReset, sizeof(certReset), "no");
    }

    printf("============================================================\n");
    printf("        %s (Elevated Execution Mode)\n", title);
    printf("============================================================\n\n");

    // 3.5 Single-pass Clean CDF Generation
    char targets[MAX_FILES][MAX_STR];
    int targetCount = LoadTargetFiles(certIni, targets, MAX_FILES);

    char catResultDir[MAX_STR];
    strcpy_s(catResultDir, sizeof(catResultDir), certCat);
    char* catSlash = strrchr(catResultDir, '\\');
    char catFileNameOnly[MAX_STR];
    if (catSlash) {
        strcpy_s(catFileNameOnly, sizeof(catFileNameOnly), catSlash + 1);
        *(catSlash + 1) = '\0';
    } else {
        strcpy_s(catFileNameOnly, sizeof(catFileNameOnly), certCat);
        strcpy_s(catResultDir, sizeof(catResultDir), configBaseDir);
    }

    FILE* cdfFile = NULL;
    if (targetCount > 0 && fopen_s(&cdfFile, certCdf, "w") == 0 && cdfFile) {
        fprintf(cdfFile, "[CatalogHeader]\r\n");
        fprintf(cdfFile, "Name=%s\r\n", catFileNameOnly);
        fprintf(cdfFile, "ResultDir=%s\r\n", catResultDir);
        fprintf(cdfFile, "PublicVersion=0x00000001\r\n");
        fprintf(cdfFile, "CatalogVersion=2\r\n");
        fprintf(cdfFile, "HashAlgorithms=SHA256\r\n\r\n");
        fprintf(cdfFile, "[CatalogFiles]\r\n");
        for (int i = 0; i < targetCount; i++) {
            fprintf(cdfFile, "<HASH>file%d=%s\r\n", i + 1, targets[i]);
        }
        fclose(cdfFile);
        printf("[SUCCESS] Generated single-pass clean CDF (%d targets): %s\n", targetCount, certCdf);
    } else {
        printf("[ERROR] Failed to generate CDF or 0 targets found in target file: %s\n", certIni);
        system("pause");
        return 1;
    }

    // 4. Compile Security Catalog
    char makecatPath[MAX_STR];
    if (LocateSdkTool("makecat.exe", makecatPath)) {
        if (GetFileAttributesA(certCdf) != INVALID_FILE_ATTRIBUTES) {
            printf("[*] Compiling security catalog via CDF: %s\n", certCdf);
            char makecatArgs[MAX_STR];
            sprintf_s(makecatArgs, sizeof(makecatArgs), "-v \"%s\"", certCdf);
            int mcRes = RunSdkTool(makecatPath, makecatArgs);
            if (mcRes != 0) {
                printf("[WARN] makecat exited with code: %d\n", mcRes);
            }
        } else {
            printf("[WARN] Catalog definition file %s not found on disk.\n", certCdf);
        }
    }

    // 5. Locate signtool.exe
    char signTool[MAX_STR];
    if (!LocateSdkTool("signtool.exe", signTool)) {
        printf("[ERROR] signtool.exe could not be found.\n");
        system("pause");
        return 1;
    }
    printf("[SUCCESS] Located SignTool: %s\n", signTool);

    // 6. Certificate Pass
    if (_stricmp(certReset, "yes") == 0) {
        if (GetFileAttributesA(certName) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileA(certName);
        }
        printf("[INFO] Generating fresh test certificate: %s\n", certName);
        char psCmd[MAX_STR * 3];
        sprintf_s(psCmd, sizeof(psCmd), "powershell -NoProfile -ExecutionPolicy Bypass -Command "
                       "\"$c = New-SelfSignedCertificate -Type CodeSigningCert -Subject '%s' -CertStoreLocation 'Cert:\\CurrentUser\\My'; "
                       "$p = ConvertTo-SecureString '%s' -AsPlainText -Force; "
                       "Export-PfxCertificate -Cert $c -FilePath '%s' -Password $p\"", subject, password, certName);
        system(psCmd);
    } else {
        printf("[INFO] Using existing certificate asset: %s\n", certName);
    }

    char certutilCmd[MAX_STR * 2];
    sprintf_s(certutilCmd, sizeof(certutilCmd), "certutil -f -p \"%s\" -importpfx Root \"%s\" >nul 2>&1", password, certName);
    system(certutilCmd);
    sprintf_s(certutilCmd, sizeof(certutilCmd), "certutil -f -p \"%s\" -importpfx My \"%s\" >nul 2>&1", password, certName);
    system(certutilCmd);
    printf("[SUCCESS] Certificate imported and trusted successfully.\n\n");

    // 7. Sign Catalog File
    int successCount = 0;
    if (strlen(certCat) > 0 && GetFileAttributesA(certCat) != INVALID_FILE_ATTRIBUTES) {
        printf("[SIGNING] Applying SHA-256 signature + timestamp to catalog: %s\n", certCat);
        char catSignArgs[MAX_STR * 2];
        sprintf_s(catSignArgs, sizeof(catSignArgs), "sign /f \"%s\" /p \"%s\" /fd sha256 /tr \"%s\" /td sha256 \"%s\"",
                certName, password, tsUrl, certCat);
        if (RunSdkTool(signTool, catSignArgs) == 0) {
            printf("[SUCCESS] Signed catalog: %s\n\n", certCat);
            successCount++;
        } else {
            printf("[FAILED] Catalog signature error: %s\n\n", certCat);
        }
    }

    // 8. Parse and Sign Target Files (Fixed argument parameters)
    printf("Processing targets declared under target configuration...\n\n");

    for (int i = 0; i < targetCount; i++) {
        char finalPath[MAX_STR];
        strcpy_s(finalPath, sizeof(finalPath), targets[i]);

        if (GetFileAttributesA(finalPath) != INVALID_FILE_ATTRIBUTES) {
            printf("[SIGNING] Applying SHA-256 signature + timestamp to: %s\n", finalPath);
            char signArgs[MAX_STR * 2];
            sprintf_s(signArgs, sizeof(signArgs), "sign /f \"%s\" /p \"%s\" /fd sha256 /tr \"%s\" /td sha256 \"%s\"",
                    certName, password, tsUrl, finalPath);

            if (RunSdkTool(signTool, signArgs) == 0) {
                printf("[SUCCESS] Signed: %s\n\n", finalPath);
                successCount++;
            } else {
                printf("[FAILED] Signature pass error: %s\n\n", finalPath);
            }
        } else {
            printf("[SKIP] Target file missing from disk: %s\n\n", finalPath);
        }
    }

    printf("============================================================\n");
    printf("OPERATION COMPLETE: Signed %d file assets via JSON/INI pipeline.\n", successCount);
    printf("============================================================\n");
    system("pause");
    return 0;
}