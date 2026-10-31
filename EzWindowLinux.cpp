// Linux backend without GTK development headers or a link-time GTK dependency.
// GTK is discovered at runtime and supplies the desktop's themed native widgets.
// Linux has no OS-level, desktop-independent widget API; see README for this tradeoff.
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace ezwindow {
    static std::vector<std::string> splitItems(const std::string& items) {
        std::vector<std::string> result; std::istringstream input(items); std::string item;
        while (std::getline(input, item)) { if (!item.empty() && item.back() == '\r') item.pop_back(); if (!item.empty()) result.push_back(item); }
        return result;
    }
    using ClickCallback = std::function<void()>;
    using ChangeCallback = std::function<void(const std::string&)>;

    struct GtkWidget;
    struct GtkTextBuffer;
    using gboolean = int;
    using gint = int;
    struct GtkTextIter { alignas(16) unsigned char opaque[128]; };
    struct GtkAllocation { gint x; gint y; gint width; gint height; };
    using guint = unsigned int;
    using gulong = unsigned long;
    using gpointer = void*;
    using GCallback = void (*)();
    using GClosureNotify = void (*)(gpointer, gpointer);

    struct GtkApi {
        void* gtkHandle{};
        void* gobjectHandle{};
        void* glibHandle{};
        int (*gtk_init_check)(int*, char***){};
        GtkWidget* (*gtk_window_new)(int){};
        void (*gtk_window_set_title)(GtkWidget*, const char*){};
        gboolean (*gtk_window_set_icon_from_file)(GtkWidget*, const char*, void**){};
        void (*gtk_window_set_default_size)(GtkWidget*, int, int){};
        void (*gtk_window_present)(GtkWidget*){};
        GtkWidget* (*gtk_box_new)(int, int){};
        GtkWidget* (*gtk_fixed_new)(){};
        void (*gtk_fixed_put)(GtkWidget*, GtkWidget*, gint, gint){};
        void (*gtk_fixed_move)(GtkWidget*, GtkWidget*, gint, gint){};
        gint (*gtk_widget_get_allocated_width)(GtkWidget*){};
        gint (*gtk_widget_get_allocated_height)(GtkWidget*){};
        void (*gtk_widget_size_allocate)(GtkWidget*, const GtkAllocation*){};
        void (*gtk_container_add)(GtkWidget*, GtkWidget*){};
        void (*gtk_container_set_border_width)(GtkWidget*, guint){};
        void (*gtk_box_pack_start)(GtkWidget*, GtkWidget*, gboolean, gboolean, guint){};
        void (*gtk_widget_show_all)(GtkWidget*){};
        void (*gtk_widget_destroy)(GtkWidget*){};
        void (*gtk_widget_set_size_request)(GtkWidget*, int, int){};
        GtkWidget* (*gtk_label_new)(const char*){};
        void (*gtk_label_set_text)(GtkWidget*, const char*){};
        const char* (*gtk_label_get_text)(GtkWidget*){};
        GtkWidget* (*gtk_button_new_with_label)(const char*){};
        void (*gtk_button_set_label)(GtkWidget*, const char*){};
        const char* (*gtk_button_get_label)(GtkWidget*){};
        GtkWidget* (*gtk_entry_new)(){};
        void (*gtk_entry_set_placeholder_text)(GtkWidget*, const char*){};
        void (*gtk_entry_set_visibility)(GtkWidget*, gboolean){};
        void (*gtk_entry_set_invisible_char)(GtkWidget*, unsigned int){};
        void (*gtk_entry_set_text)(GtkWidget*, const char*){};
        const char* (*gtk_entry_get_text)(GtkWidget*){};
        GtkWidget* (*gtk_combo_box_text_new)(){};
        void (*gtk_combo_box_text_append_text)(GtkWidget*, const char*){};
        char* (*gtk_combo_box_text_get_active_text)(GtkWidget*){};
        gint (*gtk_combo_box_get_active)(GtkWidget*){};
        void (*gtk_combo_box_set_active)(GtkWidget*, gint){};
        GtkWidget* (*gtk_radio_button_new_with_label)(void*, const char*){};
        GtkWidget* (*gtk_radio_button_new_with_label_from_widget)(GtkWidget*, const char*){};
        GtkWidget* (*gtk_scale_new_with_range)(int, double, double, double){};
        double (*gtk_range_get_value)(GtkWidget*){};
        void (*gtk_range_set_value)(GtkWidget*, double){};
        void (*gtk_range_set_range)(GtkWidget*, double, double){};
        GtkWidget* (*gtk_progress_bar_new)(){};
        void (*gtk_progress_bar_set_fraction)(GtkWidget*, double){};
        double (*gtk_progress_bar_get_fraction)(GtkWidget*){};
        GtkWidget* (*gtk_text_view_new)(){};
        GtkTextBuffer* (*gtk_text_view_get_buffer)(GtkWidget*){};
        void (*gtk_text_view_set_editable)(GtkWidget*, gboolean){};
        void (*gtk_text_buffer_set_text)(GtkTextBuffer*, const char*, gint){};
        void (*gtk_text_buffer_get_bounds)(GtkTextBuffer*, GtkTextIter*, GtkTextIter*){};
        char* (*gtk_text_buffer_get_text)(GtkTextBuffer*, const GtkTextIter*, const GtkTextIter*, gboolean){};
        GtkWidget* (*gtk_scrolled_window_new)(void*, void*){};
        void (*gtk_scrolled_window_set_policy)(GtkWidget*, int, int){};
        GtkWidget* (*gtk_check_button_new_with_label)(const char*){};
        void (*gtk_toggle_button_set_active)(GtkWidget*, gboolean){};
        gboolean (*gtk_toggle_button_get_active)(GtkWidget*){};
        void (*gtk_main)(){};
        void (*gtk_main_quit)(){};
        GtkWidget* (*gtk_message_dialog_new)(GtkWidget*, int, int, int, const char*, ...){};
        int (*gtk_dialog_run)(GtkWidget*){};
        gulong (*g_signal_connect_data)(gpointer, const char*, GCallback, gpointer, GClosureNotify, int){};
        void (*g_free)(gpointer){};
        void (*g_error_free)(void*){};
        bool initialized{};
        bool ready{};
        std::string error;
    };
    static GtkApi g_gtk;

    enum class ControlKind { Window, Label, Button, Entry, PasswordEntry, TextBox, CheckBox, ComboBox, RadioButton, Slider, ProgressBar, WebView };
    enum class LayoutMode { Auto, Absolute, Relative };
    struct ControlGeometry {
        LayoutMode mode{ LayoutMode::Auto };
        int x{ 12 }, y{ 12 }, width{ 400 }, height{ 28 };
        int xPercent{ 0 }, yPercent{ 0 }, widthPercent{ 100 }, heightPercent{ 10 };
    };
    struct NativeControl {
        GtkWidget* widget{};       // widget positioned inside the fixed container
        GtkWidget* native{};       // actual input control, if different from widget
        GtkTextBuffer* textBuffer{};
        ControlKind kind{ ControlKind::Window };
        int owner{};
        ClickCallback click;
        ChangeCallback change;
        ControlGeometry geometry{};
        int minimum{ 0 };
        int maximum{ 100 };
        std::vector<std::string> items;
    };
    static std::unordered_map<int, NativeControl> g_controls;
    static std::unordered_map<int, GtkWidget*> g_windows;
    static std::unordered_map<int, GtkWidget*> g_boxes;
    static std::unordered_map<int, std::pair<int, int>> g_windowDefaults;
    static std::unordered_map<int, std::vector<int>> g_controlOrder;
    static std::unordered_map<int, GtkWidget*> g_radioGroups;
    static std::unordered_set<int> g_layouting;
    static int g_nextId = 1;

    template<class T>
    static bool loadSymbol(void* handle, const char* name, T& out) {
        out = reinterpret_cast<T>(dlsym(handle, name));
        return out != nullptr;
    }
    static bool ensureLibraries() noexcept {
        if (g_gtk.ready) return true;
        if (!g_gtk.gtkHandle) g_gtk.gtkHandle = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!g_gtk.gobjectHandle) g_gtk.gobjectHandle = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!g_gtk.glibHandle) g_gtk.glibHandle = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!g_gtk.gtkHandle || !g_gtk.gobjectHandle || !g_gtk.glibHandle) {
            g_gtk.error = "EzWindow：未找到系统 GTK 3 运行库 (libgtk-3.so.0)。在 Linux 上需要桌面环境提供 GTK；Nimple 不需要 GTK 开发头文件或链接期依赖。";
            return false;
        }
