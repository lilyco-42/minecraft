import ctypes
import sys

lib = ctypes.CDLL('./webview.dll')
lib.webview_create.restype = ctypes.c_void_p
lib.webview_create.argtypes = [ctypes.c_int]
lib.webview_set_title.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.webview_set_size.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
lib.webview_navigate.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.webview_set_html.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.webview_run.argtypes = [ctypes.c_void_p]
lib.webview_destroy.argtypes = [ctypes.c_void_p]

w = lib.webview_create(0)
lib.webview_set_title(w, b"Hello from Python")
lib.webview_set_size(w, 800, 600, 0)
lib.webview_set_html(w, b"<html><body style='background:lime'><h1>Python + WebView2!</h1></body></html>")
lib.webview_run(w)
lib.webview_destroy(w)
