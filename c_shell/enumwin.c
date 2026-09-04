// enumwin — 列出所有 Chrome_WidgetWin_1 顶层窗口及其父窗口/进程
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>

int main(void) {
    HWND h = NULL;
    int n = 0;
    while (n < 20) {
        h = FindWindowExW(NULL, h, L"Chrome_WidgetWin_1", NULL);
        if (!h) break;
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        HWND parent = GetParent(h);
        wchar_t title[128] = L"";
        GetWindowTextW(h, title, 128);
        wprintf(L"pid=%lu hwnd=%p parent=%p title=%s\n", pid, (void*)h, (void*)parent, title);
        n++;
    }
    if (n == 0) wprintf(L"NO Chrome_WidgetWin_1 found\n");
    return 0;
}
