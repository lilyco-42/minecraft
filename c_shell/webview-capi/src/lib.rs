//! webview-capi - C API for WebView2
//! 
//! # Example
//! ```c
//! #include "webview.h"
//! webview_t w = webview_create(0, NULL);
//! webview_run(w);
//! webview_destroy(w);
//! ```

use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_void};

/// Opaque webview handle
pub struct Webview {
    // Placeholder - actual implementation in C
}

extern "C" {
    fn webview_create(debug: c_int, window: *mut c_void) -> *mut Webview;
    fn webview_destroy(w: *mut Webview);
    fn webview_run(w: *mut Webview) -> i32;
    fn webview_terminate(w: *mut Webview) -> i32;
    fn webview_set_title(w: *mut Webview, title: *const c_char);
    fn webview_set_size(w: *mut Webview, width: c_int, height: c_int, hints: c_int);
    fn webview_navigate(w: *mut Webview, url: *const c_char);
    fn webview_set_html(w: *mut Webview, html: *const c_char);
    fn webview_eval(w: *mut Webview, js: *const c_char) -> i32;
}

/// Safe wrapper around webview C API
pub struct WebView {
    ptr: *mut Webview,
}

impl WebView {
    /// Create a new webview instance
    pub fn new(debug: bool) -> Option<Self> {
        let ptr = unsafe { webview_create(debug as i32, std::ptr::null_mut()) };
        if ptr.is_null() {
            None
        } else {
            Some(Self { ptr })
        }
    }

    /// Set window title
    pub fn set_title(&self, title: &str) {
        let c_title = std::ffi::CString::new(title).unwrap();
        unsafe { webview_set_title(self.ptr, c_title.as_ptr()) };
    }

    /// Set window size
    pub fn set_size(&self, width: i32, height: i32, hints: i32) {
        unsafe { webview_set_size(self.ptr, width, height, hints) };
    }

    /// Navigate to URL
    pub fn navigate(&self, url: &str) {
        let c_url = std::ffi::CString::new(url).unwrap();
        unsafe { webview_navigate(self.ptr, c_url.as_ptr()) };
    }

    /// Set HTML content
    pub fn set_html(&self, html: &str) {
        let c_html = std::ffi::CString::new(html).unwrap();
        unsafe { webview_set_html(self.ptr, c_html.as_ptr()) };
    }

    /// Evaluate JavaScript
    pub fn eval(&self, js: &str) -> i32 {
        let c_js = std::ffi::CString::new(js).unwrap();
        unsafe { webview_eval(self.ptr, c_js.as_ptr()) }
    }

    /// Run the main loop
    pub fn run(&self) -> i32 {
        unsafe { webview_run(self.ptr) }
    }

    /// Terminate the main loop
    pub fn terminate(&self) -> i32 {
        unsafe { webview_terminate(self.ptr) }
    }
}

impl Drop for WebView {
    fn drop(&mut self) {
        unsafe { webview_destroy(self.ptr) };
    }
}

// Re-export C types for FFI
pub use std::ffi::CString;
pub use std::os::raw::{c_char, c_int, c_void};
