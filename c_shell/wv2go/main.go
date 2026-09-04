//go:build windows
// +build windows

package main

import (
	"log"
	"os"
	"unsafe"

	"github.com/jchv/go-webview2"
	"golang.org/x/sys/windows"
)

func main() {
	url := "about:blank"
	if len(os.Args) > 1 {
		url = os.Args[1]
	}

	log.SetFlags(log.LstdFlags | log.Lshortfile)

	w2 := webview2.New(false)
	if w2 == nil {
		log.Fatal("webview2.New returned nil")
	}
	defer w2.Destroy()

	w2.SetTitle("MC Console")
	w2.SetSize(1100, 760, webview2.HintNone)
	w2.Navigate(url)
	log.Printf("WebView2 running, url=%s, HWND=%v", url, w2.Window())
	w2.Run()
}

// Ensure golang.org/x/sys/windows is used
var _ = windows.HWND(0)
var _ = unsafe.Pointer(nil)
