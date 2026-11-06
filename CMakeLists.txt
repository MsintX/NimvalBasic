cmake_minimum_required(VERSION 3.20)
project(NimpleBasic VERSION 0.1.0 LANGUAGES CXX)

option(NIMPLE_ENABLE_WEBVIEW2 "Enable the optional Microsoft WebView2 control on Windows" OFF)
set(WEBVIEW2_SDK_DIR "$ENV{WEBVIEW2_SDK_DIR}" CACHE PATH "Path to the WebView2 SDK build/native directory")

add_executable(nimple-basic NimpleBasic.cpp)
target_compile_features(nimple-basic PRIVATE cxx_std_14)

# Each platform uses its own system UI stack. Windows and macOS need only OS APIs.
# Linux dynamically loads the desktop's GTK runtime: no GTK headers or link-time
# GTK development package is required, but GTK's runtime must be present.
if(WIN32)
    target_compile_definitions(nimple-basic PRIVATE NIMPLE_EZWINDOW_WIN32=1)
    target_link_libraries(nimple-basic PRIVATE user32 gdi32 comctl32)
    if(NIMPLE_ENABLE_WEBVIEW2)
        if(NOT EXISTS "${WEBVIEW2_SDK_DIR}/include/WebView2.h")
            message(FATAL_ERROR "NIMPLE_ENABLE_WEBVIEW2 is ON, but WEBVIEW2_SDK_DIR/include/WebView2.h was not found. Set WEBVIEW2_SDK_DIR to the WebView2 SDK build/native directory.")
        endif()
        if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
            set(_webview2_arch arm64)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_webview2_arch x64)
        else()
            set(_webview2_arch x86)
        endif()
        find_library(WEBVIEW2_LOADER_STATIC_LIBRARY NAMES WebView2LoaderStatic
            PATHS "${WEBVIEW2_SDK_DIR}/${_webview2_arch}" NO_DEFAULT_PATH)
        if(NOT WEBVIEW2_LOADER_STATIC_LIBRARY)
            message(FATAL_ERROR "WebView2LoaderStatic.lib was not found in ${WEBVIEW2_SDK_DIR}/${_webview2_arch}. Check the SDK directory and target architecture.")
        endif()
        target_compile_definitions(nimple-basic PRIVATE NIMPLE_ENABLE_WEBVIEW2=1)
        target_include_directories(nimple-basic PRIVATE "${WEBVIEW2_SDK_DIR}/include")
        target_link_libraries(nimple-basic PRIVATE "${WEBVIEW2_LOADER_STATIC_LIBRARY}" ole32 shlwapi)
        message(STATUS "Nimple EzWindow: WebView2 enabled for ${_webview2_arch}; Windows 8.1 is not supported by WebView2 Runtime")
    else()
        message(STATUS "Nimple EzWindow: Win32 native backend enabled; WebView2 is optional and disabled")
    endif()
    message(STATUS "Nimple EzWindow: Windows visual styles are enabled")
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(nimple-basic PRIVATE EzWindowMac.mm)
    target_link_libraries(nimple-basic PRIVATE "-framework Cocoa")
    set_property(TARGET nimple-basic PROPERTY OBJCXX_STANDARD 14)
    set_property(TARGET nimple-basic PROPERTY OBJCXX_STANDARD_REQUIRED ON)
    message(STATUS "Nimple EzWindow: native Cocoa/AppKit backend enabled")
elseif(UNIX)
    target_sources(nimple-basic PRIVATE EzWindowLinux.cpp)
    target_link_libraries(nimple-basic PRIVATE ${CMAKE_DL_LIBS})
    message(STATUS "Nimple EzWindow: GTK native-widget backend enabled via runtime loading (no GTK development dependency)")
else()
    message(FATAL_ERROR "EzWindow has no backend for this operating system")
endif()

