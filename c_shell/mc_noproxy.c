#include "sdk/include/WebView2.h"
#include <windows.h>
#include <stdio.h>

static HWND g_h;
static ICoreWebView2Controller *g_ctrl;
static ICoreWebView2 *g_web;
typedef HRESULT(WINAPI*PFNCreate)(PCWSTR,PCWSTR,ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

static HRESULT __stdcall QI(void*s,REFIID r,void**o){*o=s;return S_OK;}
static ULONG __stdcall AddRef(void*s){return 2;}
static ULONG __stdcall Release(void*s){return 1;}

// ICoreWebView2EnvironmentOptions 实现
typedef struct{
    void **vptr;
    wchar_t args[256];
} Options;

static HRESULT __stdcall OptQI(void*s,REFIID r,void**o){
    Options *self=(Options*)s;
    if(memcmp(r,&IID_IUnknown,sizeof(IID))==0 ||
       memcmp(r,&IID_ICoreWebView2EnvironmentOptions,sizeof(IID))==0){
        *o=self;return S_OK;
    }
    return E_NOINTERFACE;
}
static HRESULT __stdcall OptGetArgs(void*s,LPWSTR*v){
    Options *self=(Options*)s;
    *v=(LPWSTR)CoTaskMemAlloc(sizeof(self->args));
    lstrcpyW(*v,self->args);
    return S_OK;
}
static HRESULT __stdcall OptPutArgs(void*s,LPCWSTR v){
    Options *self=(Options*)s;
    lstrcpynW(self->args,v,255);
    return S_OK;
}
static HRESULT __stdcall OptGetLang(void*s,LPWSTR*v){
    *v=(LPWSTR)CoTaskMemAlloc(sizeof(L"en"));
    lstrcpyW(*v,L"en");
    return S_OK;
}
static HRESULT __stdcall OptPutLang(void*s,LPCWSTR v){return S_OK;}

// 关键:--no-proxy-server 绕过 Clash TUN 代理
static HRESULT __stdcall OnEnv(void*s,HRESULT hr,ICoreWebView2Environment*env){
    if(FAILED(hr)||!env){PostQuitMessage(1);return S_OK;}
    return ICoreWebView2Environment_CreateCoreWebView2Controller(env,g_h,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*)s);
}
static HRESULT __stdcall OnCtrl(void*s,HRESULT hr,ICoreWebView2Controller*ctrl){
    if(FAILED(hr)||!ctrl){PostQuitMessage(1);return S_OK;}
    ICoreWebView2Controller_put_IsVisible(ctrl,TRUE);
    RECT rc;GetClientRect(g_h,&rc);
    ICoreWebView2Controller_put_Bounds(ctrl,rc);
    ICoreWebView2Controller_get_CoreWebView2(ctrl,&g_web);
    if(g_web) ICoreWebView2_Navigate(g_web,L"http://192.168.10.165:8765");
    return S_OK;
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE hPrev,PWSTR cmd,int show){
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    WNDCLASSW wc={0};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=L"W";
    RegisterClassW(&wc);
    g_h=CreateWindowExW(0,L"W",L"MC",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,100,100,1100,760,0,0,GetModuleHandleW(NULL),0);
    ShowWindow(g_h,SW_SHOW);

    // 构造 options:[QI,AddRef,Release,get_Args,put_Args,get_Lang,put_Lang]
    static void*ov[7]={OptQI,AddRef,Release,OptGetArgs,OptPutArgs,OptGetLang,OptPutLang};
    static Options opt={ov,L"--no-proxy-server --disable-gpu"};

    static void*ev[4]={QI,AddRef,Release,OnEnv};
    static void*cv[4]={QI,AddRef,Release,OnCtrl};
    static char eo[8]={0},co[8]={0};
    *(void**)eo=ev;*(void**)co=cv;

    HMODULE dll=LoadLibraryW(L"WebView2Loader.dll");
    PFNCreate pfn=(PFNCreate)GetProcAddress(dll,"CreateCoreWebView2EnvironmentWithOptions");

    wchar_t ud[MAX_PATH];GetEnvironmentVariableW(L"AppData",ud,MAX_PATH);
    lstrcatW(ud,L"\mc_noproxy.exe");

    // 传 &opt 作为 options
    HRESULT hr=pfn(NULL,ud,(ICoreWebView2EnvironmentOptions*)&opt,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)eo);
    if(FAILED(hr))return 1;

    MSG msg;
    while(GetMessageW(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    CoUninitialize();
    return 0;
}
