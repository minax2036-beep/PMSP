# PMSP

**PSP-Min-Secure-Protocol** is an experimental HTTPS client library intended for PSP-1000. A POSIX prototype is used for early development and testing.

> PSPでHTTPS通信だぞぉぉぉぉぉ！！！

## PSP HTTPS PRX API

The repository contains a PSP PRX module and a tester application. GitHub Actions builds both PSP outputs. Pushing a version tag such as `v0.2.0` runs the builds and publishes `pmsp.prx` and `EBOOT.PBP` as GitHub Release assets.

### API v1.1 — standard PRX imports

The PMSP PRX exports its functions through a standard PSPSDK export library named `PMSP`. The Tester generates weak-import stubs from `psp/prx/exports.exp` with `psp-build-exports -k`, then links those stubs into its EBOOT.

After loading and starting `pmsp.prx`, the functions can be called directly:

- `pmspGetVersion()` — returns the API version.
- `pmspGetStatus()` — returns module status.
- `pmspGetLastError()` — returns the last PMSP error.
- `pmspHttpsGet(url, output_path)` — performs HTTPS GET and saves the response body.

Example:

```c
int result = pmspHttpsGet("https://example.com/", "ms0:/index.html");
```

The old `PMSPApi` function-pointer table has been removed. The weak-import stubs allow the Tester to start before the PMSP PRX is loaded; loading the PRX should resolve the matching NID imports.

### PSP HTTPS backend

The PSP backend uses the system `sceHttp` / `sceHttps` libraries and loads the system default certificate set. Only `https://` URLs are accepted. Redirects are disabled for now to prevent an automatic downgrade to plain HTTP.

The calling Homebrew must initialize the PSP network stack and connect to Wi-Fi before calling `pmspHttpsGet`. Response data is streamed in 4 KiB blocks to a temporary file, then renamed after a successful 2xx response. The full response is not held in RAM.

This is an initial backend. A successful build does not prove real-device TLS interoperability or certificate/hostname validation behavior; those must be verified on hardware before production use. The PSP backend currently uses system HTTPS rather than the Mbed TLS 3.6.3 backend used by the POSIX prototype.

## PSP installation test

1. Download `pmsp.prx` and `EBOOT.PBP` from a GitHub Release.
2. Create `ms0:/PSP/GAME/PMSP/` on the Memory Stick.
3. Copy both `EBOOT.PBP` and `pmsp.prx` into that same folder.
4. Run **PMSP Tester** from the `PMSP` folder. It loads `pmsp.prx` and checks the standard NID-based imports.
5. A separate Homebrew should initialize Wi-Fi before calling `pmspHttpsGet()`.

## Automatic builds and releases

Normal pushes and pull requests build the POSIX client and PSP targets. A version tag triggers a GitHub Release after both build jobs succeed:

```sh
git tag v0.2.0
git push origin v0.2.0
```

Release assets: `pmsp.prx` and `EBOOT.PBP`.

Build locally with PSPDEV/PSPSDK:

```sh
make -C psp/prx
make -C psp/tester
```

## POSIX HTTPS prototype

- TCP: POSIX sockets.
- TLS: Mbed TLS 3.6.3.
- Verification: CA chain and hostname verification are required.
- HTTP: HTTP/1.1 GET, status/header parsing, Content-Length and chunked bodies.
- Redirects: up to five 301/302/303/307/308 redirects; HTTPS downgrade is rejected.
- Response: fixed-size streaming.
- Download: temporary file then rename.
- GitHub tag archives and latest Release metadata retrieval.

PMSP does not implement its own cryptographic algorithms. TLS security checks must remain enabled during future size optimization.

## Build on Linux

Requirements: CMake 3.20.2 or later, a C99 compiler, Git, and internet access for the first configure (CMake fetches Mbed TLS).

```sh
git clone https://github.com/minax2036-beep/PMSP.git
cd PMSP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

## HTTPS test

```sh
./build/pmsp-get example.com /
./build/pmsp-get example.com /index.html --output index.html
./build/pmsp-get raw.githubusercontent.com /minax2036-beep/PMSP/main/README.md --output README-downloaded.md
./build/pmsp-get github-tag minax2036-beep/PMSP v0.1.0 --output PMSP-v0.1.0.zip
./build/pmsp-get github-release minax2036-beep/PMSP --output latest-release.json
```

## Memory and security goals

- Executable/library footprint target: 15 MB or less.
- Runtime RAM target: well below 15 MB.
- Measure peak memory before optimizing.
- Never disable TLS certificate verification, hostname verification, or secure random generation to save memory.

See `docs/ROADMAP.md` for the development plan.