if(MSVC AND NOT MSVC_VERSION LESS 1900)
    set_property(TARGET nimple-basic PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set_target_properties(nimple-basic PROPERTIES
    OUTPUT_NAME "nb"
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
# Standard library modules are plain text files beside the executable.
# Copy them into the same output directory as nimple-basic.exe/nimple-basic on every build.
add_custom_command(TARGET nimple-basic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Standard library"
            "$<TARGET_FILE_DIR:nimple-basic>/Standard library"
    COMMENT "Copying Nimple Basic standard library beside the interpreter"
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(nimple-basic PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()
if(MSVC)
    target_compile_options(nimple-basic PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(nimple-basic PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME typecheck-valid
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-valid.nb")
    add_test(NAME ezwindow-example-typecheck
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/examples/EzWindowHello.nb")
    add_test(NAME ezwindow-module-from-standard-library-folder
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-builtin.nb")
    add_test(NAME ezwindow-relative-layout-api
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-relative-layout.nb")
    add_test(NAME ezwindow-relative-layout-example
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/examples/EzWindowRelativeLayout.nb")
    add_test(NAME ezwindow-extended-controls-api
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-extended-controls.nb")
    add_test(NAME ezwindow-extended-controls-example
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/examples/EzWindowExtendedControls.nb")
    add_test(NAME ezwindow-webview2-example-api
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/examples/EzWindowWebView2.nb")
    add_test(NAME ezwindow-layout-rejects-wrong-types
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-bad-layout-type.nb")
    add_test(NAME math-module-from-standard-library-folder
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-math-builtin.nb")
    add_test(NAME stdlib-input-statement-bundle
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/input-module.nb")
    add_test(NAME stdlib-output-statement-bundle
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/output-module.nb")
    add_test(NAME stdlib-qualified-output-requires-statement-definition
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/qualified-output-requires-definition.nb")
    set_tests_properties(stdlib-qualified-output-requires-statement-definition PROPERTIES WILL_FAIL TRUE)
    add_test(NAME undefined-statement-uses-generic-diagnostic
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/qualified-output-requires-definition.nb
            "-DEXPECTED=error NB2016: print 未定义。"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
    add_test(NAME stdlib-iosystem-statement-bundle
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/iosystem-module.nb")
    add_test(NAME stdlib-qualified-sys-prefix-still-works
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/modules-keep-sys-prefix.nb")
    add_test(NAME stdlib-input-definition-is-scoped
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/input-does-not-define-output.nb")
    set_tests_properties(stdlib-input-definition-is-scoped PROPERTIES WILL_FAIL TRUE)
    add_test(NAME stdlib-output-definition-is-scoped
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/output-does-not-define-input.nb")
    set_tests_properties(stdlib-output-definition-is-scoped PROPERTIES WILL_FAIL TRUE)
    add_test(NAME const-does-not-define-native-statement
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/const-is-not-definition.nb")
    set_tests_properties(const-does-not-define-native-statement PROPERTIES WILL_FAIL TRUE)
    add_test(NAME invalid-native-statement-definition
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/invalid-statement-definition.nb")
    set_tests_properties(invalid-native-statement-definition PROPERTIES WILL_FAIL TRUE)
    add_test(NAME unknown-module-diagnostic
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-module-not-found.nb")
    set_tests_properties(unknown-module-diagnostic PROPERTIES WILL_FAIL TRUE)
    add_test(NAME ezwindow-invalid-callback
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-bad-callback.nb")
    add_test(NAME standard-library-module-copy
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-copy.nb")
    add_test(NAME typecheck-bad-operator
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-operator.nb")
    add_test(NAME typecheck-uninitialized
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-uninitialized.nb")
    add_test(NAME typecheck-uninitialized-after-loop
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-uninitialized-after-loop.nb")
    add_test(NAME typecheck-bad-return-path
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-return-path.nb")
    add_test(NAME typecheck-bad-argument
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-argument.nb")
    add_test(NAME typecheck-bad-assignment
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-assignment.nb")
    add_test(NAME typecheck-bad-format
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-format.nb")
    add_test(NAME sys-prefix-required
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-missing-system.nb")
    add_test(NAME output-target-required
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-missing-output-target.nb")
    add_test(NAME old-system-open-rejected
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-old-system-open-rejected.nb")
    set_tests_properties(old-system-open-rejected PROPERTIES WILL_FAIL TRUE)
    add_test(NAME dollar-prefixed-directive-rejected
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-dollar-directive-rejected.nb")
    set_tests_properties(dollar-prefixed-directive-rejected PROPERTIES WILL_FAIL TRUE)
    add_test(NAME sys-specific-import
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-specific-import.nb")
    add_test(NAME sys-specific-import-is-scoped
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-specific-denies-read.nb")
    set_tests_properties(sys-specific-import-is-scoped PROPERTIES WILL_FAIL TRUE)
    add_test(NAME class-warning-suppression
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-class-suppressed.nb")
    add_test(NAME nbf-header-standalone-check
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/nbf/MathHeader.nbf")
    add_test(NAME module-qualified-call
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-module-qualified.nb")
    add_test(NAME module-internal-call
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-module-internal-call.nb")
    add_test(NAME module-out-of-line-definition
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-out-of-module-definition.nb")
    add_test(NAME global-scope-call
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-global-scope.nb")
    add_test(NAME global-scope-variable
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-global-variable-scope.nb")
    add_test(NAME sys-qualified-print
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-qualified.nb")
    add_test(NAME sys-namespace-import
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-import.nb")
    add_test(NAME sys-qualified-read
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-read.nb")
    add_test(NAME sys-pause-import
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-pause-import.nb")
    add_test(NAME sys-pause-qualified
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-sys-pause-qualified.nb")
    add_test(NAME nbf-opening-function
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-nbf-opening.nb")
    add_test(NAME nbf-empty-opening-function
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-nbf-empty-opening.nb")
    add_test(NAME nbf-copy-fallback
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-nbf-copy.nb")
    add_test(NAME deprecated-class-alias
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-class-deprecated.nb")
    add_test(NAME ezwindow-window-entry
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-window-entry.nb")
    add_test(NAME ezwindow-default-geometry
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-defaults.nb")
    add_test(NAME ezwindow-custom-geometry
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-custom-title.nb")
    add_test(NAME ezwindow-custom-icon-argument
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-icon-argument.nb")
    add_test(NAME nbt-tail-config-valid
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-tail-valid.nb")
    add_test(NAME nbt-tail-config-invalid
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-tail-invalid.nb")
    set_tests_properties(nbt-tail-config-invalid PROPERTIES WILL_FAIL TRUE)

    add_test(NAME old-introduce-keyword-rejected COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-old-introduce-rejected.nb")
    set_tests_properties(old-introduce-keyword-rejected PROPERTIES WILL_FAIL TRUE)
    add_test(NAME input-module-still-requires-sys-prefix COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdlib/input-requires-sys-prefix.nb")
    set_tests_properties(input-module-still-requires-sys-prefix PROPERTIES WILL_FAIL TRUE)
    add_test(NAME empty-source-check-is-clean COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-empty.nb")
    add_test(NAME comment-only-source-check-is-clean COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-comment-only.nb")
    add_test(NAME unified-syntax-diagnostic
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-diagnostic-syntax.nb
            "-DEXPECTED=error NB2001: 此处需要字面量、变量名或括号表达式。"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
    add_test(NAME unified-type-diagnostic
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-bad-operator.nb
            "-DEXPECTED=error NB-T1016: 运算符 - 需要数值操作数"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
    add_test(NAME unified-preprocessor-diagnostic
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-module-not-found.nb
            "-DEXPECTED=error NB3003: 标准库中不存在模块"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
    add_test(NAME unified-runtime-warning
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-diagnostic-runtime-warning.nb
            "-DEXPECTED=warning NB-W1041: 除数为零，结果为 Null。"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-runtime-diagnostic.cmake)

    set_tests_properties(
        typecheck-bad-operator
        typecheck-uninitialized
        typecheck-uninitialized-after-loop
        typecheck-bad-return-path
        typecheck-bad-argument
        typecheck-bad-assignment
        typecheck-bad-format
        ezwindow-invalid-callback
        ezwindow-layout-rejects-wrong-types
        sys-prefix-required
        output-target-required
        PROPERTIES WILL_FAIL TRUE)
endif()

if(BUILD_TESTING)
    add_test(NAME ezwindow-declarative-syntax
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-declarative.nb")
    add_test(NAME ezwindow-layout-units-and-explicit-size
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-layout-units-and-size.nb")
    add_test(NAME ezwindow-zero-size-is-rejected
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-zero-size-invalid.nb
            "-DEXPECTED=NB-EZ1002"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
    add_test(NAME ezwindow-missing-window-instance
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-missing-instance.nb
            "-DEXPECTED=EzWindow：未指定窗口。必须在 void Window 中声明"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
endif()

if(BUILD_TESTING)
    add_test(NAME const-quoted-relative-path-import
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-const-quoted-relative-path.nb")
    add_test(NAME ezwindow-declarative-explicit-control-instances
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-declarative-explicit-controls.nb")
    add_test(NAME ezwindow-declarative-all-native-controls
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-declarative-controls.nb")
    add_test(NAME ezwindow-style-requires-css-extension
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-style-extension.nb
            "-DEXPECTED=error NB-EZ1010: EzWindow Style 文件必须使用 .css 后缀。"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
endif()

if(BUILD_TESTING)
    add_test(NAME ezwindow-style-accepts-css-extension
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-style-valid-extension.nb")
endif()

if(BUILD_TESTING)
    add_test(NAME ezwindow-controls-declared-after-window-are-included
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:nimple-basic>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/check-ezwindow-controls-after-window.nb
            "-DEXPECTED=missingFunctionForRegression 未定义。"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert-diagnostic.cmake)
endif()

if(BUILD_TESTING)
    add_test(NAME nimval-brace-and-parameter-syntax
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-new-syntax.nb")
    add_test(NAME easywindow-include-and-declarative-braces
        COMMAND $<TARGET_FILE:nimple-basic> --check "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-easywindow-new-syntax.nb")
endif()
