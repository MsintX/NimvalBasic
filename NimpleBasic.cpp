#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include "Compat14.h"
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <unordered_set>
#include <utility>

#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <CommCtrl.h>
#if defined(NIMPLE_ENABLE_WEBVIEW2)
#include <WebView2.h>
#include <wrl.h>
#endif

// Request Common Controls v6 so built-in Win32 controls use the active Windows
// visual style instead of the legacy unthemed appearance. This uses system DLLs.
#if defined(_MSC_VER)
#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
// EM_SETCUEBANNER is hidden by some Windows SDK headers unless the target OS
// macro is set to Vista or later. The message value is part of the Win32 API,
// so provide the documented value as a compatibility fallback without forcing
// a higher _WIN32_WINNT target for the entire interpreter.
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif
#endif


namespace fs = nbcompat::fs;
using nbcompat::optional;
using nbcompat::nullopt;

static std::ifstream openInputFile(const fs::path& path, std::ios::openmode mode = std::ios::in) {
#ifdef _WIN32
    return std::ifstream(path.wstring().c_str(), mode);
#else
    return std::ifstream(path.string().c_str(), mode);
#endif
}
static std::ofstream openOutputFile(const fs::path& path, std::ios::openmode mode = std::ios::out) {
#ifdef _WIN32
    return std::ofstream(path.wstring().c_str(), mode);
#else
    return std::ofstream(path.string().c_str(), mode);
#endif
}

// Read an environment variable without triggering MSVC's unsafe-CRT warning.
// On MSVC, _dupenv_s allocates the result; this helper owns and frees that buffer.
static optional<std::string> getEnvironmentVariable(const char* name) {
#if defined(_WIN32) && defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    const auto error = ::_dupenv_s(&value, &length, name);
    if (error != 0 || value == nullptr) {
        std::free(value);
        return nullopt;
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) return nullopt;
    return std::string(value);
#endif
}

static bool environmentVariableEquals(const char* name, const std::string& expected) {
    const auto value = getEnvironmentVariable(name);
    return value.has_value() && *value == expected;
}

// EasyWindow platform-native GUI backend. Windows uses Win32 directly; no third-party GUI toolkit is required.
namespace ezwindow {
#if defined(_WIN32)
    static std::vector<std::string> splitItems(const std::string& items) {
        std::vector<std::string> result; std::istringstream input(items); std::string item;
        while (std::getline(input, item)) { if (!item.empty() && item.back() == '\r') item.pop_back(); if (!item.empty()) result.push_back(item); }
        return result;
    }
#endif
    using ClickCallback = std::function<void()>;
    using ChangeCallback = std::function<void(const std::string&)>;

    bool available() noexcept;
    int createWindow(const std::string& title, int width, int height, const std::string& iconPath);
    int addLabel(int windowId, const std::string& text);
    int addButton(int windowId, const std::string& text);
    int addEntry(int windowId, const std::string& placeholder);
    int addTextBox(int windowId, const std::string& text);
    int addCheckBox(int windowId, const std::string& text);
    int addPasswordEntry(int windowId, const std::string& placeholder);
    int addComboBox(int windowId, const std::string& items);
    int addRadioButton(int windowId, const std::string& text);
    int addSlider(int windowId, int minimum, int maximum);
    int addProgressBar(int windowId, int minimum, int maximum);
    int addWebView(int windowId, const std::string& url);
    void webViewNavigate(int widgetId, const std::string& url);
    void webViewSetHtml(int widgetId, const std::string& html);
    void setValue(int widgetId, int value);
    int getValue(int widgetId);
    void setText(int widgetId, const std::string& text);
    std::string getText(int widgetId);
    void setChecked(int widgetId, bool checked);
    bool getChecked(int widgetId);
    void setTitle(int windowId, const std::string& title);
    void showWindow(int windowId);
    void setPosition(int widgetId, int x, int y);
    void setSize(int widgetId, int width, int height);
    void setBounds(int widgetId, int x, int y, int width, int height);
    void setRelativeBounds(int widgetId, int xPercent, int yPercent, int widthPercent, int heightPercent);
    void setOnClick(int widgetId, ClickCallback callback);
    void setOnChange(int widgetId, ChangeCallback callback);
    void mainLoop();
    void quit();
    void message(const std::string& title, const std::string& text);

#if defined(_WIN32)
    // Dependency-free Win32 backend. All controls are provided by Windows itself.
    enum class ControlKind { Window, Label, Button, Entry, PasswordEntry, TextBox, CheckBox, ComboBox, RadioButton, Slider, ProgressBar, WebView };
    enum class LayoutMode { Auto, Absolute, Relative };
    struct ControlGeometry {
        LayoutMode mode{ LayoutMode::Auto };
        int x{ 12 }, y{ 12 }, width{ 400 }, height{ 28 };
        int xPercent{ 0 }, yPercent{ 0 }, widthPercent{ 100 }, heightPercent{ 10 };
    };
    struct NativeControl {
        HWND native;
        HWND owner;
        ControlKind kind;
        ClickCallback click;
        ChangeCallback change;
        ControlGeometry geometry;
        int minimum;
        int maximum;
        NativeControl()
            : native(nullptr), owner(nullptr), kind(ControlKind::Window), geometry(), minimum(0), maximum(100) {}
        NativeControl(HWND nativeHandle, HWND ownerHandle, ControlKind controlKind,
            ClickCallback onClick, ChangeCallback onChange, ControlGeometry controlGeometry,
            int minValue, int maxValue)
            : native(nativeHandle), owner(ownerHandle), kind(controlKind),
              click(std::move(onClick)), change(std::move(onChange)), geometry(controlGeometry),
              minimum(minValue), maximum(maxValue) {}
        NativeControl(HWND nativeHandle, HWND ownerHandle, ControlKind controlKind,
            ClickCallback onClick, ChangeCallback onChange)
            : native(nativeHandle), owner(ownerHandle), kind(controlKind),
              click(std::move(onClick)), change(std::move(onChange)), geometry(), minimum(0), maximum(100) {}
    };
    static std::unordered_map<int, NativeControl> g_controls;
    static std::unordered_map<int, HWND> g_windows;
    static std::unordered_map<int, HICON> g_windowIcons;
    static std::unordered_map<HWND, std::vector<int>> g_controlOrder;
#if defined(NIMPLE_ENABLE_WEBVIEW2)
    struct WebViewState {
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> view;
        std::string pendingValue;
        bool pendingHtml{ false };
    };
    static std::unordered_map<int, WebViewState> g_webviews;
    static bool g_webviewComInitialized{ false };
    static void updateWebViewBounds(int id);
#endif
    static int g_nextId = 1;
    static bool g_classRegistered = false;
    static HINSTANCE g_instance = GetModuleHandleW(nullptr);
    static constexpr wchar_t kWindowClass[] = L"NimpleEasyWindowNativeClass";

    static std::wstring toWide(const std::string& value) {
        if (value.empty()) return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0) return std::wstring(value.begin(), value.end());
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), &result[0], count);
        return result;
    }
    static std::string toUtf8(const std::wstring& value) {
        if (value.empty()) return {};
        const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0) return std::string(value.begin(), value.end());
        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], count, nullptr, nullptr);
        return result;
    }
    static std::string readControlText(HWND hwnd) {
        const int length = GetWindowTextLengthW(hwnd);
        std::wstring value(static_cast<size_t>(std::max(0, length)) + 1, L'\0');
        const int copied = GetWindowTextW(hwnd, &value[0], length + 1);
        value.resize(static_cast<size_t>(std::max(0, copied)));
        return toUtf8(value);
    }
    static int defaultControlHeight(ControlKind kind) {
        switch (kind) {
        case ControlKind::TextBox: return 100;
        case ControlKind::Label: return 24;
        case ControlKind::CheckBox: return 26;
        case ControlKind::Button: return 30;
        case ControlKind::Entry:
        case ControlKind::PasswordEntry:
        case ControlKind::ComboBox: return 28;
        case ControlKind::RadioButton: return 26;
        case ControlKind::Slider: return 36;
        case ControlKind::ProgressBar: return 24;
        case ControlKind::WebView: return 180;
        case ControlKind::Window: return 0;
        }
        return 28;
    }
    static void layoutWindow(HWND hwnd) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int clientWidth = std::max(1L, rc.right - rc.left);
        const int clientHeight = std::max(1L, rc.bottom - rc.top);
        int autoY = 12;
        const auto order = g_controlOrder.find(hwnd);
        if (order == g_controlOrder.end()) return;
        for (const int controlId : order->second) {
            auto found = g_controls.find(controlId);
            if (found == g_controls.end()) continue;
            NativeControl& c = found->second;
            if (c.kind == ControlKind::Window || !c.native || !IsWindow(c.native)) continue;
            const int defaultHeight = defaultControlHeight(c.kind);
            int x = c.geometry.x;
            int y = c.geometry.y;
            int width = c.geometry.width;
            int height = c.geometry.height;
            if (c.geometry.mode == LayoutMode::Auto) {
                x = 12; y = autoY; width = std::max(1, clientWidth - 24); height = defaultHeight;
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = width; c.geometry.height = height;
            } else if (c.geometry.mode == LayoutMode::Relative) {
                x = static_cast<int>((static_cast<long long>(clientWidth) * c.geometry.xPercent) / 100);
                y = static_cast<int>((static_cast<long long>(clientHeight) * c.geometry.yPercent) / 100);
                width = std::max(1, static_cast<int>((static_cast<long long>(clientWidth) * c.geometry.widthPercent) / 100));
                height = std::max(1, static_cast<int>((static_cast<long long>(clientHeight) * c.geometry.heightPercent) / 100));
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = width; c.geometry.height = height;
            }
            MoveWindow(c.native, x, y, std::max(1, width), std::max(1, height), TRUE);
#if defined(NIMPLE_ENABLE_WEBVIEW2)
            if (c.kind == ControlKind::WebView) updateWebViewBounds(controlId);
#endif
            // Preserve each auto-layout slot when a control is manually positioned.
            autoY += defaultHeight + 7;
        }
    }
    static void validateBounds(int width, int height) {
        if (width <= 0 || height <= 0) throw std::runtime_error("EasyWindow：控件宽度和高度必须大于 0。");
    }
    static void validateRelativeBounds(int x, int y, int width, int height) {
        if (x < 0 || x > 100 || y < 0 || y > 100 || width <= 0 || width > 100 || height <= 0 || height > 100 ||
            x + width > 100 || y + height > 100)
            throw std::runtime_error("EasyWindow：相对布局参数必须在 0..100 范围内，且 x+width、y+height 不能超过 100。");
    }
    static void applyLayoutForControl(NativeControl& c) {
        if (c.kind == ControlKind::Window) throw std::runtime_error("EasyWindow：布局 API 的目标必须是控件，不能是窗口。");
        layoutWindow(c.owner);
    }
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        if (message == WM_SIZE) layoutWindow(hwnd);
        if (message == WM_HSCROLL && lParam) {
            HWND slider = reinterpret_cast<HWND>(lParam);
            const int controlId = GetDlgCtrlID(slider);
            auto it = g_controls.find(controlId);
            if (it != g_controls.end() && it->second.kind == ControlKind::Slider && it->second.change) {
                try { it->second.change(std::to_string(static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0)))); }
                catch (const std::exception& e) { const auto text = toWide(e.what()); MessageBoxW(hwnd, text.c_str(), L"Nimple callback error", MB_OK | MB_ICONERROR); }
            }
        }
        // Let built-in controls repaint on visual-style, high-contrast, or system
        // color changes. Do not custom-paint buttons: Windows owns their theme.
        if (message == WM_THEMECHANGED || message == WM_SYSCOLORCHANGE || message == WM_SETTINGCHANGE) {
            const auto order = g_controlOrder.find(hwnd);
            if (order != g_controlOrder.end()) {
                for (const int controlId : order->second) {
                    const auto child = g_controls.find(controlId);
                    if (child != g_controls.end() && child->second.native && IsWindow(child->second.native)) {
                        SendMessageW(child->second.native, WM_THEMECHANGED, 0, 0);
                        InvalidateRect(child->second.native, nullptr, TRUE);
                    }
                }
            }
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        if (message == WM_COMMAND) {
            const int id = LOWORD(wParam);
            const int notification = HIWORD(wParam);
            auto it = g_controls.find(id);
            if (it != g_controls.end()) {
                NativeControl& control = it->second;
                try {
                    if (control.kind == ControlKind::Button && notification == BN_CLICKED && control.click) control.click();
                    else if ((control.kind == ControlKind::Entry || control.kind == ControlKind::PasswordEntry || control.kind == ControlKind::TextBox) && notification == EN_CHANGE && control.change)
                        control.change(readControlText(control.native));
                    else if (control.kind == ControlKind::ComboBox && notification == CBN_SELCHANGE && control.change)
                        control.change(readControlText(control.native));
                    else if ((control.kind == ControlKind::CheckBox || control.kind == ControlKind::RadioButton) && notification == BN_CLICKED && control.change)
                        control.change((SendMessageW(control.native, BM_GETCHECK, 0, 0) == BST_CHECKED) ? "true" : "false");
                } catch (const std::exception& e) {
                    const auto messageText = toWide(e.what());
                    MessageBoxW(hwnd, messageText.c_str(), L"Nimple callback error", MB_OK | MB_ICONERROR);
                } catch (...) {
                    MessageBoxW(hwnd, L"Unknown Nimple callback error", L"Nimple callback error", MB_OK | MB_ICONERROR);
                }
                return 0;
            }
        }
        if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
        if (message == WM_DESTROY) {
            bool anyOpen = false;
            for (const auto& pair : g_windows) if (pair.second && pair.second != hwnd && IsWindow(pair.second)) { anyOpen = true; break; }
            for (auto it = g_windows.begin(); it != g_windows.end(); ++it) {
                if (it->second == hwnd) {
                    const int windowId = it->first;
                    auto icon = g_windowIcons.find(windowId);
                    if (icon != g_windowIcons.end()) { DestroyIcon(icon->second); g_windowIcons.erase(icon); }
                    g_controls.erase(windowId);
                    g_controlOrder.erase(hwnd);
                    g_windows.erase(it);
                    break;
                }
            }
            if (!anyOpen) PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    static void ensureStarted() {
        if (g_classRegistered) return;
        INITCOMMONCONTROLSEX controls{};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&controls);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = windowProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kWindowClass;
        wc.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("EasyWindow：无法注册 Win32 窗口类。");
        g_classRegistered = true;
    }
    static HWND findWindow(int id) {
        auto it = g_windows.find(id);
        if (it == g_windows.end() || !it->second || !IsWindow(it->second))
            throw std::runtime_error("EasyWindow：无效的窗口 ID " + std::to_string(id) + "。");
        return it->second;
    }
    static NativeControl& findControl(int id) {
        auto it = g_controls.find(id);
        if (it == g_controls.end() || !it->second.native || !IsWindow(it->second.native))
            throw std::runtime_error("EasyWindow：无效的控件 ID " + std::to_string(id) + "。");
        return it->second;
    }
    static int addControl(int windowId, const std::string& text, ControlKind kind, int minimum = 0, int maximum = 100) {
        HWND parent = findWindow(windowId);
        DWORD style = WS_CHILD | WS_VISIBLE;
        const wchar_t* className = L"STATIC";
        switch (kind) {
        case ControlKind::Label: style |= SS_LEFT; className = L"STATIC"; break;
        case ControlKind::Button: style |= WS_TABSTOP | BS_PUSHBUTTON; className = L"BUTTON"; break;
        case ControlKind::Entry: style |= WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL; className = L"EDIT"; break;
        case ControlKind::PasswordEntry: style |= WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD; className = L"EDIT"; break;
        case ControlKind::TextBox: style |= WS_TABSTOP | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL; className = L"EDIT"; break;
        case ControlKind::CheckBox: style |= WS_TABSTOP | BS_AUTOCHECKBOX; className = L"BUTTON"; break;
        case ControlKind::RadioButton: {
            style |= WS_TABSTOP | BS_AUTORADIOBUTTON;
            const auto order = g_controlOrder.find(parent); bool hasRadio = false;
            if (order != g_controlOrder.end()) for (int oldId : order->second) { const auto old = g_controls.find(oldId); if (old != g_controls.end() && old->second.kind == ControlKind::RadioButton) { hasRadio = true; break; } }
            if (!hasRadio) style |= WS_GROUP;
            className = L"BUTTON"; break;
        }
        case ControlKind::ComboBox: style |= WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST; className = L"COMBOBOX"; break;
        case ControlKind::Slider: style |= WS_TABSTOP | TBS_AUTOTICKS | TBS_HORZ; className = TRACKBAR_CLASSW; break;
        case ControlKind::ProgressBar: style |= PBS_SMOOTH; className = PROGRESS_CLASSW; break;
        case ControlKind::WebView: style |= SS_LEFT; className = L"STATIC"; break;
        case ControlKind::Window: break;
        }
        const int id = g_nextId++;
        const std::wstring wideText = toWide(text);
        HWND native = CreateWindowExW(0, className, wideText.c_str(), style, 12, 12, 400, 28, parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
        if (!native) throw std::runtime_error("EasyWindow：创建原生控件失败。");
        SendMessageW(native, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        if (kind == ControlKind::PasswordEntry) SendMessageW(native, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'*'), 0);
        g_controls.emplace(id, NativeControl{ native, parent, kind, {}, {}, {}, minimum, maximum });
        g_controlOrder[parent].push_back(id);
        if (kind == ControlKind::Slider || kind == ControlKind::ProgressBar) {
            if (maximum <= minimum) { DestroyWindow(native); g_controls.erase(id); g_controlOrder[parent].pop_back(); throw std::runtime_error("EasyWindow：控件最大值必须大于最小值。"); }
            if (kind == ControlKind::Slider) { SendMessageW(native, TBM_SETRANGE, TRUE, MAKELPARAM(minimum, maximum)); SendMessageW(native, TBM_SETPOS, TRUE, minimum); }
            else { SendMessageW(native, PBM_SETRANGE32, minimum, maximum); SendMessageW(native, PBM_SETPOS, minimum, 0); }
        }
        if (kind == ControlKind::ComboBox) {
            for (const auto& item : splitItems(text)) { const auto w = toWide(item); SendMessageW(native, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str())); }
            if (SendMessageW(native, CB_GETCOUNT, 0, 0) > 0) SendMessageW(native, CB_SETCURSEL, 0, 0);
        }
        layoutWindow(parent); return id;
    }
    bool available() noexcept { return true; }
    int createWindow(const std::string& title, int width, int height, const std::string& iconPath) {
        ensureStarted();
        const int id = g_nextId++;
        const std::wstring wideTitle = toWide(title);
        HWND hwnd = CreateWindowExW(0, kWindowClass, wideTitle.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, std::max(200, width), std::max(200, height),
            nullptr, nullptr, g_instance, reinterpret_cast<void*>(static_cast<INT_PTR>(id)));
        if (!hwnd) throw std::runtime_error("EasyWindow：创建原生窗口失败。");
        g_windows.emplace(id, hwnd);
        if (!iconPath.empty()) {
            const auto iconFile = toWide(iconPath);
            HICON icon = static_cast<HICON>(LoadImageW(nullptr, iconFile.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
            if (!icon) { DestroyWindow(hwnd); g_windows.erase(id); throw std::runtime_error("EasyWindow：无法从文件加载窗口图标：" + iconPath); }
            g_windowIcons.emplace(id, icon);
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        }
        g_controlOrder.emplace(hwnd, std::vector<int>{});
        g_controls.emplace(id, NativeControl{ hwnd, hwnd, ControlKind::Window, {}, {} });
        return id;
    }
    int addLabel(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Label); }
    int addButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Button); }
    int addEntry(int windowId, const std::string& placeholder) { const int id = addControl(windowId, {}, ControlKind::Entry); if (!placeholder.empty()) { const auto hint = toWide(placeholder); SendMessageW(findControl(id).native, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(hint.c_str())); } return id; }
    int addTextBox(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::TextBox); }
    int addCheckBox(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::CheckBox); }
    int addPasswordEntry(int windowId, const std::string& placeholder) { const int id = addControl(windowId, {}, ControlKind::PasswordEntry); if (!placeholder.empty()) { const auto hint = toWide(placeholder); SendMessageW(findControl(id).native, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(hint.c_str())); } return id; }
    int addComboBox(int windowId, const std::string& items) { return addControl(windowId, items, ControlKind::ComboBox); }
    int addRadioButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::RadioButton); }
    int addSlider(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::Slider, minimum, maximum); }
    int addProgressBar(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::ProgressBar, minimum, maximum); }
    void setText(int widgetId, const std::string& text) {
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window || c.kind == ControlKind::Slider || c.kind == ControlKind::ProgressBar || c.kind == ControlKind::WebView) throw std::runtime_error("setText 的目标必须是文本或选项控件。");
        const auto wide = toWide(text);
        if (c.kind == ControlKind::ComboBox) { const LRESULT index = SendMessageW(c.native, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(wide.c_str())); if (index == CB_ERR) throw std::runtime_error("setText：ComboBox 中不存在选项“" + text + "”。"); SendMessageW(c.native, CB_SETCURSEL, index, 0); }
        else SetWindowTextW(c.native, wide.c_str());
    }
    std::string getText(int widgetId) { NativeControl& c = findControl(widgetId); if (c.kind == ControlKind::Window || c.kind == ControlKind::Slider || c.kind == ControlKind::ProgressBar || c.kind == ControlKind::WebView) throw std::runtime_error("getText 的目标必须是文本或选项控件。"); return readControlText(c.native); }
    void setChecked(int widgetId, bool checked) { NativeControl& c = findControl(widgetId); if (c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton) throw std::runtime_error("setChecked 只支持 checkBox 或 radioButton 控件。"); SendMessageW(c.native, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); }
    bool getChecked(int widgetId) { NativeControl& c = findControl(widgetId); if (c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton) throw std::runtime_error("getChecked 只支持 checkBox 或 radioButton 控件。"); return SendMessageW(c.native, BM_GETCHECK, 0, 0) == BST_CHECKED; }
    void setValue(int widgetId, int value) { NativeControl& c = findControl(widgetId); if (c.kind != ControlKind::Slider && c.kind != ControlKind::ProgressBar) throw std::runtime_error("setValue 只支持 slider 或 progressBar 控件。"); value = std::max(c.minimum, std::min(c.maximum, value)); if (c.kind == ControlKind::Slider) SendMessageW(c.native, TBM_SETPOS, TRUE, value); else SendMessageW(c.native, PBM_SETPOS, value, 0); }
    int getValue(int widgetId) { NativeControl& c = findControl(widgetId); if (c.kind == ControlKind::Slider) return static_cast<int>(SendMessageW(c.native, TBM_GETPOS, 0, 0)); if (c.kind == ControlKind::ProgressBar) return static_cast<int>(SendMessageW(c.native, PBM_GETPOS, 0, 0)); throw std::runtime_error("getValue 只支持 slider 或 progressBar 控件。"); }
#if defined(NIMPLE_ENABLE_WEBVIEW2)
    // Windows 7/8/8.1 stopped at WebView2 Runtime 109. On those systems,
    // prefer an app-local Fixed Version runtime if supplied; otherwise let
    // WebView2Loader use the machine-installed Evergreen runtime (which is
    // also pinned at 109 on those operating systems).
    static std::wstring webView2BrowserExecutableFolder() {
        typedef LONG (WINAPI *RtlGetVersionProc)(OSVERSIONINFOW*);
        OSVERSIONINFOW version; ZeroMemory(&version, sizeof(version));
        version.dwOSVersionInfoSize = sizeof(version);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        RtlGetVersionProc rtlGetVersion = ntdll ? reinterpret_cast<RtlGetVersionProc>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
        bool legacyWindows = false;
        if (rtlGetVersion && rtlGetVersion(&version) == 0)
            legacyWindows = version.dwMajorVersion < 10;
        if (!legacyWindows) return std::wstring();

        wchar_t configured[MAX_PATH];
        DWORD configuredLength = GetEnvironmentVariableW(L"NIMPLE_WEBVIEW2_FIXED_RUNTIME", configured, MAX_PATH);
        if (configuredLength > 0 && configuredLength < MAX_PATH) {
            if (GetFileAttributesW(configured) != INVALID_FILE_ATTRIBUTES) return std::wstring(configured);
        }
        wchar_t executable[MAX_PATH];
        DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return std::wstring();
        std::wstring folder(executable, length);
        const size_t slash = folder.find_last_of(L"\\/");
        if (slash != std::wstring::npos) folder.resize(slash + 1);
        folder += L"WebView2Runtime109";
        if (GetFileAttributesW(folder.c_str()) != INVALID_FILE_ATTRIBUTES) return folder;
        return std::wstring();
    }
