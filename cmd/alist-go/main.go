// alist-go is an experimental Go migration binary, not a production server.
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"github.com/MoLik-Labs/ALIST/internal/goserver"
	"github.com/MoLik-Labs/ALIST/internal/localfs"
)

func usage(out io.Writer) {
	fmt.Fprintln(out, "Kova Go migration "+goserver.Version+" (incomplete; not for production)")
	fmt.Fprintln(out, "Usage: alist-go version | server --experimental [--addr 127.0.0.1] [--port 5244] [--root directory]")
	fmt.Fprintln(out, "A local root requires the KOVA_GO_TOKEN environment variable; use a dedicated test directory.")
}

func portFromEnvironment() (int, error) {
	value := os.Getenv("ALIST_HTTP_PORT")
	if value == "" {
		value = os.Getenv("PORT")
	}
	if value == "" {
		return 5244, nil
	}
	port, err := strconv.Atoi(value)
	if err != nil || port < 1 || port > 65535 {
		return 0, errors.New("invalid ALIST_HTTP_PORT or PORT")
	}
	return port, nil
}

// boolFromEnvironment accepts only explicit lowercase booleans; misspelled
// cloud settings must never silently grant (or appear to grant) permissions.
func boolFromEnvironment(name string) (bool, error) {
	switch os.Getenv(name) {
	case "", "false":
		return false, nil
	case "true":
		return true, nil
	default:
		return false, fmt.Errorf("%s must be true or false", name)
	}
}

func run(args []string, out, errOut io.Writer) int {
	if len(args) == 0 {
		usage(errOut)
		return 2
	}
	switch args[0] {
	case "version", "-v", "--version":
		fmt.Fprintln(out, "Kova "+goserver.Version+" — experimental Go migration (incomplete)")
		return 0
	case "help", "-h", "--help":
		usage(out)
		return 0
	case "server":
		defaultPort, err := portFromEnvironment()
		if err != nil {
			fmt.Fprintln(errOut, err)
			return 2
		}
		var values [5]bool
		for i, name := range []string{"KOVA_GO_ALLOW_WRITE", "KOVA_GO_ALLOW_DELETE", "KOVA_GO_ALLOW_MOVE", "KOVA_GO_ENABLE_WEBDAV", "KOVA_GO_ENABLE_SHARES"} {
			values[i], err = boolFromEnvironment(name)
			if err != nil {
				fmt.Fprintln(errOut, err)
				return 2
			}
		}
		fs := flag.NewFlagSet("server", flag.ContinueOnError)
		fs.SetOutput(errOut)
		experimental := fs.Bool("experimental", false, "acknowledge this server is incomplete")
		addr := fs.String("addr", "127.0.0.1", "bind address")
		port := fs.Int("port", defaultPort, "HTTP port")
		root := fs.String("root", os.Getenv("KOVA_GO_ROOT"), "optional local root directory")
		allowWrite := fs.Bool("allow-write", values[0], "enable create-only uploads to the configured root")
		allowDelete := fs.Bool("allow-delete", values[1], "enable deletion of files and empty directories")
		allowMove := fs.Bool("allow-move", values[2], "enable non-overwriting local API and WebDAV moves")
		enableWebDAV := fs.Bool("enable-webdav", values[3], "enable experimental WebDAV; writes require --allow-write")
		enableShares := fs.Bool("enable-shares", values[4], "enable experimental temporary file shares")
		if err := fs.Parse(args[1:]); err != nil {
			if errors.Is(err, flag.ErrHelp) {
				return 0
			}
			return 2
		}
		if fs.NArg() != 0 || !*experimental || *addr == "" || *port < 1 || *port > 65535 {
			fmt.Fprintln(errOut, "The incomplete Go server requires --experimental, a bind address and a valid port; it does not implement Kova APIs.")
			return 2
		}
		if (*allowWrite || *allowDelete || *allowMove || *enableWebDAV || *enableShares) && *root == "" {
			fmt.Fprintln(errOut, "--allow-write, --allow-delete, --allow-move, --enable-webdav or --enable-shares requires --root or KOVA_GO_ROOT")
			return 2
		}
		server := goserver.New(net.JoinHostPort(*addr, strconv.Itoa(*port)))
		if *root != "" {
			store, err := localfs.Open(*root)
			if err != nil {
				fmt.Fprintln(errOut, "local root:", err)
				return 1
			}
			defer store.Close()
			if err := goserver.ConfigureLocalWithFeatures(server, store, os.Getenv("KOVA_GO_TOKEN"), *allowWrite, *allowDelete, *allowMove, *enableShares); err != nil {
				fmt.Fprintln(errOut, "local API:", err)
				return 2
			}
			if *enableWebDAV {
				handler, err := goserver.WebDAVHandlerWithMove(store, os.Getenv("KOVA_GO_TOKEN"), server.Handler, *allowWrite, *allowDelete, *allowMove)
				if err != nil {
					fmt.Fprintln(errOut, "WebDAV:", err)
					return 2
				}
				server.Handler = handler
			}
		}
		listener, err := net.Listen("tcp", server.Addr)
		if err != nil {
			fmt.Fprintln(errOut, "listen:", err)
			return 1
		}
		ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
		defer stop()
		result := make(chan error, 1)
		go func() { result <- server.Serve(listener) }()
		fmt.Fprintf(errOut, "EXPERIMENTAL: Go server is incomplete; local API: %t, creates: %t, deletes: %t, moves: %t, WebDAV: %t, temporary shares: %t. Listening on %s\n", *root != "", *allowWrite, *allowDelete, *allowMove, *enableWebDAV, *enableShares, listener.Addr())
		select {
		case err := <-result:
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Fprintln(errOut, "server:", err)
				return 1
			}
		case <-ctx.Done():
			shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()
			if err := server.Shutdown(shutdownCtx); err != nil {
				_ = server.Close()
				fmt.Fprintln(errOut, "shutdown:", err)
				return 1
			}
			if err := <-result; err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Fprintln(errOut, "server:", err)
				return 1
			}
		}
		return 0
	default:
		usage(errOut)
		return 2
	}
}

func main() { os.Exit(run(os.Args[1:], os.Stdout, os.Stderr)) }
