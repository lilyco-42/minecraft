#include <stdio.h>
#include <windows.h>

typedef void* (*fn_create)(int, void*);
typedef int (*fn_run)(void*);
typedef void (*fn_navigate)(void*, const char*);
typedef void (*fn_set_title)(void*, const char*);
typedef void (*fn_set_size)(void*, int, int, int);
typedef const char* (*fn_version)(void);

int main() {
    HMODULE dll = LoadLibraryW(L"webview.dll");
    if(!dll){printf("DLL load failed %lu\n",GetLastError());return 1;}
    
    fn_create create = (fn_create)GetProcAddress(dll, "webview_create");
    fn_run run = (fn_run)GetProcAddress(dll, "webview_run");
    fn_navigate nav = (fn_navigate)GetProcAddress(dll, "webview_navigate");
    fn_set_title title = (fn_set_title)GetProcAddress(dll, "webview_set_title");
    fn_set_size size = (fn_set_size)GetProcAddress(dll, "webview_set_size");
    fn_version ver = (fn_version)GetProcAddress(dll, "webview_version");
    
    printf("webview_version: %s\n", ver ? ver() : "NULL");
    printf("create=%p run=%p nav=%p title=%p size=%p\n",create,run,nav,title,size);
    
    if(create && run && nav && title && size) {
        void* w = create(0, NULL);
        printf("webview created w=%p\n",w);
        title(w, "DLL Test");
        size(w, 800, 600, 0);
        nav(w, "https://example.com");
        printf("running...\n");
        run(w);
    }
    return 0;
}
