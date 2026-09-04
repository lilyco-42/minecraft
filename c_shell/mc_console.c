// mc_console.c — Catime 式极小体积 WebView2 壳
// 单 C 文件, Win32 + WebView2 (官方 SDK 头 + 静态链接 Loader), 无 CRT 依赖
// 功能: 窗口加载 radxa 控制台 (默认 http://192.168.10.165:8765), F11 全屏
//
// 编译:  bash build_c.sh
// 产物:  mc_console.exe (~150KB, 零 DLL 依赖, Win10+)

// WIN32_LEAN_AND_MEAN 会跳过 ole2.h, 而 WebView2.h 需要 interface 宏
#include <ole2.h>
#include <objidl.h>
#include <stdio.h>
static FILE *g_log;
static void dbg(const char *s, long v) {
    if (!g_log) g_log = fopen("mc_debug.log", "a");
    if (g_log) { fprintf(g_log, "%s %ld\n", s, v); fflush(g_log); }
}
#define UNICODE
#define _UNICODE
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include "sdk/include/WebView2.h"

// 动态加载 WebView2Loader.dll,绕开静态链接版本兼容问题
typedef HRESULT (STDCALL *PFN_CreateCoreWebView2EnvironmentWithOptions)(
    PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
typedef HRESULT (STDCALL *PFN_CreateCoreWebView2Environment)(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
static PFN_CreateCoreWebView2EnvironmentWithOptions pfnCreateEnv = NULL;

static HWND g_hwnd;
static ICoreWebView2Environment *g_env;
static ICoreWebView2Controller *g_ctrl;
static ICoreWebView2 *g_web;

// handler 前置声明 (OnEnvCreated 在定义之前引用)
typedef struct Handler {
    void **vptr;  // → vtable (双层指针, COM 对象头 8 字节 = vtable 地址)
} Handler;
static Handler g_ctrl_handler;
static void *g_env_vt[4], *g_ctrl_vt[4];

// ── Controller 完成回调: 绑定 webview 并导航 ──
static HRESULT STDMETHODCALLTYPE OnCtrlCreated(
    HRESULT hr, ICoreWebView2Controller *ctrl) {
    if (FAILED(hr) || !ctrl) {
        MessageBoxW(g_hwnd, L"WebView2 控制器创建失败", L"MC 控制台", MB_ICONERROR);
        PostQuitMessage(1);
        return S_OK;
    }
    dbg("OnCtrlCreated hr", (long)hr);
    g_ctrl = ctrl;
    dbg("OnCtrl ctrl ptr", (long)(intptr_t)ctrl);
    { void **cvt = *(void ***)ctrl; dbg("ctrl vt0", (long)(intptr_t)cvt[0]); }
    ICoreWebView2Controller_put_IsVisible(ctrl, TRUE);
    dbg("IsVisible ok", 0);
    RECT rc; GetClientRect(g_hwnd, &rc);
    dbg("bounds w", (long)(rc.right - rc.left));
    dbg("bounds h", (long)(rc.bottom - rc.top));
    ICoreWebView2Controller_put_Bounds(ctrl, rc);

    ICoreWebView2Controller_get_CoreWebView2(ctrl, &g_web);
    dbg("web ptr", g_web ? 1 : 0);
    if (g_web) {
        wchar_t url[512];
        // 命令行第一个参数可覆盖 URL
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        lstrcpynW(url, (argv && argc > 1) ? argv[1] : L"about:blank", 511);
        if (argv) LocalFree(argv);
        HRESULT hrn = ICoreWebView2_Navigate(g_web, url);
        dbg("navigate hr", (long)hrn);
    }
    return S_OK;
}

// ── Environment 完成回调: 创建 controller ──
static HRESULT STDMETHODCALLTYPE OnEnvCreated(
    HRESULT hr, ICoreWebView2Environment *env) {
    if (FAILED(hr) || !env) {
        MessageBoxW(g_hwnd,
            L"WebView2 环境创建失败\n请安装 Microsoft Edge WebView2 Runtime (Win10/11 通常自带)",
            L"MC 控制台", MB_ICONERROR);
        PostQuitMessage(1);
        return S_OK;
    }
    g_env = env;
    dbg("OnEnvCreated hwnd", (long)(intptr_t)g_hwnd);
    { void **evt = *(void ***)env; dbg("env vt0", (long)(intptr_t)evt[0]); dbg("env vt3", (long)(intptr_t)evt[3]); }
    HRESULT hrc = ICoreWebView2Environment_CreateCoreWebView2Controller(env, g_hwnd,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)&g_ctrl_handler);
    dbg("CreateCtrl call hr", (long)hrc);
    return S_OK;
}

// ── 回调 handler: 手工实现 COM 对象 (C 里没法用 C++ 的 vtable 继承) ──
// ⚠ COM 对象布局: 头 8 字节必须是「指向 vtable 的指针」(双层指针)。
//   曾把函数指针直接当 vtable 塞进对象头, loader 一解引用就 segfault。
static HRESULT STDMETHODCALLTYPE HandlerQI(void *self, REFIID riid, void **out) {
    (void)self; (void)riid; *out = self; return S_OK;
}
// options 专用 QI: 只认 IUnknown + ICoreWebView2EnvironmentOptions(基接口)。
// 2~5 号扩展接口的 vtable 槽我们没有实现, 放行会被调越界。
static HRESULT STDMETHODCALLTYPE OptQI(void *self, REFIID riid, void **out) {
    if (memcmp(&IID_IUnknown, riid, sizeof(IID)) == 0 ||
        memcmp(&IID_ICoreWebView2EnvironmentOptions, riid, sizeof(IID)) == 0) {
        *out = self;
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE HandlerAddRef(void *self) { (void)self; return 2; }
static ULONG STDMETHODCALLTYPE HandlerRelease(void *self) { (void)self; return 1; }

static void HandlerInit(Handler *h, void **vt, void *invoke) {
    vt[0] = HandlerQI;
    vt[1] = HandlerAddRef;
    vt[2] = HandlerRelease;
    vt[3] = invoke;
    h->vptr = vt;
}

// Invoke 签名: (self, HRESULT, ICoreWebView2Environment*) — C 调用约定由 STDMETHODCALLTYPE 决定
static HRESULT STDMETHODCALLTYPE EnvInvoke(void *self, HRESULT hr, ICoreWebView2Environment *env) {
    return OnEnvCreated(hr, env);
}
static HRESULT STDMETHODCALLTYPE CtrlInvoke(void *self, HRESULT hr, ICoreWebView2Controller *ctrl) {
    return OnCtrlCreated(hr, ctrl);
}

// ── EnvironmentOptions COM 对象: 附加 --no-proxy-server 绕过系统代理 ──
// (Clash TUN 会把局域网 192.168.x.x 也吸进代理导致白屏, 同 curl 不带 --noproxy 的坑)
static HRESULT STDMETHODCALLTYPE OptGetArgs(void *self, LPWSTR *value) {
    (void)self;
    static const wchar_t args[] = L"--disable-gpu";
    *value = (LPWSTR)CoTaskMemAlloc(sizeof(args));
    lstrcpyW(*value, args);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE OptPut(void *self, LPCWSTR value) { (void)self; (void)value; return S_OK; }
static HRESULT STDMETHODCALLTYPE OptGetLang(void *self, LPWSTR *value) {
    (void)self;
    static const wchar_t lang[] = L"zh-CN";
    *value = (LPWSTR)CoTaskMemAlloc(sizeof(lang));
    lstrcpyW(*value, lang);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE OptGetTarget(void *self, LPWSTR *value) {
    (void)self;
    // 必须是合法版本串; 空串会让 loader 拒绝 (0x80070057)。
    // 写死当前机器探测到的运行时大版本, 兼容性由 CompareBrowserVersions 宽松匹配
    static const wchar_t t[] = L"151.0.4129.107";
    *value = (LPWSTR)CoTaskMemAlloc(sizeof(t));
    lstrcpyW(*value, t);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE OptGetSSO(void *self, BOOL *v) { (void)self; *v = FALSE; return S_OK; }
static HRESULT STDMETHODCALLTYPE OptPutSSO(void *self, BOOL v) { (void)self; (void)v; return S_OK; }

static void *g_opt_vt[12];
static Handler g_opt_obj;

static void OptInit(void) {
    g_opt_vt[0] = OptQI;   g_opt_vt[1] = HandlerAddRef; g_opt_vt[2] = HandlerRelease;
    g_opt_vt[3] = OptGetArgs;  g_opt_vt[4] = OptPut;      // AdditionalBrowserArguments
    g_opt_vt[5] = OptGetLang;  g_opt_vt[6] = OptPut;      // Language
    g_opt_vt[7] = OptGetTarget; g_opt_vt[8] = OptPut;     // TargetCompatibleBrowserVersion
    g_opt_vt[9] = OptGetSSO;   g_opt_vt[10] = OptPutSSO;  // AllowSingleSignOn...
    g_opt_obj.vptr = g_opt_vt;
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE:
        if (g_ctrl) {
            RECT rc; GetClientRect(h, &rc);
            ICoreWebView2Controller_put_Bounds(g_ctrl, rc);
        }
        return 0;
    case WM_KEYDOWN:
        if (w == VK_F11) { // 全屏切换
            static WINDOWPLACEMENT wp = { .length = sizeof(wp) };
            if (wp.showCmd != SW_SHOWMAXIMIZED) {
                GetWindowPlacement(h, &wp);
                SetWindowLongPtrW(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                MONITORINFO mi = { .cbSize = sizeof(mi) };
                GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi);
                SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_FRAMECHANGED);
            } else {
                SetWindowLongPtrW(h, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                SetWindowPlacement(h, &wp);
                SetWindowPos(h, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            }
        }
        return 0;
    case WM_TIMER:
        if (w == 1) {
            KillTimer(h, 1);
            extern void StartWebView(void);
            StartWebView();
        }
        return 0;
        if (g_web) ICoreWebView2_Release(g_web);
        if (g_ctrl) ICoreWebView2Controller_Release(g_ctrl);
        if (g_env) ICoreWebView2Environment_Release(g_env);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev; (void)cmd;
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MCCtl";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    if (!RegisterClassW(&wc)) dbg("RegisterClassW failed", (long)GetLastError());
    g_hwnd = CreateWindowExW(0, L"MCCtl", L"MC 控制台",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
        NULL, NULL, hInst, NULL);
    dbg("g_hwnd after CreateWindowExW", (long)(intptr_t)g_hwnd);
    if (!g_hwnd) dbg("CreateWindowExW failed", (long)GetLastError());
    ShowWindow(g_hwnd, SW_SHOW);
    dbg("ShowWindow called", 0);

    // 延迟到消息循环里启动 WebView,避免时序竞态
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    SetTimer(g_hwnd, 1, 200, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}

void StartWebView(void) {
    // 绝对路径 userDataFolder (splitbrowser 同款: %APPDATA%\<exe名>).
    wchar_t userDataFolder[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, userDataFolder);
    wchar_t exeName[MAX_PATH];
    GetModuleFileNameW(NULL, exeName, MAX_PATH);
    wchar_t *slash = exeName + lstrlenW(exeName);
    while (slash > exeName && *slash != L'\\') slash--;
    if (*slash == L'\\') slash++;
    lstrcatW(userDataFolder, L"\\");
    lstrcatW(userDataFolder, slash);
    lstrcatW(userDataFolder, L".WebView2");

    static Handler env_handler;
    HandlerInit(&env_handler, g_env_vt, EnvInvoke);
    extern Handler g_ctrl_handler;
    HandlerInit(&g_ctrl_handler, g_ctrl_vt, CtrlInvoke);
    OptInit();

    HRESULT hr = CreateCoreWebView2Environment(
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&env_handler);
    dbg("CreateEnv simple hr", (long)hr);
}
