# webview-capi

C API for WebView2 - callable from any language.

[![Release](https://img.shields.io/github/v/release/lilyco-42/webview-capi)](https://github.com/lilyco-42/webview-capi/releases)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

## Quick Start (30 seconds)

1. Download `webview-capi-v1.0.0-windows-x64.zip` from [Releases](https://github.com/lilyco-42/webview-capi/releases)
2. Extract to your project
3. Write 8 lines of code:

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

4. Compile: `cl /O2 /MT main.c /link /SUBSYSTEM:WINDOWS webview.lib`

## Language Bindings

| Language | Method | Example |
|----------|--------|---------|
| **C/C++** | Direct | `#include "webview.h"` |
| **Python** | ctypes | `ctypes.CDLL('./webview.dll')` |
| **Go** | cgo | `// #cgo LDFLAGS: -lwebview` |
| **Rust** | FFI | `#[link(name = "webview")]` |
| **Node.js** | ffi-napi | `ffi.Library('./webview.dll', {...})` |

See [examples/](examples/) for complete code.

## Requirements

- Windows 10+ (WebView2 Runtime included)
- No additional dependencies

## Documentation

- [QUICKSTART.md](release/QUICKSTART.md) - 30-second quick start
- [PITFALLS.md](release/PITFALLS.md) - Technical pitfall guide for AI

## Build from Source

```bash
git clone https://github.com/webview/webview.git
cd webview && mkdir build && cd build
cmake .. -DWEBVIEW_BUILD_SHARED_LIBRARY=ON
cmake --build . --config Release --target webview_core_shared
```

## License

MIT
