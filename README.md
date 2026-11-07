# PMSP

**PSP-Min-Secure-Protocol** is an experimental C HTTPS client library intended
for PSP-1000. A POSIX prototype is used for early development and testing.

> PSPでHTTPS通信だぞぉぉぉぉぉ！！！

## Current milestone: PSP PRX bootstrap

The repository now contains an initial PSP PRX module and a small tester
application:

- `psp/prx/pmsp.prx`: user-mode PSP PRX module.
- `psp/tester/EBOOT.PBP`: Homebrew application that loads, starts, stops,
  and unloads the PRX from `ms0:/PMSP/pmsp.prx`.
- GitHub Actions builds both PSP outputs using the PSPDEV toolchain.
- Pushing a version tag such as `v0.2.0` runs the builds and publishes
  `pmsp.prx` and `EBOOT.PBP` as GitHub Release assets.

**Important:** this is currently a PRX lifecycle/build test only. The existing
HTTPS implementation still uses POSIX sockets and has not yet been ported to
the PSP networking API. The initial PRX does not provide the HTTPS API yet.
The next porting step is to add the PSP network transport and integrate
Mbed TLS, then expose a stable PRX API to other Homebrew applications.

## PMSP PRX API (v1.0)

The PSP PRX exports the `PMSP` library functions:

- `pmspGetVersion()` — returns the API version as major/minor in a 32-bit value.
- `pmspGetStatus()` — returns the current module status.
- `pmspGetLastError()` — returns the most recent PMSP error code.

The public C header is `psp/include/pmsp_api.h`.

For a dynamically loaded PRX, the application passes a `PMSPApi` structure
to `sceKernelStartModule()`. The PRX fills that structure with callable
function pointers during `module_start()`. This avoids requiring the
application's imports to be resolved before it has loaded the PRX.

The Tester demonstrates this startup API-table handshake and calls all
three functions. The exported NIDs are also declared in
`psp/prx/exports.exp` for applications that use normal PSPSDK PRX imports.

The current API is a bootstrap interface only. HTTPS operations will be
added after the PSP networking and TLS backends are ported.

## PSP installation test

1. Download `pmsp.prx` and `EBOOT.PBP` from a GitHub Release.
2. Copy `EBOOT.PBP` into a PSP game folder, for example
   `ms0:/PSP/GAME/PMSPTEST/EBOOT.PBP`.
3. Create `ms0:/PMSP/` and copy `pmsp.prx` into that folder.
4. Run **PMSP Tester** from the PSP XMB.
5. The tester attempts to load and start the PRX, then stops and unloads it
   when you press X or O.

## Automatic builds and releases

Normal pushes and pull requests build the POSIX client and PSP targets.
A version tag triggers a GitHub Release after both build jobs succeed:

```sh
git tag v0.2.0
git push origin v0.2.0
```

The release contains:

- `pmsp.prx` — PSP PRX module.
- `EBOOT.PBP` — PMSP Tester application.

The PSP build can also be run locally in an environment with PSPDEV/PSPSDK:

```sh
make -C psp/prx
make -C psp/tester
```

## POSIX HTTPS prototype

The first development target is Chromebook Linux:

- TCP: POSIX sockets.
- TLS: Mbed TLS 3.6.3.
- Verification: CA chain and hostname verification are required.
- HTTP: HTTP/1.1 GET, status/header parsing, Content-Length and chunked bodies.
- Redirects: follows up to five 301/302/303/307/308 redirects; HTTPS downgrade is rejected.
- Response: streams the response body in fixed-size chunks.
- Download: writes to a temporary file and renames it after a successful transfer.
- GitHub tag archives: downloads a ZIP source archive for a specified tag.
- GitHub Releases: retrieves latest Release metadata as raw JSON.

PMSP does not implement its own cryptographic algorithms. TLS security checks
must remain enabled during future size optimization.

## Build on Linux

Requirements: CMake 3.20.2 or later, a C99 compiler, Git, and internet access
for the first configure (CMake fetches Mbed TLS).

```sh
git clone https://github.com/minax2036-beep/PMSP.git
cd PMSP
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

## HTTPS test

Print the decoded response body to standard output:

```sh
./build/pmsp-get example.com /
```

Download a page to a file:

```sh
./build/pmsp-get example.com /index.html --output index.html
```

GitHub RAW example:

```sh
./build/pmsp-get raw.githubusercontent.com /minax2036-beep/PMSP/main/README.md --output README-downloaded.md
```

Download a ZIP source archive for a specific GitHub tag:

```sh
./build/pmsp-get github-tag minax2036-beep/PMSP v0.1.0 --output PMSP-v0.1.0.zip
```

Retrieve metadata for the latest published GitHub Release:

```sh
./build/pmsp-get github-release minax2036-beep/PMSP --output latest-release.json
```

## Memory and security goals

- Executable/library footprint target: 15 MB or less.
- Runtime RAM target: well below 15 MB.
- Measure peak memory before optimizing.
- Never disable TLS certificate verification, hostname verification, or
  secure random generation to save memory.

See `docs/ROADMAP.md` for the development plan.
