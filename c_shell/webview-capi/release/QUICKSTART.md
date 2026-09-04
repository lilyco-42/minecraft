# webview-capi 快速入门

## 30 秒上手

### 1. 下载

从 [Releases](https://github.com/lilyco-42/webview-capi/releases) 下载 `webview-capi-v1.0.0-windows-x64.zip`

解压后得到:
```
webview.dll   (213 KB)
webview.lib   (5 KB)
webview.h     (211 KB, 单头文件)
```

### 2. 复制到项目

```
your-project/
├── webview.dll
├── webview.h
└── main.c
```

### 3. 写代码 (8 行)

```c
#include "webview.h"

int main() {
    webview_t w = webview_create(0, NULL);
    webview_set_title(w, "My App");
    webview_set_size(w, 800, 600, 0);
    webview_navigate(w, "https://example.com");
    webview_run(w);
    webview_destroy(w);
    return 0;
}
```

### 4. 编译

```bash
# MSVC
cl /O2 /MT main.c /link /SUBSYSTEM:WINDOWS webview.lib

# MinGW
gcc -O2 main.c -L. -lwebview -mwindows -o main.exe

# xmake
# xmake.lua:
#   add_links("webview")
#   add_linkdirs(".")
```

## 语言绑定

### Python (ctypes)
```python
import ctypes
lib = ctypes.CDLL('./webview.dll')
lib.webview_create.restype = ctypes.c_void_p
w = lib.webview_create(0)
lib.webview_set_title(w, b"Hello")
lib.webview_navigate(w, b"https://example.com")
lib.webview_run(w)
```

### Go (cgo)
```go
// #cgo LDFLAGS: -L. -lwebview
// #include "webview.h"
import "C"
w := C.webview_create(0, nil)
C.webview_run(w)
```

### Rust
```rust
#[link(name = "webview")]
extern "C" { fn webview_run(w: *mut c_void); }
```

### Node.js (ffi-napi)
```js
const ffi = require('ffi-napi');
const lib = ffi.Library('./webview.dll', {
  'webview_run': ['void', ['pointer']]
});
```

## 依赖

- Windows 10+ (WebView2 运行时自带)
- 无需额外安装
