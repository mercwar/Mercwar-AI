#include "cyborg.h"

unsigned __stdcall ExecuteMercwarAIThread(void* pArguments) {
    HWND hWnd = (HWND)pArguments;
    
    char targetExe[MAX_STR];
    GetModuleFileNameA(NULL, targetExe, MAX_STR);
    char* lastSlash = strrchr(targetExe, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
        strcat_s(targetExe, sizeof(targetExe), "mercwarai.exe");
    } else {
        strcpy_s(targetExe, sizeof(targetExe), "C:\\Mercwar-AI\\mercwarai.exe");
    }

    if (GetFileAttributesA(targetExe) == INVALID_FILE_ATTRIBUTES) {
        LogJsonTelemetry("ERROR", "EXECUTE_ENGINE", "mercwarai.exe not found.", targetExe);
        EnableWindow(hBtnRun, TRUE);
        return 0;
    }

    char cmdLine[MAX_STR * 2];
    sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" --config \"%s\"", targetExe, selectedJsonPath);

    // Initialize Security Attributes for redirection piping
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        LogJsonTelemetry("ERROR", "PIPE_CREATE", "Failed to create stdout pipe.", "");
        EnableWindow(hBtnRun, TRUE);
        return 0;
    }

    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        EnableWindow(hBtnRun, TRUE);
        return 0;
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_OUT_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; 

    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hChildStd_OUT_Wr); // Close write end so read loop terminates on process exit

        char readBuf[256];
        DWORD bytesRead;
        
        LogJsonTelemetry("RUNNING", "EXECUTE_ENGINE", "Streaming engine output...", selectedJsonPath);

        // Read stream dynamically and push chunks back to the GUI thread safely
        while (ReadFile(hChildStd_OUT_Rd, readBuf, sizeof(readBuf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            readBuf[bytesRead] = '\0';
            char* pStreamData = _strdup(readBuf);
            if (pStreamData) {
                PostMessageA(hWnd, WM_USER_STREAM_OUTPUT, (WPARAM)pStreamData, 0);
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hChildStd_OUT_Rd);

        char msg[128];
        sprintf_s(msg, sizeof(msg), "Execution complete. Exit code: %lu", exitCode);
        LogJsonTelemetry("SUCCESS", "EXECUTE_ENGINE", msg, selectedJsonPath);
    } else {
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        LogJsonTelemetry("ERROR", "EXECUTE_ENGINE", "Failed to create application process.", selectedJsonPath);
    }

    EnableWindow(hBtnRun, TRUE);
    return 0;
}

void ExecuteMercwarAI(HWND hOwnerWnd) {
    if (selectedJsonPath[0] == '\0') {
        LogJsonTelemetry("WARNING", "EXECUTE_ENGINE", "No configuration target verified.", "");
        return;
    }

    SaveJsonContent(); 
    EnableWindow(hBtnRun, FALSE); 

    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ExecuteMercwarAIThread, (void*)hOwnerWnd, 0, NULL);
    if (hThread) {
        CloseHandle(hThread); 
    } else {
        EnableWindow(hBtnRun, TRUE);
    }
}
