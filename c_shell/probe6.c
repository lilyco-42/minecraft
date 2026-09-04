// probe6.c — 最小 handler: 只验证 loader 是否调到我们的 vtable
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

static FILE *g_f;
static void logline(const char *s) {
    g_f = fopen("probe6.log", "a");
    if (g_f) { fprintf(g_f, "%s\n", s); fclose(g_f); }
}

static HRESULT STDMETHODCALLTYPE HQI(void *s, REFIID r, void **o) {
    logline("HQI called");
    *o = s;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE HAR(void *s) { (void)s; logline("HAR called"); return 2; }
static ULONG STDMETHODCALLTYPE HRL(void *s) { (void)s; logline("HRL called"); return 1; }
static HRESULT STDMETHODCALLTYPE HInvoke(void *s, HRESULT hr, ICoreWebView2Environment *env) {
    (void)s; (void)env;
    char buf[64];
    snprintf(buf, sizeof buf, "HInvoke hr=0x%08lX", (unsigned long)hr);
    logline(buf);
    PostQuitMessage(0);
    return S_OK;
}

typedef struct Handler {
    void *vt[4];
} Handler;

int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    DeleteFileW(L"probe6.log");
    logline("1 start");

    static Handler hd;  // static 而非栈上, 排除生命周期问题
    hd.vt[0] = HQI;
    hd.vt[1] = HAR;
    hd.vt[2] = HRL;
    hd.vt[3] = HInvoke;

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(NULL, NULL, NULL,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&hd);
    char buf[64];
    snprintf(buf, sizeof buf, "2 create rc=0x%08lX", (unsigned long)hr);
    logline(buf);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { DispatchMessageW(&m); }
    logline("3 done");
    return 0;
}
