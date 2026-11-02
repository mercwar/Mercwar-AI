#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#define MAX_STR 512

#define IDC_BTN_BROWSE  3001
#define IDC_LBL_PATH    3002
#define IDC_LIST_JSON   3003
#define IDC_EDIT_JSON   3004
#define IDC_EDIT_STATUS 3005
#define IDC_BTN_SAVE    3006
#define IDC_BTN_RUN     3007

HWND hLblPath, hListJson, hEditJson, hEditStatus;
HWND hBtnBrowse, hBtnSave, hBtnRun;
HINSTANCE hInst;

char currentDir[MAX_STR] = "C:\\Mercwar-AI";
char selectedJsonPath[MAX_STR] = "";

// Forward declarations
void LoadJsonFilesFromDirectory(HWND hWnd);
void LoadSelectedJsonContent();
void SaveJsonContent();
void ExecuteMercwarAI();
void LogJsonTelemetry(const char* status, const char* action, const char* message, const char* target);

// Format and display structured JSON output in the telemetry view
void LogJsonTelemetry(const char* status, const char* action, const char* message, const char* target) {
    char jsonBuf[2048];
    sprintf_s(jsonBuf, sizeof(jsonBuf),
        "{\n"
        "  \"CyborgTelemetry\": {\n"
        "    \"Status\": \"%s\",\n"
        "    \"Action\": \"%s\",\n"
        "    \"Message\": \"%s\",\n"
        "    \"Target\": \"%s\"\n"
        "  }\n"
        "}",
        status, action, message, target ? target : "None"
    );
    SetWindowTextA(hEditStatus, jsonBuf);
}

// Browse directory callback
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {
    if (uMsg == BFFM_INITIALIZED) {
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)currentDir);
    }
    return 0;
}

void SelectDirectory(HWND hWnd) {
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = hWnd;
    bi.lpszTitle = "Select Directory Containing JSON Configurations";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = BrowseCallbackProc;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        char path[MAX_STR];
        if (SHGetPathFromIDListA(pidl, path)) {
            strcpy_s(currentDir, sizeof(currentDir), path);
            SetWindowTextA(hLblPath, currentDir);
            LoadJsonFilesFromDirectory(hWnd);
            LogJsonTelemetry("SUCCESS", "SELECT_DIRECTORY", "Directory loaded successfully.", currentDir);
        }
        CoTaskMemFree(pidl);
    }
}

// Populate ListBox with .json files from the active directory
void LoadJsonFilesFromDirectory(HWND hWnd) {
    SendMessageA(hListJson, LB_RESETCONTENT, 0, 0);
    SetWindowTextA(hEditJson, "");
    selectedJsonPath[0] = '\0';

    char searchPattern[MAX_STR];
    sprintf_s(searchPattern, sizeof(searchPattern), "%s\\*.json", currentDir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                SendMessageA(hListJson, LB_ADDSTRING, 0, (LPARAM)fd.cFileName);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
}

// Load selected JSON file into the edit control
void LoadSelectedJsonContent() {
    int selIdx = (int)SendMessageA(hListJson, LB_GETCURSEL, 0, 0);
    if (selIdx == LB_ERR) return;

    char filename[MAX_STR];
    SendMessageA(hListJson, LB_GETTEXT, selIdx, (LPARAM)filename);

    sprintf_s(selectedJsonPath, sizeof(selectedJsonPath), "%s\\%s", currentDir, filename);

    FILE* f = fopen(selectedJsonPath, "r");
    if (!f) {
        LogJsonTelemetry("ERROR", "LOAD_JSON", "Failed to open selected JSON file.", selectedJsonPath);
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(len + 1);
    if (buf) {
        fread(buf, 1, len, f);
        buf[len] = '\0';
        SetWindowTextA(hEditJson, buf);
        free(buf);
        LogJsonTelemetry("SUCCESS", "LOAD_JSON", "JSON configuration loaded into editor.", selectedJsonPath);
    }
    fclose(f);
}

// Save edits back to the active JSON file
void SaveJsonContent() {
    if (selectedJsonPath[0] == '\0') {
        LogJsonTelemetry("WARNING", "SAVE_JSON", "No JSON file selected to save.", "");
        return;
    }

    int len = GetWindowTextLengthA(hEditJson);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return;

    GetWindowTextA(hEditJson, buf, len + 1);
    FILE* f = fopen(selectedJsonPath, "w");
    if (f) {
        fwrite(buf, 1, len, f);
        fclose(f);
        LogJsonTelemetry("SUCCESS", "SAVE_JSON", "JSON configuration saved successfully to disk.", selectedJsonPath);
    } else {
        LogJsonTelemetry("ERROR", "SAVE_JSON", "Failed to write JSON file to disk.", selectedJsonPath);
    }
    free(buf);
}

// Trigger mercwarai.exe with the selected JSON config
void ExecuteMercwarAI() {
    if (selectedJsonPath[0] == '\0') {
        LogJsonTelemetry("WARNING", "EXECUTE_ENGINE", "No JSON configuration selected for execution.", "");
        return;
    }

    SaveJsonContent(); // Auto-save changes before execution

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
        LogJsonTelemetry("ERROR", "EXECUTE_ENGINE", "mercwarai.exe not found in execution directory.", targetExe);
        return;
    }

    char cmdLine[MAX_STR * 2];
    sprintf_s(cmdLine, sizeof(cmdLine), "\"%s\" --config \"%s\"", targetExe, selectedJsonPath);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        char msg[64];
        sprintf_s(msg, sizeof(msg), "Mercwar AI executed with exit code %lu", exitCode);
        LogJsonTelemetry("SUCCESS", "EXECUTE_ENGINE", msg, selectedJsonPath);
    } else {
        LogJsonTelemetry("ERROR", "EXECUTE_ENGINE", "Failed to execute mercwarai.exe process.", selectedJsonPath);
    }
}

