// probe8 — 修正 COM 对象布局: 对象头 8 字节必须是 *指向 vtable* 的指针
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

static FILE *g_f;
static void logline(const char *s) {
    g_f = fopen("probe8.log", "a");
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

// 正确布局: 对象 = { vptr } — vptr 指向函数指针数组
static void *g_vt[4];
typedef struct Handler { void **vptr; } Handler;

int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    DeleteFileW(L"probe8.log");
    logline("1 start");
    static Handler hd;
    g_vt[0] = HQI; g_vt[1] = HAR; g_vt[2] = HRL; g_vt[3] = HInvoke;
    hd.vptr = g_vt;   // 对象头 8 字节 = vtable 地址 ✓

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(NULL, NULL, NULL,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&hd);
    char buf[64]; snprintf(buf, sizeof buf, "2 create rc=0x%08lX", (unsigned long)hr);
    logline(buf);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { DispatchMessageW(&m); }
    logline("3 done");
    return 0;
}
