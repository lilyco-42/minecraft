const ffi = require('ffi-napi');
const lib = ffi.Library('./webview.dll', {
  'webview_create': ['pointer', ['int']],
  'webview_set_title': ['void', ['pointer', 'string']],
  'webview_set_size': ['void', ['pointer', 'int', 'int', 'int']],
  'webview_set_html': ['void', ['pointer', 'string']],
  'webview_run': ['void', ['pointer']],
  'webview_destroy': ['void', ['pointer']]
});
const w = lib.webview_create(0);
lib.webview_set_title(w, "Hello from Node.js");
lib.webview_set_size(w, 800, 600, 0);
lib.webview_set_html(w, "<h1>Node.js + WebView2!</h1>");
lib.webview_run(w);
lib.webview_destroy(w);
