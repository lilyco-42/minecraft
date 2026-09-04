// 诊断: 哪一步 segfault
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"
typedef struct { void *vt[4]; } Handler;
static HRESULT STDMETHODCALLTYPE QI(void *s, REFIID r, void **o) { *o = s; return S_OK; }
static ULONG STDMETHODCALLTYPE AR(void *s) { return 2; }
static ULONG STDMETHODCALLTYPE RL(void *s) { return 1; }
static HRESULT STDMETHODCALLTYPE EnvInvoke(void *self, HRESULT hr, ICoreWebView2Environment *env) {
    FILE *f = fopen("probe3.log", "a");
    if (f) { fprintf(f, "EnvInvoke hr=0x%08lX\n", (unsigned long)hr); fclose(f); }
    PostQuitMessage(0);
    return S_OK;
}
int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    FILE *f = fopen("probe3.log", "w");
    if (f) { fprintf(f, "1 start\n"); fclose(f); }
    // 先探测运行时版本 (loader 推荐入口)
    PWSTR ver = NULL;
    HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(NULL, &ver);
    f = fopen("probe3.log", "a");
    if (f) { fprintf(f, "2 version rc=0x%08lX ver=%ls\n", (unsigned long)hr, ver ? ver : L"(null)"); fclose(f); }
    Handler hd;
    void *vt[4] = { QI, AR, RL, EnvInvoke };
    hd.vt[0] = vt[0]; hd.vt[1] = vt[1]; hd.vt[2] = vt[2]; hd.vt[3] = vt[3];
    f = fopen("probe3.log", "a");
    if (f) { fprintf(f, "3 calling create\n"); fclose(f); }
    hr = CreateCoreWebView2EnvironmentWithOptions(NULL, NULL, NULL, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&hd);
    f = fopen("probe3.log", "a");
    if (f) { fprintf(f, "4 create rc=0x%08lX\n", (unsigned long)hr); fclose(f); }
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { DispatchMessageW(&m); }
    f = fopen("probe3.log", "a");
    if (f) { fprintf(f, "5 done\n"); fclose(f); }
    return 0;
}
