// probe9 — 渲染管线诊断: 回调时序 + bounds + navigate
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include "WebView2.h"

static FILE *f;
static void logline(const char *s) { f = fopen("probe9.log","a"); if(f){fprintf(f,"%s\n",s);fclose(f);} }

static HRESULT STDMETHODCALLTYPE QI(void *s, REFIID r, void **o) { *o = s; return S_OK; }
static ULONG STDMETHODCALLTYPE AR(void *s) { (void)s; return 2; }
static ULONG STDMETHODCALLTYPE RL(void *s) { (void)s; return 1; }

static HWND g_hwnd;
static void *g_env_vt[4], *g_ctrl_vt[4];
typedef struct { void **vptr; } Handler;
static Handler g_env_h, g_ctrl_h;

static HRESULT STDMETHODCALLTYPE CtrlInvoke2(void *self, HRESULT hr, ICoreWebView2Controller *ctrl) {
    (void)self;
    char buf[80];
    if (FAILED(hr) || !ctrl) { snprintf(buf,sizeof buf,"ctrl hr=0x%08lX",(unsigned long)hr); logline(buf); PostQuitMessage(1); return S_OK; }
    logline("ctrl OK");
    ICoreWebView2Controller_put_IsVisible(ctrl, TRUE);
    RECT rc; GetClientRect(g_hwnd, &rc);
    snprintf(buf,sizeof buf,"rect %ld %ld %ld %ld", rc.left, rc.top, rc.right, rc.bottom);
    logline(buf);
    ICoreWebView2Controller_put_Bounds(ctrl, rc);
    ICoreWebView2 *web = NULL;
    ICoreWebView2Controller_get_CoreWebView2(ctrl, &web);
    if (web) {
        logline("web OK");
        ICoreWebView2_Navigate(web, L"data:text/html,<h1 style='background:green;color:white'>RENDER-OK-9</h1>");
    } else logline("web NULL");
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE EnvInvoke2(void *self, HRESULT hr, ICoreWebView2Environment *env) {
    (void)self;
    char buf[80];
    if (FAILED(hr) || !env) { snprintf(buf,sizeof buf,"env hr=0x%08lX",(unsigned long)hr); logline(buf); PostQuitMessage(1); return S_OK; }
    logline("env OK");
    ICoreWebView2Environment_CreateCoreWebView2Controller(env, g_hwnd,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&g_ctrl_h);
    return S_OK;
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev; (void)cmd;
    DeleteFileW(L"probe9.log");
    logline("start");
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Probe9";
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, L"Probe9", L"P9", WS_OVERLAPPEDWINDOW|WS_VISIBLE, 100,100,800,600, NULL,NULL,hInst,NULL);

    g_env_vt[0]=QI; g_env_vt[1]=AR; g_env_vt[2]=RL; g_env_vt[3]=EnvInvoke2;
    g_ctrl_vt[0]=QI; g_ctrl_vt[1]=AR; g_ctrl_vt[2]=RL; g_ctrl_vt[3]=CtrlInvoke2;
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
