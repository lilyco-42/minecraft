package("webview-capi")
    set_homepage("https://github.com/lilyco-42/webview-capi")
    set_description("C API for WebView2 - callable from any language (Python/Go/Rust/Node.js)")
    set_license("MIT")

    set_urls("https://github.com/lilyco-42/webview-capi/archive/refs/tags/$(version).tar.gz",
             "https://github.com/lilyco-42/webview-capi.git")

    add_versions("1.0.0", "main")

    add_syslinks("user32", "shell32", "ole32", "oleaut32", "shlwapi", "version")
    add_includedirs(".")

    on_install(function (package)
        os.cp("webview.h", package:installdir("include"))
        os.cp("webview.dll", package:installdir("bin"))
        os.cp("webview.lib", package:installdir("lib"))
    end)

    on_test(function (package)
        assert(package:has_cfuncs("webview_create", {includes = "webview.h"}))
    end)
package_end()
