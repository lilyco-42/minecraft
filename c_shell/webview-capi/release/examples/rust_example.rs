use std::ffi::CString;
#[link(name = "webview")]
extern "C" {
    fn webview_create(debug: i32, window: *mut std::ffi::c_void) -> *mut std::ffi::c_void;
    fn webview_destroy(w: *mut std::ffi::c_void);
    fn webview_set_title(w: *mut std::ffi::c_void, title: *const i8);
    fn webview_set_size(w: *mut std::ffi::c_void, width: i32, height: i32, hints: i32);
    fn webview_set_html(w: *mut std::ffi::c_void, html: *const i8);
    fn webview_run(w: *mut std::ffi::c_void);
}
fn main() {
    unsafe {
        let w = webview_create(0, std::ptr::null_mut());
        let t = CString::new("Hello from Rust").unwrap();
        webview_set_title(w, t.as_ptr());
        webview_set_size(w, 800, 600, 0);
        let h = CString::new("<h1>Rust + WebView2!</h1>").unwrap();
        webview_set_html(w, h.as_ptr());
        webview_run(w);
        webview_destroy(w);
    }
}
