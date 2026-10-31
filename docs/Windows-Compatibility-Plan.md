# Windows compatibility

## Supported build target

- Visual Studio 2015 Update 3, MSVC v140, x64.
- Windows 8.1 SDK with `WINVER=0x0601` and `_WIN32_WINNT=0x0601`.
- Windows 7 SP1 is the minimum intended OS. Windows 8/8.1/10/11 are also intended targets.
- The interpreter core is C++14-compatible and uses `Compat14.h` for optional values, tagged value storage, and filesystem operations. The Linux CMake build is also kept at C++14.

## WebView2 policy

WebView2 is optional. Use a WebView2 SDK no newer than 1.0.1518.46 when a single legacy-compatible build is required; SDK 1.0.1519.0 and later dropped Windows 7/8/8.1 support.

- Windows 10/11: pass no browser executable folder, allowing WebView2Loader to use the installed Evergreen Runtime.
- Windows 7 SP1/8/8.1: Runtime 109 is the final supported line. If `NIMPLE_WEBVIEW2_FIXED_RUNTIME` points to a Fixed Version Runtime folder, that folder is selected. Otherwise the app checks for `WebView2Runtime109` beside the executable. If neither exists, WebView2Loader falls back to the machine-installed Runtime 109.
- Runtime 109 is end-of-support and receives no security updates. Do not use it for untrusted content.

The Fixed Version Runtime is not bundled by default because of its substantial size. If distributing it, unpack the runtime so `msedgewebview2.exe` is at the root of `WebView2Runtime109` (or the configured directory).

## Build

Open `NimpleBasic.sln` in Visual Studio 2015 Update 3, select Release|x64, and build. The project uses v140 and Windows 8.1 SDK. WebView2 is disabled unless `WebView2SDKDir` is defined; when enabled, point it at the SDK's `build\native` directory and use SDK 1.0.1518.46 or an earlier compatible release.

CMake builds use C++14. For Windows builds with WebView2 enabled, set `NIMPLE_ENABLE_WEBVIEW2=ON` and `WEBVIEW2_SDK_DIR` to the SDK native directory.

## Validation status

The C++14 build and 47 CTest cases were executed in a Linux environment. The Visual Studio 2015 compiler and Windows 7/8/8.1 runtime environments were not available in the build container; those platform checks remain to be performed on the target systems. The native Win32 GUI and WebView2 initialization require Windows smoke tests.

## Windows 7 deployment prerequisite

For Windows 7 SP1, install the Visual C++ 2015 runtime/UCRT servicing prerequisite (including KB2999226 where it is not already present). The project uses the v140 toolset; test the final deployment on a clean Windows 7 SP1 VM rather than relying on the developer machine's installed runtimes.
