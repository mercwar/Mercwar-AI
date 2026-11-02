#ifndef CYBORG_H
#define CYBORG_H

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <process.h>

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

// Custom Message for Thread-Safe UI Streaming
#define WM_USER_STREAM_OUTPUT (WM_USER + 1)

// Global UI Handles and variables
extern HWND hLblPath, hListJson, hEditJson, hEditStatus;
extern HWND hBtnBrowse, hBtnSave, hBtnRun;
extern HINSTANCE hInst;

extern char currentDir[MAX_STR];
extern char selectedJsonPath[MAX_STR];

// Helpers implemented in cyborg.c
void SelectDirectory(HWND hWnd);
void LoadJsonFilesFromDirectory(HWND hWnd);
void LoadSelectedJsonContent();
void SaveJsonContent();
void LogJsonTelemetry(const char* status, const char* action, const char* message, const char* target);
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

// Background execution controls implemented in cyborg_proc.c
void ExecuteMercwarAI(HWND hOwnerWnd);
unsigned __stdcall ExecuteMercwarAIThread(void* pArguments);

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif // CYBORG_H