#define NIMPLE_GTK_SYMBOL(handle, name) if (!loadSymbol(g_gtk.handle, #name, g_gtk.name)) { g_gtk.error = std::string("EzWindow：系统 GTK 缺少函数 ") + #name; return false; }
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_init_check)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_window_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_window_set_title)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_window_set_icon_from_file)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_window_set_default_size)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_window_present)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_box_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_fixed_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_fixed_put)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_fixed_move)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_get_allocated_width)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_get_allocated_height)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_size_allocate)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_container_add)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_container_set_border_width)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_box_pack_start)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_show_all)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_destroy)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_widget_set_size_request)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_label_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_label_set_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_label_get_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_button_new_with_label)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_button_set_label)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_button_get_label)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_set_placeholder_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_set_visibility)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_set_invisible_char)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_set_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_entry_get_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_combo_box_text_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_combo_box_text_append_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_combo_box_text_get_active_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_combo_box_get_active)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_combo_box_set_active)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_radio_button_new_with_label)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_radio_button_new_with_label_from_widget)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_scale_new_with_range)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_range_get_value)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_range_set_value)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_range_set_range)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_progress_bar_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_progress_bar_set_fraction)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_progress_bar_get_fraction)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_view_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_view_get_buffer)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_view_set_editable)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_buffer_set_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_buffer_get_bounds)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_text_buffer_get_text)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_scrolled_window_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_scrolled_window_set_policy)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_check_button_new_with_label)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_toggle_button_set_active)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_toggle_button_get_active)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_main)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_main_quit)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_message_dialog_new)
        NIMPLE_GTK_SYMBOL(gtkHandle, gtk_dialog_run)
        NIMPLE_GTK_SYMBOL(gobjectHandle, g_signal_connect_data)
        NIMPLE_GTK_SYMBOL(glibHandle, g_free)
        NIMPLE_GTK_SYMBOL(glibHandle, g_error_free)