#endif

    int addWebView(int windowId, const std::string& url) {
#if defined(NIMPLE_ENABLE_WEBVIEW2)
        if (!g_webviewComInitialized) {
            const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
                throw std::runtime_error("EasyWindow：无法初始化 COM，WebView2 不能启动。");
            if (comResult == RPC_E_CHANGED_MODE)
                throw std::runtime_error("EasyWindow：WebView2 需要 STA COM 线程；当前线程已使用不同的 COM apartment。");
            g_webviewComInitialized = true;
        }
        const int id = addControl(windowId, {}, ControlKind::WebView);
        g_webviews.emplace(id, WebViewState{});
        g_webviews[id].pendingValue = url;
        const std::wstring fixedRuntimeFolder = webView2BrowserExecutableFolder();
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr,
            fixedRuntimeFolder.empty() ? nullptr : fixedRuntimeFolder.c_str(), nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [id](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                    if (FAILED(result) || !environment) {
                        MessageBoxW(nullptr, L"WebView2 环境初始化失败。请确认系统受支持并安装了 Microsoft Edge WebView2 Runtime。", L"EasyWindow WebView2", MB_OK | MB_ICONERROR);
                        return FAILED(result) ? result : E_FAIL;
                    }
                    const auto control = g_controls.find(id);
                    if (control == g_controls.end() || !control->second.owner || !IsWindow(control->second.owner)) return E_ABORT;
                    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environmentHold = environment;
                    return environmentHold->CreateCoreWebView2Controller(control->second.owner,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [id, environmentHold](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT {
                                auto state = g_webviews.find(id);
                                const auto controlIt = g_controls.find(id);
                                if (state == g_webviews.end() || controlIt == g_controls.end()) return E_ABORT;
                                if (FAILED(controllerResult) || !controller) {
                                    MessageBoxW(controlIt->second.owner, L"无法创建 WebView2 控件。请检查系统版本、Runtime 与 SDK 配置。", L"EasyWindow WebView2", MB_OK | MB_ICONERROR);
                                    return FAILED(controllerResult) ? controllerResult : E_FAIL;
                                }
                                state->second.controller = controller;
                                const HRESULT viewResult = controller->get_CoreWebView2(&state->second.view);
                                if (FAILED(viewResult) || !state->second.view) return viewResult;
                                updateWebViewBounds(id);
                                const std::wstring pending = toWide(state->second.pendingValue);
                                if (state->second.pendingHtml) return state->second.view->NavigateToString(pending.c_str());
                                if (!pending.empty()) return state->second.view->Navigate(pending.c_str());
                                return S_OK;
                            }).Get());
                }).Get());
        if (FAILED(hr)) {
            g_webviews.erase(id);
            throw std::runtime_error("EasyWindow：启动 WebView2 失败。请确认已安装 WebView2 SDK 并链接 WebView2LoaderStatic.lib。");
        }
        return id;
#else
        (void)windowId; (void)url;
        throw std::runtime_error("此 Nimple 构建未启用 WebView2。请安装 WebView2 SDK 后启用 NIMPLE_ENABLE_WEBVIEW2；旧系统需要兼容的 WebView2 Runtime。");
#endif
    }
    void webViewNavigate(int widgetId, const std::string& url) {
#if defined(NIMPLE_ENABLE_WEBVIEW2)
        auto state = g_webviews.find(widgetId);
        if (state == g_webviews.end()) throw std::runtime_error("WebView2 ID 无效。");
        state->second.pendingValue = url; state->second.pendingHtml = false;
        if (state->second.view) { const std::wstring value = toWide(url); const HRESULT hr = state->second.view->Navigate(value.c_str()); if (FAILED(hr)) throw std::runtime_error("WebView2 导航失败。"); }
#else
        (void)widgetId; (void)url;
        throw std::runtime_error("此 Nimple 构建未启用 WebView2；旧系统需要兼容的 WebView2 Runtime。");
#endif
    }
    void webViewSetHtml(int widgetId, const std::string& html) {
#if defined(NIMPLE_ENABLE_WEBVIEW2)
        auto state = g_webviews.find(widgetId);
        if (state == g_webviews.end()) throw std::runtime_error("WebView2 ID 无效。");
        state->second.pendingValue = html; state->second.pendingHtml = true;
        if (state->second.view) { const std::wstring value = toWide(html); const HRESULT hr = state->second.view->NavigateToString(value.c_str()); if (FAILED(hr)) throw std::runtime_error("WebView2 加载 HTML 失败。"); }
#else
        (void)widgetId; (void)html;
        throw std::runtime_error("此 Nimple 构建未启用 WebView2；旧系统需要兼容的 WebView2 Runtime。");
#endif
    }
#if defined(NIMPLE_ENABLE_WEBVIEW2)
    static void updateWebViewBounds(int id) {
        auto state = g_webviews.find(id); auto control = g_controls.find(id);
        if (state == g_webviews.end() || control == g_controls.end() || !state->second.controller || !control->second.native) return;
        RECT bounds{}; GetWindowRect(control->second.native, &bounds);
        MapWindowPoints(nullptr, control->second.owner, reinterpret_cast<POINT*>(&bounds), 2);
        bounds.right -= bounds.left; bounds.bottom -= bounds.top;
        state->second.controller->put_Bounds(bounds);
    }
#endif
    void setTitle(int windowId, const std::string& title) {
        const auto wide = toWide(title); SetWindowTextW(findWindow(windowId), wide.c_str());
    }
    void showWindow(int windowId) { HWND hwnd = findWindow(windowId); ShowWindow(hwnd, SW_SHOWNORMAL); UpdateWindow(hwnd); }
    void setPosition(int widgetId, int x, int y) {
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setPosition 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y;
        applyLayoutForControl(c);
    }
    void setSize(int widgetId, int width, int height) {
        validateBounds(width, height);
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setSize 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.width = width; c.geometry.height = height;
        applyLayoutForControl(c);
    }
    void setBounds(int widgetId, int x, int y, int width, int height) {
        validateBounds(width, height);
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y;
        c.geometry.width = width; c.geometry.height = height;
        applyLayoutForControl(c);
    }
    void setRelativeBounds(int widgetId, int xPercent, int yPercent, int widthPercent, int heightPercent) {
        validateRelativeBounds(xPercent, yPercent, widthPercent, heightPercent);
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setRelativeBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Relative;
        c.geometry.xPercent = xPercent; c.geometry.yPercent = yPercent;
        c.geometry.widthPercent = widthPercent; c.geometry.heightPercent = heightPercent;
        applyLayoutForControl(c);
    }
    void setOnClick(int widgetId, ClickCallback callback) {
        NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::Button) throw std::runtime_error("onClick 只支持 button 控件。");
        c.click = std::move(callback);
    }
    void setOnChange(int widgetId, ChangeCallback callback) {
        NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::Entry && c.kind != ControlKind::PasswordEntry && c.kind != ControlKind::TextBox && c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton && c.kind != ControlKind::ComboBox && c.kind != ControlKind::Slider)
            throw std::runtime_error("onChange 只支持 entry、passwordEntry、textBox、checkBox、radioButton、comboBox 或 slider 控件。");
        c.change = std::move(callback);
    }
    void mainLoop() {
        if (g_windows.empty()) throw std::runtime_error("尚未创建 EasyWindow 窗口。");
        for (const auto& pair : g_windows) if (pair.second && IsWindow(pair.second)) { ShowWindow(pair.second, SW_SHOWNORMAL); UpdateWindow(pair.second); }
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        for (const auto& icon : g_windowIcons) if (icon.second) DestroyIcon(icon.second);
        g_windowIcons.clear(); g_controls.clear(); g_windows.clear(); g_controlOrder.clear();
#if defined(NIMPLE_ENABLE_WEBVIEW2)
        g_webviews.clear();
        if (g_webviewComInitialized) { CoUninitialize(); g_webviewComInitialized = false; }
#endif
        g_nextId = 1;
    }
    void quit() { PostQuitMessage(0); }
    void message(const std::string& title, const std::string& text) {
        const auto wideTitle = toWide(title); const auto wideText = toWide(text);
        MessageBoxW(nullptr, wideText.c_str(), wideTitle.c_str(), MB_OK | MB_ICONINFORMATION);
    }
#elif !defined(__APPLE__) && !defined(__linux__)
    [[noreturn]] static void unavailable() {
        throw std::runtime_error("此平台的 EasyWindow 原生后端尚未实现；当前版本在 Windows 上使用 Win32 原生控件，且不需要第三方 GUI 依赖。");
    }
    bool available() noexcept { return false; }
    int createWindow(const std::string&, int, int, const std::string&) { unavailable(); }
    int addLabel(int, const std::string&) { unavailable(); }
    int addButton(int, const std::string&) { unavailable(); }
    int addEntry(int, const std::string&) { unavailable(); }
    int addTextBox(int, const std::string&) { unavailable(); }
    int addCheckBox(int, const std::string&) { unavailable(); }
    int addPasswordEntry(int, const std::string&) { unavailable(); }
    int addComboBox(int, const std::string&) { unavailable(); }
    int addRadioButton(int, const std::string&) { unavailable(); }
    int addSlider(int, int, int) { unavailable(); }
    int addProgressBar(int, int, int) { unavailable(); }
    int addWebView(int, const std::string&) { unavailable(); }
    void webViewNavigate(int, const std::string&) { unavailable(); }
    void webViewSetHtml(int, const std::string&) { unavailable(); }
    void setValue(int, int) { unavailable(); }
    int getValue(int) { unavailable(); }
    void setText(int, const std::string&) { unavailable(); }
    std::string getText(int) { unavailable(); }
    void setChecked(int, bool) { unavailable(); }
    bool getChecked(int) { unavailable(); }
    void setTitle(int, const std::string&) { unavailable(); }
    void showWindow(int) { unavailable(); }
    void setPosition(int, int, int) { unavailable(); }
    void setSize(int, int, int) { unavailable(); }
    void setBounds(int, int, int, int, int) { unavailable(); }
    void setRelativeBounds(int, int, int, int, int) { unavailable(); }
    void setOnClick(int, ClickCallback) { unavailable(); }
    void setOnChange(int, ChangeCallback) { unavailable(); }
    void mainLoop() { unavailable(); }
    void quit() { unavailable(); }
    void message(const std::string&, const std::string&) { unavailable(); }
#endif
}

namespace nimple {

    static fs::path g_executablePath;

