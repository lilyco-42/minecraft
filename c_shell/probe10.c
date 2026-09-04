// probe10 — 对齐官方 HelloWebView.cpp 顺序: UpdateWindow -> env -> ctrl -> settings -> bounds -> navigate
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

static FILE *f;
static void logline(const char *s) { f = fopen("probe10.log","a"); if(f){fprintf(f,"%s\n",s);fclose(f);} }

static HRESULT STDMETHODCALLTYPE QI(void *s, REFIID r, void **o) { *o = s; return S_OK; }
static ULONG STDMETHODCALLTYPE AR(void *s) { (void)s; return 2; }
static ULONG STDMETHODCALLTYPE RL(void *s) { (void)s; return 1; }

static HWND g_hwnd;
static void *g_env_vt[4], *g_ctrl_vt[4];
typedef struct { void **vptr; } Handler;
static Handler g_env_h, g_ctrl_h;

static HRESULT STDMETHODCALLTYPE CtrlInvoke3(void *self, HRESULT hr, ICoreWebView2Controller *ctrl) {
    (void)self;
    char buf[80];
    if (FAILED(hr) || !ctrl) { snprintf(buf,sizeof buf,"ctrl hr=0x%08lX",(unsigned long)hr); logline(buf); PostQuitMessage(1); return S_OK; }
    logline("ctrl OK");
    ICoreWebView2 *web = NULL;
    ICoreWebView2Controller_get_CoreWebView2(ctrl, &web);
    if (!web) { logline("web NULL"); PostQuitMessage(1); return S_OK; }
    // settings (官方样例做了一遍默认值)
    ICoreWebView2Settings *st = NULL;
    ICoreWebView2_get_Settings(web, &st);
    if (st) {
        ICoreWebView2Settings_put_IsScriptEnabled(st, TRUE);
        ICoreWebView2Settings_put_AreDefaultScriptDialogsEnabled(st, TRUE);
        ICoreWebView2Settings_put_IsWebMessageEnabled(st, TRUE);
        ICoreWebView2Settings_Release(st);
    }
    RECT rc; GetClientRect(g_hwnd, &rc);
    ICoreWebView2Controller_put_Bounds(ctrl, rc);
    logline("nav...");
    ICoreWebView2_Navigate(web, L"https://www.bing.com");
    ICoreWebView2Controller_put_IsVisible(ctrl, TRUE);
    logline("ctrl done");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE EnvInvoke3(void *self, HRESULT hr, ICoreWebView2Environment *env) {
    (void)self;
    char buf[80];
    if (FAILED(hr) || !env) { snprintf(buf,sizeof buf,"env hr=0x%08lX",(unsigned long)hr); logline(buf); PostQuitMessage(1); return S_OK; }
    logline("env OK");
    ICoreWebView2Environment_CreateCoreWebView2Controller(env, g_hwnd,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&g_ctrl_h);
    return S_OK;
}

static LRESULT CALLBACK WP(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(h, m, w, l);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev; (void)cmd;
    DeleteFileW(L"probe10.log");
    logline("start");
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WP;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Probe10";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, L"Probe10", L"P10", WS_OVERLAPPEDWINDOW, 100,100,1000,700, NULL,NULL,hInst,NULL);
    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    g_env_vt[0]=QI; g_env_vt[1]=AR; g_env_vt[2]=RL; g_env_vt[3]=EnvInvoke3;
    g_ctrl_vt[0]=QI; g_ctrl_vt[1]=AR; g_ctrl_vt[2]=RL; g_ctrl_vt[3]=CtrlInvoke3;
    g_env_h.vptr=g_env_vt; g_ctrl_h.vptr=g_ctrl_vt;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(NULL,NULL,NULL,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&g_env_h);
    char buf[64]; snprintf(buf,sizeof buf,"create 0x%08lX",(unsigned long)hr); logline(buf);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    logline("done");
    return 0;
}
