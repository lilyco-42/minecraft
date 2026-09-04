fn main() {
    // Tell cargo to look for shared libraries in the specified directory
    println!("cargo:rustc-link-search=native=.");
    // Tell cargo to tell rustc to link the system webview shared library.
    println!("cargo:rustc-link-lib=dylib=webview-capi");
}