    // Resolve the actual running executable, not argv[0] relative to the caller's
    // working directory. The Standard library directory always lives beside it.
    static void setExecutablePath(const fs::path& fallbackPath) {
        std::error_code ec;
#ifdef _WIN32
        std::wstring buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, &buffer[0], static_cast<DWORD>(buffer.size()));
        if (length > 0 && static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(length);
            g_executablePath = fs::path(buffer);
            return;
        }
#elif defined(__linux__)
        std::vector<char> buffer(4096);
        const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (length > 0) {
            buffer[static_cast<std::size_t>(length)] = '\0';
            g_executablePath = fs::u8path(buffer.data());
            return;
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        (void)_NSGetExecutablePath(nullptr, &size);
        if (size > 0) {
            std::vector<char> buffer(size + 1);
            if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
                g_executablePath = fs::weakly_canonical(fs::u8path(buffer.data()), ec);
                if (!ec) return;
            }
        }
#endif
        g_executablePath = fs::absolute(fallbackPath, ec);
        if (ec) g_executablePath = fallbackPath;
    }

    static fs::path standardLibraryDirectory() {
        return g_executablePath.parent_path() / fs::u8path("Standard library");
    }

    static optional<std::string> readStandardLibraryModule(const std::string& moduleName) {
        // Module names are single filenames, never paths; statement module names cannot traverse
        // outside the sibling Standard library directory.
        if (moduleName.empty() || moduleName == "." || moduleName == ".." ||
            moduleName.find_first_of("/\\:") != std::string::npos) return nullopt;
        const fs::path modulePath = standardLibraryDirectory() / fs::u8path(moduleName);
        std::error_code ec;
        if (!fs::is_regular_file(modulePath, ec) || ec) return nullopt;
        std::ifstream file = openInputFile(modulePath, std::ios::binary);
        if (!file) return nullopt;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB && static_cast<unsigned char>(source[2]) == 0xBF)
            source.erase(0, 3);
        return source;
    }

    // All compiler and runtime messages share one diagnostic data model.
    // Producers may still throw NimpleError to unwind the current operation, but
    // the diagnostic itself is structured and rendered only by DiagnosticEngine.
    enum class DiagnosticSeverity { Error, Warning, Note };
    enum class DiagnosticPhase { Infrastructure, Preprocessor, Lexer, Parser, TypeChecker, Runtime };

    struct Diagnostic {
        DiagnosticSeverity severity;
        DiagnosticPhase phase;
        std::string code;
        int line;
        int column;
        std::string message;
        Diagnostic(DiagnosticSeverity s, DiagnosticPhase p, std::string c, int l, int col, std::string m)
            : severity(s), phase(p), code(std::move(c)), line(l), column(col), message(std::move(m)) {}
    };

    static DiagnosticPhase phaseForCode(const std::string& code) {
        if (code.compare(0, 4, "NB10") == 0 || code == "NB-W2090") return DiagnosticPhase::Lexer;
        if (code.compare(0, 4, "NB20") == 0) return DiagnosticPhase::Parser;
        if (code.compare(0, 4, "NB-T") == 0) return DiagnosticPhase::TypeChecker;
        if (code.compare(0, 4, "NB30") == 0 || code == "NB-W2101") return DiagnosticPhase::Preprocessor;
        if (code.compare(0, 5, "NB-R") == 0 || code.compare(0, 5, "NB-EZ") == 0 ||
            code == "NB-W2001" || code == "NB-W1041" || code == "NB-W1051" || code == "NB-W1081")
            return DiagnosticPhase::Runtime;
        return DiagnosticPhase::Infrastructure;
    }

    struct NimpleError : std::runtime_error {
        Diagnostic diagnostic;
        std::string code;
        int line;
        int column;
        NimpleError(std::string c, int l, int col, const std::string& message)
            : std::runtime_error(message), diagnostic(DiagnosticSeverity::Error, phaseForCode(c), c, l, col, message),
              code(std::move(c)), line(l), column(col) {}
    };

    class DiagnosticEngine {
    public:
        void add(const Diagnostic& diagnostic) { items.push_back(diagnostic); }
        void add(const NimpleError& error) { add(error.diagnostic); }
        const std::vector<Diagnostic>& diagnostics() const { return items; }
        static std::string render(const Diagnostic& d) {
            const char* severity = d.severity == DiagnosticSeverity::Warning ? "warning" : d.severity == DiagnosticSeverity::Note ? "note" : "error";
            return std::string(severity) + " " + d.code + ": " + d.message;
        }
        static void emit(const std::string& file, int line, int column, DiagnosticSeverity severity,
                         const std::string& code, const std::string& message) {
            Diagnostic d(severity, phaseForCode(code), code, line, column, message);
            if (!file.empty()) {
                std::cerr << file;
                if (line > 0) std::cerr << ':' << line << ':' << column;
                std::cerr << ": ";
            } else if (line > 0) std::cerr << "line " << line << ": ";
            std::cerr << render(d) << '\n';
        }
        int flush(const std::string& file, std::size_t limit = 100) const {
            std::vector<Diagnostic> ordered = items;
            std::stable_sort(ordered.begin(), ordered.end(), [](const Diagnostic& a, const Diagnostic& b) {
                if (a.line != b.line) return a.line < b.line;
                if (a.column != b.column) return a.column < b.column;
                if (a.code != b.code) return a.code < b.code;
                return a.message < b.message;
            });
            std::size_t printed = 0;
            const Diagnostic* previous = nullptr;
            for (const auto& d : ordered) {
                if (previous && previous->line == d.line && previous->column == d.column &&
                    previous->code == d.code && previous->message == d.message && previous->severity == d.severity) continue;
                if (printed >= limit) {
                    emit(file, 0, 0, DiagnosticSeverity::Error, "NB-I0002", "诊断数量超过 " + std::to_string(limit) + " 条，剩余错误已省略。");
                    break;
                }
                emit(file, d.line, d.column, d.severity, d.code, d.message);
                ++printed;
                previous = &d;
            }
            return printed ? 1 : 0;
        }
    private:
        std::vector<Diagnostic> items;
    };

    enum class ValueType { Void, Null, NoThing, NoType, Int, Float, Double, Bool, String, Char };

    struct ValueData {
        int32_t integerValue; double doubleValue; bool boolValue; std::string stringValue;
        ValueData() : integerValue(0), doubleValue(0.0), boolValue(false) {}
        ValueData(std::nullptr_t) : integerValue(0), doubleValue(0.0), boolValue(false) {}
        ValueData(int32_t v) : integerValue(v), doubleValue(0.0), boolValue(false) {}
        ValueData(double v) : integerValue(0), doubleValue(v), boolValue(false) {}
        ValueData(bool v) : integerValue(0), doubleValue(0.0), boolValue(v) {}
        ValueData(const std::string& v) : integerValue(0), doubleValue(0.0), boolValue(false), stringValue(v) {}
        ValueData(std::string&& v) : integerValue(0), doubleValue(0.0), boolValue(false), stringValue(std::move(v)) {}
    };
    template<class T> static T dataGet(const ValueData& d);
    template<> inline int32_t dataGet<int32_t>(const ValueData& d) { return d.integerValue; }
    template<> inline double dataGet<double>(const ValueData& d) { return d.doubleValue; }
    template<> inline bool dataGet<bool>(const ValueData& d) { return d.boolValue; }
    template<> inline std::string dataGet<std::string>(const ValueData& d) { return d.stringValue; }

    struct Value {
        ValueType type;
        ValueData data;
        Value() : type(ValueType::NoThing), data() {}
        Value(ValueType valueType, const ValueData& valueData) : type(valueType), data(valueData) {}
        Value(ValueType valueType, std::nullptr_t) : type(valueType), data(nullptr) {}
        Value(ValueType valueType, int32_t value) : type(valueType), data(value) {}
        Value(ValueType valueType, double value) : type(valueType), data(value) {}
        Value(ValueType valueType, bool value) : type(valueType), data(value) {}
        Value(ValueType valueType, const std::string& value) : type(valueType), data(value) {}
        Value(ValueType valueType, std::string&& value) : type(valueType), data(std::move(value)) {}

        static Value voidValue() { return { ValueType::Void, nullptr }; }
        static Value nullValue() { return { ValueType::Null, nullptr }; }
        static Value noThing() { return { ValueType::NoThing, nullptr }; }
        static Value noType() { return { ValueType::NoType, nullptr }; }
        static Value integer(int32_t v) { return { ValueType::Int, v }; }
        static Value floating(double v, ValueType type = ValueType::Double) { return { type, v }; }
        static Value boolean(bool v) { return { ValueType::Bool, v }; }
        static Value string(std::string v) { return { ValueType::String, std::move(v) }; }
        static Value character(std::string utf8) { return { ValueType::Char, std::move(utf8) }; }

        bool isNumeric() const { return type == ValueType::Int || type == ValueType::Float || type == ValueType::Double; }
        double number() const {
            if (type == ValueType::Int) return static_cast<double>(dataGet<int32_t>(data));
            if (isNumeric()) return dataGet<double>(data);
            throw std::runtime_error("数值运算要求 int、float 或 double 操作数。");
        }
        std::string typeName() const {
            switch (type) {
            case ValueType::Void: return "void";
            case ValueType::Null: return "Null";
            case ValueType::NoThing: return "NoThing";
            case ValueType::NoType: return "NoType";
            case ValueType::Int: return "int";
            case ValueType::Float: return "float";
            case ValueType::Double: return "double";
            case ValueType::Bool: return "bool";
            case ValueType::String: return "string";
            case ValueType::Char: return "char";
            }
            return "NoThing";
        }
        std::string toString() const {
            switch (type) {
            case ValueType::Void: return "";
            case ValueType::Null: return "Null";
            case ValueType::NoThing: return "NoThing";
            case ValueType::NoType: return "NoType";
            case ValueType::Int: return std::to_string(dataGet<int32_t>(data));
            case ValueType::Float:
            case ValueType::Double: {
                double v = dataGet<double>(data);
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
                std::ostringstream out;
                out << std::setprecision(15) << v;
                return out.str();
            }
            case ValueType::Bool: return dataGet<bool>(data) ? "true" : "false";
            case ValueType::String:
            case ValueType::Char: return dataGet<std::string>(data);
            }
            return "NoThing";
        }
    };

    static optional<Value> convertValue(const Value& value, const std::string& target) {
        if (target == "string") return Value::string(value.toString());
        if (target == "bool") {
            if (value.type == ValueType::Bool) return value;
            if (value.type == ValueType::String) {
                auto s = dataGet<std::string>(value.data);
                if (s == "true") return Value::boolean(true);
                if (s == "false") return Value::boolean(false);
                return nullopt;
            }
            if (value.type == ValueType::Int) {
                int32_t n = dataGet<int32_t>(value.data);
                if (n == 0 || n == 1) return Value::boolean(n == 1);
                return nullopt;
            }
            if (value.isNumeric()) {
                double n = value.number();
                if (n == 0 || n == 1) return Value::boolean(n == 1);
                return nullopt;
            }
            if (value.type == ValueType::Char) {
                const auto& s = dataGet<std::string>(value.data);
                if (s == "0") return Value::boolean(false);
                if (s == "1") return Value::boolean(true);
            }
            return nullopt;
        }
        if (target == "int") {
            if (value.type == ValueType::Int) return value;
            if (value.type == ValueType::Bool) return Value::integer(dataGet<bool>(value.data) ? 1 : 0);
            if (value.type == ValueType::Float || value.type == ValueType::Double) {
                double n = value.number();
                if (!std::isfinite(n) || std::floor(n) != n || n < std::numeric_limits<int32_t>::min() || n > std::numeric_limits<int32_t>::max()) return nullopt;
                return Value::integer(static_cast<int32_t>(n));
            }
            if (value.type == ValueType::String) {
                try {
                    std::size_t used = 0;
                    long long n = std::stoll(dataGet<std::string>(value.data), &used, 10);
                    if (used == dataGet<std::string>(value.data).size() && n >= INT32_MIN && n <= INT32_MAX)
                        return Value::integer(static_cast<int32_t>(n));
                }
                catch (...) {}
                return nullopt;
            }
            if (value.type == ValueType::Char) {
                const auto& s = dataGet<std::string>(value.data);
                if (s.size() == 1) return Value::integer(static_cast<unsigned char>(s[0]));
            }
            return nullopt;
        }
        if (target == "float" || target == "double") {
            if (value.isNumeric()) return Value::floating(value.number(), target == "float" ? ValueType::Float : ValueType::Double);
            if (value.type == ValueType::Bool) return Value::floating(dataGet<bool>(value.data) ? 1.0 : 0.0, target == "float" ? ValueType::Float : ValueType::Double);
            if (value.type == ValueType::String) {
                try {
                    const auto& s = dataGet<std::string>(value.data);
                    std::size_t used = 0;
                    double n = std::stod(s, &used);
                    if (used == s.size()) return Value::floating(n, target == "float" ? ValueType::Float : ValueType::Double);
                }
                catch (...) {}
            }
            return nullopt;
        }
        if (target == "char") {
            if (value.type == ValueType::Char) return value;
            if (value.type == ValueType::String) {
                const auto& s = dataGet<std::string>(value.data);
                if (!s.empty()) {
                    // A UTF-8 scalar has 1-4 bytes; reject multiple scalars using a small decoder.
                    unsigned char c = static_cast<unsigned char>(s[0]);
                    std::size_t n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
                    if (n && s.size() == n) return Value::character(s);
                }
                return nullopt;
            }
            if (value.type == ValueType::Bool) return Value::character(dataGet<bool>(value.data) ? "1" : "0");
            if (value.type == ValueType::Int) {
                uint32_t cp = static_cast<uint32_t>(dataGet<int32_t>(value.data));
                if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return nullopt;
                std::string out;
                if (cp < 0x80) out.push_back(static_cast<char>(cp));
                else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                return Value::character(out);
            }
            return nullopt;
        }
        if (target == "void") return Value::voidValue();
        return nullopt;
    }

    struct Token {
        enum class Kind { Identifier, Number, String, Character, Operator, Punct, Newline, End } kind;
        std::string text;
        int line;
        int column;
        Token() : kind(Kind::End), line(1), column(1) {}
        Token(Kind tokenKind, std::string tokenText, int tokenLine, int tokenColumn)
            : kind(tokenKind), text(std::move(tokenText)), line(tokenLine), column(tokenColumn) {}
    };

    static bool isIdentStart(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 0x80; }
    static bool isIdentPart(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }
    static std::string decodeEscapes(const std::string& input, int line, int column) {
        std::string out;
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i] != '\\') { out.push_back(input[i]); continue; }
            if (++i >= input.size()) throw NimpleError("NB1003", line, column, "字符串中的转义序列不完整。");
            switch (input[i]) {
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            case '\'': out.push_back('\''); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '0': out.push_back('\0'); break;
            case 'u': case 'U': {
                std::size_t digits = input[i] == 'u' ? 4 : 8;
                if (i + digits >= input.size()) throw NimpleError("NB1003", line, column, "Unicode 转义序列长度不正确。");
                uint32_t cp = 0;
                for (std::size_t j = 0; j < digits; ++j) {
                    char h = input[++i];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= h - '0';
                    else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                    else throw NimpleError("NB1003", line, column, "Unicode 转义序列包含无效十六进制字符。");
                }
                if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) throw NimpleError("NB1003", line, column, "Unicode 转义不是有效的 Unicode 标量值。");
                if (cp < 0x80) out.push_back(static_cast<char>(cp));
                else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
                break;
            }
            default: throw NimpleError("NB1003", line, column, "未知转义序列：\\" + std::string(1, input[i]));
            }
        }
        return out;
    }

    class Lexer {
    public:
        explicit Lexer(const std::string& src, bool suppressClassWarning = false) : source(src), suppressClassWarning(suppressClassWarning) {}
        std::vector<Token> scan() {
            while (!atEnd()) {
                const std::size_t positionBefore = pos;
                try {
                    scanOne();
                }
                catch (const NimpleError& error) {
                    errors.push_back(error);
                    // A lexer error must not prevent diagnostics on later lines.
                    // If the failing scanner did not consume input, consume one byte
                    // here to guarantee forward progress.
                    if (pos == positionBefore && !atEnd()) take();
                }
            }
            tokens.push_back({ Token::Kind::End, "", line, column });
            return tokens;
        }
        const std::vector<NimpleError>& diagnostics() const { return errors; }
    private:
        const std::string& source;
        bool suppressClassWarning{ false };
        std::vector<Token> tokens;
        std::vector<NimpleError> errors;
        std::size_t pos{ 0 };
        int line{ 1 }, column{ 1 };
        bool atEnd() const { return pos >= source.size(); }
        char peek(std::size_t n = 0) const { return pos + n < source.size() ? source[pos + n] : '\0'; }
        char take() { char c = peek(); if (c) { ++pos; if (c == '\n') { ++line; column = 1; } else ++column; } return c; }
        void add(Token::Kind kind, std::string text, int startLine, int startColumn) { tokens.push_back({ kind, std::move(text), startLine, startColumn }); }
        bool lineStart() const {
            std::size_t i = pos;
            while (i > 0 && source[i - 1] != '\n') --i;
            while (i < pos && (source[i] == ' ' || source[i] == '\t' || source[i] == '\r')) ++i;
            return i == pos;
        }
        void scanOne() {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') { take(); return; }
            if (c == '\n' || c == ';') {
                int l = line, col = column; take(); add(Token::Kind::Newline, "\n", l, col); return;
            }
            if (c == '/' && peek(1) == '/') { while (!atEnd() && peek() != '\n') take(); return; }
            if (c == '/' && peek(1) == '*') { scanBlockComment(); return; }
            if (c == '*' && lineStart() && (peek(1) == ' ' || peek(1) == '\t')) {
                while (!atEnd() && peek() != '\n') take();
                return;
            }
            if (c == '"') { scanString(); return; }
            if (c == '\'') { scanCharacter(); return; }
            if (isIdentStart(static_cast<unsigned char>(c))) { scanIdentifier(); return; }
            if (std::isdigit(static_cast<unsigned char>(c))) { scanNumber(); return; }

            int l = line, col = column;
            static const std::vector<std::string> ops = { "as!", "::", "==", "!=", "<=", ">=", "~=", "++", "--", "->", "<-" };
            for (const auto& op : ops) {
                if (source.compare(pos, op.size(), op) == 0) {
                    for (std::size_t i = 0; i < op.size(); ++i) take();
                    add(Token::Kind::Operator, op, l, col); return;
                }
            }
            if (std::string("+-*/%^=<>!").find(c) != std::string::npos) { take(); add(Token::Kind::Operator, std::string(1, c), l, col); return; }
            if (std::string("(),.:{}").find(c) != std::string::npos) { take(); add(Token::Kind::Punct, std::string(1, c), l, col); return; }
            throw NimpleError("NB1000", line, column, "无法识别的字符：" + std::string(1, c));
        }
        void scanBlockComment() {
            int startLine = line, startColumn = column; take(); take();
            while (!atEnd()) {
                if (peek() == '*' && peek(1) == '/') { take(); take(); return; }
                take();
            }
            throw NimpleError("NB1001", startLine, startColumn, "多行注释未闭合；需要使用 */ 结束。");
        }
        void scanString() {
            int startLine = line, startColumn = column;
            take();
            bool triple = peek() == '"' && peek(1) == '"';
            if (triple) { take(); take(); }
            std::string raw;
            bool closed = false;
            while (!atEnd()) {
                if (triple && peek() == '\\' && peek(1) == '"' && peek(2) == '"' && peek(3) == '"') {
                    take(); take(); take(); take(); raw += "\"\"\""; continue;
                }
                if (triple && peek() == '"' && peek(1) == '"' && peek(2) == '"') { take(); take(); take(); closed = true; break; }
                if (!triple && peek() == '"') { take(); closed = true; break; }
                if (!triple && peek() == '\n') break;
                if (peek() == '\\') { raw.push_back(take()); if (!atEnd()) raw.push_back(take()); continue; }
                raw.push_back(take());
            }
            if (!closed) throw NimpleError(triple ? "NB1002" : "NB1004", startLine, startColumn, triple ? "多行字符串未闭合。" : "普通字符串未闭合。 ");
            add(Token::Kind::String, decodeEscapes(raw, startLine, startColumn), startLine, startColumn);
        }
        void scanCharacter() {
            int startLine = line, startColumn = column; take();
            std::string raw;
            bool closed = false;
            while (!atEnd() && peek() != '\n') {
                if (peek() == '\'') { take(); closed = true; break; }
                if (peek() == '\\') { raw.push_back(take()); if (!atEnd()) raw.push_back(take()); }
                else raw.push_back(take());
            }
            if (!closed) throw NimpleError("NB1005", startLine, startColumn, "字符字面量未闭合。");
            auto decoded = decodeEscapes(raw, startLine, startColumn);
            if (decoded.empty()) throw NimpleError("NB1006", startLine, startColumn, "字符字面量必须包含一个 Unicode 标量值。");
            unsigned char c = static_cast<unsigned char>(decoded[0]);
            std::size_t n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
            if (!n || decoded.size() != n) throw NimpleError("NB1006", startLine, startColumn, "字符字面量必须且只能包含一个 Unicode 标量值。");
            add(Token::Kind::Character, decoded, startLine, startColumn);
        }
        void scanIdentifier() {
            int l = line, col = column; std::string s;
            while (isIdentPart(static_cast<unsigned char>(peek()))) s.push_back(take());
            if (s == "class") {
                if (!suppressClassWarning)
                    DiagnosticEngine::emit(std::string(), l, col, DiagnosticSeverity::Warning, "NB-W2090", "'class' 在 Nimple Basic 中已弃用。");
                s = "module";
            }
            add(Token::Kind::Identifier, std::move(s), l, col);
        }
        void scanNumber() {
            int l = line, col = column; std::string s;
            while (std::isdigit(static_cast<unsigned char>(peek()))) s.push_back(take());
            if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) { s.push_back(take()); while (std::isdigit(static_cast<unsigned char>(peek()))) s.push_back(take()); }
            if (peek() == 'e' || peek() == 'E') {
                s.push_back(take()); if (peek() == '+' || peek() == '-') s.push_back(take());
                if (!std::isdigit(static_cast<unsigned char>(peek()))) throw NimpleError("NB1009", l, col, "浮点指数缺少数字。");
                while (std::isdigit(static_cast<unsigned char>(peek()))) s.push_back(take());
            }
            add(Token::Kind::Number, std::move(s), l, col);
        }
    };

    struct Environment {
        Environment* parent{ nullptr };
        std::unordered_map<std::string, Value> values;
        explicit Environment(Environment* p = nullptr) : parent(p) {}
        bool existsLocal(const std::string& name) const { return values.find(name) != values.end(); }
        bool exists(const std::string& name) const { return values.find(name) != values.end() || (parent && parent->exists(name)); }
        Value get(const std::string& name, int line) const {
            auto it = values.find(name);
            if (it != values.end()) return it->second;
            if (parent) return parent->get(name, line);
            throw NimpleError("NB-R1001", line, 1, "未声明变量：" + name);
        }
        void declare(const std::string& name, Value value, int line) {
            if (values.find(name) != values.end()) throw NimpleError("NB-R1002", line, 1, "变量重复声明：" + name);
            values.emplace(name, std::move(value));
        }
        void assign(const std::string& name, Value value, int line) {
            auto it = values.find(name);
            if (it != values.end()) { it->second = std::move(value); return; }
            if (parent && parent->exists(name)) { parent->assign(name, std::move(value), line); return; }
            throw NimpleError("NB-R1001", line, 1, "赋值目标尚未声明：" + name);
        }
    };

    struct Expr {
        enum class Kind { Literal, Variable, Unary, Binary, Cast, Call } kind{ Kind::Literal };
        int line{ 1 };
        Value literal{ Value::noThing() };
        std::string text;
        std::unique_ptr<Expr> left;
        std::unique_ptr<Expr> right;
        std::vector<std::unique_ptr<Expr>> args;
        bool forceCast{ false };
    };

    struct Stmt {
        enum class Kind { Variable, Print, If, For, Return, Break, Next, Expression, Increment } kind{ Kind::Expression };
        int line{ 1 };
        std::string name;
        std::string declaredType;
        std::string outputTarget;
        bool declareNew{ false };
        bool autoType{ false };
        bool typeConversionOnly{ false };
        bool hasElse{ false };
        std::unique_ptr<Expr> expression;
        std::vector<std::unique_ptr<Expr>> expressions;
        std::vector<std::vector<std::unique_ptr<Stmt>>> branchBodies;
        std::vector<std::unique_ptr<Expr>> branchConditions;
        std::vector<std::unique_ptr<Stmt>> body;
        std::vector<std::unique_ptr<Stmt>> elseBody;
        int loopDepth{ 1 };
    };

    struct Parameter { std::string name; std::string type; optional<Value> defaultValue; };
    struct Method { std::string name; std::string returnType; std::vector<Parameter> params; std::vector<std::unique_ptr<Stmt>> body; int line{ 1 }; bool paramsUnknown{ false }; };

    struct ExecResult {
        enum class Signal { Normal, Return, Break, Next } signal;
        Value value;
        int depth;
        ExecResult() : signal(Signal::Normal), value(Value::voidValue()), depth(1) {}
        ExecResult(Signal resultSignal, Value resultValue, int resultDepth = 1)
            : signal(resultSignal), value(std::move(resultValue)), depth(resultDepth) {}
    };

    class Runtime;
    static Value evalExpr(const Expr& expr, Runtime& runtime, Environment& env);
    static ExecResult executeStmt(const Stmt& stmt, Runtime& runtime, Environment& env, int loopDepth);
    static ExecResult executeBlock(const std::vector<std::unique_ptr<Stmt>>& body, Runtime& runtime, Environment& env, int loopDepth);

    class Parser {
    public:
        explicit Parser(std::vector<Token> ts, bool hasSystemOpen = false,
            std::unordered_set<std::string> importedSystemNames = {},
            std::unordered_set<std::string> definedStatements = {})
            : tokens(std::move(ts)), systemOpen(hasSystemOpen),
              importedSystemNames(std::move(importedSystemNames)),
              definedStatements(std::move(definedStatements)) {}
        struct Program { std::unordered_map<std::string, Method> methods; std::vector<std::unique_ptr<Stmt>> globals; };
        Program parse() {
            Program p;
            skipSeparators();
            while (!check(Token::Kind::End)) {
                const std::size_t positionBefore = current;
                try {
                    if (isText("module")) {
                        parseModule(p);
                    }
                    else if (looksLikeMethod()) {
                        Method m = parseMethod();
                        if (p.methods.find(m.name) != p.methods.end()) fail(previous(), "NB2002", "方法重复声明：" + m.name);
                        p.methods.emplace(m.name, std::move(m));
                    }
                    else {
                        p.globals.push_back(parseStatement());
                    }
                }
                catch (const NimpleError& error) {
                    errors.push_back(error);
                    synchronize();
                }
                if (current == positionBefore && !check(Token::Kind::End)) advance();
                skipSeparators();
            }
            return p;
        }
        const std::vector<NimpleError>& diagnostics() const { return errors; }
    private:
        std::vector<Token> tokens;
        std::vector<NimpleError> errors;
        bool systemOpen{ false };
        std::unordered_set<std::string> importedSystemNames;
        std::unordered_set<std::string> definedStatements;
        std::string currentNamespace;
        std::size_t current{ 0 };
        const Token& peek(std::size_t n = 0) const { return tokens[std::min(current + n, tokens.size() - 1)]; }
        const Token& previous() const { return tokens[current ? current - 1 : 0]; }
        bool check(Token::Kind k) const { return peek().kind == k; }
        bool isText(const std::string& s) const { return peek().text == s; }
        bool isNextText(std::size_t n, const std::string& s) const { return peek(n).text == s; }
        Token advance() { if (!check(Token::Kind::End)) ++current; return previous(); }
        bool matchText(const std::string& s) { if (isText(s)) { advance(); return true; } return false; }
        bool matchKind(Token::Kind k) { if (check(k)) { advance(); return true; } return false; }
        Token expectText(const std::string& s, const std::string& message) { if (isText(s)) return advance(); fail(peek(), "NB2001", message); }
        Token expectKind(Token::Kind k, const std::string& message) { if (check(k)) return advance(); fail(peek(), "NB2001", message); }
        [[noreturn]] static void fail(const Token& t, const std::string& code, const std::string& message) { throw NimpleError(code, t.line, t.column, message); }
        void requireDefinedStatement(const Token& token, const std::string& name) const {
            if (definedStatements.find(name) == definedStatements.end())
                fail(token, "NB2016", name + " 未定义。");
        }
        void skipSeparators() { while (check(Token::Kind::Newline)) advance(); }
        bool isTypeName(const Token& t) const {
            static const std::vector<std::string> names = { "void", "int", "float", "double", "bool", "string", "char", "auto" };
            return std::find(names.begin(), names.end(), t.text) != names.end();
        }
        bool looksLikeMethod() const {
            std::size_t n = 0;
            if (peek(n).text == "public" || peek(n).text == "private" || peek(n).text == "protected") ++n;
            if (peek(n).kind != Token::Kind::Identifier || !isTypeName(peek(n))) return false;
            if (peek(n + 1).kind != Token::Kind::Identifier) return false;
            n += 2;
            while (peek(n).text == "::" && peek(n + 1).kind == Token::Kind::Identifier) n += 2;
            if (peek(n).text == "{" || peek(n).text == "(" || peek(n).text == ":") return true;
            if (peek(n).text == "with") {
                while (peek(n).kind != Token::Kind::End && peek(n).kind != Token::Kind::Newline) {
                    if (peek(n).text == ":") return true;
                    ++n;
                }
            }
            return false;
        }
        Method parseMethod() {
            if (isText("public") || isText("private") || isText("protected")) advance();
            Token type = expectKind(Token::Kind::Identifier, "方法声明缺少返回类型。");
            Token name = expectKind(Token::Kind::Identifier, "方法声明缺少名称。");
            std::string qualifiedMethodName = name.text;
            while (matchText("::")) qualifiedMethodName += "::" + expectKind(Token::Kind::Identifier, "方法作用域解析符后缺少方法名。").text;
            Method method; method.name = qualifiedMethodName; method.returnType = type.text; method.line = name.line;
            if (matchText("(")) {
                if (matchText(")")) {
                    method.paramsUnknown = true;
                } else if (matchText("void")) {
                    expectText(")", "(void) 后缺少右括号。");
                } else {
                    bool sawDefaultParameter = false;
                    do {
                        Token paramType = expectKind(Token::Kind::Identifier, "参数声明缺少类型。");
                        Token paramName = expectKind(Token::Kind::Identifier, "参数声明缺少名称。");
                        Parameter parameter{ paramName.text, paramType.text, nullopt };
                        if (matchText("=")) {
                            sawDefaultParameter = true;
                            auto defaultExpression = parseExpression();
                            if (defaultExpression->kind != Expr::Kind::Literal)
                                fail(paramName, "NB2002", "当前默认参数只允许使用字面量。");
                            if (paramType.text != "auto" && defaultExpression->literal.typeName() != paramType.text)
                                fail(paramName, "NB2003", "参数默认值类型必须与声明类型一致。");
                            parameter.defaultValue = defaultExpression->literal;
                        } else if (sawDefaultParameter) {
                            fail(paramName, "NB2004", "带默认值的参数必须位于参数列表末尾。");
                        }
                        method.params.push_back(std::move(parameter));
                    } while (matchText(","));
                    expectText(")", "方法参数列表缺少右括号。");
                }
            } else if (matchText("with")) {
                bool sawDefaultParameter = false;
                while (!isText(":") && !check(Token::Kind::End) && !check(Token::Kind::Newline)) {
                    Token paramName = expectKind(Token::Kind::Identifier, "参数声明缺少名称。");
                    Parameter parameter{ paramName.text, "", nullopt };
                    if (matchText("as")) parameter.type = expectKind(Token::Kind::Identifier, "参数类型缺失。").text;
                    if (matchText("=")) {
                        sawDefaultParameter = true;
                        auto defaultExpression = parseExpression();
                        if (defaultExpression->kind != Expr::Kind::Literal)
                            fail(paramName, "NB2002", "当前默认参数只允许使用字面量。");
                        if (!parameter.type.empty() && parameter.type != "auto" && defaultExpression->literal.typeName() != parameter.type)
                            fail(paramName, "NB2003", "参数默认值类型必须与声明类型一致。");
                        parameter.defaultValue = defaultExpression->literal;
                    } else if (sawDefaultParameter) {
                        fail(paramName, "NB2004", "带默认值的参数必须位于参数列表末尾。");
                    }
                    method.params.push_back(std::move(parameter));
                }
            }
            if (matchText("{")) {
                skipSeparators();
                method.body = parseBlock({ "}" });
                expectText("}", "方法体缺少右大括号。");
            } else {
                expectText(":", "方法体需要使用 { 开始。");
                skipSeparators();
                method.body = parseBlock({ "end" });
                expectText("end", "方法体缺少 end。");
            }
            return method;
        }
        void parseModule(Program& program) {
            Token moduleToken = advance();
            Token moduleName = expectKind(Token::Kind::Identifier, "module 后缺少模块名称。");
            const bool braceBlock = matchText("{");
            if (!braceBlock) expectText(":", "module 声明需要使用 { 开始。");
            skipSeparators();
            const std::string previousNamespace = currentNamespace;
            currentNamespace = previousNamespace.empty() ? moduleName.text : previousNamespace + "::" + moduleName.text;
            while (!check(Token::Kind::End) && !(braceBlock ? isText("}") : isText("end"))) {
                const std::size_t before = current;
                try {
                    if (isText("module")) parseModule(program);
                    else if (looksLikeMethod()) {
                        Method method = parseMethod();
                        if (method.name.find("::") == std::string::npos)
                            method.name = currentNamespace + "::" + method.name;
                        if (program.methods.find(method.name) != program.methods.end())
                            fail(moduleToken, "NB2002", "模块方法重复声明：" + method.name);
                        program.methods.emplace(method.name, std::move(method));
                    } else {
                        fail(peek(), "NB2001", "module 当前只允许包含方法声明或嵌套 module。");
                    }
                } catch (const NimpleError& error) {
                    errors.push_back(error);
                    synchronize();
                }
                if (current == before && !check(Token::Kind::End)) advance();
                skipSeparators();
            }
            if (braceBlock) expectText("}", "module 缺少右大括号。");
            else expectText("end", "module 缺少 end。");
            currentNamespace = previousNamespace;
        }

        void synchronize(const std::vector<std::string>* stops = nullptr) {
            // Recover at a statement boundary. Do not consume structural terminators
            // such as else/end; the enclosing parser still needs to see them.
            while (!check(Token::Kind::End) && !check(Token::Kind::Newline)) {
                if (stops && std::find(stops->begin(), stops->end(), peek().text) != stops->end()) return;
                advance();
            }
        }
        std::vector<std::unique_ptr<Stmt>> parseBlock(const std::vector<std::string>& stops) {
            std::vector<std::unique_ptr<Stmt>> body;
            skipSeparators();
            while (!check(Token::Kind::End) && std::find(stops.begin(), stops.end(), peek().text) == stops.end()) {
                const std::size_t positionBefore = current;
                try {
                    auto statement = parseStatement();
                    if (!check(Token::Kind::Newline) && !check(Token::Kind::End) &&
                        std::find(stops.begin(), stops.end(), peek().text) == stops.end())
                        fail(peek(), "NB2001", "语句必须以换行、分号或代码块结束符分隔。");
                    body.push_back(std::move(statement));
                }
                catch (const NimpleError& error) {
                    errors.push_back(error);
                    synchronize(&stops);
                }
                if (current == positionBefore && !check(Token::Kind::End) &&
                    std::find(stops.begin(), stops.end(), peek().text) == stops.end()) advance();
                skipSeparators();
            }
            return body;
        }
        std::unique_ptr<Stmt> parseStatement() {
            if (isText("var")) return parseVar();
            if (isText("sys") && isNextText(1, "::") &&
                (isNextText(2, "print") || isNextText(2, "printf") || isNextText(2, "printfn"))) {
                const std::string name = isNextText(2, "print") ? "print" : (isNextText(2, "printf") ? "printf" : "printfn");
                requireDefinedStatement(peek(), name);
                advance(); advance(); return parsePrint();
            }
            if (isText("sys") && isNextText(1, "::") && isNextText(2, "read")) {
                requireDefinedStatement(peek(), "read");
                advance(); advance(); return parseRead();
            }
            if (isText("print") || isText("printf") || isText("printfn")) return parsePrint();
            if (isText("if")) return parseIf();
            if (isText("for")) return parseFor();
            if (isText("return")) return parseReturn();
            if (isText("break") || isText("next")) return parseLoopControl();
            if (isText("read")) return parseRead();
            if (isText("sys") && isNextText(1, "::") && isNextText(2, "pause")) {
                advance(); advance(); Token command = advance();
                auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Expression; s->line = command.line; s->name = "pause"; return s;
            }
            if (isText("pause")) {
                Token command = advance();

                if (!systemOpen && importedSystemNames.find("pause") == importedSystemNames.end())
                    fail(command, "NB2012", "sys::pause 未定义。");
                auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Expression; s->line = command.line; s->name = "pause"; return s;
            }
            if (check(Token::Kind::Identifier) && isNextText(1, "with")) return parseWithCall();
            if (check(Token::Kind::Identifier) && (isNextText(1, "++") || isNextText(1, "--"))) return parseIncrement();
            if (isText("class") || isText("interface")) fail(peek(), "NB2090", "当前原生解释器版本尚未实现 class/interface 语义。请先使用变量、方法、表达式、条件和循环等基础功能。");
            auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Expression; s->line = peek().line; s->expression = parseExpression(); return s;
        }
        std::unique_ptr<Stmt> parseVar() {
            Token start = advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Variable; s->line = start.line;
            s->declareNew = matchText("new");
            if (matchText("(")) { expectText("auto", "自动类型推断应写作 var (auto)name。"); expectText(")", "缺少 )。"); s->autoType = true; }
            Token name = expectKind(Token::Kind::Identifier, "var 后应跟变量名。"); s->name = name.text;
            if (matchText("=")) { s->expression = parseExpression(); return s; }
            if (matchText("as") || matchText("as!")) {
                bool force = previous().text == "as!";
                std::string target = expectKind(Token::Kind::Identifier, "类型转换缺少目标类型。").text;
                if (s->declareNew) s->declaredType = target;
                else { auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Cast; e->line = start.line; e->text = target; e->forceCast = force; e->left = std::make_unique<Expr>(); e->left->kind = Expr::Kind::Variable; e->left->text = s->name; e->left->line = start.line; s->expression = std::move(e); s->typeConversionOnly = true; }
                return s;
            }
            return s;
        }
        std::unique_ptr<Stmt> parsePrint() {
            const bool qualifiedSystemCall = previous().text == "::";
            Token cmd = advance();
            requireDefinedStatement(cmd, cmd.text);
            auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Print; s->line = cmd.line; s->name = cmd.text;
            if (check(Token::Kind::Newline) || check(Token::Kind::End) || isText("->")) fail(peek(), "NB2010", cmd.text + " 缺少输出表达式。");
            s->expressions.push_back(parseExpression());
            while (!check(Token::Kind::Newline) && !check(Token::Kind::End) && !isText("->")) {
                if (isText(",")) advance();
                s->expressions.push_back(parseExpression());
            }
            if (!systemOpen && !qualifiedSystemCall && importedSystemNames.find(cmd.text) == importedSystemNames.end()) {
                fail(cmd, "NB2012", "sys::" + cmd.text + " 未定义。");
            }
            if (matchText("->")) {
                if (check(Token::Kind::Newline) || check(Token::Kind::End))
                    fail(peek(), "NB2013", "输出运算符 -> 后缺少目标；请指定 System 或文件路径。");
                if (peek().text == "System") {
                    s->outputTarget = "System";
                    advance();
                    if (!check(Token::Kind::Newline) && !check(Token::Kind::End))
                        fail(peek(), "NB2011", "System 后存在多余的输出目标内容。");
                } else {
                    if (peek().kind == Token::Kind::String) {
                        s->outputTarget = peek().text;
                        advance();
                    } else {
                        while (!check(Token::Kind::Newline) && !check(Token::Kind::End)) {
                            s->outputTarget += peek().text;
                            advance();
                        }
                        if (s->outputTarget.empty())
                            fail(peek(), "NB2013", "输出运算符 -> 后缺少目标；请指定 System 或文件路径。");
                    }
                    if (!check(Token::Kind::Newline) && !check(Token::Kind::End))
                        fail(peek(), "NB2014", "输出目标后存在多余内容。");
                }
            }
            return s;
        }
        std::unique_ptr<Stmt> parseIf() {
            Token start = advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::If; s->line = start.line;
            s->branchConditions.push_back(parseExpression());
            bool braceBlock = matchText("{");
            bool singleLine = false;
            if (!braceBlock) {
                if (!matchText("then")) fail(peek(), "NB2001", "if 条件后需要 { 或 then。");
                singleLine = !check(Token::Kind::Newline);
            }
            if (braceBlock) {
                skipSeparators();
                s->branchBodies.push_back(parseBlock({ "else", "}" }));
                expectText("}", "if 代码块缺少右大括号。");
            } else if (singleLine) {
                std::vector<std::unique_ptr<Stmt>> body;
                body.push_back(parseStatement());
                s->branchBodies.push_back(std::move(body));
            } else {
                skipSeparators();
                s->branchBodies.push_back(parseBlock({ "else", "end" }));
            }
            while (true) {
                const std::size_t beforeElseSeparators = current;
                while (check(Token::Kind::Newline)) advance();
                if (!matchText("else")) {
                    current = beforeElseSeparators;
                    break;
                }
                if (matchText("if")) {
                    s->branchConditions.push_back(parseExpression());
                    const bool elseBrace = matchText("{");
                    if (!elseBrace && !matchText("then")) fail(peek(), "NB2001", "else if 条件后需要 { 或 then。");
                    const bool elseSingle = !elseBrace && !check(Token::Kind::Newline);
                    if (braceBlock && !elseBrace && !elseSingle)
                        fail(peek(), "NB2001", "大括号代码块中的 else if 也必须使用大括号或单行 then。");
                    if (elseBrace) {
                        skipSeparators();
                        s->branchBodies.push_back(parseBlock({ "else", "}" }));
                        expectText("}", "else if 代码块缺少右大括号。");
                    } else if (elseSingle) {
                        std::vector<std::unique_ptr<Stmt>> body;
                        body.push_back(parseStatement());
                        s->branchBodies.push_back(std::move(body));
                    } else {
                        skipSeparators();
                        s->branchBodies.push_back(parseBlock({ "else", "end" }));
                    }
                } else {
                    s->hasElse = true;
                    const bool elseBrace = matchText("{");
                    const bool hasElseThen = !elseBrace && matchText("then");
                    if (!elseBrace && !hasElseThen && !check(Token::Kind::Newline))
                        fail(peek(), "NB2001", "else 后需要 {、then 或换行。");
                    const bool elseSingle = hasElseThen && !check(Token::Kind::Newline);
                    if (braceBlock && !elseBrace && !elseSingle)
                        fail(peek(), "NB2001", "大括号代码块中的 else 必须使用大括号或单行 then。");
                    if (elseBrace) {
                        skipSeparators();
                        s->elseBody = parseBlock({ "}" });
                        expectText("}", "else 代码块缺少右大括号。");
                    } else if (elseSingle) {
                        s->elseBody.push_back(parseStatement());
                    } else {
                        skipSeparators();
                        s->elseBody = parseBlock({ "end" });
                    }
                    break;
                }
            }
            if (!braceBlock && !singleLine) expectText("end", "if 代码块缺少 end。");
            return s;
        }
        std::unique_ptr<Stmt> parseFor() {
            Token start = advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::For; s->line = start.line;
            s->expression = parseExpression();
            const bool braceBlock = matchText("{");
            if (braceBlock) {
                skipSeparators();
                s->body = parseBlock({ "}" });
                expectText("}", "for 代码块缺少右大括号。");
            } else {
                expectText("then", "for 条件后需要 { 或 then。");
                if (!check(Token::Kind::Newline)) s->body.push_back(parseStatement());
                else {
                    skipSeparators();
                    s->body = parseBlock({ "end" });
                    expectText("end", "for 代码块缺少 end。");
                }
            }
            return s;
        }
        std::unique_ptr<Stmt> parseReturn() {
            Token start = advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Return; s->line = start.line;
            if (!check(Token::Kind::Newline) && !check(Token::Kind::End) && !isText("->")) s->expression = parseExpression();
            if (matchText("->")) {
                if (check(Token::Kind::Identifier)) advance();
            }
            return s;
        }
        std::unique_ptr<Stmt> parseRead() {
            const bool qualifiedSystemCall = previous().text == "::";
            Token start = advance();
            requireDefinedStatement(start, "read");
            auto s = std::make_unique<Stmt>();
            s->kind = Stmt::Kind::Expression;
            s->line = start.line;
            s->name = "read";
            s->declaredType = "string";
            if (matchText("(")) {
                Token type = expectKind(Token::Kind::Identifier, "read(type) 缺少类型名。");
                if (!isTypeName(type) || type.text == "void" || type.text == "auto")
                    fail(type, "NB2014", "read(type) 只支持 int、float、double、bool、string 和 char。");
                s->declaredType = type.text;
                expectText(")", "read(type) 缺少右括号 )。");
            }
            expectText("<-", "read 输入语句需要使用 <- System。");
            if (check(Token::Kind::Newline) || check(Token::Kind::End))
                fail(peek(), "NB2015", "read 的 <- 后缺少输入目标；请指定 System。");
            if (!isText("System")) fail(peek(), "NB2015", "当前解释器版本的输入目标仅支持 System。");
            advance();
            if (!check(Token::Kind::Newline) && !check(Token::Kind::End))
                fail(peek(), "NB2015", "输入目标后存在多余内容。");
            if (!systemOpen && !qualifiedSystemCall && importedSystemNames.find("read") == importedSystemNames.end())
                fail(start, "NB2012", "sys::read 未定义。");
            return s;
        }
        std::unique_ptr<Stmt> parseLoopControl() {
            Token start = advance(); auto s = std::make_unique<Stmt>(); s->kind = start.text == "break" ? Stmt::Kind::Break : Stmt::Kind::Next; s->line = start.line;
            if (check(Token::Kind::Number)) {
                try { s->loopDepth = std::max(1, std::stoi(advance().text)); }
                catch (...) { fail(previous(), "NB2012", "循环控制层数无效。"); }
            }
            return s;
        }
        std::unique_ptr<Stmt> parseIncrement() {
            Token name = advance(); Token op = advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Increment; s->line = name.line; s->name = name.text; s->declaredType = op.text; return s;
        }
        std::unique_ptr<Stmt> parseWithCall() {
            Token name = advance(); advance(); auto s = std::make_unique<Stmt>(); s->kind = Stmt::Kind::Expression; s->line = name.line;
            auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Call; e->text = name.text; e->line = name.line;
            while (!check(Token::Kind::Newline) && !check(Token::Kind::End) && !isText("->")) e->args.push_back(parseExpression());
            s->expression = std::move(e); return s;
        }
        std::unique_ptr<Expr> parseExpression() { return parseOr(); }
        std::unique_ptr<Expr> binary(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, int line) {
            auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Binary; e->text = std::move(op); e->line = line; e->left = std::move(left); e->right = std::move(right); return e;
        }
        std::unique_ptr<Expr> parseOr() { auto e = parseAnd(); while (matchText("or")) { Token op = previous(); e = binary(op.text, std::move(e), parseAnd(), op.line); } return e; }
        std::unique_ptr<Expr> parseAnd() { auto e = parseEquality(); while (matchText("and")) { Token op = previous(); e = binary(op.text, std::move(e), parseEquality(), op.line); } return e; }
        std::unique_ptr<Expr> parseEquality() { auto e = parseComparison(); while (isText("==") || isText("!=") || isText("~=")) { Token op = advance(); e = binary(op.text, std::move(e), parseComparison(), op.line); } return e; }
        std::unique_ptr<Expr> parseComparison() { auto e = parseAdd(); while (isText("<") || isText(">") || isText("<=") || isText(">=")) { Token op = advance(); e = binary(op.text, std::move(e), parseAdd(), op.line); } return e; }
        std::unique_ptr<Expr> parseAdd() { auto e = parseMultiply(); while (isText("+") || isText("-")) { Token op = advance(); e = binary(op.text, std::move(e), parseMultiply(), op.line); } return e; }
        std::unique_ptr<Expr> parseMultiply() { auto e = parseUnary(); while (isText("*") || isText("/") || isText("%")) { Token op = advance(); e = binary(op.text, std::move(e), parseUnary(), op.line); } return e; }
        std::unique_ptr<Expr> parseUnary() {
            if (isText("+") || isText("-") || isText("no")) { Token op = advance(); auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Unary; e->text = op.text; e->line = op.line; e->left = parseUnary(); return e; }
            return parsePower();
        }
        std::unique_ptr<Expr> parsePower() {
            auto e = parseCast();
            if (matchText("^")) { Token op = previous(); e = binary(op.text, std::move(e), parseUnary(), op.line); if (isText("^")) fail(peek(), "NB2013", "不允许连续幂运算；请使用括号明确计算顺序。"); }
            return e;
        }
        std::unique_ptr<Expr> parseCast() {
            auto e = parsePrimary();
            while (isText("as") || isText("as!")) {
                Token op = advance(); Token type = expectKind(Token::Kind::Identifier, "类型转换缺少目标类型。");
                auto cast = std::make_unique<Expr>(); cast->kind = Expr::Kind::Cast; cast->line = op.line; cast->text = type.text; cast->forceCast = op.text == "as!"; cast->left = std::move(e); e = std::move(cast);
            }
            return e;
        }
        std::unique_ptr<Expr> parsePrimary() {
            Token t = advance();
            if (t.text == "::") {
                Token first = expectKind(Token::Kind::Identifier, "全局作用域解析符 :: 后需要名称。");
                std::string globalName = first.text;
                while (matchText("::")) globalName += "::" + expectKind(Token::Kind::Identifier, "作用域解析符后缺少成员名称。").text;
                if (matchText("(")) {
                    auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Call; e->line = t.line; e->text = globalName;
                    if (!isText(")")) { do { e->args.push_back(parseExpression()); } while (matchText(",")); }
                    expectText(")", "全局方法调用缺少右括号 )。"); return e;
                }
                auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Variable; e->line = t.line; e->text = globalName; return e;
            }
            if (t.kind == Token::Kind::Number) {
                auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Literal; e->line = t.line;
                if (t.text.find_first_of(".eE") == std::string::npos) {
                    try { long long v = std::stoll(t.text); if (v < INT32_MIN || v > INT32_MAX) fail(t, "NB2020", "int 字面量超出 32 位范围。"); e->literal = Value::integer(static_cast<int32_t>(v)); }
                    catch (const NimpleError&) { throw; }
                    catch (...) { fail(t, "NB2020", "整数格式无效。"); }
                }
                else { try { e->literal = Value::floating(std::stod(t.text)); } catch (...) { fail(t, "NB2021", "浮点字面量格式无效。"); } }
                return e;
            }
            if (t.kind == Token::Kind::String) { auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Literal; e->line = t.line; e->literal = Value::string(t.text); return e; }
            if (t.kind == Token::Kind::Character) { auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Literal; e->line = t.line; e->literal = Value::character(t.text); return e; }
            if (t.text == "(") { auto e = parseExpression(); expectText(")", "表达式缺少右括号 )。"); return e; }
            if (t.kind == Token::Kind::Identifier) {
                bool qualified = false;
                std::string qualifiedName = t.text;
                while (matchText("::")) {
                    qualified = true;
                    Token part = expectKind(Token::Kind::Identifier, "作用域解析符 :: 后需要成员名称。");
                    qualifiedName += "::" + part.text;
                }
                if (qualifiedName.rfind("global::", 0) == 0) qualifiedName.erase(0, 8);
                if (qualifiedName == "true" || qualifiedName == "false" || qualifiedName == "Null" || qualifiedName == "NoThing" || qualifiedName == "NoType" || qualifiedName == "NaN" || qualifiedName == "Infinity") {
                    auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Literal; e->line = t.line;
                    if (qualifiedName == "true" || qualifiedName == "false") e->literal = Value::boolean(qualifiedName == "true");
                    else if (qualifiedName == "Null") e->literal = Value::nullValue();
                    else if (qualifiedName == "NoType") e->literal = Value::noType();
                    else if (qualifiedName == "NaN") e->literal = Value::floating(std::numeric_limits<double>::quiet_NaN());
                    else if (qualifiedName == "Infinity") e->literal = Value::floating(std::numeric_limits<double>::infinity());
                    else e->literal = Value::noThing();
                    return e;
                }
                if (matchText("(")) {
                    if (!qualified && !currentNamespace.empty() && qualifiedName != "read")
                        qualifiedName = currentNamespace + "::" + qualifiedName;
                    const bool isSystemRead = qualifiedName == "sys::read" || qualifiedName == "read";
                    if (isSystemRead) requireDefinedStatement(t, "read");
                    if (isSystemRead && !systemOpen && !qualified && importedSystemNames.find("read") == importedSystemNames.end())
                        fail(t, "NB2012", "sys::read 未定义。");
                    auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Call; e->line = t.line; e->text = isSystemRead ? "read" : qualifiedName;
                    if (!isText(")")) {
                        do {
                            if (isSystemRead && check(Token::Kind::Identifier) && isTypeName(peek())) {
                                Token typ = advance(); auto literal = std::make_unique<Expr>(); literal->kind = Expr::Kind::Literal; literal->line = typ.line; literal->literal = Value::string(typ.text); e->args.push_back(std::move(literal));
                            }
                            else e->args.push_back(parseExpression());
                        } while (matchText(","));
                    }
                    expectText(")", "方法调用缺少右括号 )。"); return e;
                }
                auto e = std::make_unique<Expr>(); e->kind = Expr::Kind::Variable; e->line = t.line; e->text = qualifiedName; return e;
            }
            fail(t, "NB2001", "此处需要字面量、变量名或括号表达式。");
        }
    };

    struct BuiltinSignature {
        std::vector<std::string> parameters;
        std::string result;
    };
    static optional<BuiltinSignature> ezWindowBuiltinSignature(const std::string& name) {
        static const std::unordered_map<std::string, BuiltinSignature> signatures = {
            {"ezWindowCreate", {{"string", "int", "int", "string"}, "int"}},
            {"ezWindowLabel", {{"int", "string"}, "int"}},
            {"ezWindowButton", {{"int", "string"}, "int"}},
            {"ezWindowEntry", {{"int", "string"}, "int"}},
            {"ezWindowTextBox", {{"int", "string"}, "int"}},
            {"ezWindowCheckBox", {{"int", "string"}, "int"}},
            {"ezWindowPasswordEntry", {{"int", "string"}, "int"}},
            {"ezWindowComboBox", {{"int", "string"}, "int"}},
            {"ezWindowRadioButton", {{"int", "string"}, "int"}},
            {"ezWindowSlider", {{"int", "int", "int"}, "int"}},
            {"ezWindowProgressBar", {{"int", "int", "int"}, "int"}},
            {"ezWindowWebView", {{"int", "string"}, "int"}},
            {"ezWindowWebViewNavigate", {{"int", "string"}, "void"}},
            {"ezWindowWebViewSetHtml", {{"int", "string"}, "void"}},
            {"ezWindowSetValue", {{"int", "int"}, "void"}},
            {"ezWindowGetValue", {{"int"}, "int"}},
            {"ezWindowSetText", {{"int", "string"}, "void"}},
            {"ezWindowGetText", {{"int"}, "string"}},
            {"ezWindowSetChecked", {{"int", "bool"}, "void"}},
            {"ezWindowGetChecked", {{"int"}, "bool"}},
            {"ezWindowSetTitle", {{"int", "string"}, "void"}},
            {"ezWindowShow", {{"int"}, "void"}},
            {"ezWindowSetPosition", {{"int", "int", "int"}, "void"}},
            {"ezWindowSetSize", {{"int", "int", "int"}, "void"}},
            {"ezWindowSetBounds", {{"int", "int", "int", "int", "int"}, "void"}},
            {"ezWindowSetRelativeBounds", {{"int", "int", "int", "int", "int"}, "void"}},
            {"ezWindowOnClick", {{"int", "string"}, "void"}},
            {"ezWindowOnChange", {{"int", "string"}, "void"}},
            {"ezWindowMainLoop", {{}, "void"}},
            {"ezWindowQuit", {{}, "void"}},
            {"ezWindowMessage", {{"string", "string"}, "void"}},
            {"ezWindowPack", {{"int"}, "void"}},
            {"ezWindowInvalidStyleExtension", {{"string"}, "void"}},
            {"ezWindowInvalidLayout", {{}, "void"}}
        };
        auto it = signatures.find(name);
        if (it == signatures.end()) return nullopt;
        return it->second;
    }

    // Whole-program static analysis for the implemented Nimple subset. This runs
    // after preprocessing/parsing and before any program side effects.
    class TypeChecker {
    public:
        using Program = Parser::Program;
        explicit TypeChecker(const Program& p, bool requireEntryPoint = true) : program(p), requireEntryPoint(requireEntryPoint) {}
        std::vector<NimpleError> check() {
            validateMethodSignatures();
            scopes.clear();
            scopes.emplace_back(); // globals
            for (const auto& statement : program.globals) checkStatement(*statement, 0);
            const Scope globals = scopes.front();
            auto windowIt = program.methods.find("Window");
            auto mainIt = program.methods.find("Main");
            if (!requireEntryPoint) {
                // A .nbf is a declaration/header unit and need not define an app entry.
            } else if (windowIt != program.methods.end()) {
                if (!windowIt->second.params.empty() || windowIt->second.paramsUnknown) error(windowIt->second.line, "NB-T1003", "EasyWindow 入口 Window 必须明确声明为无参数方法；请使用 (void) 或省略参数列表。");
                if (windowIt->second.returnType != "void") error(windowIt->second.line, "NB-T1004", "EasyWindow 入口 Window 必须返回 void。");
            } else if (mainIt != program.methods.end()) {
                if (!mainIt->second.params.empty() || mainIt->second.paramsUnknown) error(mainIt->second.line, "NB-T1003", "入口方法 Main 必须明确声明为无参数方法；请使用 (void) 或省略参数列表。");
                if (mainIt->second.returnType != "int" && mainIt->second.returnType != "void")
                    error(mainIt->second.line, "NB-T1004", "入口方法 Main 必须返回 int 或 void。");
            } else {
                error(1, "NB-T1002", "找不到入口方法 Main；EasyWindow 程序可以改用 void Window。");
            }

            for (const auto& entry : program.methods) {
                const Method& method = entry.second;
                scopes.clear();
                scopes.push_back(globals);
                scopes.emplace_back(); // method-local scope
                currentMethod = &method;
                loopDepth = 0;
                for (const auto& param : method.params) {
                    if (!lookupLocal(param.name))
                        scopes.back().emplace(param.name, Symbol{ param.type.empty() || param.type == "auto" ? "any" : param.type, true, !param.type.empty() && param.type != "auto" });
                }
                const bool canReachEnd = checkBlock(method.body, false);
                const bool isIncludingHook = method.name == "including" ||
                    (method.name.size() > 11 && method.name.compare(method.name.size() - 11, 11, "::including") == 0);
                if (method.returnType != "void" && canReachEnd && !isIncludingHook)
                    error(method.line, "NB-T1006", "非 void 方法 " + method.name + " 存在未返回值的执行路径。");
                currentMethod = nullptr;
            }
            return diagnostics;
        }
    private:
        struct Symbol {
            std::string type;
            bool initialized;
            bool fixedType;
            Symbol() : initialized(false), fixedType(false) {}
            Symbol(const std::string& symbolType, bool isInitialized = false, bool hasFixedType = false)
                : type(symbolType), initialized(isInitialized), fixedType(hasFixedType) {}
        };
        using Scope = std::unordered_map<std::string, Symbol>;
        const Program& program;
        bool requireEntryPoint{ true };
        std::vector<Scope> scopes;
        std::vector<NimpleError> diagnostics;
        const Method* currentMethod{ nullptr };
        int loopDepth{ 0 };

        void error(int line, const std::string& code, const std::string& message) {
            diagnostics.emplace_back(code, line, 1, message);
        }
        static bool knownType(const std::string& type) {
            return type == "void" || type == "int" || type == "float" || type == "double" ||
                type == "bool" || type == "string" || type == "char" || type == "auto" ||
                type == "any" || type == "Null" || type == "NoThing" || type == "NoType";
        }
        static bool primitiveType(const std::string& type) {
            return type == "int" || type == "float" || type == "double" || type == "bool" || type == "string" || type == "char";
        }
        static bool numericType(const std::string& type) {
            return type == "int" || type == "float" || type == "double";
        }
        static bool isDynamic(const std::string& type) { return type == "any"; }
        // Explicit casts may request every conversion supported by convertValue.
        static bool canConvert(const std::string& from, const std::string& to) {
            if (from == "any" || to == "any") return true;
            if (from == "void" || to == "void" || to == "auto") return false;
            if (from == to) return true;
            if (to == "string") return true;
            if (from == "NoType") return to == "string";
            if (from == "Null" || from == "NoThing") return false;
            if (to == "bool") return from == "int" || from == "float" || from == "double" || from == "char" || from == "string";
            if (to == "int") return numericType(from) || from == "bool" || from == "string" || from == "char";
            if (to == "float" || to == "double") return numericType(from) || from == "bool" || from == "string";
            if (to == "char") return from == "string" || from == "bool" || from == "int" || from == "char";
            return false;
        }
        // Implicit conversions must be guaranteed by the static source type.
        // Potentially failing conversions (string->int, double->int, int->char)
        // require an explicit as/as! cast, unless the exact constant is known valid.
        static bool canImplicitConvert(const std::string& from, const std::string& to) {
            if (from == to) return true;
            if (from == "any" || to == "any") return true;
            if (from == "void" || to == "void" || to == "auto") return false;
            if (to == "string") return true;
            if (from == "bool" && (to == "int" || to == "float" || to == "double")) return true;
            if (numericType(from) && numericType(to)) {
                if (from == "int" && (to == "float" || to == "double")) return true;
                if (from == "float" && to == "double") return true;
            }
            return false;
        }
        static bool canConvertExpression(const Expr& expr, const std::string& target) {
            if (expr.kind == Expr::Kind::Literal) {
                // Validate constants more precisely than their broad source type:
                // e.g. "not-an-int" is statically known to fail an int conversion.
                return convertValue(expr.literal, target).has_value();
            }
            return true; // non-constant explicit casts are checked by the runtime
        }
        static bool canImplicitConvertExpression(const Expr& expr, const std::string& from, const std::string& to) {
            if (canImplicitConvert(from, to)) return true;
            return expr.kind == Expr::Kind::Literal && convertValue(expr.literal, to).has_value();
        }
        void validateNamedType(const std::string& type, int line, const std::string& context, bool allowVoid = false, bool allowAuto = false) {
            if (!knownType(type) || type == "any" || type == "Null" || type == "NoThing" || type == "NoType") {
                error(line, "NB-T1007", context + "使用了未知类型“" + type + "”。");
                return;
            }
            if (type == "void" && !allowVoid) error(line, "NB-T1008", context + "不能使用 void 类型。");
            if (type == "auto" && !allowAuto) error(line, "NB-T1009", context + "不能使用 auto 类型。");
        }
        void validateMethodSignatures() {
            for (const auto& entry : program.methods) {
                const Method& m = entry.second;
                validateNamedType(m.returnType, m.line, "方法返回类型", true, false);
                std::unordered_set<std::string> paramNames;
                for (const auto& param : m.params) {
                    if (!paramNames.insert(param.name).second)
                        error(m.line, "NB-T1010", "方法 " + m.name + " 的参数重复声明：" + param.name + "。");
                    if (param.type.empty())
                        error(m.line, "NB-T1046", "参数“" + param.name + "”必须显式标注类型（as int/string 等），才能进行完整静态类型检查。");
                    else validateNamedType(param.type, m.line, "参数 " + param.name, false, false);
                }
            }
        }
        Symbol* lookupLocal(const std::string& name) {
            if (scopes.empty()) return nullptr;
            auto it = scopes.back().find(name);
            return it == scopes.back().end() ? nullptr : &it->second;
        }
        Symbol* lookup(const std::string& name) {
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
                auto found = it->find(name);
                if (found != it->end()) return &found->second;
            }
            return nullptr;
        }
        std::string lookupType(const std::string& name, int line) {
            if (auto* symbol = lookup(name)) {
                if (!symbol->initialized) error(line, "NB-T1011", "变量“" + name + "”在初始化前被使用。");
                return symbol->type;
            }
            auto method = program.methods.find(name);
            if (method != program.methods.end() && method->second.params.empty() && !method->second.paramsUnknown) return method->second.returnType;
            error(line, "NB-T1012", "未声明的变量或无参数方法：“" + name + "”。");
            return "any";
        }
        static std::string literalType(const Value& value) { return value.typeName(); }
        static std::string numericJoin(const std::string& a, const std::string& b) {
            if (a == "double" || b == "double") return "double";
            if (a == "float" || b == "float") return "float";
            return "int";
        }
        static bool comparable(const std::string& a, const std::string& b) {
            if (a == "any" || b == "any") return true;
            if (a == b) return true;
            if (numericType(a) && numericType(b)) return true;
            if ((a == "string" || a == "char") && (b == "string" || b == "char")) return true;
            return false;
        }
        std::string checkExpression(const Expr& expr) {
            switch (expr.kind) {
            case Expr::Kind::Literal:
                return literalType(expr.literal);
            case Expr::Kind::Variable:
                return lookupType(expr.text, expr.line);
            case Expr::Kind::Unary: {
                const std::string operand = checkExpression(*expr.left);
                if (expr.text == "no") {
                    if (operand != "bool" && operand != "any")
                        error(expr.line, "NB-T1013", "逻辑运算符 no 需要 bool 操作数，实际为 " + operand + "。");
                    return "bool";
                }
                if (!numericType(operand) && operand != "any")
                    error(expr.line, "NB-T1014", "一元运算符 " + expr.text + " 需要数值操作数，实际为 " + operand + "。");
                return operand;
            }
            case Expr::Kind::Binary: {
                const std::string left = checkExpression(*expr.left);
                const std::string right = checkExpression(*expr.right);
                const std::string& op = expr.text;
                if (left == "void" || right == "void") {
                    error(expr.line, "NB-T1048", "void 方法调用不能参与运算。");
                    return "any";
                }
                if (op == "+") {
                    if (left == "string" || left == "char" || right == "string" || right == "char") return "string";
                    if ((!numericType(left) && left != "any") || (!numericType(right) && right != "any"))
                        error(expr.line, "NB-T1015", "+ 需要数值操作数，或至少一个 string/char 操作数；实际为 " + left + " 和 " + right + "。");
                    return (isDynamic(left) || isDynamic(right)) ? "any" : numericJoin(left, right);
                }
                if (op == "-" || op == "*" || op == "/" || op == "%" || op == "^") {
                    if ((!numericType(left) && left != "any") || (!numericType(right) && right != "any"))
                        error(expr.line, "NB-T1016", "运算符 " + op + " 需要数值操作数，实际为 " + left + " 和 " + right + "。");
                    if (isDynamic(left) || isDynamic(right)) return "any";
                    if (op == "^") return "double";
                    if (op == "/" && left == "int" && right == "int") return "int";
                    if (op == "%" && left == "int" && right == "int") return "int";
                    return numericJoin(left, right);
                }
                if (op == "and" || op == "or") {
                    if ((left != "bool" && left != "any") || (right != "bool" && right != "any"))
                        error(expr.line, "NB-T1017", "逻辑运算符 " + op + " 只接受 bool 操作数，实际为 " + left + " 和 " + right + "。");
                    return "bool";
                }
                if (op == "==" || op == "!=") {
                    if (!comparable(left, right)) error(expr.line, "NB-T1018", "不能比较不兼容的类型：" + left + " 和 " + right + "。");
                    return "bool";
                }
                if (op == "~=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
                    if ((!numericType(left) && left != "any") || (!numericType(right) && right != "any"))
                        error(expr.line, "NB-T1019", "比较运算符 " + op + " 只接受数值操作数，实际为 " + left + " 和 " + right + "。");
                    return "bool";
                }
                return "any";
            }
            case Expr::Kind::Cast: {
                const std::string from = checkExpression(*expr.left);
                validateNamedType(expr.text, expr.line, "类型转换目标", false, false);
                if (!canConvert(from, expr.text) || !canConvertExpression(*expr.left, expr.text))
                    error(expr.line, "NB-T1020", "不支持从 " + from + " 转换到 " + expr.text + "（或该常量无法完成转换）。");
                return expr.text;
            }
            case Expr::Kind::Call: {
                std::vector<std::string> argTypes;
                argTypes.reserve(expr.args.size());
                for (const auto& arg : expr.args) argTypes.push_back(checkExpression(*arg));
                if (expr.text == "ezWindowMissingInstance") {
                    error(expr.line, "NB-EZ1003", "EasyWindow：未指定窗口。必须在 void Window 中声明 var new (auto)Name = EasyWindow.Window。");
                    return "void";
                }
                if (expr.text == "typeof") {
                    if (argTypes.size() != 1) error(expr.line, "NB-T1021", "typeof 需要且只接受一个参数。");
                    return "string";
                }
                if (expr.text == "read") {
                    if (argTypes.size() > 1) error(expr.line, "NB-T1022", "read 最多接受一个类型参数。");
                    if (argTypes.empty()) return "string";
                    if (argTypes[0] != "string") {
                        error(expr.line, "NB-T1023", "read 的类型参数必须是类型名称字符串。");
                        return "any";
                    }
                    if (!expr.args.empty() && expr.args[0]->kind == Expr::Kind::Literal && expr.args[0]->literal.type == ValueType::String) {
                        const std::string target = dataGet<std::string>(expr.args[0]->literal.data);
                        if (!primitiveType(target)) {
                            error(expr.line, "NB-T1024", "read 不支持输入类型“" + target + "”。");
                            return "any";
                        }
                        return target;
                    }
                    error(expr.line, "NB-T1024", "read 的参数必须是字面量类型名称，例如 read(int) 或 read(\"string\")。");
                    return "any";
                }
                if (expr.text == "ezWindowInvalidLayout") { throw NimpleError("NB-EZ1002", expr.line, 1, "EasyWindow：布局尺寸必须大于 0，且百分比边距不能占满整个方向。"); }
                if (expr.text == "ezWindowInvalidStyleExtension")
                    error(expr.line, "NB-EZ1010", "EasyWindow Style 文件必须使用 .css 后缀。" );
                if (const auto builtin = ezWindowBuiltinSignature(expr.text)) {
                    if (argTypes.size() != builtin->parameters.size()) {
                        error(expr.line, "NB-T1026", "内置函数 " + expr.text + " 需要 " + std::to_string(builtin->parameters.size()) + " 个参数，实际收到 " + std::to_string(argTypes.size()) + " 个。");
                    }
                    const std::size_t count = std::min(argTypes.size(), builtin->parameters.size());
                    for (std::size_t i = 0; i < count; ++i) {
                        if (argTypes[i] != builtin->parameters[i] && argTypes[i] != "any")
                            error(expr.args[i]->line, "NB-T1050", "内置函数 " + expr.text + " 的第 " + std::to_string(i + 1) + " 个参数要求 " + builtin->parameters[i] + "，实际为 " + argTypes[i] + "。");
                    }
                    return builtin->result;
                }
                // Verify literal event handler names early, rather than waiting for a click.
                if (expr.text == "onClick" || expr.text == "onChange") {
                    const std::size_t callbackIndex = 1;
                    if (expr.args.size() > callbackIndex && expr.args[callbackIndex]->kind == Expr::Kind::Literal &&
                        expr.args[callbackIndex]->literal.type == ValueType::String) {
                        const std::string callbackName = dataGet<std::string>(expr.args[callbackIndex]->literal.data);
                        auto callback = program.methods.find(callbackName);
                        if (callback == program.methods.end()) {
                            error(expr.line, "NB-T1051", "EasyWindow 事件回调方法未声明：“" + callbackName + "”。");
                        } else {
                            const std::size_t requiredParams = expr.text == "onClick" ? 0u : 1u;
                            if (!callback->second.paramsUnknown && callback->second.params.size() != requiredParams)
                                error(expr.line, "NB-T1052", "EasyWindow 的 " + expr.text + " 回调“" + callbackName + "”必须接收 " + std::to_string(requiredParams) + " 个参数。");
                            else if (!callback->second.paramsUnknown && expr.text == "onChange" && callback->second.params[0].type != "string")
                                error(expr.line, "NB-T1053", "onChange 回调的唯一参数必须是 string。");
                            if (callback->second.returnType != "void")
                                error(expr.line, "NB-T1054", "EasyWindow 事件回调方法必须返回 void。");
                        }
                    }
                }
                auto method = program.methods.find(expr.text);
                if (method == program.methods.end()) {
                    error(expr.line, "NB-T1025", expr.text + " 未定义。");
                    return "any";
                }
                if (!method->second.paramsUnknown) {
                    std::size_t requiredParameters = 0;
                    for (const auto& parameter : method->second.params) if (!parameter.defaultValue) ++requiredParameters;
                    if (argTypes.size() < requiredParameters || argTypes.size() > method->second.params.size()) {
                        error(expr.line, "NB-T1026", "方法 " + expr.text + " 需要 " + std::to_string(requiredParameters) + ".." + std::to_string(method->second.params.size()) + " 个参数，实际收到 " + std::to_string(argTypes.size()) + " 个。");
                    }
                    const std::size_t count = std::min(argTypes.size(), method->second.params.size());
                    for (std::size_t i = 0; i < count; ++i) {
                        const std::string& target = method->second.params[i].type;
                        if (argTypes[i] == "void")
                            error(expr.args[i]->line, "NB-T1027", "void 方法调用不能作为参数值传递。");
                        else if (!target.empty() && target != "auto" && (!canImplicitConvertExpression(*expr.args[i], argTypes[i], target)))
                            error(expr.args[i]->line, "NB-T1027", "方法 " + expr.text + " 的参数 " + method->second.params[i].name + " 需要 " + target + "，实际为 " + argTypes[i] + "。");
                    }
                }
                return method->second.returnType;
            }
            }
            return "any";
        }
        static bool isLiteralString(const Expr& expr, std::string& value) {
            if (expr.kind != Expr::Kind::Literal || expr.literal.type != ValueType::String) return false;
            value = dataGet<std::string>(expr.literal.data);
            return true;
        }
        void checkFormat(const Stmt& stmt, const std::vector<std::string>& argTypes) {
            if (stmt.name != "printf" && stmt.name != "printfn") return;
            if (stmt.expressions.empty()) return;
            std::string format;
            if (!isLiteralString(*stmt.expressions.front(), format)) {
                error(stmt.line, "NB-T1047", "printf/printfn 的格式串必须是字符串字面量，才能在编译期验证占位符。");
                return;
            }
            std::vector<std::string> specs;
            for (std::size_t i = 0; i + 3 < format.size(); ++i) {
                if (format[i] != '{' || format[i + 1] != '%') continue;
                const auto close = format.find('}', i + 2);
                if (close == std::string::npos) break;
                const std::string spec = format.substr(i + 1, close - i - 1);
                if (spec == "%s" || spec == "%d" || spec == "%f" || spec == "%c" || spec == "%A") {
                    specs.push_back(spec);
                    i = close;
                }
            }
            if (specs.size() != argTypes.size() - 1)
                error(stmt.line, "NB-T1028", "格式化占位符数量（" + std::to_string(specs.size()) + "）与参数数量（" + std::to_string(argTypes.size() - 1) + "）不匹配。");
            const std::size_t count = std::min(specs.size(), argTypes.size() > 0 ? argTypes.size() - 1 : 0);
            for (std::size_t i = 0; i < count; ++i) {
                const std::string& actual = argTypes[i + 1];
                bool ok = true;
                if (specs[i] == "%s") ok = actual == "string" || actual == "char" || actual == "any" || actual == "NoType";
                else if (specs[i] == "%d") ok = actual == "int" || actual == "any" || actual == "NoType";
                else if (specs[i] == "%f") ok = numericType(actual) || actual == "any" || actual == "NoType";
                else if (specs[i] == "%c") ok = actual == "char" || actual == "any" || actual == "NoType";
                if (!ok) error(stmt.line, "NB-T1029", "格式占位符 " + specs[i] + " 与参数类型 " + actual + " 不匹配。");
            }
        }
        static bool sameScopeShape(const std::vector<Scope>& a, const std::vector<Scope>& b) { return a.size() == b.size(); }
        void mergeOuterScopes(const std::vector<std::vector<Scope>>& paths, const std::vector<Scope>& baseline, int line) {
            scopes = baseline;
            if (paths.empty()) return;
            for (std::size_t depth = 0; depth < baseline.size(); ++depth) {
                for (auto& pair : scopes[depth]) {
                    bool allInitialized = true;
                    std::string mergedType;
                    bool sameType = true;
                    for (const auto& path : paths) {
                        auto found = path[depth].find(pair.first);
                        if (found == path[depth].end()) { allInitialized = false; sameType = false; continue; }
                        allInitialized = allInitialized && found->second.initialized;
                        if (mergedType.empty()) mergedType = found->second.type;
                        else if (mergedType != found->second.type) sameType = false;
                    }
                    pair.second.initialized = allInitialized;
                    if (!sameType) {
                        if (allInitialized)
                            error(line, "NB-T1049", "不同分支为同一变量赋予了不兼容的类型；请在各分支中保持类型一致或显式声明类型。");
                        pair.second.type = "any";
                    }
                    else if (!mergedType.empty()) pair.second.type = mergedType;
                }
            }
        }
        bool checkBlock(const std::vector<std::unique_ptr<Stmt>>& body, bool makeScope) {
            if (makeScope) scopes.emplace_back();
            bool canReachEnd = true;
            for (const auto& stmt : body) {
                const bool stmtCanReach = checkStatement(*stmt, loopDepth);
                if (canReachEnd) canReachEnd = stmtCanReach;
            }
            if (makeScope) scopes.pop_back();
            return canReachEnd;
        }
        bool checkStatement(const Stmt& stmt, int loops) {
            switch (stmt.kind) {
            case Stmt::Kind::Variable: {
                if (stmt.declareNew) {
                    if (lookupLocal(stmt.name)) {
                        error(stmt.line, "NB-T1030", "变量重复声明：“" + stmt.name + "”。");
                        if (stmt.expression) (void)checkExpression(*stmt.expression);
                        return true;
                    }
                    std::string type = "any";
                    bool initialized = false;
                    bool fixed = false;
                    if (!stmt.declaredType.empty()) {
                        validateNamedType(stmt.declaredType, stmt.line, "变量 " + stmt.name, false, false);
                        type = stmt.declaredType;
                        fixed = true;
                    }
                    if (stmt.expression) {
                        const std::string valueType = checkExpression(*stmt.expression);
                        if (valueType == "void")
                            error(stmt.line, "NB-T1031", "void 方法调用不能用于初始化变量“" + stmt.name + "”。");
                        else if (!stmt.declaredType.empty() && (!canImplicitConvertExpression(*stmt.expression, valueType, stmt.declaredType)))
                            error(stmt.line, "NB-T1031", "变量“" + stmt.name + "”初始化类型为 " + valueType + "，不能转换为 " + stmt.declaredType + "。");
                        if (stmt.declaredType.empty()) type = valueType;
                        initialized = true;
                        fixed = true;
                    }
                    else if (stmt.autoType) {
                        type = "any";
                    }
                    scopes.back().emplace(stmt.name, Symbol{ type, initialized, fixed });
                } else {
                    Symbol* symbol = lookup(stmt.name);
                    if (!symbol) {
                        error(stmt.line, "NB-T1032", stmt.name + " 未定义。");
                        if (stmt.expression) (void)checkExpression(*stmt.expression);
                        return true;
                    }
                    if (stmt.expression) {
                        const std::string valueType = checkExpression(*stmt.expression);
                        if (stmt.typeConversionOnly && stmt.expression->kind == Expr::Kind::Cast) {
                            symbol->type = stmt.expression->text;
                            symbol->fixedType = true;
                            symbol->initialized = true;
                        } else if (!symbol->initialized && symbol->fixedType) {
                            if (symbol->type != valueType)
                                error(stmt.line, "NB-T1033", "赋值类型不匹配：变量“" + stmt.name + "”的声明类型为 " + symbol->type + "，右侧表达式为 " + valueType + "；需要显式使用 as/as! 转换。");
                            else symbol->initialized = true;
                        } else if (!symbol->initialized || symbol->type == "any") {
                            symbol->type = valueType;
                            symbol->initialized = true;
                        } else if (symbol->type != valueType) {
                            error(stmt.line, "NB-T1033", "赋值类型不匹配：变量“" + stmt.name + "”的类型为 " + symbol->type + "，右侧表达式为 " + valueType + "；需要显式使用 as/as! 转换。");
                        }
                    } else if (!symbol->initialized) {
                        // Reading a declaration without assigning does not initialize it.
                    }
                }
                return true;
            }
            case Stmt::Kind::Print: {
                std::vector<std::string> types;
                for (const auto& expr : stmt.expressions) {
                    const std::string type = checkExpression(*expr);
                    if (type == "void") error(expr->line, "NB-T1045", "void 方法调用不能作为 print/printf/printfn 的输出值。");
                    types.push_back(type);
                }
                checkFormat(stmt, types);
                return true;
            }
            case Stmt::Kind::If: {
                for (const auto& condition : stmt.branchConditions) {
                    const std::string type = checkExpression(*condition);
                    if (type != "bool" && type != "any")
                        error(condition->line, "NB-T1034", "if 条件必须是 bool，实际为 " + type + "。");
                }
                const auto baseline = scopes;
                std::vector<std::vector<Scope>> paths;
                std::vector<bool> branchFlows;
                for (const auto& branch : stmt.branchBodies) {
                    scopes = baseline;
                    const bool flow = checkBlock(branch, true);
                    branchFlows.push_back(flow);
                    paths.push_back(scopes);
                }
                bool hasElse = stmt.hasElse;
                if (hasElse) {
                    scopes = baseline;
                    branchFlows.push_back(checkBlock(stmt.elseBody, true));
                    paths.push_back(scopes);
                } else {
                    branchFlows.push_back(true);
                    paths.push_back(baseline);
                }
                std::vector<std::vector<Scope>> reachablePaths;
                for (std::size_t i = 0; i < paths.size() && i < branchFlows.size(); ++i)
                    if (branchFlows[i]) reachablePaths.push_back(paths[i]);
                mergeOuterScopes(reachablePaths, baseline, stmt.line);
                return std::any_of(branchFlows.begin(), branchFlows.end(), [](bool x) { return x; });
            }
            case Stmt::Kind::For: {
                const std::string condition = stmt.expression ? checkExpression(*stmt.expression) : "any";
                if (condition != "bool" && condition != "any")
                    error(stmt.line, "NB-T1035", "for 条件必须是 bool，实际为 " + condition + "。");
                const auto beforeLoop = scopes;
                ++loopDepth;
                (void)checkBlock(stmt.body, true);
                --loopDepth;
                // A for loop can execute zero times, so assignments inside it do
                // not prove that an outer variable is initialized afterward.
                scopes = beforeLoop;
                return true;
            }
            case Stmt::Kind::Return: {
                if (!currentMethod) {
                    if (stmt.expression) (void)checkExpression(*stmt.expression);
                    error(stmt.line, "NB-T1036", "return 不能出现在方法之外。");
                    return false;
                }
                if (currentMethod->returnType == "void") {
                    if (stmt.expression) {
                        (void)checkExpression(*stmt.expression);
                        error(stmt.line, "NB-T1037", "void 方法不能返回一个值。");
                    }
                } else if (!stmt.expression) {
                    error(stmt.line, "NB-T1038", "非 void 方法必须返回一个值。");
                } else {
                    const std::string actual = checkExpression(*stmt.expression);
                    if (actual == "void" || !canImplicitConvertExpression(*stmt.expression, actual, currentMethod->returnType))
                        error(stmt.line, "NB-T1039", "返回类型不匹配：方法要求 " + currentMethod->returnType + "，实际为 " + actual + "。");
                }
                return false;
            }
            case Stmt::Kind::Break:
            case Stmt::Kind::Next:
                if (loops <= 0) error(stmt.line, "NB-T1040", (stmt.kind == Stmt::Kind::Break ? "break" : "next") + std::string(" 只能在循环内部使用。"));
                else if (stmt.loopDepth > loops) error(stmt.line, "NB-T1041", "循环控制层数超过当前嵌套深度。");
                return false;
            case Stmt::Kind::Expression:
                if (stmt.name == "read") {
                    if (!primitiveType(stmt.declaredType)) error(stmt.line, "NB-T1042", "read 不支持输入类型“" + stmt.declaredType + "”。");
                } else if (stmt.name != "pause" && stmt.expression) {
                    (void)checkExpression(*stmt.expression);
                }
                return true;
            case Stmt::Kind::Increment: {
                Symbol* symbol = lookup(stmt.name);
                if (!symbol) error(stmt.line, "NB-T1043", stmt.name + " 未定义。");
                else {
                    if (!symbol->initialized) error(stmt.line, "NB-T1011", "变量“" + stmt.name + "”在初始化前被使用。");
                    if (symbol->type != "int" && symbol->type != "any") error(stmt.line, "NB-T1044", "++/-- 仅支持 int 变量，实际为 " + symbol->type + "。");
                }
                return true;
            }
            }
            return true;
        }
    };

    class Runtime {
    public:
        using Program = Parser::Program;
        Runtime(Program p, std::string file) : sourceFile(std::move(file)), program(std::move(p)) {}
        int run() {
            for (const auto& stmt : program.globals) {
                auto result = executeStmt(*stmt, *this, globals, 0);
                if (result.signal != ExecResult::Signal::Normal) throw NimpleError("NB-R1010", stmt->line, 1, "全局初始化不能使用 return/break/next。");
            }
            auto windowIt = program.methods.find("Window");
            const bool useWindow = windowIt != program.methods.end();
            auto it = useWindow ? windowIt : program.methods.find("Main");
            if (it == program.methods.end()) throw NimpleError("NB-R1011", 1, 1, "找不到入口方法 Main 或 void Window。");
            if (!it->second.params.empty() || it->second.paramsUnknown) throw NimpleError("NB-R1012", it->second.line, 1, "入口方法必须明确声明为无参数方法。");
            if (useWindow && it->second.returnType != "void") throw NimpleError("NB-R1013", it->second.line, 1, "EasyWindow 入口 Window 必须返回 void。");
            Value result = call(useWindow ? "Window" : "Main", {}, it->second.line);
            if (useWindow || result.type == ValueType::Void || result.type == ValueType::NoThing) return 0;
            if (result.type == ValueType::Int) return dataGet<int32_t>(result.data) & 0xFF;
            throw NimpleError("NB-R1014", it->second.line, 1, "入口方法 Main 必须返回 int 或 void。");
        }
        int runInteractive(Program submission) {
            for (auto& method : submission.methods) {
                if (program.methods.find(method.first) != program.methods.end())
                    throw NimpleError("NB2002", method.second.line, 1, "交互模式中方法重复声明：" + method.first);
                program.methods.emplace(method.first, std::move(method.second));
            }
            for (const auto& stmt : submission.globals) {
                auto result = executeStmt(*stmt, *this, globals, 0);
                if (result.signal != ExecResult::Signal::Normal)
                    throw NimpleError("NB-R1010", stmt->line, 1, "交互式语句不能在全局作用域使用 return/break/next。");
            }
            return 0;
        }
        fs::path sourcePath() const { return fs::u8path(sourceFile); }
        bool hasZeroParameterMethod(const std::string& name) const {
            auto it = program.methods.find(name);
            return it != program.methods.end() && it->second.params.empty() && !it->second.paramsUnknown;
        }
        Value call(const std::string& name, const std::vector<Value>& args, int line) {
            if (callDepth >= 256) throw NimpleError("NB-R1020", line, 1, "方法调用超过最大递归深度 256。");
            if (ezWindowBuiltinSignature(name)) {
                const auto fail = [&](const std::string& message) -> void { throw NimpleError("NB-EZ1001", line, 1, message); };
                const auto requireCount = [&](std::size_t count) {
                    if (args.size() != count) fail("EasyWindow 内置函数 " + name + " 参数数量错误：需要 " + std::to_string(count) + "，实际 " + std::to_string(args.size()) + "。");
                };
                const auto intArg = [&](std::size_t i) -> int {
                    if (i >= args.size() || args[i].type != ValueType::Int) fail("EasyWindow 内置函数 " + name + " 的参数必须为 int。");
                    return dataGet<int32_t>(args[i].data);
                };
                const auto stringArg = [&](std::size_t i) -> std::string {
                    if (i >= args.size() || args[i].type != ValueType::String) fail("EasyWindow 内置函数 " + name + " 的参数必须为 string。");
                    return dataGet<std::string>(args[i].data);
                };
                const auto boolArg = [&](std::size_t i) -> bool {
                    if (i >= args.size() || args[i].type != ValueType::Bool) fail("EasyWindow 内置函数 " + name + " 的参数必须为 bool。");
                    return dataGet<bool>(args[i].data);
                };
                if (!ezwindow::available()) fail("当前平台没有可用的 EasyWindow 图形后端或桌面运行环境。请检查该平台的系统 GUI 运行环境和 README.md。");
                try {
                    if (name == "ezWindowCreate") {
                        requireCount(4);
                        std::string iconPath = stringArg(3);
                        if (!iconPath.empty() && !fs::u8path(iconPath).is_absolute())
                            iconPath = (sourcePath().parent_path() / fs::u8path(iconPath)).lexically_normal().u8string();
                        return Value::integer(ezwindow::createWindow(stringArg(0), intArg(1), intArg(2), iconPath));
                    }
                    if (name == "ezWindowLabel") { requireCount(2); return Value::integer(ezwindow::addLabel(intArg(0), stringArg(1))); }
                    if (name == "ezWindowButton") { requireCount(2); return Value::integer(ezwindow::addButton(intArg(0), stringArg(1))); }
                    if (name == "ezWindowEntry") { requireCount(2); return Value::integer(ezwindow::addEntry(intArg(0), stringArg(1))); }
                    if (name == "ezWindowTextBox") { requireCount(2); return Value::integer(ezwindow::addTextBox(intArg(0), stringArg(1))); }
                    if (name == "ezWindowCheckBox") { requireCount(2); return Value::integer(ezwindow::addCheckBox(intArg(0), stringArg(1))); }
                    if (name == "ezWindowPasswordEntry") { requireCount(2); return Value::integer(ezwindow::addPasswordEntry(intArg(0), stringArg(1))); }
                    if (name == "ezWindowComboBox") { requireCount(2); return Value::integer(ezwindow::addComboBox(intArg(0), stringArg(1))); }
                    if (name == "ezWindowRadioButton") { requireCount(2); return Value::integer(ezwindow::addRadioButton(intArg(0), stringArg(1))); }
                    if (name == "ezWindowSlider") { requireCount(3); return Value::integer(ezwindow::addSlider(intArg(0), intArg(1), intArg(2))); }
                    if (name == "ezWindowProgressBar") { requireCount(3); return Value::integer(ezwindow::addProgressBar(intArg(0), intArg(1), intArg(2))); }
                    if (name == "ezWindowWebView") { requireCount(2); return Value::integer(ezwindow::addWebView(intArg(0), stringArg(1))); }
                    if (name == "ezWindowWebViewNavigate") { requireCount(2); ezwindow::webViewNavigate(intArg(0), stringArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowWebViewSetHtml") { requireCount(2); ezwindow::webViewSetHtml(intArg(0), stringArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowSetValue") { requireCount(2); ezwindow::setValue(intArg(0), intArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowGetValue") { requireCount(1); return Value::integer(ezwindow::getValue(intArg(0))); }
                    if (name == "ezWindowSetText") { requireCount(2); ezwindow::setText(intArg(0), stringArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowGetText") { requireCount(1); return Value::string(ezwindow::getText(intArg(0))); }
                    if (name == "ezWindowSetChecked") { requireCount(2); ezwindow::setChecked(intArg(0), boolArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowGetChecked") { requireCount(1); return Value::boolean(ezwindow::getChecked(intArg(0))); }
                    if (name == "ezWindowSetTitle") { requireCount(2); ezwindow::setTitle(intArg(0), stringArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowShow") { requireCount(1); ezwindow::showWindow(intArg(0)); return Value::voidValue(); }
                    if (name == "ezWindowSetPosition") { requireCount(3); ezwindow::setPosition(intArg(0), intArg(1), intArg(2)); return Value::voidValue(); }
                    if (name == "ezWindowSetSize") { requireCount(3); ezwindow::setSize(intArg(0), intArg(1), intArg(2)); return Value::voidValue(); }
                    if (name == "ezWindowSetBounds") { requireCount(5); ezwindow::setBounds(intArg(0), intArg(1), intArg(2), intArg(3), intArg(4)); return Value::voidValue(); }
                    if (name == "ezWindowSetRelativeBounds") { requireCount(5); ezwindow::setRelativeBounds(intArg(0), intArg(1), intArg(2), intArg(3), intArg(4)); return Value::voidValue(); }
                    if (name == "ezWindowOnClick") {
                        requireCount(2); const int widgetId = intArg(0); const std::string callbackName = stringArg(1);
                        ezwindow::setOnClick(widgetId, [this, callbackName, line]() {
                            try { (void)this->call(callbackName, {}, line); }
                            catch (const NimpleError& e) { DiagnosticEngine::emit(sourceFile, e.line, e.column, DiagnosticSeverity::Error, e.code, std::string("EasyWindow callback: ") + e.what()); }
                            catch (const std::exception& e) { DiagnosticEngine::emit(sourceFile, line, 1, DiagnosticSeverity::Error, "NB-EZ1099", e.what()); }
                        });
                        return Value::voidValue();
                    }
                    if (name == "ezWindowOnChange") {
                        requireCount(2); const int widgetId = intArg(0); const std::string callbackName = stringArg(1);
                        ezwindow::setOnChange(widgetId, [this, callbackName, line](const std::string& text) {
                            try { (void)this->call(callbackName, { Value::string(text) }, line); }
                            catch (const NimpleError& e) { DiagnosticEngine::emit(sourceFile, e.line, e.column, DiagnosticSeverity::Error, e.code, std::string("EasyWindow callback: ") + e.what()); }
                            catch (const std::exception& e) { DiagnosticEngine::emit(sourceFile, line, 1, DiagnosticSeverity::Error, "NB-EZ1099", e.what()); }
                        });
                        return Value::voidValue();
                    }
                    if (name == "ezWindowMainLoop") { requireCount(0); ezwindow::mainLoop(); return Value::voidValue(); }
                    if (name == "ezWindowQuit") { requireCount(0); ezwindow::quit(); return Value::voidValue(); }
                    if (name == "ezWindowMessage") { requireCount(2); ezwindow::message(stringArg(0), stringArg(1)); return Value::voidValue(); }
                    if (name == "ezWindowPack") { requireCount(1); (void)intArg(0); return Value::voidValue(); }
                    if (name == "ezWindowInvalidLayout") { requireCount(0); fail("EasyWindow 布局尺寸必须大于 0，且百分比边距不能占满整个方向。"); }
                    if (name == "ezWindowInvalidStyleExtension") { requireCount(1); fail("EasyWindow Style 文件必须使用 .css 后缀。"); }
                } catch (const NimpleError&) { throw; }
                catch (const std::exception& e) { throw NimpleError("NB-EZ1002", line, 1, e.what()); }
                fail("未实现的 EasyWindow 内置函数：“" + name + "”。");
            }
            auto it = program.methods.find(name);
            if (it == program.methods.end()) throw NimpleError("NB-R1021", line, 1, "找不到方法：" + name);
            const Method& method = it->second;
            if (!method.paramsUnknown && method.params.size() != args.size()) throw NimpleError("NB-R1022", line, 1, "方法 " + name + " 需要 " + std::to_string(method.params.size()) + " 个参数，实际收到 " + std::to_string(args.size()) + " 个。");
            Environment local(&globals);
            for (std::size_t i = 0; !method.paramsUnknown && i < args.size(); ++i) {
                Value v = args[i];
                if (!method.params[i].type.empty()) {
                    auto converted = convertValue(v, method.params[i].type);
                    if (!converted) throw NimpleError("NB-R1023", line, 1, "参数 " + method.params[i].name + " 无法转换为 " + method.params[i].type + "。");
                    v = *converted;
                }
                local.declare(method.params[i].name, v, line);
            }
            ++callDepth;
            ExecResult result;
            try { result = executeBlock(method.body, *this, local, 0); }
            catch (...) { --callDepth; throw; }
            --callDepth;
            if (result.signal == ExecResult::Signal::Break || result.signal == ExecResult::Signal::Next)
                throw NimpleError("NB-R1024", line, 1, "break/next 不能越过方法边界。");
            if (method.returnType == "void") return Value::voidValue();
            if (result.signal != ExecResult::Signal::Return) throw NimpleError("NB-R1025", method.line, 1, "非 void 方法必须显式 return。");
            if (result.value.type == ValueType::NoThing || result.value.type == ValueType::Void) throw NimpleError("NB-R1026", line, 1, "非 void 方法未返回值。");
            auto converted = convertValue(result.value, method.returnType);
            if (!converted) throw NimpleError("NB-R1027", line, 1, "方法返回值类型不匹配：期望 " + method.returnType + "，实际 " + result.value.typeName() + "。");
            return *converted;
        }
        Value readValue(const std::string& type, int line) {
            std::string input;
            if (environmentVariableEquals("NIMPLE_STUDIO_INPUT_PROTOCOL", "1"))
                std::cout << "\x1e" "NIMPLE_READ:" << (type.empty() ? "string" : type) << "\n" << std::flush;
            else
                std::cout << "Input (" << (type.empty() ? "string" : type) << "): " << std::flush;
            if (!std::getline(std::cin, input)) input.clear();
            if (type.empty() || type == "string") return Value::string(input);
            auto converted = convertValue(Value::string(input), type);
            if (converted) return *converted;
            DiagnosticEngine::emit(sourceFile, line, 1, DiagnosticSeverity::Warning, "NB-W2001", "输入无法转换为 " + type + "，返回目标类型默认值。");
            if (type == "bool" || type == "string") return Value::noThing();
            if (type == "char") return Value::character(std::string(1, '\0'));
            return Value::nullValue();
        }
        std::string sourceFile;
    private:
        Program program;
        Environment globals;
        int callDepth{ 0 };
        friend Value evalExpr(const Expr&, Runtime&, Environment&);
        friend ExecResult executeStmt(const Stmt&, Runtime&, Environment&, int);
    };

    static Value numericResult(double n, const Value& a, const Value& b) {
        if (a.type == ValueType::Int && b.type == ValueType::Int && std::isfinite(n) && std::floor(n) == n && n >= INT32_MIN && n <= INT32_MAX)
            return Value::integer(static_cast<int32_t>(n));
        return Value::floating(n, (a.type == ValueType::Float && b.type == ValueType::Float) ? ValueType::Float : ValueType::Double);
    }

    static bool truthy(const Value& v, bool forLoop, int line) {
        if (v.type == ValueType::Bool) return dataGet<bool>(v.data);
        if (v.type == ValueType::NoThing && !forLoop) return false;
        auto converted = convertValue(v, "bool");
        if (converted) return dataGet<bool>(converted->data);
        throw NimpleError("NB-R1030", line, 1, forLoop ? "for 条件无法转换为 bool。" : "if 条件无法转换为 bool。");
    }

    static Value evalBinary(const std::string& op, const Value& a, const Value& b, int line) {
        if (op == "+") {
            if (a.type == ValueType::String || a.type == ValueType::Char || b.type == ValueType::String || b.type == ValueType::Char)
                return Value::string(a.toString() + b.toString());
            if (!a.isNumeric() || !b.isNumeric()) throw NimpleError("NB-R1040", line, 1, "+ 要求数值操作数，或至少一个 string/char 操作数。");
            if (a.type == ValueType::Int && b.type == ValueType::Int) {
                uint32_t x = static_cast<uint32_t>(dataGet<int32_t>(a.data));
                uint32_t y = static_cast<uint32_t>(dataGet<int32_t>(b.data));
                return Value::integer(static_cast<int32_t>(x + y));
            }
            return numericResult(a.number() + b.number(), a, b);
        }
        if (op == "-") {
            if (!a.isNumeric() || !b.isNumeric()) throw NimpleError("NB-R1040", line, 1, "- 要求数值操作数。");
            if (a.type == ValueType::Int && b.type == ValueType::Int) return Value::integer(static_cast<int32_t>(static_cast<uint32_t>(dataGet<int32_t>(a.data)) - static_cast<uint32_t>(dataGet<int32_t>(b.data))));
            return numericResult(a.number() - b.number(), a, b);
        }
        if (op == "*" || op == "/" || op == "%" || op == "^") {
            if (!a.isNumeric() || !b.isNumeric()) throw NimpleError("NB-R1040", line, 1, op + " 要求数值操作数。");
            if ((op == "/" || op == "%") && b.number() == 0.0) {
                DiagnosticEngine::emit(std::string(), line, 1, DiagnosticSeverity::Warning, "NB-W1041", "除数为零，结果为 Null。");
                return Value::nullValue();
            }
            if (op == "*") {
                if (a.type == ValueType::Int && b.type == ValueType::Int) return Value::integer(static_cast<int32_t>(static_cast<uint32_t>(dataGet<int32_t>(a.data)) * static_cast<uint32_t>(dataGet<int32_t>(b.data))));
                return numericResult(a.number() * b.number(), a, b);
            }
            if (op == "/") {
                if (a.type == ValueType::Int && b.type == ValueType::Int) {
                    const double rounded = std::round(a.number() / b.number());
                    const int64_t wide = static_cast<int64_t>(rounded);
                    return Value::integer(static_cast<int32_t>(static_cast<uint32_t>(wide)));
                }
                return Value::floating(a.number() / b.number(), a.type == ValueType::Float && b.type == ValueType::Float ? ValueType::Float : ValueType::Double);
            }
            if (op == "%") {
                if (a.type == ValueType::Int && b.type == ValueType::Int) {
                    const int32_t leftInt = dataGet<int32_t>(a.data);
                    const int32_t rightInt = dataGet<int32_t>(b.data);
                    // Avoid signed overflow in the one undefined C++ case (INT_MIN % -1).
                    if (leftInt == INT32_MIN && rightInt == -1) return Value::integer(0);
                    return Value::integer(leftInt % rightInt);
                }
                return Value::floating(std::fmod(a.number(), b.number()));
            }
            return Value::floating(std::pow(a.number(), b.number()));
        }
        if (op == "and" || op == "or") {
            bool av = truthy(a, false, line), bv = truthy(b, false, line);
            return Value::boolean(op == "and" ? av && bv : av || bv);
        }
        if (op == "==" || op == "!=") {
            bool equal = false;
            if (a.type == b.type) {
                if (a.type == ValueType::Int) equal = dataGet<int32_t>(a.data) == dataGet<int32_t>(b.data);
                else if (a.isNumeric()) equal = a.number() == b.number();
                else if (a.type == ValueType::Bool) equal = dataGet<bool>(a.data) == dataGet<bool>(b.data);
                else if (a.type == ValueType::String || a.type == ValueType::Char) equal = dataGet<std::string>(a.data) == dataGet<std::string>(b.data);
                else equal = true;
            }
            return Value::boolean(op == "==" ? equal : !equal);
        }
        if (op == "~=") {
            if (!a.isNumeric() || !b.isNumeric()) throw NimpleError("NB-R1040", line, 1, "~= 要求数值操作数。");
            double x = a.number(), y = b.number();
            bool sameInf = std::isinf(x) && x == y;
            bool result = sameInf || (std::isfinite(x) && std::isfinite(y) && std::abs(x - y) <= std::max(1e-9, 1e-9 * std::max(std::abs(x), std::abs(y))));
            return Value::boolean(result);
        }
        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (!a.isNumeric() || !b.isNumeric()) throw NimpleError("NB-R1040", line, 1, "大小比较只支持数值类型。");
            double x = a.number(), y = b.number();
            if (op == "<") return Value::boolean(x < y);
            if (op == ">") return Value::boolean(x > y);
            if (op == "<=") return Value::boolean(x <= y);
            return Value::boolean(x >= y);
        }
        throw NimpleError("NB-R1049", line, 1, "尚未实现运算符：" + op);
    }

    static Value evalExpr(const Expr& e, Runtime& runtime, Environment& env) {
        switch (e.kind) {
        case Expr::Kind::Literal: return e.literal;
        case Expr::Kind::Variable:
            if (env.exists(e.text)) return env.get(e.text, e.line);
            if (runtime.hasZeroParameterMethod(e.text)) return runtime.call(e.text, {}, e.line);
            return env.get(e.text, e.line);
        case Expr::Kind::Unary: {
            Value v = evalExpr(*e.left, runtime, env);
            if (e.text == "no") return Value::boolean(!truthy(v, false, e.line));
            if (!v.isNumeric()) throw NimpleError("NB-R1050", e.line, 1, "一元正负号要求数值类型。");
            if (e.text == "+") return v;
            if (v.type == ValueType::Int) return Value::integer(static_cast<int32_t>(0u - static_cast<uint32_t>(dataGet<int32_t>(v.data))));
            return Value::floating(-v.number(), v.type);
        }
        case Expr::Kind::Binary: {
            Value left = evalExpr(*e.left, runtime, env);
            if (e.text == "and" && !truthy(left, false, e.line)) return Value::boolean(false);
            if (e.text == "or" && truthy(left, false, e.line)) return Value::boolean(true);
            Value right = evalExpr(*e.right, runtime, env);
            return evalBinary(e.text, left, right, e.line);
        }
        case Expr::Kind::Cast: {
            Value v = evalExpr(*e.left, runtime, env);
            auto converted = convertValue(v, e.text);
            if (converted) return *converted;
            if (e.forceCast) throw NimpleError("NB-R1051", e.line, 1, "强制类型转换失败：" + v.typeName() + " -> " + e.text + "。");
            DiagnosticEngine::emit(runtime.sourceFile, e.line, 1, DiagnosticSeverity::Warning, "NB-W1051", "类型转换失败，返回 Null（" + v.typeName() + " -> " + e.text + "）。");
            return Value::nullValue();
        }
        case Expr::Kind::Call: {
            std::vector<Value> args; args.reserve(e.args.size());
            for (const auto& arg : e.args) args.push_back(evalExpr(*arg, runtime, env));
            if (e.text == "read") return runtime.readValue(args.empty() ? "string" : args.front().toString(), e.line);
            if (e.text == "typeof") { if (args.size() != 1) throw NimpleError("NB-R1022", e.line, 1, "typeof 需要一个参数。"); return Value::string(args[0].typeName()); }
            return runtime.call(e.text, args, e.line);
        }
        }
        return Value::noThing();
    }

    static Value defaultValueForType(const std::string& type) {
        if (type == "bool" || type == "string") return Value::noThing();
        if (type == "char") return Value::character(std::string(1, '\0'));
        if (type == "void") return Value::voidValue();
        if (type == "auto") return Value::noType();
        return Value::nullValue();
    }

    static ExecResult executeBlock(const std::vector<std::unique_ptr<Stmt>>& body, Runtime& runtime, Environment& env, int loopDepth) {
        for (const auto& stmt : body) {
            ExecResult r = executeStmt(*stmt, runtime, env, loopDepth);
            if (r.signal != ExecResult::Signal::Normal) return r;
        }
        return {};
    }

    static void printStatement(const Stmt& s, Runtime& runtime, Environment& env) {
        (void)runtime;
        std::vector<Value> values;
        for (const auto& e : s.expressions) values.push_back(evalExpr(*e, runtime, env));
        std::string output = values.empty() ? "" : values.front().toString();
        if (s.name == "printf" || s.name == "printfn") {
            std::size_t argIndex = 1;
            std::string formatted;
            for (std::size_t i = 0; i < output.size();) {
                if (output[i] == '{' && i + 3 < output.size() && output[i + 1] == '%' && output.find('}', i + 2) != std::string::npos) {
                    auto close = output.find('}', i + 2);
                    std::string placeholder = output.substr(i + 1, close - i - 1);
                    if (placeholder == "%s" || placeholder == "%d" || placeholder == "%f" || placeholder == "%c" || placeholder == "%A") {
                        if (argIndex >= values.size()) throw NimpleError("NB-R1060", s.line, 1, "格式化输出的参数数量不足。");
                        formatted += values[argIndex++].toString(); i = close + 1; continue;
                    }
                }
                formatted.push_back(output[i++]);
            }
            if (argIndex != values.size()) throw NimpleError("NB-R1060", s.line, 1, "格式化输出的占位符与参数数量不匹配。");
            output = std::move(formatted);
        }
        else if (values.size() > 1) {
            for (std::size_t i = 1; i < values.size(); ++i) { output.push_back(' '); output += values[i].toString(); }
        }
        if (s.name == "printfn") output.push_back('\n');
        if (s.outputTarget.empty() || s.outputTarget == "System") {
            std::cout << output;
            std::cout.flush();
        } else {
            fs::path target = fs::u8path(s.outputTarget);
            if (target.is_relative()) target = runtime.sourcePath().parent_path() / target;
            std::error_code ec;
            const fs::path parent = target.parent_path();
            if (!parent.empty()) fs::create_directories(parent, ec);
            if (ec) throw NimpleError("NB-R1061", s.line, 1, "无法创建输出目录：" + parent.u8string() + "（" + ec.message() + "）。");
            std::ofstream file = openOutputFile(target, std::ios::binary | std::ios::app);
            if (!file) throw NimpleError("NB-R1061", s.line, 1, "无法写入输出文件：" + target.u8string());
            file.write(output.data(), static_cast<std::streamsize>(output.size()));
            if (!file) throw NimpleError("NB-R1061", s.line, 1, "写入输出文件失败：" + target.u8string());
        }
    }

    static ExecResult executeStmt(const Stmt& s, Runtime& runtime, Environment& env, int loopDepth) {
        switch (s.kind) {
        case Stmt::Kind::Variable: {
            if (s.declareNew) {
                Value initial = s.autoType ? Value::noType() : Value::nullValue();
                if (s.expression) initial = evalExpr(*s.expression, runtime, env);
                if (!s.declaredType.empty()) {
                    if (s.expression) {
                        auto converted = convertValue(initial, s.declaredType);
                        if (!converted) throw NimpleError("NB-R1070", s.line, 1, "初始值无法转换为声明类型 " + s.declaredType + "。");
                        initial = *converted;
                    }
                    else {
                        initial = defaultValueForType(s.declaredType);
                    }
                }
                env.declare(s.name, initial, s.line);
            }
            else {
                if (!env.exists(s.name)) throw NimpleError("NB-R1001", s.line, 1, "赋值目标尚未声明：" + s.name + "（新变量应使用 var new）。");
                if (s.expression) {
                    Value updated = evalExpr(*s.expression, runtime, env);
                    env.assign(s.name, updated, s.line);
                }
            }
            return {};
        }
        case Stmt::Kind::Print: printStatement(s, runtime, env); return {};
        case Stmt::Kind::If: {
            for (std::size_t i = 0; i < s.branchConditions.size(); ++i) {
                if (truthy(evalExpr(*s.branchConditions[i], runtime, env), false, s.line)) {
                    Environment branchEnv(&env);
                    return executeBlock(s.branchBodies[i], runtime, branchEnv, loopDepth);
                }
            }
            Environment branchEnv(&env);
            return executeBlock(s.elseBody, runtime, branchEnv, loopDepth);
        }
        case Stmt::Kind::For: {
            std::size_t iterations = 0;
            while (truthy(evalExpr(*s.expression, runtime, env), true, s.line)) {
                if (++iterations > 1000000) throw NimpleError("NB-R1080", s.line, 1, "循环迭代超过安全上限 1,000,000；可能存在无限循环。");
                Environment iterationEnv(&env);
                ExecResult r = executeBlock(s.body, runtime, iterationEnv, loopDepth + 1);
                if (r.signal == ExecResult::Signal::Return) return r;
                if (r.signal == ExecResult::Signal::Break) { if (r.depth > 1) { --r.depth; return r; } break; }
                if (r.signal == ExecResult::Signal::Next) { if (r.depth > 1) { --r.depth; return r; } continue; }
            }
            return {};
        }
        case Stmt::Kind::Return: return { ExecResult::Signal::Return, s.expression ? evalExpr(*s.expression, runtime, env) : Value::voidValue(), 1 };
        case Stmt::Kind::Break: case Stmt::Kind::Next:
            if (loopDepth <= 0) { DiagnosticEngine::emit(runtime.sourceFile, s.line, 1, DiagnosticSeverity::Warning, "NB-W1081", "在循环之外使用 " + std::string(s.kind == Stmt::Kind::Break ? "break" : "next") + "，已忽略。"); return {}; }
            if (s.loopDepth > loopDepth) throw NimpleError("NB-R1082", s.line, 1, "循环控制层数超过当前嵌套深度。");
            return { s.kind == Stmt::Kind::Break ? ExecResult::Signal::Break : ExecResult::Signal::Next, Value::voidValue(), s.loopDepth };
        case Stmt::Kind::Expression:
            if (s.name == "pause") {
                std::string dummy;
                if (environmentVariableEquals("NIMPLE_STUDIO_INPUT_PROTOCOL", "1")) std::cout << "\x1e" "NIMPLE_READ:pause\n" << std::flush;
                else std::cout << "按 Enter 继续…" << std::flush;
                std::getline(std::cin, dummy);
                return {};
            }
            if (s.name == "read") { (void)runtime.readValue(s.declaredType.empty() ? "string" : s.declaredType, s.line); return {}; }
            if (s.expression) (void)evalExpr(*s.expression, runtime, env);
            return {};
        case Stmt::Kind::Increment: {
            Value v = env.get(s.name, s.line);
            if (v.type != ValueType::Int) throw NimpleError("NB-R1090", s.line, 1, "++/-- 仅支持 int 变量。");
            uint32_t n = static_cast<uint32_t>(dataGet<int32_t>(v.data));
            env.assign(s.name, Value::integer(static_cast<int32_t>(s.declaredType == "++" ? n + 1u : n - 1u)), s.line);
            return {};
        }
        }
        return {};
    }

    static std::string trim(const std::string& value) {
        const auto first = value.find_first_not_of(" \t\r");
        if (first == std::string::npos) return {};
        const auto last = value.find_last_not_of(" \t\r");
        return value.substr(first, last - first + 1);
    }

    static bool isIdentifierStartByte(unsigned char c) {
        return std::isalpha(c) || c == '_' || c >= 0x80;
    }

    static bool isIdentifierPartByte(unsigned char c) {
        return std::isalnum(c) || c == '_' || c >= 0x80;
    }

    static std::string transformReplacementLine(
        const std::string& line,
        const std::unordered_map<std::string, std::string>& replacements,
        bool& inBlockComment,
        bool& inTripleString) {
        const auto first = line.find_first_not_of(" \t\r");
        if (!inBlockComment && !inTripleString && first != std::string::npos &&
            line.compare(first, 2, "* ") == 0) return line;

        std::string out;
        out.reserve(line.size());
        std::size_t i = 0;
        while (i < line.size()) {
            if (inBlockComment) {
                if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '/') {
                    out += "*/"; i += 2; inBlockComment = false;
                }
                else out.push_back(line[i++]);
                continue;
            }
            if (inTripleString) {
                if (i + 3 < line.size() && line[i] == '\\' && line.compare(i + 1, 3, "\"\"\"") == 0) {
                    out += "\\\"\"\""; i += 4; continue;
                }
                if (i + 2 < line.size() && line.compare(i, 3, "\"\"\"") == 0) {
                    out += "\"\"\""; i += 3; inTripleString = false;
                }
                else out.push_back(line[i++]);
                continue;
            }
            if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '*') {
                out += "/*"; i += 2; inBlockComment = true; continue;
            }
            if (i + 2 < line.size() && line.compare(i, 3, "\"\"\"") == 0) {
                out += "\"\"\""; i += 3; inTripleString = true; continue;
            }
            if (line[i] == '\"' || line[i] == '\'') {
                const char quote = line[i];
                out.push_back(line[i++]);
                while (i < line.size()) {
                    char c = line[i++]; out.push_back(c);
                    if (c == '\\' && i < line.size()) { out.push_back(line[i++]); continue; }
                    if (c == quote) break;
                }
                continue;
            }

            const auto c = static_cast<unsigned char>(line[i]);
            if (isIdentifierStartByte(c)) {
                const std::size_t begin = i++;
                while (i < line.size() && isIdentifierPartByte(static_cast<unsigned char>(line[i]))) ++i;
                const std::string word = line.substr(begin, i - begin);
                auto replacement = replacements.find(word);
                out += replacement == replacements.end() ? word : replacement->second;
                continue;
            }
            out.push_back(line[i++]);
        }
        return out;
    }

    static bool readDirectiveString(const std::string& input, std::string& decoded, int line, std::string& error) {
        const std::string text = trim(input);
        if (text.size() < 2 || text.front() != '\"') {
            error = "预处理指令需要使用双引号包裹字符串参数。";
            return false;
        }
        std::string raw;
        bool closed = false;
        std::size_t i = 1;
        for (; i < text.size(); ++i) {
            if (text[i] == '\\') {
                if (i + 1 >= text.size()) break;
                raw.push_back(text[i++]);
                raw.push_back(text[i]);
            }
            else if (text[i] == '\"') {
                closed = true;
                ++i;
                break;
            }
            else raw.push_back(text[i]);
        }
        if (!closed || !trim(text.substr(i)).empty()) {
            error = "预处理指令的字符串参数未正确闭合，或包含多余内容。";
            return false;
        }
        try { decoded = decodeEscapes(raw, line, 1); }
        catch (const NimpleError& e) { error = e.what(); return false; }
        return true;
    }

    // statement module imports are deliberately restricted to a single standard-library module
    // name; unlike copy it cannot traverse directories or open arbitrary files.
    static optional<fs::path> resolveCopyFile(const std::string& requestedName, const fs::path& sourcePath) {
        // A bare name first resolves from the Standard library directory next to
        // nimple-basic.exe. Paths containing directories remain source-relative (or absolute).
        fs::path requested = fs::u8path(requestedName);
        std::error_code ec;
        if (!requested.has_parent_path() && !requested.is_absolute()) {
            fs::path module = standardLibraryDirectory() / requested;
            if (fs::is_regular_file(module, ec) && !ec) return module;
            ec.clear();
        }
        if (requested.is_absolute()) {
            if (fs::is_regular_file(requested, ec) && !ec) return requested;
        } else {
            fs::path local = fs::absolute(sourcePath).parent_path() / requested;
            if (fs::is_regular_file(local, ec) && !ec) return local;
        }
        return nullopt;
    }

    static bool lineContainsSourceCode(const std::string& line, bool inBlockComment, bool inTripleString) {
        const std::string stripped = trim(line);
        if (!inBlockComment && !inTripleString && stripped.rfind("* ", 0) == 0) return false;
        bool hasCode = false;
        std::size_t i = 0;
        while (i < line.size()) {
            if (inBlockComment) {
                const auto close = line.find("*/", i);
                if (close == std::string::npos) return hasCode;
                inBlockComment = false;
                i = close + 2;
                continue;
            }
            if (inTripleString) {
                if (i + 3 < line.size() && line[i] == '\\' && line.compare(i + 1, 3, "\"\"\"") == 0) {
                    i += 4;
                    continue;
                }
                if (i + 2 < line.size() && line.compare(i, 3, "\"\"\"") == 0) {
                    inTripleString = false;
                    i += 3;
                    continue;
                }
                ++i;
                continue;
            }
            if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') return hasCode;
            if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '*') {
                inBlockComment = true;
                i += 2;
                continue;
            }
            if (i + 2 < line.size() && line.compare(i, 3, "\"\"\"") == 0) {
                hasCode = true;
                inTripleString = true;
                i += 3;
                continue;
            }
            if (line[i] == '"' || line[i] == '\'') {
                const char quote = line[i++];
                hasCode = true;
                while (i < line.size()) {
                    if (line[i] == '\\' && i + 1 < line.size()) { i += 2; continue; }
                    if (line[i++] == quote) break;
                }
                continue;
            }
            if (!std::isspace(static_cast<unsigned char>(line[i]))) hasCode = true;
            ++i;
        }
        return hasCode;
    }

    // EasyWindow's compact declarative surface. These lines are syntax sugar over
    // the same checked standard-library API, so both --check and execution agree.
    static std::string transformEasyWindowSyntax(const std::string& source) {
        const bool declarativeEasyWindow = source.find("__NIMPLE_EZWINDOW_DSL__") != std::string::npos;
        std::istringstream input(source); std::ostringstream output; std::string line;
        const std::regex windowStart(R"(^\s*void\s+Window\s*(?:\(\s*void\s*\))?\s*(?:\{|:)\s*(?://.*)?$)");
        const std::regex instancePattern(R"(^\s*var\s+new\s+\(auto\)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*EasyWindow\.Window\s*(?://.*)?$)");
        const std::regex titlePattern(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\.Title\s*=\s*("(?:\\.|[^"\\])*")\s*(?://.*)?$)");
        const std::regex iconPattern(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\.Icon\s*=\s*("(?:\\.|[^"\\])*")\s*(?://.*)?$)");
        const std::regex geometryPattern(R"REGEX(^\s*[A-Za-z_][A-Za-z0-9_]*\.Geometry\(\s*"([0-9]+)x([0-9]+)"\s*\)\s*(?://.*)?$)REGEX");
        const std::regex controlStart(R"(^\s*void\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\(\s*void\s*\))?\s*(?:\{|:)\s*(?://.*)?$)");
        const std::regex locationPattern(R"(^\s*loction\(\s*(%|px)\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)\s*(?://.*)?$)");
        const std::regex sizePattern(R"(^\s*size\(\s*px\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([0-9]+(?:\.[0-9]+)?)\s*\)\s*(?://.*)?$)");
        const std::regex controlInstancePattern(R"(^\s*var\s+new\s+\(auto\)([A-Za-z_][A-Za-z0-9_]*)\s*=\s*EasyWindow\.([A-Za-z_][A-Za-z0-9_]*)\s*(?://.*)?$)");
        const std::regex oldWindow(R"(^\s*window\s+("(?:\\.|[^"\\])*")(?:\s+(\d+)\s+(\d+))?\s+as\s+([A-Za-z_][A-Za-z0-9_]*)\s*;?\s*$)");
        const std::regex oldControl(R"(^\s*(label|button|entry|textBox|checkBox)\s+([A-Za-z_][A-Za-z0-9_]*)\s+("(?:\\.|[^"\\])*")\s+as\s+([A-Za-z_][A-Za-z0-9_]*)\s*;?\s*$)");
        const std::regex eventPattern(R"(^\s*on\s+([A-Za-z_][A-Za-z0-9_]*)\s+(click|change)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;?\s*$)");
        const std::regex simplePattern(R"(^\s*(show|run|pack)(?:\s+([A-Za-z_][A-Za-z0-9_]*))?\s*;?\s*$)");
        struct ControlDecl { std::string name; std::string kind; std::vector<std::string> body; bool instantiated{false}; bool firstStatement{true}; std::array<std::string,4> edges{{"0","0","0","0"}}; bool hasLocation{false}; bool locationPercent{true}; bool hasSize{false}; int sizeLength{0}; int sizeWidth{0}; };
        bool inWindow=false, inControls=false, inControl=false, hasInstance=false, hasWindowDeclaration=false;
        std::string windowName="Window", title="\"EasyWindow Application\"", icon="\"\"", style;
        int width=400, height=300; std::vector<ControlDecl> controls; ControlDecl current;
        std::vector<std::string> windowBody;
        std::ostringstream generatedWindow;
        auto emitWindow = [&]() {
            generatedWindow << "void Window(void) {\n";
            if (!hasInstance) { generatedWindow << "    ezWindowMissingInstance()\n    return\n}\n"; return; }
            generatedWindow << "    var new (auto)" << windowName << " = window(" << title << ", " << width << ", " << height << ", " << icon << ")\n";
            for (const auto& c : controls) {
                if (!c.instantiated) continue;
                const std::string kind = c.kind.empty() ? "button" : c.kind;
                if (kind == "slider" || kind == "progressBar") generatedWindow << "    var new (auto)" << c.name << " = " << kind << "(" << windowName << ", 0, 100)\n";
                else generatedWindow << "    var new (auto)" << c.name << " = " << kind << "(" << windowName << ", \"\")\n";
                if (c.hasLocation) {
                    const double topValue=std::stod(c.edges[0]), bottomValue=std::stod(c.edges[1]);
                    const double leftValue=std::stod(c.edges[2]), rightValue=std::stod(c.edges[3]);
                    if (c.locationPercent) {
                        const int top=static_cast<int>(std::lround(topValue)), bottom=static_cast<int>(std::lround(bottomValue));
                        const int left=static_cast<int>(std::lround(leftValue)), right=static_cast<int>(std::lround(rightValue));
                        if (left+right>100 || top+bottom>100 || (!c.hasSize && (left+right==100 || top+bottom==100))) generatedWindow << "    ezWindowInvalidLayout()\n";
                        else if (c.hasSize) {
                            if (c.sizeLength<=0 || c.sizeWidth<=0) generatedWindow << "    ezWindowInvalidLayout()\n";
                            else {
                                const int x=static_cast<int>(std::lround(width*leftValue/100.0));
                                const int y=static_cast<int>(std::lround(height*topValue/100.0));
                                generatedWindow << "    setBounds(" << c.name << ", " << x << ", " << y << ", " << c.sizeLength << ", " << c.sizeWidth << ")\n";
                            }
                        } else generatedWindow << "    setRelativeBounds(" << c.name << ", " << left << ", " << top << ", " << (100-left-right) << ", " << (100-top-bottom) << ")\n";
                    } else {
                        const int x=static_cast<int>(std::lround(leftValue)), y=static_cast<int>(std::lround(topValue));
                        const int w=c.hasSize?c.sizeLength:width-static_cast<int>(std::lround(leftValue+rightValue));
                        const int h=c.hasSize?c.sizeWidth:height-static_cast<int>(std::lround(topValue+bottomValue));
                        if (w<=0 || h<=0) generatedWindow << "    ezWindowInvalidLayout()\n";
                        else generatedWindow << "    setBounds(" << c.name << ", " << x << ", " << y << ", " << w << ", " << h << ")\n";
                    }
                } else if (c.hasSize) {
                    if (c.sizeLength<=0 || c.sizeWidth<=0) generatedWindow << "    ezWindowInvalidLayout()\n";
                    else generatedWindow << "    setSize(" << c.name << ", " << c.sizeLength << ", " << c.sizeWidth << ")\n";
                }
                for (const auto& bodyLine : c.body) generatedWindow << "    " << bodyLine << '\n';
            }
            for (const auto& bodyLine : windowBody) generatedWindow << "    " << bodyLine << '\n';
            generatedWindow << "    show(" << windowName << ")\n    mainloop()\n    return\n}\n";
        };
        while (std::getline(input,line)) {
            std::smatch m; const std::string t=trim(line);
            if (declarativeEasyWindow && !inWindow && !inControls && std::regex_match(line,windowStart)) { inWindow=true; hasInstance=false; title="\"EasyWindow Application\""; icon="\"\""; style.clear(); width=400; height=300; controls.clear(); windowBody.clear(); continue; }
            if (inWindow) {
                if (std::regex_match(line,m,instancePattern)) { windowName=m[1].str(); hasInstance=true; continue; }
                if (std::regex_match(line,m,titlePattern)) { title=m[1].str(); continue; }
                if (std::regex_match(line,m,iconPattern)) { icon=m[1].str(); continue; }
                if (std::regex_match(line,m,geometryPattern)) { width=std::stoi(m[1].str()); height=std::stoi(m[2].str()); continue; }
                static const std::regex stylePattern(R"(^\s*[A-Za-z_][A-Za-z0-9_]*\.Style\s*=\s*(\"(?:\\.|[^\"\\])*\")\s*(?://.*)?$)");
                if (std::regex_match(line,m,stylePattern)) {
                    style=m[1].str();
                    const std::string styleValue=style.substr(1,style.size()-2);
                    if (styleValue.size()<4 || styleValue.substr(styleValue.size()-4)!=".css")
                        windowBody.push_back("ezWindowInvalidStyleExtension(" + style + ")");
                    continue;
                }
                if (t=="}" || t=="end") { inWindow=false; hasWindowDeclaration=true; continue; }
                if (!t.empty() && t.rfind("//",0)!=0) windowBody.push_back(t);
                continue;
            }
            if (declarativeEasyWindow && !inControls && (t=="module Controls {" || t=="module Controls :")) { inControls=true; continue; }
            if (inControls && !inControl && std::regex_match(line,m,controlStart)) {
                current=ControlDecl{}; current.name=m[1].str();
                static const std::unordered_map<std::string,std::string> controlKinds = {
                    {"Button","button"},{"Label","label"},{"Entry","entry"},{"TextBox","textBox"},
                    {"CheckBox","checkBox"},{"PasswordEntry","passwordEntry"},{"ComboBox","comboBox"},
                    {"RadioButton","radioButton"},{"Slider","slider"},{"ProgressBar","progressBar"},{"WebView","webView"}
                };
                auto kind=controlKinds.find(current.name);
                if (kind==controlKinds.end()) {
                    const std::string lowered = [&]() { std::string v=current.name; std::transform(v.begin(),v.end(),v.begin(),[](unsigned char ch){ return static_cast<char>(std::tolower(ch)); }); return v; }();
                    const std::array<std::pair<const char*,const char*>,11> prefixes{{
                        {"passwordentry","passwordEntry"},{"radiobutton","radioButton"},{"progressbar","progressBar"},
                        {"combobox","comboBox"},{"checkbox","checkBox"},{"textbox","textBox"},
                        {"webview","webView"},{"button","button"},{"label","label"},{"entry","entry"},{"slider","slider"}
                    }};
                    current.kind="button";
                    for (const auto& prefix : prefixes) if (lowered.rfind(prefix.first,0)==0) { current.kind=prefix.second; break; }
                } else current.kind=kind->second;
                inControl=true; continue;
            }
            if (inControls && inControl) {
                if (std::regex_match(line,m,controlInstancePattern) && current.firstStatement) {
                    current.name=m[1].str();
                    const std::string type=m[2].str();
                    static const std::unordered_map<std::string,std::string> kinds = {
                        {"Button","button"},{"Label","label"},{"Entry","entry"},{"TextBox","textBox"},
                        {"CheckBox","checkBox"},{"PasswordEntry","passwordEntry"},{"ComboBox","comboBox"},
                        {"RadioButton","radioButton"},{"Slider","slider"},{"ProgressBar","progressBar"},{"WebView","webView"}
                    };
                    auto found=kinds.find(type);
                    if (found!=kinds.end()) { current.kind=found->second; current.instantiated=true; }
                    continue;
                }
                if (std::regex_match(line,m,locationPattern)) { current.firstStatement=false; current.locationPercent=(m[1].str()=="%"); current.edges={{m[2].str(),m[3].str(),m[4].str(),m[5].str()}}; current.hasLocation=true; continue; }
                if (std::regex_match(line,m,sizePattern)) { current.firstStatement=false; current.sizeLength=static_cast<int>(std::lround(std::stod(m[1].str()))); current.sizeWidth=static_cast<int>(std::lround(std::stod(m[2].str()))); current.hasSize=true; continue; }
                if (t=="}" || t=="end") { controls.push_back(current); inControl=false; continue; }
                if (!t.empty() && t.rfind("//",0)!=0) { current.firstStatement=false; current.body.push_back(t); }
                continue;
            }
            if (inControls && !inControl && (t=="}" || t=="end")) { inControls=false; continue; }
            if (std::regex_match(line,m,oldWindow)) {
                const std::string w=m[4].str(); output << "var new " << w << " = window(" << m[1].str() << ", " << (m[2].matched?m[2].str():"200") << ", " << (m[3].matched?m[3].str():"200") << ")";
            } else if (std::regex_match(line,m,oldControl)) output << "var new " << m[4].str() << " = " << m[1].str() << "(" << m[2].str() << ", " << m[3].str() << ")";
            else if (std::regex_match(line,m,eventPattern)) output << (m[2].str()=="click"?"onClick(":"onChange(") << m[1].str() << ", \"" << m[3].str() << "\")";
            else if (std::regex_match(line,m,simplePattern)) { if(m[1].str()=="run") output<<"mainloop()"; else if(m[1].str()=="show") output<<"show("<<m[2].str()<<")"; else output<<"pack("<<m[2].str()<<")"; }
            else output << line;
            if (!input.eof()) output << '\n';
        }
        // If the declaration is malformed, leave a tiny invalid program for the
        // EasyWindow-specific validator rather than silently creating a window.
        if (inWindow) output << "__NIMPLE_EZWINDOW_UNCLOSED__\n";
        // Controls are allowed to be declared after void Window. Defer emitting
        // the generated runtime body until the entire DSL has been scanned, so
        // emitWindow sees the complete Controls module instead of an empty list.
        if (hasWindowDeclaration) emitWindow();
        if (!generatedWindow.str().empty()) output << generatedWindow.str();
        (void)hasInstance;
        return output.str();
    }

    static bool sourceHasExecutableCode(const std::string& source) {
        std::istringstream input(source);
        std::string line;
        bool inBlockComment = false, inTripleString = false;
        while (std::getline(input, line))
            if (lineContainsSourceCode(line, inBlockComment, inTripleString)) return true;
        return false;
    }

    static std::string preprocessSource(
        const std::string& source,
        const fs::path& sourcePath,
        std::vector<fs::path>& includeStack,
        std::vector<NimpleError>& diagnostics,
        int depth = 0) {
        if (depth > 32) {
            diagnostics.emplace_back("NB3002", 1, 1, "statement/copy 嵌套深度超过 32 层。可能存在循环包含。");
            return {};
        }

        std::error_code pathError;
        fs::path normalizedPath = fs::weakly_canonical(fs::absolute(sourcePath), pathError);
        if (pathError) normalizedPath = fs::absolute(sourcePath).lexically_normal();
        if (std::find(includeStack.begin(), includeStack.end(), normalizedPath) != includeStack.end()) {
            diagnostics.emplace_back("NB3002", 1, 1, "statement/copy 检测到循环包含文件：" + normalizedPath.u8string());
            return {};
        }

        includeStack.push_back(normalizedPath);
        struct StackGuard {
            std::vector<fs::path>& stack;
            ~StackGuard() { stack.pop_back(); }
        } guard{ includeStack };

        std::unordered_map<std::string, std::string> replacements;
        bool inBlockComment = false;
        bool inTripleString = false;
        bool seenSourceStatement = false;
        std::istringstream input(source);
        std::ostringstream output;
        std::string lineText;
        int lineNumber = 0;

        while (std::getline(input, lineText)) {
            ++lineNumber;
            const bool hasNewline = !input.eof();
            const std::string trimmed = trim(lineText);
            std::istringstream directiveProbe(trimmed);
            std::string directiveWord;
            directiveProbe >> directiveWord;
            const bool canBeDirective = !inBlockComment && !inTripleString &&
                (directiveWord == "replace" || directiveWord == "copy" || directiveWord == "suppress" ||
                 directiveWord == "const" || directiveWord == "include" || directiveWord == "statement");

            if (!canBeDirective) {
                const bool hasCode = lineContainsSourceCode(lineText, inBlockComment, inTripleString);
                output << transformReplacementLine(lineText, replacements, inBlockComment, inTripleString);
                if (hasCode) seenSourceStatement = true;
                if (hasNewline) output << '\n';
                continue;
            }

            std::istringstream directiveStream(trimmed);
            std::string directive;
            directiveStream >> directive;
            std::string rest;
            std::getline(directiveStream, rest);
            rest = trim(rest);
            // A trailing // comment is valid on every source directive too.
            const auto directiveComment = rest.find("//");
            if (directiveComment != std::string::npos) rest = trim(rest.substr(0, directiveComment));

            if (directive == "replace") {
                std::istringstream replaceStream(rest);
                std::string identifier;
                replaceStream >> identifier;
                std::string encoded;
                std::getline(replaceStream, encoded);
                encoded = trim(encoded);
                if (identifier.empty() || !isIdentifierStartByte(static_cast<unsigned char>(identifier.front())) ||
                    !std::all_of(identifier.begin(), identifier.end(), [](unsigned char c) { return isIdentifierPartByte(c); })) {
                    diagnostics.emplace_back("NB3000", lineNumber, 1, "replace 语法：replace 标识符 \"替换文本\"。");
                    if (hasNewline) output << '\n';
                    continue;
                }
                std::string decoded, error;
                if (!readDirectiveString(encoded, decoded, lineNumber, error)) {
                    diagnostics.emplace_back("NB3000", lineNumber, 1, "replace 参数无效：" + error);
                    if (hasNewline) output << '\n';
                    continue;
                }
                replacements[identifier] = decoded;
            }
            else if (directive == "copy") {
                std::string relativeName, error;
                if (!readDirectiveString(rest, relativeName, lineNumber, error)) {
                    diagnostics.emplace_back("NB3001", lineNumber, 1, "copy 参数无效：" + error);
                    if (hasNewline) output << '\n';
                    continue;
                }
                auto resolvedInclude = resolveCopyFile(relativeName, normalizedPath);
                if (!resolvedInclude) {
                    diagnostics.emplace_back("NB3001", lineNumber, 1, "copy 无法打开文件：" + fs::u8path(relativeName).u8string());
                    if (hasNewline) output << '\n';
                    continue;
                }
                fs::path includePath = *resolvedInclude;
                std::ifstream includeFile = openInputFile(includePath, std::ios::binary);
                if (!includeFile) {
                    diagnostics.emplace_back("NB3001", lineNumber, 1, "copy 无法打开文件：" + includePath.u8string());
                    if (hasNewline) output << '\n';
                    continue;
                }
                std::ostringstream includeBuffer; includeBuffer << includeFile.rdbuf();
                std::string includedSource = includeBuffer.str();
                if (includedSource.size() >= 3 && static_cast<unsigned char>(includedSource[0]) == 0xEF &&
                    static_cast<unsigned char>(includedSource[1]) == 0xBB && static_cast<unsigned char>(includedSource[2]) == 0xBF)
                    includedSource.erase(0, 3);
                std::string expanded = preprocessSource(includedSource, includePath, includeStack, diagnostics, depth + 1);
                output << expanded;
                if (hasNewline && (expanded.empty() || expanded.back() != '\n')) output << '\n';
            }
            else if (directive == "suppress") {
                if (rest == "NB-W2090") output << "/*__NIMPLE_SUPPRESS_CLASS__*/";
                else diagnostics.emplace_back("NB3004", lineNumber, 1, "suppress 当前只支持 NB-W2090。");
            }
            else if (directive == "const" || directive == "include") {
                if (rest == "namespace sys") {
                    output << "/*__NIMPLE_IMPORT_SYS__*/";
                } else if (rest == "sys::print" || rest == "sys::printf" || rest == "sys::printfn" || rest == "sys::read" || rest == "sys::pause") {
                    output << "/*__NIMPLE_IMPORT_SYS_" << rest.substr(5) << "__*/";
                } else if (!rest.empty() && rest.front() == '<' && rest.back() == '>') {
                    std::string moduleName = rest.substr(1, rest.size()-2);
                    if (moduleName == "EzWindow") moduleName = "EasyWindow";
                    if (seenSourceStatement) diagnostics.emplace_back("NB3003", lineNumber, 1, "标准库 include 必须放在源代码语句之前。");
                    else {
                        auto moduleSource = readStandardLibraryModule(moduleName);
                        if (!moduleSource) diagnostics.emplace_back("NB3005", lineNumber, 1, "标准库无法找到：" + moduleName);
                        else {
                            const fs::path modulePath = standardLibraryDirectory() / fs::u8path(moduleName);
                            std::string expanded = preprocessSource(*moduleSource, modulePath, includeStack, diagnostics, depth + 1);
                            if (moduleName == "EasyWindow") output << "/*__NIMPLE_EZWINDOW_DSL__*/\n";
                            output << expanded;
                            if (!expanded.empty() && expanded.back() != '\n') output << '\n';
                        }
                    }
                } else if (!rest.empty() && rest.front() == '\"') {
                    std::string pathName, error;
                    if (!readDirectiveString(rest, pathName, lineNumber, error)) diagnostics.emplace_back("NB3003", lineNumber, 1, "include 路径无效：" + error);
                    else {
                        auto path = resolveCopyFile(pathName, normalizedPath);
                        if (!path) diagnostics.emplace_back("NB3005", lineNumber, 1, "include 无法打开文件：" + pathName);
                        else {
                            std::ifstream file = openInputFile(*path, std::ios::binary);
                            std::ostringstream buffer; buffer << file.rdbuf();
                            std::string expanded = preprocessSource(buffer.str(), *path, includeStack, diagnostics, depth + 1);
                            output << expanded;
                            if (!expanded.empty() && expanded.back() != '\n') output << '\n';
                        }
                    }
                } else {
                    diagnostics.emplace_back("NB3004", lineNumber, 1, "include 使用 <标准库名称>、\"路径/文件名\" 或 sys 名称。");
                }
            }
            else if (directive == "statement" && (rest.empty() || rest.front() != '"')) {
                // A standard-library statement declaration registers a native
                // statement implementation with the parser. It is deliberately
                // distinct from `const`, which only controls prefix elision.
                static const std::unordered_set<std::string> supported = {
                    "sys::print", "sys::printf", "sys::printfn", "sys::read", "sys::pause"
                };
                if (supported.find(rest) == supported.end()) {
                    diagnostics.emplace_back("NB3006", lineNumber, 1,
                        "statement 只支持定义 sys::print / printf / printfn / read / pause。");
                } else {
                    output << "/*__NIMPLE_DEFINE_" << rest.substr(5) << "__*/";
                }
            }
            else if (directive == "statement" && !rest.empty() && rest.front() == '"') {
                std::string moduleName, error;
                if (!readDirectiveString(rest, moduleName, lineNumber, error)) {
                    diagnostics.emplace_back("NB3003", lineNumber, 1, "statement 模块参数无效：" + error);
                }
                else if (seenSourceStatement) {
                    diagnostics.emplace_back("NB3003", lineNumber, 1, "statement 模块引进必须放在源代码语句之前。");
                }
                else if (fs::u8path(moduleName).extension() == ".nbf") {
                    auto headerPath = resolveCopyFile(moduleName, normalizedPath);
                    if (!headerPath) {
                        diagnostics.emplace_back("NB3005", lineNumber, 1, "statement 无法找到首文件：" + moduleName);
                    } else {
                        std::ifstream headerFile = openInputFile(*headerPath, std::ios::binary);
                        std::ostringstream headerBuffer; headerBuffer << headerFile.rdbuf();
                        std::string headerSource = headerBuffer.str();
                        if (headerSource.size() >= 3 && static_cast<unsigned char>(headerSource[0]) == 0xEF &&
                            static_cast<unsigned char>(headerSource[1]) == 0xBB && static_cast<unsigned char>(headerSource[2]) == 0xBF)
                            headerSource.erase(0, 3);
                        static const std::regex includingPattern(R"((^|\n)\s*(public\s+|private\s+|protected\s+)?[A-Za-z_][A-Za-z0-9_]*\s+including\s*(?:\([^)]*\))?\s*\{)");
                        if (!std::regex_search(headerSource, includingPattern)) {
                            DiagnosticEngine::emit(std::string(), lineNumber, 1, DiagnosticSeverity::Warning, "NB-W2101", "首文件“" + moduleName + "”没有 including 函数；将隐式执行 copy。");
                        }
                        std::string expanded = preprocessSource(headerSource, *headerPath, includeStack, diagnostics, depth + 1);
                        output << expanded;
                        if (!expanded.empty() && expanded.back() != '\n') output << '\n';
                    }
                }
                else if (moduleName == "System") {
                    diagnostics.emplace_back("NB3003", lineNumber, 1, "System 已改为内置命名空间 sys；请使用 sys::name 或 const namespace sys。");
                }
                else {
                    // Standard library files are resolved relative to the running
                    // interpreter executable, never the current working directory.
                    auto moduleSource = readStandardLibraryModule(moduleName);
                    if (!moduleSource) {
                        diagnostics.emplace_back("NB3003", lineNumber, 1,
                            "标准库中不存在模块“" + moduleName + "”（请检查解释器同级的 Standard library 文件夹）");
                    } else {
                        const fs::path modulePath = standardLibraryDirectory() / fs::u8path(moduleName);
                        std::string expanded = preprocessSource(*moduleSource, modulePath, includeStack, diagnostics, depth + 1);
                        output << expanded;
                        if (!expanded.empty() && expanded.back() != '\n') output << '\n';
                    }
                }
            }
            else {
                diagnostics.emplace_back("NB3000", lineNumber, 1, "未知预处理指令：" + directive + "。");
            }
            if (hasNewline && directive != "copy") output << '\n';
        }
        return output.str();
    }

} // namespace nimple

static void printUsage() {
    std::cout << "Nimple Basic 0.1.0\n"
        << "Usage: nb <source.nb>\n"
        << "       nb --check <source.nb>\n"
        << "       nb --check-stdin <source-context.nb>  (read source from stdin)\n"
        << "       nb --version\n";
}

static int printDiagnostics(const fs::path& filePath, std::vector<nimple::NimpleError> diagnostics) {
    nimple::DiagnosticEngine engine;
    for (const auto& diagnostic : diagnostics) engine.add(diagnostic);
    return engine.flush(filePath.u8string());
}

// A .nbt tail file is optional project metadata. It deliberately uses a
// small key/value grammar so editors can preserve future keys without changing
// the language parser. Values are quoted strings, booleans, or numeric literals.
static void validateTailFile(const fs::path& tailPath, std::vector<nimple::NimpleError>& diagnostics) {
    std::ifstream file = openInputFile(tailPath, std::ios::binary);
    if (!file) {
        diagnostics.emplace_back("NB3006", 1, 1, "无法读取尾文件：" + tailPath.u8string());
        return;
    }
    std::unordered_set<std::string> keys;
    std::string line;
    int lineNumber = 0;
    const std::regex assignment(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$)");
    while (std::getline(file, line)) {
        ++lineNumber;
        const auto trimmed = nimple::trim(line);
        if (trimmed.empty() || trimmed.rfind("#", 0) == 0 || trimmed.rfind("*", 0) == 0) continue;
        std::smatch match;
        if (!std::regex_match(line, match, assignment) || match[2].str().empty()) {
            diagnostics.emplace_back("NB3007", lineNumber, 1, ".nbt 行应为 key = value 格式。");
            continue;
        }
        const std::string key = match[1].str();
        const std::string value = match[2].str();
        if (!keys.insert(key).second) {
            diagnostics.emplace_back("NB3008", lineNumber, 1, "尾文件配置键重复：" + key);
            continue;
        }
        const bool quoted = value.size() >= 2 && value.front() == '"' && value.back() == '"';
        const bool boolean = value == "true" || value == "false";
        char* end = nullptr;
        (void)std::strtod(value.c_str(), &end);
        const bool numeric = end != value.c_str() && end && *end == '\0';
        if (!quoted && !boolean && !numeric) {
            diagnostics.emplace_back("NB3009", lineNumber, 1, "尾文件值必须是字符串、布尔值或数字字面量。");
            continue;
        }
        if (key == "width" || key == "height") {
            if (!numeric || std::strtol(value.c_str(), nullptr, 10) <= 0 || value.find_first_of(".eE") != std::string::npos)
                diagnostics.emplace_back("NB3010", lineNumber, 1, key + " 必须是大于 0 的整数。");
        }
    }
}

static void validateAdjacentTail(const fs::path& sourcePath, std::vector<nimple::NimpleError>& diagnostics) {
    if (sourcePath.extension() != ".nb") return;
    fs::path tail = sourcePath;
    tail.replace_extension(".nbt");
    std::error_code ec;
    if (fs::is_regular_file(tail, ec) && !ec) validateTailFile(tail, diagnostics);
}

// Parse/preprocess a source buffer without executing it. The context path is
// retained so relative copy directives resolve from the edited file's folder.
static int checkSource(const fs::path& inputPath, std::string source) {
    using namespace nimple;
    const fs::path filePath = fs::absolute(inputPath);
    try {
        if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF &&
            static_cast<unsigned char>(source[1]) == 0xBB &&
            static_cast<unsigned char>(source[2]) == 0xBF) {
            source.erase(0, 3);
        }

        std::vector<NimpleError> diagnostics;
        std::vector<fs::path> includeStack;
        source = preprocessSource(source, filePath, includeStack, diagnostics);
        source = transformEasyWindowSyntax(source);
        validateAdjacentTail(filePath, diagnostics);
        if (diagnostics.empty() && !sourceHasExecutableCode(source)) return 0;
        Lexer lexer(source, source.find("__NIMPLE_SUPPRESS_CLASS__") != std::string::npos);
        auto tokens = lexer.scan();
        diagnostics.insert(diagnostics.end(), lexer.diagnostics().begin(), lexer.diagnostics().end());

        std::unordered_set<std::string> importedSystemNames;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_IMPORT_SYS_") + name + "__") != std::string::npos)
                importedSystemNames.insert(name);
        std::unordered_set<std::string> definedStatements;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_DEFINE_") + name + "__") != std::string::npos)
                definedStatements.insert(name);
        Parser parser(std::move(tokens), source.find("__NIMPLE_IMPORT_SYS__*/") != std::string::npos,
            std::move(importedSystemNames), std::move(definedStatements));
        auto program = parser.parse();
        diagnostics.insert(diagnostics.end(), parser.diagnostics().begin(), parser.diagnostics().end());
        if (diagnostics.empty()) {
            TypeChecker checker(program, filePath.extension() != ".nbf");
            auto typeDiagnostics = checker.check();
            diagnostics.insert(diagnostics.end(), typeDiagnostics.begin(), typeDiagnostics.end());
        }
        if (!diagnostics.empty()) return printDiagnostics(filePath, std::move(diagnostics));
        return 0;
    }
    catch (const NimpleError& e) {
        std::vector<NimpleError> diagnostics;
        diagnostics.emplace_back(e.code, e.line, e.column, e.what());
        return printDiagnostics(filePath, std::move(diagnostics));
    }
    catch (const std::exception& e) {
        DiagnosticEngine::emit(filePath.u8string(), 0, 0, DiagnosticSeverity::Error, "NB-I9999", e.what());
        return 1;
    }
}

static int checkFile(const fs::path& inputPath) {
    const fs::path filePath = fs::absolute(inputPath);
    std::ifstream file = openInputFile(filePath, std::ios::binary);
    if (!file) {
        nimple::DiagnosticEngine::emit(filePath.u8string(), 0, 0, nimple::DiagnosticSeverity::Error, "NB-I0001", "无法读取源文件。");
        return 2;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return checkSource(filePath, buffer.str());
}

static int checkStdin(const fs::path& contextPath) {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return checkSource(contextPath, buffer.str());
}

static int runFile(const fs::path& inputPath) {
    using namespace nimple;
    const fs::path filePath = fs::absolute(inputPath);
    try {
        std::ifstream file = openInputFile(filePath, std::ios::binary);
        if (!file) { DiagnosticEngine::emit(filePath.u8string(), 0, 0, DiagnosticSeverity::Error, "NB-I0001", "无法读取源文件。"); return 2; }
        std::ostringstream buffer; buffer << file.rdbuf();
        std::string source = buffer.str();
        if (source.size() >= 3 && static_cast<unsigned char>(source[0]) == 0xEF && static_cast<unsigned char>(source[1]) == 0xBB && static_cast<unsigned char>(source[2]) == 0xBF) source.erase(0, 3);

        std::vector<NimpleError> diagnostics;
        std::vector<fs::path> includeStack;
        source = preprocessSource(source, filePath, includeStack, diagnostics);
        source = transformEasyWindowSyntax(source);
        validateAdjacentTail(filePath, diagnostics);
        if (diagnostics.empty() && !sourceHasExecutableCode(source)) return 0;
        Lexer lexer(source, source.find("__NIMPLE_SUPPRESS_CLASS__") != std::string::npos);
        auto tokens = lexer.scan();
        diagnostics.insert(diagnostics.end(), lexer.diagnostics().begin(), lexer.diagnostics().end());

        std::unordered_set<std::string> importedSystemNames;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_IMPORT_SYS_") + name + "__") != std::string::npos)
                importedSystemNames.insert(name);
        std::unordered_set<std::string> definedStatements;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_DEFINE_") + name + "__") != std::string::npos)
                definedStatements.insert(name);
        Parser parser(std::move(tokens), source.find("__NIMPLE_IMPORT_SYS__*/") != std::string::npos,
            std::move(importedSystemNames), std::move(definedStatements));
        auto program = parser.parse();
        diagnostics.insert(diagnostics.end(), parser.diagnostics().begin(), parser.diagnostics().end());
        if (diagnostics.empty()) {
            TypeChecker checker(program);
            auto typeDiagnostics = checker.check();
            diagnostics.insert(diagnostics.end(), typeDiagnostics.begin(), typeDiagnostics.end());
        }
        if (!diagnostics.empty()) return printDiagnostics(filePath, std::move(diagnostics));

        // Syntax/preprocessor diagnostics and whole-program type diagnostics are
        // aggregated before execution. A
        // runtime failure still stops at the first failing operation because
        // subsequent effects may depend on the failed operation's result.
        Runtime runtime(std::move(program), filePath.u8string());
        return runtime.run();
    }
    catch (const NimpleError& e) {
        DiagnosticEngine::emit(filePath.u8string(), e.line, e.column, DiagnosticSeverity::Error, e.code, e.what());
        return 1;
    }
    catch (const std::exception& e) {
        DiagnosticEngine::emit(filePath.u8string(), 0, 0, DiagnosticSeverity::Error, "NB-I9999", e.what());
        return 1;
    }
}

static int runInteractiveSubmission(const std::string& input, const std::string& preamble,
    nimple::Runtime& runtime, const fs::path& contextPath) {
    using namespace nimple;
    try {
        std::vector<NimpleError> diagnostics;
        std::vector<fs::path> includeStack;
        std::string source = preprocessSource(preamble + input, contextPath, includeStack, diagnostics);
        source = transformEasyWindowSyntax(source);
        if (!diagnostics.empty()) return printDiagnostics(contextPath, std::move(diagnostics));
        if (!sourceHasExecutableCode(source)) return 0;
        Lexer lexer(source, source.find("__NIMPLE_SUPPRESS_CLASS__") != std::string::npos);
        auto tokens = lexer.scan();
        diagnostics.insert(diagnostics.end(), lexer.diagnostics().begin(), lexer.diagnostics().end());
        std::unordered_set<std::string> importedSystemNames;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_IMPORT_SYS_") + name + "__") != std::string::npos)
                importedSystemNames.insert(name);
        std::unordered_set<std::string> definedStatements;
        for (const auto* name : { "print", "printf", "printfn", "read", "pause" })
            if (source.find(std::string("__NIMPLE_DEFINE_") + name + "__") != std::string::npos)
                definedStatements.insert(name);
        Parser parser(std::move(tokens), source.find("__NIMPLE_IMPORT_SYS__*/") != std::string::npos,
            std::move(importedSystemNames), std::move(definedStatements));
        auto program = parser.parse();
        diagnostics.insert(diagnostics.end(), parser.diagnostics().begin(), parser.diagnostics().end());
        // The REPL keeps variables and methods between submissions. The regular
        // whole-program TypeChecker assumes a closed program with a Main/Window
        // entry point, so interactive submissions are parsed here and checked by
        // the persistent runtime as they execute.
        if (!diagnostics.empty()) return printDiagnostics(contextPath, std::move(diagnostics));
        return runtime.runInteractive(std::move(program));
    }
    catch (const NimpleError& e) {
        std::vector<NimpleError> diagnostics;
        diagnostics.emplace_back(e.code, e.line, e.column, e.what());
        return printDiagnostics(contextPath, std::move(diagnostics));
    }
    catch (const std::exception& e) {
        DiagnosticEngine::emit(contextPath.u8string(), 0, 0, DiagnosticSeverity::Error, "NB-I9999", e.what());
        return 1;
    }
}

#ifdef _WIN32
static bool readInteractiveSubmission(std::string& source, bool& multiline, bool& commit) {
    commit = false;
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || input == nullptr) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode)) {
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        source = line;
        commit = true;
        return true;
    }
    SetConsoleMode(input, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
    std::wstring line;
    bool submitted = false;
    while (!submitted) {
        INPUT_RECORD record{};
        DWORD readCount = 0;
        if (!ReadConsoleInputW(input, &record, 1, &readCount)) break;
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        const bool shift = (key.dwControlKeyState & SHIFT_PRESSED) != 0;
        const bool ctrl = (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        if (key.wVirtualKeyCode == VK_RETURN) {
            if (multiline && ctrl) { submitted = true; commit = true; }
            else if (!multiline && shift) { multiline = true; line.push_back(L'\n'); std::wcout << L"\n... " << std::flush; }
            else if (multiline) { line.push_back(L'\n'); std::wcout << L"\n... " << std::flush; }
            else { submitted = true; commit = true; }
            continue;
        }
        if (key.wVirtualKeyCode == VK_BACK) {
            if (!line.empty()) { line.pop_back(); std::wcout << L"\b \b"; }
            continue;
        }
        if (key.uChar.UnicodeChar >= 32 && !ctrl) {
            line.push_back(key.uChar.UnicodeChar);
            std::wcout << key.uChar.UnicodeChar;
        }
    }
    SetConsoleMode(input, mode);
    std::wcout << L"\n";
    if (!submitted) return false;
    if (!line.empty()) {
        const int needed = WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), nullptr, 0, nullptr, nullptr);
        source.resize(static_cast<std::size_t>(needed));
        if (needed) WideCharToMultiByte(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), &source[0], needed, nullptr, nullptr);
    }
    return true;
}
#else
static bool readInteractiveSubmission(std::string& source, bool& multiline, bool& commit) {
    std::string line;
    commit = false;
    if (!std::getline(std::cin, line)) return false;
    if (multiline) {
        if (line == ":end") { commit = true; return true; }
        source = line;
        return true;
    }
    if (line == ":multi") { multiline = true; return true; }
    source = line;
    commit = true;
    return true;
}
#endif