#undef NIMPLE_GTK_SYMBOL
        g_gtk.ready = true;
        return true;
    }
    static void ensureStarted() {
        if (!ensureLibraries()) throw std::runtime_error(g_gtk.error);
        if (!g_gtk.initialized) {
            if (!g_gtk.gtk_init_check(nullptr, nullptr)) {
                throw std::runtime_error("EzWindow：GTK 无法连接到图形桌面。请在 X11/Wayland 桌面会话中运行 Nimple GUI 程序。");
            }
            g_gtk.initialized = true;
        }
    }
    static std::string asString(const char* value) { return value ? std::string(value) : std::string(); }
    static NativeControl& findControl(int id) {
        auto found = g_controls.find(id);
        if (found == g_controls.end() || (found->second.kind == ControlKind::Window && !found->second.widget))
            throw std::runtime_error("EzWindow：无效的控件 ID " + std::to_string(id) + "。");
        return found->second;
    }
    static GtkWidget* findWindow(int id) {
        auto found = g_windows.find(id);
        if (found == g_windows.end() || !found->second) throw std::runtime_error("EzWindow：无效的窗口 ID " + std::to_string(id) + "。");
        return found->second;
    }
    static std::string readText(const NativeControl& c) {
        switch (c.kind) {
        case ControlKind::Window: throw std::runtime_error("EzWindow：窗口不是文本控件。");
        case ControlKind::Label: return asString(g_gtk.gtk_label_get_text(c.native));
        case ControlKind::Button:
        case ControlKind::CheckBox: return asString(g_gtk.gtk_button_get_label(c.native));
        case ControlKind::Entry:
        case ControlKind::PasswordEntry: return asString(g_gtk.gtk_entry_get_text(c.native));
        case ControlKind::ComboBox: {
            char* active = g_gtk.gtk_combo_box_text_get_active_text(c.native);
            std::string value = asString(active);
            if (active) g_gtk.g_free(active);
            return value;
        }
        case ControlKind::RadioButton: return asString(g_gtk.gtk_button_get_label(c.native));
        case ControlKind::Slider: return std::to_string(static_cast<int>(g_gtk.gtk_range_get_value(c.native)));
        case ControlKind::ProgressBar: return std::to_string(static_cast<int>(std::lround(g_gtk.gtk_progress_bar_get_fraction(c.native) * (c.maximum - c.minimum) + c.minimum)));
        case ControlKind::WebView: throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。");
        case ControlKind::TextBox: {
            GtkTextIter start{}, end{};
            g_gtk.gtk_text_buffer_get_bounds(c.textBuffer, &start, &end);
            char* text = g_gtk.gtk_text_buffer_get_text(c.textBuffer, &start, &end, 1);
            std::string result = asString(text);
            if (text) g_gtk.g_free(text);
            return result;
        }
        }
        return {};
    }
    static void clickSignal(GtkWidget*, gpointer data) {
        const int id = static_cast<int>(reinterpret_cast<intptr_t>(data));
        auto found = g_controls.find(id);
        if (found == g_controls.end()) return;
        auto callback = found->second.click;
        if (!callback) return;
        try { callback(); }
        catch (const std::exception& e) { std::fprintf(stderr, "EzWindow callback error: %s\n", e.what()); }
        catch (...) { std::fprintf(stderr, "EzWindow callback error: unknown exception\n"); }
    }
    static void changeSignal(void*, gpointer data) {
        const int id = static_cast<int>(reinterpret_cast<intptr_t>(data));
        auto found = g_controls.find(id);
        if (found == g_controls.end()) return;
        std::string value;
        if (found->second.kind == ControlKind::CheckBox || found->second.kind == ControlKind::RadioButton) {
            value = g_gtk.gtk_toggle_button_get_active(found->second.native) ? "true" : "false";
        } else {
            try { value = readText(found->second); }
            catch (...) { return; }
        }
        auto callback = found->second.change;
        if (!callback) return;
        try { callback(value); }
        catch (const std::exception& e) { std::fprintf(stderr, "EzWindow callback error: %s\n", e.what()); }
        catch (...) { std::fprintf(stderr, "EzWindow callback error: unknown exception\n"); }
    }
    static void windowDestroyed(GtkWidget*, gpointer data) {
        const int id = static_cast<int>(reinterpret_cast<intptr_t>(data));
        auto found = g_windows.find(id);
        if (found != g_windows.end()) found->second = nullptr;
        bool anyOpen = false;
        for (const auto& item : g_windows) if (item.second) { anyOpen = true; break; }
        if (!anyOpen) g_gtk.gtk_main_quit();
    }
    static void connectSignal(GtkWidget* widget, const char* signal, GCallback callback, int id) {
        g_gtk.g_signal_connect_data(widget, signal, callback,
            reinterpret_cast<gpointer>(static_cast<intptr_t>(id)), nullptr, 0);
    }

    static int defaultControlHeight(ControlKind kind) {
        switch (kind) {
        case ControlKind::TextBox: return 100;
        case ControlKind::Label: return 24;
        case ControlKind::CheckBox:
        case ControlKind::RadioButton: return 26;
        case ControlKind::Button: return 30;
        case ControlKind::Entry:
        case ControlKind::PasswordEntry:
        case ControlKind::ComboBox: return 28;
        case ControlKind::Slider: return 36;
        case ControlKind::ProgressBar: return 24;
        case ControlKind::WebView: return 180;
        case ControlKind::Window: return 0;
        }
        return 28;
    }
    static void layoutWindow(int windowId) {
        auto boxIt = g_boxes.find(windowId);
        if (boxIt == g_boxes.end() || !boxIt->second || g_layouting.count(windowId)) return;
        g_layouting.insert(windowId);
        GtkWidget* fixed = boxIt->second;
        auto defaults = g_windowDefaults.find(windowId);
        int clientWidth = g_gtk.gtk_widget_get_allocated_width(fixed);
        int clientHeight = g_gtk.gtk_widget_get_allocated_height(fixed);
        if (clientWidth <= 1 && defaults != g_windowDefaults.end()) clientWidth = defaults->second.first;
        if (clientHeight <= 1 && defaults != g_windowDefaults.end()) clientHeight = defaults->second.second;
        clientWidth = std::max(1, clientWidth);
        clientHeight = std::max(1, clientHeight);
        int autoY = 12;
        auto order = g_controlOrder.find(windowId);
        if (order == g_controlOrder.end()) { g_layouting.erase(windowId); return; }
        for (int controlId : order->second) {
            auto found = g_controls.find(controlId);
            if (found == g_controls.end()) continue;
            NativeControl& c = found->second;
            if (c.kind == ControlKind::Window || c.owner != windowId || !c.widget) continue;
            const int defaultHeight = defaultControlHeight(c.kind);
            int x = c.geometry.x, y = c.geometry.y, w = c.geometry.width, h = c.geometry.height;
            if (c.geometry.mode == LayoutMode::Auto) {
                x = 12; y = autoY; w = std::max(1, clientWidth - 24); h = defaultHeight;
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = w; c.geometry.height = h;
            } else if (c.geometry.mode == LayoutMode::Relative) {
                x = static_cast<int>((static_cast<long long>(clientWidth) * c.geometry.xPercent) / 100);
                y = static_cast<int>((static_cast<long long>(clientHeight) * c.geometry.yPercent) / 100);
                w = std::max(1, static_cast<int>((static_cast<long long>(clientWidth) * c.geometry.widthPercent) / 100));
                h = std::max(1, static_cast<int>((static_cast<long long>(clientHeight) * c.geometry.heightPercent) / 100));
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = w; c.geometry.height = h;
            }
            g_gtk.gtk_fixed_move(fixed, c.widget, x, y);
            w = std::max(1, w); h = std::max(1, h);
            // A size request is a GTK minimum size and could prevent shrinking
            // the top-level window. Allocate the child after GtkFixed's own
            // allocation instead, keeping relative layouts genuinely responsive.
            const GtkAllocation allocation{ x, y, w, h };
            g_gtk.gtk_widget_size_allocate(c.widget, &allocation);
            autoY += defaultHeight + 7;
        }
        g_layouting.erase(windowId);
    }
    static void windowSizeAllocated(GtkWidget*, void*, gpointer data) {
        const int windowId = static_cast<int>(reinterpret_cast<intptr_t>(data));
        layoutWindow(windowId);
    }
    static void validateBounds(int width, int height) {
        if (width <= 0 || height <= 0) throw std::runtime_error("EzWindow：控件宽度和高度必须大于 0。");
    }
    static void validateRelativeBounds(int x, int y, int width, int height) {
        if (x < 0 || x > 100 || y < 0 || y > 100 || width <= 0 || width > 100 || height <= 0 || height > 100 ||
            x + width > 100 || y + height > 100)
            throw std::runtime_error("EzWindow：相对布局参数必须在 0..100 范围内，且 x+width、y+height 不能超过 100。");
    }
    static void applyLayoutForControl(NativeControl& c) {
        if (c.kind == ControlKind::Window) throw std::runtime_error("EzWindow：布局 API 的目标必须是控件，不能是窗口。");
        layoutWindow(c.owner);
    }

    // Backend support exists on Linux; createWindow() reports a precise runtime
    // diagnostic if GTK or the current graphical session is missing.
    bool available() noexcept { return true; }
    int createWindow(const std::string& title, int width, int height, const std::string& iconPath) {
        ensureStarted();
        const int id = g_nextId++;
        GtkWidget* window = g_gtk.gtk_window_new(0); // GTK_WINDOW_TOPLEVEL
        if (!window) throw std::runtime_error("EzWindow：GTK 创建窗口失败。");
        g_gtk.gtk_window_set_title(window, title.c_str());
        if (!iconPath.empty()) {
            void* error = nullptr;
            if (!g_gtk.gtk_window_set_icon_from_file(window, iconPath.c_str(), &error)) {
                if (error) g_gtk.g_error_free(error);
                g_gtk.gtk_widget_destroy(window);
                throw std::runtime_error("EzWindow：无法从文件加载窗口图标：" + iconPath);
            }
        }
        g_gtk.gtk_window_set_default_size(window, std::max(200, width), std::max(200, height));
        GtkWidget* box = g_gtk.gtk_fixed_new();
        g_gtk.gtk_container_set_border_width(box, 0);
        g_gtk.gtk_container_add(window, box);
        g_windows.emplace(id, window);
        g_boxes.emplace(id, box);
        g_windowDefaults.emplace(id, std::make_pair(std::max(200, width), std::max(200, height)));
        g_controlOrder.emplace(id, std::vector<int>{});
        g_controls.emplace(id, NativeControl{ window, window, nullptr, ControlKind::Window, 0, {}, {}, {}, 0, 100, {} });
        connectSignal(window, "destroy", reinterpret_cast<GCallback>(windowDestroyed), id);
        connectSignal(box, "size-allocate", reinterpret_cast<GCallback>(windowSizeAllocated), id);
        return id;
    }
    static int addControl(int windowId, const std::string& text, ControlKind kind, int minimum = 0, int maximum = 100) {
        ensureStarted();
        (void)findWindow(windowId);
        GtkWidget* box = g_boxes.at(windowId);
        GtkWidget* widget = nullptr;
        GtkWidget* native = nullptr;
        GtkTextBuffer* buffer = nullptr;
        std::vector<std::string> items;
        switch (kind) {
        case ControlKind::Label: widget = native = g_gtk.gtk_label_new(text.c_str()); break;
        case ControlKind::Button: widget = native = g_gtk.gtk_button_new_with_label(text.c_str()); break;
        case ControlKind::Entry:
        case ControlKind::PasswordEntry:
            widget = native = g_gtk.gtk_entry_new();
            g_gtk.gtk_entry_set_placeholder_text(native, text.c_str());
            if (kind == ControlKind::PasswordEntry) {
                g_gtk.gtk_entry_set_invisible_char(native, static_cast<unsigned int>('*'));
                g_gtk.gtk_entry_set_visibility(native, 0);
            }
            break;
        case ControlKind::TextBox: {
            GtkWidget* textView = g_gtk.gtk_text_view_new();
            buffer = g_gtk.gtk_text_view_get_buffer(textView);
            g_gtk.gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<gint>(text.size()));
            widget = g_gtk.gtk_scrolled_window_new(nullptr, nullptr);
            g_gtk.gtk_scrolled_window_set_policy(widget, 2, 1);
            g_gtk.gtk_container_add(widget, textView);
            g_gtk.gtk_widget_set_size_request(widget, -1, 100);
            native = textView;
            break;
        }
        case ControlKind::CheckBox: widget = native = g_gtk.gtk_check_button_new_with_label(text.c_str()); break;
        case ControlKind::RadioButton: {
            GtkWidget* group = g_radioGroups[windowId];
            widget = native = group ? g_gtk.gtk_radio_button_new_with_label_from_widget(group, text.c_str())
                                    : g_gtk.gtk_radio_button_new_with_label(nullptr, text.c_str());
            if (!group) g_radioGroups[windowId] = native;
            break;
        }
        case ControlKind::ComboBox:
            widget = native = g_gtk.gtk_combo_box_text_new();
            items = splitItems(text);
            for (const auto& item : items) g_gtk.gtk_combo_box_text_append_text(native, item.c_str());
            if (!items.empty()) g_gtk.gtk_combo_box_set_active(native, 0);
            break;
        case ControlKind::Slider:
            if (maximum <= minimum) throw std::runtime_error("EzWindow：控件最大值必须大于最小值。");
            widget = native = g_gtk.gtk_scale_new_with_range(0, minimum, maximum, 1.0);
            g_gtk.gtk_range_set_value(native, minimum);
            break;
        case ControlKind::ProgressBar:
            if (maximum <= minimum) throw std::runtime_error("EzWindow：控件最大值必须大于最小值。");
            widget = native = g_gtk.gtk_progress_bar_new();
            g_gtk.gtk_progress_bar_set_fraction(native, 0.0);
            break;
        case ControlKind::WebView: throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。");
        case ControlKind::Window: break;
        }
        if (!widget || !native) throw std::runtime_error("EzWindow：GTK 创建原生控件失败。");
        const int id = g_nextId++;
        g_gtk.gtk_fixed_put(box, widget, 12, 12);
        g_controls.emplace(id, NativeControl{ widget, native, buffer, kind, windowId, {}, {}, {}, minimum, maximum, std::move(items) });
        g_controlOrder[windowId].push_back(id);
        layoutWindow(windowId);
        return id;
    }
    int addLabel(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Label); }
    int addButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Button); }
    int addEntry(int windowId, const std::string& placeholder) { return addControl(windowId, placeholder, ControlKind::Entry); }
    int addTextBox(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::TextBox); }
    int addCheckBox(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::CheckBox); }
    int addPasswordEntry(int windowId, const std::string& placeholder) { return addControl(windowId, placeholder, ControlKind::PasswordEntry); }
    int addComboBox(int windowId, const std::string& items) { return addControl(windowId, items, ControlKind::ComboBox); }
    int addRadioButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::RadioButton); }
    int addSlider(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::Slider, minimum, maximum); }
    int addProgressBar(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::ProgressBar, minimum, maximum); }
    int addWebView(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用；Linux 目前没有跨平台 WebView 后端。"); }
    void webViewNavigate(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。"); }
    void webViewSetHtml(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。"); }
    void setText(int widgetId, const std::string& text) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        switch (c.kind) {
        case ControlKind::Window: throw std::runtime_error("setText 的目标必须是控件，不能是窗口。");
        case ControlKind::Label: g_gtk.gtk_label_set_text(c.native, text.c_str()); break;
        case ControlKind::Button:
        case ControlKind::CheckBox:
        case ControlKind::RadioButton: g_gtk.gtk_button_set_label(c.native, text.c_str()); break;
        case ControlKind::Entry:
        case ControlKind::PasswordEntry: g_gtk.gtk_entry_set_text(c.native, text.c_str()); break;
        case ControlKind::TextBox: g_gtk.gtk_text_buffer_set_text(c.textBuffer, text.c_str(), static_cast<gint>(text.size())); break;
        case ControlKind::ComboBox: {
            auto found = std::find(c.items.begin(), c.items.end(), text);
            if (found == c.items.end()) throw std::runtime_error("setText：ComboBox 中不存在选项“" + text + "”。");
            g_gtk.gtk_combo_box_set_active(c.native, static_cast<gint>(found - c.items.begin())); break;
        }
        case ControlKind::Slider: case ControlKind::ProgressBar: case ControlKind::WebView: throw std::runtime_error("setText 不适用于此控件类型。");
        }
    }
    std::string getText(int widgetId) { ensureStarted(); NativeControl& c = findControl(widgetId); if (c.kind == ControlKind::Slider || c.kind == ControlKind::ProgressBar || c.kind == ControlKind::WebView) throw std::runtime_error("getText 不适用于 slider、progressBar 或 WebView 控件。"); return readText(c); }
    void setChecked(int widgetId, bool checked) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton) throw std::runtime_error("setChecked 只支持 checkBox 或 radioButton 控件。");
        g_gtk.gtk_toggle_button_set_active(c.native, checked ? 1 : 0);
    }
    bool getChecked(int widgetId) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton) throw std::runtime_error("getChecked 只支持 checkBox 或 radioButton 控件。");
        return g_gtk.gtk_toggle_button_get_active(c.native) != 0;
    }
    void setValue(int widgetId, int value) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::Slider && c.kind != ControlKind::ProgressBar) throw std::runtime_error("setValue 只支持 slider 或 progressBar 控件。");
        value = std::max(c.minimum, std::min(c.maximum, value));
        if (c.kind == ControlKind::Slider) g_gtk.gtk_range_set_value(c.native, value);
        else g_gtk.gtk_progress_bar_set_fraction(c.native, static_cast<double>(value - c.minimum) / (c.maximum - c.minimum));
    }
    int getValue(int widgetId) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Slider) return static_cast<int>(std::lround(g_gtk.gtk_range_get_value(c.native)));
        if (c.kind == ControlKind::ProgressBar) return static_cast<int>(std::lround(g_gtk.gtk_progress_bar_get_fraction(c.native) * (c.maximum - c.minimum) + c.minimum));
        throw std::runtime_error("getValue 只支持 slider 或 progressBar 控件。");
    }
    void setTitle(int windowId, const std::string& title) { ensureStarted(); g_gtk.gtk_window_set_title(findWindow(windowId), title.c_str()); }
    void showWindow(int windowId) { ensureStarted(); g_gtk.gtk_widget_show_all(findWindow(windowId)); g_gtk.gtk_window_present(findWindow(windowId)); }
    void setPosition(int widgetId, int x, int y) {
        ensureStarted(); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setPosition 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y; applyLayoutForControl(c);
    }
    void setSize(int widgetId, int width, int height) {
        ensureStarted(); validateBounds(width, height); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setSize 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.width = width; c.geometry.height = height; applyLayoutForControl(c);
    }
    void setBounds(int widgetId, int x, int y, int width, int height) {
        ensureStarted(); validateBounds(width, height); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y; c.geometry.width = width; c.geometry.height = height; applyLayoutForControl(c);
    }
    void setRelativeBounds(int widgetId, int xPercent, int yPercent, int widthPercent, int heightPercent) {
        ensureStarted(); validateRelativeBounds(xPercent, yPercent, widthPercent, heightPercent); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setRelativeBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Relative; c.geometry.xPercent = xPercent; c.geometry.yPercent = yPercent;
        c.geometry.widthPercent = widthPercent; c.geometry.heightPercent = heightPercent; applyLayoutForControl(c);
    }
    void setOnClick(int widgetId, ClickCallback callback) {
        NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::Button) throw std::runtime_error("onClick 只支持 button 控件。");
        c.click = std::move(callback);
        connectSignal(c.native, "clicked", reinterpret_cast<GCallback>(clickSignal), widgetId);
    }
    void setOnChange(int widgetId, ChangeCallback callback) {
        NativeControl& c = findControl(widgetId);
        if (c.kind != ControlKind::Entry && c.kind != ControlKind::PasswordEntry && c.kind != ControlKind::TextBox &&
            c.kind != ControlKind::CheckBox && c.kind != ControlKind::RadioButton && c.kind != ControlKind::ComboBox && c.kind != ControlKind::Slider)
            throw std::runtime_error("onChange 只支持 entry、passwordEntry、textBox、checkBox、radioButton、comboBox 或 slider 控件。");
        c.change = std::move(callback);
        if (c.kind == ControlKind::Entry || c.kind == ControlKind::PasswordEntry) connectSignal(c.native, "changed", reinterpret_cast<GCallback>(changeSignal), widgetId);
        else if (c.kind == ControlKind::TextBox) connectSignal(reinterpret_cast<GtkWidget*>(c.textBuffer), "changed", reinterpret_cast<GCallback>(changeSignal), widgetId);
        else if (c.kind == ControlKind::ComboBox) connectSignal(c.native, "changed", reinterpret_cast<GCallback>(changeSignal), widgetId);
        else if (c.kind == ControlKind::Slider) connectSignal(c.native, "value-changed", reinterpret_cast<GCallback>(changeSignal), widgetId);
        else connectSignal(c.native, "toggled", reinterpret_cast<GCallback>(changeSignal), widgetId);
    }
    void mainLoop() {
        ensureStarted();
        if (g_windows.empty()) throw std::runtime_error("尚未创建 EzWindow 窗口。");
        for (const auto& pair : g_windows) if (pair.second) g_gtk.gtk_widget_show_all(pair.second);
        g_gtk.gtk_main();
        g_controls.clear(); g_windows.clear(); g_boxes.clear(); g_windowDefaults.clear(); g_controlOrder.clear(); g_radioGroups.clear(); g_layouting.clear(); g_nextId = 1;
    }
    void quit() { if (ensureLibraries()) g_gtk.gtk_main_quit(); }
    void message(const std::string& title, const std::string& text) {
        ensureStarted();
        // GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK; varargs API is system GTK.
        GtkWidget* dialog = g_gtk.gtk_message_dialog_new(nullptr, 1, 0, 1, "%s", text.c_str());
        if (!dialog) throw std::runtime_error("EzWindow：无法创建原生消息框。");
        g_gtk.gtk_window_set_title(dialog, title.c_str());
        g_gtk.gtk_dialog_run(dialog);
        g_gtk.gtk_widget_destroy(dialog);
    }
}
