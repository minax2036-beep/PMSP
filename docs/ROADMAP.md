# PMSP Roadmap

## Goal

PMSP (PSP-Min-Secure-Protocol) is a C HTTPS communication library intended
for PSP-1000, with a POSIX prototype for early development.

Initial size goals:
- Executable/library footprint: target at or below 15 MB.
- Runtime RAM: substantially below 15 MB; measure peak usage.
- Never disable TLS certificate or hostname verification to save memory.

## Phase 1: POSIX HTTPS prototype

- [x] CMake project structure.
- [x] POSIX TCP transport.
- [x] Mbed TLS client setup.
- [x] Required CA-chain and hostname verification.
- [x] Basic HTTP/1.1 GET.
- [x] Stream response bytes to a fixed-size buffer callback.
- [ ] Verify GitHub Actions build.
- [x] Test HTTPS GET from Chromebook Linux.

## Phase 2: HTTP and GitHub

- [x] Parse HTTP status and headers.
- [x] Support Content-Length and chunked transfer encoding.
- [ ] Follow redirects with a safe redirect limit.
- [ ] Stream response bodies to files in chunks.
- [ ] GitHub RAW support.
- [ ] GitHub Releases and Release Assets support.

## Phase 3: Portability

- [ ] Separate platform transport interface.
- [ ] Raspberry Pi OS ARM64 build and test.
- [ ] PSP/MIPS transport and platform integration.
- [ ] PSP-1000 / ARK-5 test.

## Phase 4: Optimization

- [ ] Measure executable and library size.
- [ ] Measure peak RAM during DNS, TLS handshake, and transfer.
- [ ] Tune TLS and I/O buffers using measurements.
- [ ] Disable only unneeded cryptographic features.
- [ ] Keep certificate verification and secure random generation enabled.
- [ ] Re-test interoperability after optimization.

## Current limitations

The first version is POSIX-only. It prints the raw HTTP response, including
headers. Redirects, HTTP parsing, chunked decoding, and file downloads are
not implemented yet. The default system CA bundle path is
/etc/ssl/certs/ca-certificates.crt.