static int runInteractive() {
    using namespace nimple;
    std::cout << "Nimple Basic 0.1.0\nCopyright (c) 2026 MsintX\n\n";
    Runtime runtime(Parser(std::vector<Token>{ Token{ Token::Kind::End, "", 1, 1 } }).parse(), fs::absolute(".") / "<interactive>.nb");
    const fs::path contextPath = fs::absolute(".") / "<interactive>.nb";
    bool multiline = false;
    std::string pending;
    std::string preamble;
    for (;;) {
        std::cout << (multiline ? "... " : ">>> ") << std::flush;
        std::string submission;
        bool commit = false;
        if (!readInteractiveSubmission(submission, multiline, commit)) break;
        if (multiline) {
            pending += submission;
            if (!submission.empty() && submission.back() != '\n') pending += '\n';
            if (commit) {
                runInteractiveSubmission(pending, preamble, runtime, contextPath);
                pending.clear();
                multiline = false;
            }
            continue;
        }
        if (commit && !submission.empty()) {
            if (submission == ":quit" || submission == ":exit") break;
            const std::string trimmed = trim(submission);
            if (trimmed.rfind("statement ", 0) == 0 || trimmed.rfind("const ", 0) == 0) {
                preamble += submission + "\n";
                continue;
            }
            runInteractiveSubmission(submission, preamble, runtime, contextPath);
        }
    }
    std::cout << "\n";
    return 0;
}

