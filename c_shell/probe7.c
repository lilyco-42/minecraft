// probe7 — 动态 DLL 版 loader (避开静态 lib 的 XFG vcall 校验)
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

typedef HRESULT (__stdcall *CreateEnvFn)(LPCWSTR, LPCWSTR, ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

static FILE *g_f;
static void logline(const char *s) {
    g_f = fopen("probe7.log", "a");
    if (g_f) { fprintf(g_f, "%s\n", s); fclose(g_f); }
}
static HRESULT STDMETHODCALLTYPE HQI(void *s, REFIID r, void **o) { logline("HQI"); *o = s; return S_OK; }
static ULONG STDMETHODCALLTYPE HAR(void *s) { (void)s; logline("HAR"); return 2; }
static ULONG STDMETHODCALLTYPE HRL(void *s) { (void)s; logline("HRL"); return 1; }
static HRESULT STDMETHODCALLTYPE HInvoke(void *s, HRESULT hr, ICoreWebView2Environment *env) {
    (void)s; (void)env;
    char buf[64]; snprintf(buf, sizeof buf, "HInvoke hr=0x%08lX", (unsigned long)hr);
    logline(buf); PostQuitMessage(0); return S_OK;
}
typedef struct Handler { void *vt[4]; } Handler;

int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    DeleteFileW(L"probe7.log");
    logline("1 start");
    HMODULE ld = LoadLibraryW(L"WebView2Loader.dll");
    if (!ld) { logline("1.5 no dll"); return 1; }
    CreateEnvFn create = (CreateEnvFn)GetProcAddress(ld, "CreateCoreWebView2EnvironmentWithOptions");
    logline("2 dll loaded");
    static Handler hd;
    hd.vt[0] = HQI; hd.vt[1] = HAR; hd.vt[2] = HRL; hd.vt[3] = HInvoke;
    HRESULT hr = create(NULL, NULL, NULL, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&hd);
    char buf[64]; snprintf(buf, sizeof buf, "3 create rc=0x%08lX", (unsigned long)hr);
    logline(buf);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { DispatchMessageW(&m); }
    logline("4 done");
    return 0;
}
