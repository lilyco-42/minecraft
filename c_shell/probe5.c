// probe5.c — 定位: handler 参数是否为崩因 (传 handler=NULL)
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

static void logline(const char *s, unsigned long v) {
    FILE *f = fopen("probe5.log", "a");
    if (f) { fprintf(f, "%s 0x%08lX\n", s, v); fclose(f); }
}

int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    DeleteFileW(L"probe5.log");
    PWSTR ver = NULL;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &ver);
    logline("1 version", (unsigned long)hr);

    logline("2 before create(handler=NULL)", 0);
    hr = CreateCoreWebView2EnvironmentWithOptions(NULL, NULL, NULL, NULL);
    logline("3 create(NULL handler) rc", (unsigned long)hr);
    return 0;
}