#ifdef _WIN32
static void configureWindowsConsoleEncoding() {
    // Nimple source files and diagnostic strings are UTF-8. Make the attached
    // Windows console interpret narrow stdout/stderr bytes as UTF-8, so Chinese
    // diagnostics are not displayed using the legacy OEM code page.
    if (GetConsoleWindow() != nullptr) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
}

int wmain(int argc, wchar_t** argv) {
    configureWindowsConsoleEncoding();
    nimple::setExecutablePath(fs::path(argv[0]));
    if (argc == 2 && (std::wstring(argv[1]) == L"--version" || std::wstring(argv[1]) == L"-V")) {
        std::cout << "Nimple Basic 0.1.0 (C++14-compatible, cross-platform)\n";
        return 0;
    }
    if (argc == 3 && std::wstring(argv[1]) == L"--check")
        return checkFile(fs::path(argv[2]));
    if (argc == 3 && std::wstring(argv[1]) == L"--check-stdin")
        return checkStdin(fs::path(argv[2]));
    if (argc == 1) return runInteractive();
    if (argc != 2) { printUsage(); return 2; }
    return runFile(fs::path(argv[1]));
}
#else
int main(int argc, char** argv) {
    nimple::setExecutablePath(fs::u8path(argv[0]));
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-V")) {
        std::cout << "Nimple Basic 0.1.0 (C++14-compatible, cross-platform)\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--check")
        return checkFile(fs::u8path(argv[2]));
    if (argc == 3 && std::string(argv[1]) == "--check-stdin")
        return checkStdin(fs::u8path(argv[2]));
    if (argc == 1) return runInteractive();
    if (argc != 2) { printUsage(); return 2; }
    return runFile(fs::u8path(argv[1]));
}
#endif
