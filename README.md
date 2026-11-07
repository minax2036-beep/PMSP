# PMSP

**PSP-Min-Secure-Protocol** is an experimental C HTTPS client library for
PSP-1000. A POSIX prototype is used for early development and testing.

> PSPでHTTPS通信だぞぉぉぉぉぉ！！！

## Current milestone: Phase 1

The first goal is to make an HTTPS GET request from Chromebook Linux using
PMSP's own TLS and HTTP code.

- TCP: POSIX sockets (initial development platform)
- TLS: Mbed TLS 3.6.3
- Verification: CA chain and hostname verification are required
- HTTP: HTTP/1.1 GET, status/header parsing, Content-Length and chunked bodies
- Redirects: follows up to five 301/302/303/307/308 redirects; HTTPS downgrade is rejected
- Response: parses status and headers; streams the response body in fixed-size chunks; decodes chunked transfer encoding
- Download: writes the decoded response body to a temporary file, then renames it after a successful transfer
- GitHub tag archives: downloads a ZIP source archive for a specified repository tag
- Build: CMake and GitHub Actions

PMSP does not implement its own cryptographic algorithms. TLS security checks
must remain enabled during future size optimization.

## Build on Linux

Requirements: CMake 3.20.2 or later, a C99 compiler, Git, and internet access
for the first configure (CMake fetches Mbed TLS).

    git clone https://github.com/minax2036-beep/PMSP.git
    cd PMSP
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel 2

## HTTPS test

Print the decoded response body to standard output:

    ./build/pmsp-get example.com /

Download a page to a file:

    ./build/pmsp-get example.com /index.html --output index.html

The program prints the response body to standard output when --output is not
specified. HTTP status and diagnostics are written to standard error.

GitHub RAW example:

    ./build/pmsp-get raw.githubusercontent.com /minax2036-beep/PMSP/main/README.md --output README-downloaded.md

Download a ZIP source archive for a specific GitHub tag:

    ./build/pmsp-get github-tag minax2036-beep/PMSP v0.1.0 --output PMSP-v0.1.0.zip

The command downloads GitHub's generated source ZIP for the exact tag name.
Tag characters outside the URL-unreserved set are percent-encoded. Non-2xx
HTTP responses, including an unknown tag (404), are treated as errors and the
temporary output file is discarded.

Optional arguments:

    pmsp-get <host> <path> [--output file] [--port port] [--ca-file file]

The older positional form is also supported:

    pmsp-get <host> <path> [port] [ca-file]

The default CA bundle is /etc/ssl/certs/ca-certificates.crt. On other systems,
pass the correct CA bundle path with --ca-file.

## Current limitations

This is an early prototype, not yet a complete HTTP client. It supports
absolute HTTPS and relative redirect locations, Content-Length, chunked
transfer encoding, and close-delimited response bodies. Redirects to plain
HTTP are rejected. The download command currently writes to a temporary file
next to the requested destination and renames it after a successful transfer.

## Memory goals

- Executable/library footprint target: 15 MB or less.
- Runtime RAM target: well below 15 MB.
- Measure peak memory before optimizing.
- Never disable TLS certificate verification to save memory.

See docs/ROADMAP.md for the development plan.
