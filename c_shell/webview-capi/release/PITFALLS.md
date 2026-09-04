# webview-capi 技术避坑手册

## 问题 1: DLL 函数未导出 (C++ Name Mangling)

**现象**: `GetProcAddress` 返回 NULL,链接错误 LNK2019

**原因**: 直接编译 amalgamated webview.h 时,`WEBVIEW_API` 宏默认展开为 `static inline`(C++ 编译时),无法导出。

**错误信息**:
```
GetProcAddress(handle, "webview_create") == NULL
```

**解决方案**:
```bash
# 用 CMake 官方构建
cmake -DWEBVIEW_BUILD_SHARED_LIBRARY=ON ..
cmake --build . --target webview_core_shared

# 或手动:定义 WEBVIEW_BUILD_SHARED(不是 WEBVIEW_SHARED)
cl /DWEBVIEW_BUILD_SHARED /EHsc /c webview.cc
link /DLL /OUT:webview.dll webview.obj ...
```

**关键**:`WEBVIEW_BUILD_SHARED` 在 `webview.h` 的 `#include "macros.h"` 之前定义。

## 问题 2: Segfault 在 COM 调用

**现象**: 调用 `webview_create` 或 `ICoreWebView2Controller_put_IsVisible` 时崩溃

**原因**: `CoInitializeEx` 未调用或 COM handler vtable 布局错误。

**解决方案**:
```c
// 必须调用 CoInitializeEx
CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

// handler vtable 布局(QI/AddRef/Release/Invoke)
void *vt[4] = {QI, AddRef, Release, Invoke};
handler.vptr = vt;
```

**关键**: vtable 顺序必须是 QI→AddRef→Release→Invoke,不能乱。

## 问题 3: Browser 进程秒退

**现象**: 启动后 browser 进程立即退出,画面白屏

**原因**:
1. SDK 静态 loader 版本 (v1.0.1245) 与系统运行时 (v152) 不兼容
2. 窗口尺寸为 0x0
3. Clash TUN 代理阻断 WebView2 网络

**解决方案**:
```bash
# 方案 1: 使用 webview 库(内部动态加载正确 loader)
# 方案 2: 设置环境变量绕过代理
SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"--no-proxy-server");
# 方案 3: 确保窗口尺寸非零
webview_set_size(w, 1100, 760, 0);  // 不要 0x0
```

## 问题 4: 静态链接 vs 动态链接混淆

**现象**: LNK2005 重定义,或 LNK2019 未解析

**原因**: `WEBVIEW_API` 宏展开不一致:
- C++ 编译时默认 `static inline`(头文件内联)
- C 编译时默认 `extern`(外部链接)
- 定义 `WEBVIEW_BUILD_SHARED` 后变为 `__declspec(dllexport)`

**规则**:
```
编译类型      WEBVIEW_API 展开        效果
-----------  ----------------------  ----------------------
C++ 默认     static inline           头文件内联(静态链接)
C 默认       extern                  外部链接(静态库)
+ WEBVIEW_BUILD_SHARED               __declspec(dllexport)  DLL 导出
+ WEBVIEW_SHARED                     __declspec(dllimport)  DLL 导入
+ WEBVIEW_STATIC                     extern                 强制静态
```

## 问题 5: WebView2 Runtime 缺失

**现象**: `webview_create` 返回 NULL

**原因**: 精简版 Windows 未装 WebView2 Runtime

**解决方案**:
1. 安装 Edge WebView2 Runtime:https://developer.microsoft.com/en-us/microsoft-edge/webview2/
2. 或打包 Evergreen Bootstrapper 随应用分发

## 问题 6: Clash TUN 代理白屏

**现象**: WebView2 窗口显示但页面白屏

**原因**: Clash TUN 模式下 WebView2 的网络请求被阻断

**解决方案**:
```c
// 方案 1: 环境变量(推荐)
SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", 
    L"--no-proxy-server --disable-gpu");

// 方案 2: 关闭 Clash 后测试
// 方案 3: 使用 webview 库的 webview_navigate(已内置处理)
```

## 构建清单

### 构建 DLL(CMake,推荐)
```bash
git clone https://github.com/webview/webview.git
cd webview && mkdir build && cd build
cmake .. -DWEBVIEW_BUILD_SHARED_LIBRARY=ON
cmake --build . --config Release --target webview_core_shared
# 产物: build/core/Release/webview.dll
```

### 构建静态库
```bash
cmake .. -DWEBVIEW_BUILD_SHARED_LIBRARY=OFF
cmake --build . --config Release --target webview_core_static
# 产物: build/core/Release/webview_static.lib
```

### 构建 x64 DLL(MSVC 命令行)
```bash
call vcvars64.bat
cd webview
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DWEBVIEW_BUILD_SHARED_LIBRARY=ON
cmake --build . --config Release
```
