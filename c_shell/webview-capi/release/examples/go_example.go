package main
// #cgo LDFLAGS: -L. -lwebview
// #include "webview.h"
import "C"
import "unsafe"

func main() {
    w := C.webview_create(0, nil)
    title := C.CString("Hello from Go")
    C.webview_set_title(w, title)
    C.webview_set_size(w, 800, 600, 0)
    html := C.CString("<h1>Go + WebView2!</h1>")
    C.webview_set_html(w, html)
    C.webview_run(w)
    C.webview_destroy(w)
    C.free(unsafe.Pointer(title))
    C.free(unsafe.Pointer(html))
}