// Main Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        // Browse Directory Button
        hBtnBrowse = CreateWindowExA(0, "BUTTON", "Browse Directory...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 150, 30, hWnd, (HMENU)IDC_BTN_BROWSE, hInst, NULL);

        // Path Display Label
        hLblPath = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", currentDir, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_LEFT,
            170, 10, 595, 30, hWnd, (HMENU)IDC_LBL_PATH, hInst, NULL);

        // ListBox for .json files
        hListJson = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOTIFY,
            10, 50, 240, 240, hWnd, (HMENU)IDC_LIST_JSON, hInst, NULL);

        // Telemetry / JSON Status Console (Bottom Left)
        DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY;
        hEditStatus = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "{\n  \"CyborgTelemetry\": {\n    \"Status\": \"READY\",\n    \"Message\": \"System initialized.\"\n  }\n}", editStyle,
            10, 300, 240, 140, hWnd, (HMENU)IDC_EDIT_STATUS, hInst, NULL);

        // Edit control for JSON viewer/editor (Right side)
        DWORD jsonEditStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL;
        hEditJson = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", jsonEditStyle,
            260, 50, 505, 390, hWnd, (HMENU)IDC_EDIT_JSON, hInst, NULL);

        // Action Buttons
        hBtnSave = CreateWindowExA(0, "BUTTON", "Save JSON", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            495, 450, 130, 30, hWnd, (HMENU)IDC_BTN_SAVE, hInst, NULL);
        hBtnRun  = CreateWindowExA(0, "BUTTON", "Run Mercwar AI", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            635, 450, 130, 30, hWnd, (HMENU)IDC_BTN_RUN, hInst, NULL);

        // Initial directory scan
        LoadJsonFilesFromDirectory(hWnd);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        if (wmId == IDC_BTN_BROWSE) {
            SelectDirectory(hWnd);
        } else if (wmId == IDC_LIST_JSON && wmEvent == LBN_SELCHANGE) {
            LoadSelectedJsonContent();
        } else if (wmId == IDC_BTN_SAVE) {
            SaveJsonContent();
        } else if (wmId == IDC_BTN_RUN) {
            ExecuteMercwarAI();
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }
    return 0;
}

// WinMain Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CyborgTelemetryClass";

    if (!RegisterClassA(&wc)) {
        return 0;
    }

    HWND hWnd = CreateWindowExA(0, "CyborgTelemetryClass", "Cyborg - Mercwar AI Wrapper & JSON Telemetry Console",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 530, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}