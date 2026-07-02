package main

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"sync"
)

var (
	mu          sync.Mutex
	lastMessage = "First Message!"
)

func main() {
	port := "8080"
	if len(os.Args) >= 2 {
		p, err := strconv.Atoi(os.Args[1])
		if err == nil && p > 0 && p <= 65535 {
			port = os.Args[1]
		}
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /", indexHandler)
	mux.HandleFunc("GET /ping", pingHandler)
	mux.HandleFunc("POST /echo", echoHandler)
	mux.HandleFunc("GET /message", getMessageHandler)
	mux.HandleFunc("POST /message", postMessageHandler)

	addr := "0.0.0.0:" + port
	fmt.Fprintf(os.Stderr, "Go test server listening on %s\n", addr)
	if err := http.ListenAndServe(addr, mux); err != nil {
		fmt.Fprintf(os.Stderr, "ListenAndServe: %v\n", err)
		os.Exit(1)
	}
}

func indexHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	fmt.Fprint(w, "ExpressC test server\nGET /ping\nPOST /echo\n")
}

func pingHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	fmt.Fprint(w, "pong\n")
}

func echoHandler(w http.ResponseWriter, r *http.Request) {
	body, err := io.ReadAll(r.Body)
	if err != nil {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	ct := r.Header.Get("Content-Type")
	if ct == "" {
		ct = "application/octet-stream"
	}
	w.Header().Set("Content-Type", ct)
	w.Write(body)
}

func getMessageHandler(w http.ResponseWriter, r *http.Request) {
	mu.Lock()
	msg := lastMessage
	mu.Unlock()
	fmt.Fprint(w, msg)
}

func postMessageHandler(w http.ResponseWriter, r *http.Request) {
	body, err := io.ReadAll(r.Body)
	if err != nil {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	if len(body) > 255 {
		w.WriteHeader(http.StatusBadRequest)
		return
	}
	mu.Lock()
	lastMessage = string(body)
	mu.Unlock()
}
