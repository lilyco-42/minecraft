// probe4.c — 验证: 手工实现 ICoreWebView2EnvironmentOptions 传给静态 loader
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

typedef struct { void *vt[4]; } Handler;

// ICoreWebView2EnvironmentOptions C vtable: QI AddRef Release + 4 get/put 对
// 顺序: get_AdditionalBrowserArguments, put_..., get_Language, put_...,
//       get_TargetCompatibleBrowserVersion, put_...,
//       get_AllowSingleSignOnUsingOSPrimaryAccount, put_...
static HRESULT STDMETHODCALLTYPE QI(void *s, REFIID r, void **o) {
    char buf[128];
    snprintf(buf, 128, "QI %08lX-%04hX\n", (unsigned long)r->Data1, r->Data2);
    FILE *q = fopen("probe4.log", "a");
    if (q) { fputs(buf, q); fclose(q); }
    (void)s;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE AR(void *s) { (void)s; return 2; }
static ULONG STDMETHODCALLTYPE RL(void *s) { (void)s; return 1; }

static HRESULT STDMETHODCALLTYPE GetArgs(void *s, LPWSTR *v) {
    (void)s;
    static const wchar_t args[] = L"";
    *v = (LPWSTR)CoTaskMemAlloc(sizeof(args));
    lstrcpyW(*v, args);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PutArgs(void *s, LPCWSTR v) { (void)s; (void)v; return S_OK; }
static HRESULT STDMETHODCALLTYPE GetLang(void *s, LPWSTR *v) {
    (void)s;
    static const wchar_t lang[] = L"zh-CN";
    *v = (LPWSTR)CoTaskMemAlloc(sizeof(lang));
    lstrcpyW(*v, lang);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PutLang(void *s, LPCWSTR v) { (void)s; (void)v; return S_OK; }
static HRESULT STDMETHODCALLTYPE GetTarget(void *s, LPWSTR *v) {
    (void)s;
    // 兼容任意已装运行时: 目标版本设为当前探测到的即可, 用一个较宽的版本
    static const wchar_t t[] = L"152.0.4191.53";
    *v = (LPWSTR)CoTaskMemAlloc(sizeof(t));
    lstrcpyW(*v, t);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PutTarget(void *s, LPCWSTR v) { (void)s; (void)v; return S_OK; }
static HRESULT STDMETHODCALLTYPE GetSSO(void *s, BOOL *b) { (void)s; *b = FALSE; return S_OK; }
static HRESULT STDMETHODCALLTYPE PutSSO(void *s, BOOL b) { (void)s; (void)b; return S_OK; }

static HRESULT STDMETHODCALLTYPE EnvInvoke(void *self, HRESULT hr, ICoreWebView2Environment *env) {
    FILE *f = fopen("probe4.log", "a");
    if (f) { fprintf(f, "EnvInvoke hr=0x%08lX env=%p\n", (unsigned long)hr, (void*)env); fclose(f); }
    PostQuitMessage(0);
    return S_OK;
}

int APIENTRY wWinMain(HINSTANCE h, HINSTANCE p, PWSTR c, int s) {
    FILE *f = fopen("probe4.log", "w");
    if (f) { fprintf(f, "1 start\n"); fclose(f); }

    // options 对象: 12 槽 vtable (3 IUnknown + 8 options + 1 padding 到安全)
    void **opt_vt = (void **)CoTaskMemAlloc(12 * sizeof(void*));
    opt_vt[0] = QI; opt_vt[1] = AR; opt_vt[2] = RL;
    opt_vt[3] = GetArgs;  opt_vt[4] = PutArgs;
    opt_vt[5] = GetLang;  opt_vt[6] = PutLang;
    opt_vt[7] = GetTarget; opt_vt[8] = PutTarget;
    opt_vt[9] = GetSSO;   opt_vt[10] = PutSSO;
    void *options = (void *)opt_vt;  // COM 对象首 8 字节即 vtable 指针

    Handler hd;
    void *vt[4] = { QI, AR, RL, EnvInvoke };
    hd.vt[0] = vt[0]; hd.vt[1] = vt[1]; hd.vt[2] = vt[2]; hd.vt[3] = vt[3];

    f = fopen("probe4.log", "a");
    if (f) { fprintf(f, "3 calling create\n"); fclose(f); }
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(NULL, NULL,
        (ICoreWebView2EnvironmentOptions *)options,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&hd);
    f = fopen("probe4.log", "a");
    if (f) { fprintf(f, "4 create rc=0x%08lX\n", (unsigned long)hr); fclose(f); }
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { DispatchMessageW(&m); }
    f = fopen("probe4.log", "a");
    if (f) { fprintf(f, "5 done\n"); fclose(f); }
    return 0;
}
