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
- HTTP: basic HTTP/1.1 GET
- Response: parses status and headers; streams the response body in fixed-size chunks; decodes chunked transfer encoding
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

## First HTTPS test

    ./build/pmsp-get example.com /

The program prints only the decoded HTTP response body to standard output. HTTP status and diagnostics are written to standard error.

GitHub RAW example:

    ./build/pmsp-get raw.githubusercontent.com /minax2036-beep/PMSP/main/README.md

Optional arguments:

    pmsp-get <host> <path> [port] [ca-file]

The default CA bundle is /etc/ssl/certs/ca-certificates.crt. On other systems,
pass the correct CA bundle path as the fourth argument.

## Current limitations

This is an early prototype, not yet a complete HTTP client. It does not yet follow redirects or save downloaded files. It currently supports Content-Length, chunked transfer encoding, and close-delimited response bodies.

## Memory goals

- Executable/library footprint target: 15 MB or less.
- Runtime RAM target: well below 15 MB.
- Measure peak memory before optimizing.
- Never disable TLS certificate verification to save memory.

See docs/ROADMAP.md for the development plan.
