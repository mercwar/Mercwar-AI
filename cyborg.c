#include "cyborg.h"

HWND hLblPath = NULL, hListJson = NULL, hEditJson = NULL, hEditStatus = NULL;
HWND hBtnBrowse = NULL, hBtnSave = NULL, hBtnRun = NULL;
HINSTANCE hInst = NULL;

char currentDir[MAX_STR] = "C:\\Mercwar-AI";
char selectedJsonPath[MAX_STR] = "";

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

void LoadSelectedJsonContent() {
    int selIdx = (int)SendMessageA(hListJson, LB_GETCURSEL, 0, 0);
    if (selIdx == LB_ERR) return;

    char filename[MAX_STR];
    SendMessageA(hListJson, LB_GETTEXT, selIdx, (LPARAM)filename);
    sprintf_s(selectedJsonPath, sizeof(selectedJsonPath), "%s\\%s", currentDir, filename);

    FILE* f = NULL;
    if (fopen_s(&f, selectedJsonPath, "r") != 0 || !f) {
        LogJsonTelemetry("ERROR", "LOAD_JSON", "Failed to open selected JSON file.", selectedJsonPath);
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(len + 1);
    if (buf) {
        size_t readBytes = fread(buf, 1, len, f);
        buf[readBytes] = '\0';
        SetWindowTextA(hEditJson, buf);
        free(buf);
        LogJsonTelemetry("SUCCESS", "LOAD_JSON", "JSON configuration loaded into editor.", selectedJsonPath);
    }
    fclose(f);
}

void SaveJsonContent() {
    if (selectedJsonPath[0] == '\0') {
        LogJsonTelemetry("WARNING", "SAVE_JSON", "No JSON file selected to save.", "");
        return;
    }

    int len = GetWindowTextLengthA(hEditJson);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return;

    GetWindowTextA(hEditJson, buf, len + 1);
    FILE* f = NULL;
    if (fopen_s(&f, selectedJsonPath, "w") == 0 && f) {
        fwrite(buf, 1, len, f);
        fclose(f);
        LogJsonTelemetry("SUCCESS", "SAVE_JSON", "JSON configuration saved successfully.", selectedJsonPath);
    } else {
        LogJsonTelemetry("ERROR", "SAVE_JSON", "Failed to write JSON file to disk.", selectedJsonPath);
    }
    free(buf);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        hBtnBrowse = CreateWindowExA(0, "BUTTON", "Browse Directory...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 10, 150, 30, hWnd, (HMENU)IDC_BTN_BROWSE, hInst, NULL);

        hLblPath = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", currentDir, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_LEFT,
            170, 10, 595, 30, hWnd, (HMENU)IDC_LBL_PATH, hInst, NULL);

        hListJson = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOTIFY,
            10, 50, 240, 240, hWnd, (HMENU)IDC_LIST_JSON, hInst, NULL);

        DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY;
        hEditStatus = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "{\n  \"CyborgTelemetry\": {\n    \"Status\": \"READY\",\n    \"Message\": \"System initialized.\"\n  }\n}", editStyle,
            10, 300, 240, 140, hWnd, (HMENU)IDC_EDIT_STATUS, hInst, NULL);

        DWORD jsonEditStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL;
        hEditJson = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", jsonEditStyle,
            260, 50, 505, 390, hWnd, (HMENU)IDC_EDIT_JSON, hInst, NULL);

        hBtnSave = CreateWindowExA(0, "BUTTON", "Save JSON", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            495, 450, 130, 30, hWnd, (HMENU)IDC_BTN_SAVE, hInst, NULL);
        hBtnRun  = CreateWindowExA(0, "BUTTON", "Run Mercwar AI", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            635, 450, 130, 30, hWnd, (HMENU)IDC_BTN_RUN, hInst, NULL);

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
            ExecuteMercwarAI(hWnd); 
        }
        break;
    }
    case WM_USER_STREAM_OUTPUT: {
        char* pLogChunk = (char*)wParam;
        if (pLogChunk) {
            int len = GetWindowTextLengthA(hEditStatus);
            SendMessageA(hEditStatus, EM_SETSEL, len, len);
            SendMessageA(hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)pLogChunk);
            free(pLogChunk); 
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CyborgTelemetryClass";

    if (!RegisterClassA(&wc)) return 0;

    HWND hWnd = CreateWindowExA(0, "CyborgTelemetryClass", "Cyborg - Mercwar AI Wrapper & JSON Telemetry Console",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 530, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
