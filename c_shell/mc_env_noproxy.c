#include "webview/webview.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hr, PWSTR cmd, int show) {
    // 关键:在 webview_create 之前设置环境变量
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--no-proxy-server --disable-gpu");
    
    webview_t w = webview_create(0, NULL);
    webview_set_title(w, "MC NoProxy");
    webview_set_size(w, 1100, 760, WEBVIEW_HINT_NONE);
    webview_navigate(w, "http://192.168.10.165:8765");
    webview_run(w);
    webview_destroy(w);
    return 0;
}
