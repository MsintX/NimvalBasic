#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cfloat>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <functional>
#include <sstream>
#include <cmath>

namespace ezwindow {
    using ClickCallback = std::function<void()>;
    using ChangeCallback = std::function<void(const std::string&)>;
    static std::vector<std::string> splitItems(const std::string& items) {
        std::vector<std::string> result;
        std::istringstream input(items); std::string item;
        while (std::getline(input, item)) { if (!item.empty() && item.back() == '\r') item.pop_back(); if (!item.empty()) result.push_back(item); }
        return result;
    }

    enum class ControlKind { Window, Label, Button, Entry, PasswordEntry, TextBox, CheckBox, ComboBox, RadioButton, Slider, ProgressBar, WebView };
    enum class LayoutMode { Auto, Absolute, Relative };
    struct ControlGeometry {
        LayoutMode mode{ LayoutMode::Auto };
        int x{ 12 }, y{ 12 }, width{ 400 }, height{ 28 };
        int xPercent{ 0 }, yPercent{ 0 }, widthPercent{ 100 }, heightPercent{ 10 };
    };
    struct NativeControl {
        NSWindow* window{ nil };
        NSView* view{ nil };       // View packed into the vertical stack.
        NSView* editable{ nil };   // NSTextField or NSTextView when applicable.
        ControlKind kind{ ControlKind::Window };
        int owner{ 0 };
        ClickCallback click;
        ChangeCallback change;
        id target{ nil };
        ControlGeometry geometry{};
        int minimum{ 0 };
        int maximum{ 100 };
        std::vector<std::string> items;
    };

    static std::unordered_map<int, NativeControl> g_controls;
    static std::unordered_map<int, NSWindow*> g_windows;
    static std::unordered_map<int, NSView*> g_contentViews;
    static std::unordered_map<int, std::vector<int>> g_controlOrder;
    static int g_nextId = 1;
    static void layoutWindow(int windowId);
    static void validateBounds(int width, int height);
    static void validateRelativeBounds(int x, int y, int width, int height);
    static NSAutoreleasePool* g_pool = nil;

    static NSString* toNSString(const std::string& value) {
        NSString* result = [NSString stringWithUTF8String:value.c_str()];
        return result ?: @"";
    }
    static std::string fromNSString(NSString* value) {
        if (!value) return {};
        const char* text = [value UTF8String];
        return text ? std::string(text) : std::string();
    }

    static std::string readText(const NativeControl& control) {
        switch (control.kind) {
        case ControlKind::Label: return fromNSString([(NSTextField*)control.editable stringValue]);
        case ControlKind::Button:
        case ControlKind::CheckBox:
        case ControlKind::RadioButton: return fromNSString([(NSButton*)control.editable title]);
        case ControlKind::Entry:
        case ControlKind::PasswordEntry: return fromNSString([(NSTextField*)control.editable stringValue]);
        case ControlKind::TextBox: return fromNSString([(NSTextView*)control.editable string]);
        case ControlKind::ComboBox: return fromNSString([(NSComboBox*)control.editable objectValueOfSelectedItem]);
        case ControlKind::Slider: return std::to_string(static_cast<int>(std::lround([(NSSlider*)control.editable integerValue])));
        case ControlKind::ProgressBar: return std::to_string(static_cast<int>(std::lround([(NSProgressIndicator*)control.editable doubleValue])));
        case ControlKind::WebView: throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。");
        case ControlKind::Window: break;
        }
        throw std::runtime_error("EzWindow：窗口不是文本控件。");
    }

    static void dispatchClick(int id) {
        auto found = g_controls.find(id);
        if (found == g_controls.end()) return;
        auto callback = found->second.click;
        if (callback) {
            try { callback(); }
            catch (const std::exception& e) {
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Nimple callback error"];
                [alert setInformativeText:toNSString(e.what())];
                [alert runModal];
                [alert release];
            } catch (...) {
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Nimple callback error"];
                [alert setInformativeText:@"Unknown Nimple callback error"];
                [alert runModal];
                [alert release];
            }
        }
    }
    static void dispatchChange(int id, const std::string& value) {
        auto found = g_controls.find(id);
        if (found == g_controls.end()) return;
        auto callback = found->second.change;
        if (callback) {
            try { callback(value); }
            catch (const std::exception& e) {
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Nimple callback error"];
                [alert setInformativeText:toNSString(e.what())];
                [alert runModal];
                [alert release];
            } catch (...) {
                NSAlert* alert = [[NSAlert alloc] init];
                [alert setMessageText:@"Nimple callback error"];
                [alert setInformativeText:@"Unknown Nimple callback error"];
                [alert runModal];
                [alert release];
            }
        }
    }

} // namespace ezwindow

@interface NimpleEzWindowTarget : NSObject {
    @public
        int controlId;
    }
    - (void)controlAction:(id)sender;
    - (void)textDidChange:(NSNotification*)notification;
    - (void)windowWillClose:(NSNotification*)notification;
    - (void)windowDidResize:(NSNotification*)notification;
    @end

@implementation NimpleEzWindowTarget
    - (void)controlAction:(id)sender {
        auto found = ezwindow::g_controls.find(controlId);
        if (found == ezwindow::g_controls.end()) return;
        if (found->second.kind == ezwindow::ControlKind::CheckBox || found->second.kind == ezwindow::ControlKind::RadioButton) {
            if (found->second.kind == ezwindow::ControlKind::RadioButton) {
                for (auto& entry : ezwindow::g_controls) {
                    if (entry.first != controlId && entry.second.owner == found->second.owner && entry.second.kind == ezwindow::ControlKind::RadioButton)
                        [(NSButton*)entry.second.editable setState:NSControlStateValueOff];
                }
            }
            const bool checked = [(NSButton*)sender state] == NSControlStateValueOn;
            ezwindow::dispatchChange(controlId, checked ? "true" : "false");
        } else if (found->second.kind == ezwindow::ControlKind::Slider) {
            ezwindow::dispatchChange(controlId, std::to_string(static_cast<int>([(NSSlider*)sender integerValue])));
        } else if (found->second.kind == ezwindow::ControlKind::ComboBox) {
            ezwindow::dispatchChange(controlId, ezwindow::readText(found->second));
        } else {
            ezwindow::dispatchClick(controlId);
        }
    }
    - (void)textDidChange:(NSNotification*)notification {
        (void)notification;
        auto found = ezwindow::g_controls.find(controlId);
        if (found == ezwindow::g_controls.end()) return;
        ezwindow::dispatchChange(controlId, ezwindow::readText(found->second));
    }
    - (void)windowDidResize:(NSNotification*)notification {
        (void)notification;
        ezwindow::layoutWindow(controlId);
    }
    - (void)windowWillClose:(NSNotification*)notification {
        bool anyOpen = false;
        for (const auto& item : ezwindow::g_windows) {
            NSWindow* candidate = item.second;
            if (candidate && candidate != [notification object] && [candidate isVisible]) {
                anyOpen = true;
                break;
            }
        }
        if (!anyOpen) {
            [NSApp stop:nil];
            NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
                context:nil subtype:0 data1:0 data2:0];
            [NSApp postEvent:wake atStart:YES];
        }
    }
@end

namespace ezwindow {
    static void ensureStarted() {
        if (!g_pool) g_pool = [[NSAutoreleasePool alloc] init];
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
    static NativeControl& findControl(int id) {
        auto found = g_controls.find(id);
        if (found == g_controls.end()) throw std::runtime_error("EzWindow：无效的控件 ID " + std::to_string(id) + "。");
        return found->second;
    }
    static NSWindow* findWindow(int id) {
        auto found = g_windows.find(id);
        if (found == g_windows.end() || !found->second) throw std::runtime_error("EzWindow：无效的窗口 ID " + std::to_string(id) + "。");
        return found->second;
    }
    static NimpleEzWindowTarget* targetFor(int id) {
        NativeControl& control = findControl(id);
        if (!control.target) {
            NimpleEzWindowTarget* target = [[NimpleEzWindowTarget alloc] init];
            target->controlId = id;
            control.target = target;
        }
        return (NimpleEzWindowTarget*)control.target;
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
        auto winIt = g_windows.find(windowId);
        auto contentIt = g_contentViews.find(windowId);
        auto orderIt = g_controlOrder.find(windowId);
        if (winIt == g_windows.end() || !winIt->second || contentIt == g_contentViews.end() || orderIt == g_controlOrder.end()) return;
        NSView* content = contentIt->second;
        const CGFloat clientWidth = std::max<CGFloat>(1, NSWidth([content bounds]));
        const CGFloat clientHeight = std::max<CGFloat>(1, NSHeight([content bounds]));
        int autoY = 12;
        for (int controlId : orderIt->second) {
            auto found = g_controls.find(controlId);
            if (found == g_controls.end()) continue;
            NativeControl& c = found->second;
            if (c.kind == ControlKind::Window || !c.view) continue;
            const int defaultHeight = defaultControlHeight(c.kind);
            int x = c.geometry.x, y = c.geometry.y, width = c.geometry.width, height = c.geometry.height;
            if (c.geometry.mode == LayoutMode::Auto) {
                x = 12; y = autoY; width = std::max(1, static_cast<int>(clientWidth) - 24); height = defaultHeight;
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = width; c.geometry.height = height;
            } else if (c.geometry.mode == LayoutMode::Relative) {
                x = static_cast<int>((static_cast<double>(clientWidth) * c.geometry.xPercent) / 100.0);
                y = static_cast<int>((static_cast<double>(clientHeight) * c.geometry.yPercent) / 100.0);
                width = std::max(1, static_cast<int>((static_cast<double>(clientWidth) * c.geometry.widthPercent) / 100.0));
                height = std::max(1, static_cast<int>((static_cast<double>(clientHeight) * c.geometry.heightPercent) / 100.0));
                c.geometry.x = x; c.geometry.y = y; c.geometry.width = width; c.geometry.height = height;
            }
            // Nimple coordinates start at the content area's top-left; Cocoa uses bottom-left.
            [c.view setFrame:NSMakeRect(x, clientHeight - y - height, width, height)];
            autoY += defaultHeight + 7;
        }
    }
    static void validateBounds(int width, int height) {
        if (width <= 0 || height <= 0) throw std::runtime_error("EzWindow：控件宽度和高度必须大于 0。");
    }
    static void validateRelativeBounds(int x, int y, int width, int height) {
        if (x < 0 || x > 100 || y < 0 || y > 100 || width <= 0 || width > 100 || height <= 0 || height > 100 ||
            x + width > 100 || y + height > 100)
            throw std::runtime_error("EzWindow：相对布局参数必须在 0..100 范围内，且 x+width、y+height 不能超过 100。");
    }
    static void applyLayoutForControl(NativeControl& control) {
        if (control.kind == ControlKind::Window) throw std::runtime_error("EzWindow：布局 API 的目标必须是控件，不能是窗口。");
        layoutWindow(control.owner);
    }

    bool available() noexcept { return true; }

    int createWindow(const std::string& title, int width, int height, const std::string& iconPath) {
        ensureStarted();
        const int id = g_nextId++;
        NSRect frame = NSMakeRect(0, 0, std::max(200, width), std::max(200, height));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
            backing:NSBackingStoreBuffered defer:NO];
        [window setTitle:toNSString(title)];
        if (!iconPath.empty()) {
            NSImage* icon = [[NSImage alloc] initWithContentsOfFile:toNSString(iconPath)];
            if (!icon) { [window release]; throw std::runtime_error("EzWindow：无法从文件加载应用图标：" + iconPath); }
            [NSApp setApplicationIconImage:icon];
            [icon release];
        }
        [window setReleasedWhenClosed:NO];

        NSView* content = [[NSView alloc] initWithFrame:frame];
        [window setContentView:content];
        [content release]; // NSWindow owns the content view now.
        NSView* ownedContent = [window contentView];
        [ownedContent setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        g_windows.emplace(id, window);
        g_contentViews.emplace(id, ownedContent);
        g_controlOrder.emplace(id, std::vector<int>{});
        g_controls.emplace(id, NativeControl{ window, ownedContent, nullptr, ControlKind::Window, 0, {}, {}, nil });
        NimpleEzWindowTarget* target = targetFor(id);
        [[NSNotificationCenter defaultCenter] addObserver:target selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:target selector:@selector(windowDidResize:)
            name:NSWindowDidResizeNotification object:window];
        return id;
    }

    static int addControl(int windowId, const std::string& text, ControlKind kind, int minimum = 0, int maximum = 100) {
        NSWindow* window = findWindow(windowId);
        NSView* content = g_contentViews.at(windowId);
        NSString* nsText = toNSString(text);
        NSView* view = nil;
        NSView* editable = nil;
        std::vector<std::string> items;
        switch (kind) {
        case ControlKind::Label: {
            NSTextField* label = [NSTextField labelWithString:(NSString*)nsText]; editable = label; view = label; break;
        }
        case ControlKind::Button: {
            NSButton* button = [NSButton buttonWithTitle:(NSString*)nsText target:nil action:nil];
            [button setButtonType:NSButtonTypeMomentaryPushIn]; editable = button; view = button; break;
        }
        case ControlKind::Entry: {
            NSTextField* entry = [NSTextField textFieldWithString:@""];
            [entry setPlaceholderString:(NSString*)nsText]; [entry setPreferredMaxLayoutWidth:500]; editable = entry; view = entry; break;
        }
        case ControlKind::PasswordEntry: {
            NSSecureTextField* entry = [NSSecureTextField textFieldWithString:@""];
            [entry setPlaceholderString:(NSString*)nsText]; [entry setPreferredMaxLayoutWidth:500]; editable = entry; view = entry; break;
        }
        case ControlKind::TextBox: {
            NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 400, 100)];
            [scroll setBorderType:NSBezelBorder]; [scroll setHasVerticalScroller:YES];
            NSTextView* textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 400, 100)];
            [textView setString:nsText]; [textView setMinSize:NSMakeSize(0, 100)]; [textView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
            [textView setVerticallyResizable:YES]; [textView setHorizontallyResizable:NO]; [textView setAutoresizingMask:NSViewWidthSizable];
            [scroll setDocumentView:textView]; [textView release]; editable = [scroll documentView]; view = scroll; break;
        }
        case ControlKind::CheckBox: {
            NSButton* check = [NSButton checkboxWithTitle:(NSString*)nsText target:nil action:nil]; editable = check; view = check; break;
        }
        case ControlKind::RadioButton: {
            NSButton* radio = [NSButton radioButtonWithTitle:(NSString*)nsText target:nil action:nil]; editable = radio; view = radio; break;
        }
        case ControlKind::ComboBox: {
            NSComboBox* combo = [[NSComboBox alloc] initWithFrame:NSMakeRect(0, 0, 400, 28)];
            [combo setEditable:NO]; items = splitItems(text);
            for (const auto& item : items) [combo addItemWithObjectValue:toNSString(item)];
            if (!items.empty()) [combo selectItemAtIndex:0];
            editable = combo; view = combo; break;
        }
        case ControlKind::Slider: {
            if (maximum <= minimum) throw std::runtime_error("EzWindow：控件最大值必须大于最小值。");
            NSSlider* slider = [NSSlider sliderWithValue:minimum minValue:minimum maxValue:maximum target:nil action:nil];
            editable = slider; view = slider; break;
        }
        case ControlKind::ProgressBar: {
            if (maximum <= minimum) throw std::runtime_error("EzWindow：控件最大值必须大于最小值。");
            NSProgressIndicator* progress = [NSProgressIndicator progressIndicatorWithStyle:NSProgressIndicatorStyleBar];
            [progress setIndeterminate:NO]; [progress setMinValue:minimum]; [progress setMaxValue:maximum]; [progress setDoubleValue:minimum];
            editable = progress; view = progress; break;
        }
        case ControlKind::WebView: throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。");
        case ControlKind::Window: break;
        }
        if (!view) throw std::runtime_error("EzWindow：创建原生控件失败。");
        const int id = g_nextId++;
        [view setTranslatesAutoresizingMaskIntoConstraints:YES];
        g_controls.emplace(id, NativeControl{ window, view, editable, kind, windowId, {}, {}, nil, {}, minimum, maximum, std::move(items) });
        g_controlOrder[windowId].push_back(id);
        [content addSubview:view];
        if (kind == ControlKind::TextBox || kind == ControlKind::ComboBox) [view release];
        layoutWindow(windowId);
        if (kind == ControlKind::Button || kind == ControlKind::CheckBox || kind == ControlKind::RadioButton || kind == ControlKind::Slider || kind == ControlKind::ComboBox) {
            NimpleEzWindowTarget* target = targetFor(id);
            if (kind == ControlKind::Button || kind == ControlKind::CheckBox || kind == ControlKind::RadioButton) {
                [(NSButton*)editable setTarget:target]; [(NSButton*)editable setAction:@selector(controlAction:)];
            } else if (kind == ControlKind::Slider) { [(NSSlider*)editable setTarget:target]; [(NSSlider*)editable setAction:@selector(controlAction:)]; }
            else { [(NSComboBox*)editable setTarget:target]; [(NSComboBox*)editable setAction:@selector(controlAction:)]; }
        }
        if (kind == ControlKind::Entry || kind == ControlKind::PasswordEntry || kind == ControlKind::TextBox) {
            NimpleEzWindowTarget* target = targetFor(id);
            [[NSNotificationCenter defaultCenter] addObserver:target selector:@selector(textDidChange:)
                name:(kind == ControlKind::TextBox ? NSTextDidChangeNotification : NSControlTextDidChangeNotification) object:editable];
        }
        (void)window;
        return id;
    }
    int addLabel(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Label); }
    int addButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::Button); }
    int addEntry(int windowId, const std::string& placeholder) { return addControl(windowId, placeholder, ControlKind::Entry); }
    int addPasswordEntry(int windowId, const std::string& placeholder) { return addControl(windowId, placeholder, ControlKind::PasswordEntry); }
    int addComboBox(int windowId, const std::string& items) { return addControl(windowId, items, ControlKind::ComboBox); }
    int addRadioButton(int windowId, const std::string& text) { return addControl(windowId, text, ControlKind::RadioButton); }
    int addSlider(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::Slider, minimum, maximum); }
    int addProgressBar(int windowId, int minimum, int maximum) { return addControl(windowId, {}, ControlKind::ProgressBar, minimum, maximum); }
    int addWebView(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用；macOS 目前没有 WebView2 后端。"); }
    void webViewNavigate(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。"); }
    void webViewSetHtml(int, const std::string&) { throw std::runtime_error("WebView2 仅在启用 WebView2 的 Windows 构建中可用。"); }
    void setText(int widgetId, const std::string& text) {
        NativeControl& control = findControl(widgetId); NSString* value = toNSString(text);
        switch (control.kind) {
        case ControlKind::Label: [(NSTextField*)control.editable setStringValue:value]; break;
        case ControlKind::Button: case ControlKind::CheckBox: case ControlKind::RadioButton: [(NSButton*)control.editable setTitle:value]; break;
        case ControlKind::Entry: case ControlKind::PasswordEntry: [(NSTextField*)control.editable setStringValue:value]; break;
        case ControlKind::TextBox: [(NSTextView*)control.editable setString:value]; break;
        case ControlKind::ComboBox: {
            auto found = std::find(control.items.begin(), control.items.end(), text);
            if (found == control.items.end()) throw std::runtime_error("setText：ComboBox 中不存在选项“" + text + "”。");
            [(NSComboBox*)control.editable selectItemAtIndex:static_cast<NSInteger>(found - control.items.begin())]; break;
        }
        case ControlKind::Slider: case ControlKind::ProgressBar: case ControlKind::WebView: throw std::runtime_error("setText 不适用于此控件类型。");
        case ControlKind::Window: throw std::runtime_error("setText 的目标必须是控件，不能是窗口。");
        }
    }
    std::string getText(int widgetId) { NativeControl& control = findControl(widgetId); if (control.kind == ControlKind::Slider || control.kind == ControlKind::ProgressBar || control.kind == ControlKind::WebView) throw std::runtime_error("getText 不适用于 slider、progressBar 或 WebView 控件。"); return readText(control); }
    void setChecked(int widgetId, bool checked) {
        NativeControl& control = findControl(widgetId);
        if (control.kind != ControlKind::CheckBox && control.kind != ControlKind::RadioButton) throw std::runtime_error("setChecked 只支持 checkBox 或 radioButton 控件。");
        if (checked && control.kind == ControlKind::RadioButton) for (auto& entry : g_controls) {
            if (entry.first != widgetId && entry.second.owner == control.owner && entry.second.kind == ControlKind::RadioButton)
                [(NSButton*)entry.second.editable setState:NSControlStateValueOff];
        }
        [(NSButton*)control.editable setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
    }
    bool getChecked(int widgetId) {
        NativeControl& control = findControl(widgetId);
        if (control.kind != ControlKind::CheckBox && control.kind != ControlKind::RadioButton) throw std::runtime_error("getChecked 只支持 checkBox 或 radioButton 控件。");
        return [(NSButton*)control.editable state] == NSControlStateValueOn;
    }
    void setValue(int widgetId, int value) {
        NativeControl& control = findControl(widgetId);
        if (control.kind != ControlKind::Slider && control.kind != ControlKind::ProgressBar) throw std::runtime_error("setValue 只支持 slider 或 progressBar 控件。");
        value = std::max(control.minimum, std::min(control.maximum, value));
        if (control.kind == ControlKind::Slider) [(NSSlider*)control.editable setIntegerValue:value];
        else [(NSProgressIndicator*)control.editable setDoubleValue:value];
    }
    int getValue(int widgetId) {
        NativeControl& control = findControl(widgetId);
        if (control.kind == ControlKind::Slider) return static_cast<int>([(NSSlider*)control.editable integerValue]);
        if (control.kind == ControlKind::ProgressBar) return static_cast<int>([(NSProgressIndicator*)control.editable doubleValue]);
        throw std::runtime_error("getValue 只支持 slider 或 progressBar 控件。");
    }
    void setTitle(int windowId, const std::string& title) { [findWindow(windowId) setTitle:toNSString(title)]; }
    void showWindow(int windowId) { [findWindow(windowId) makeKeyAndOrderFront:nil]; }
    void setPosition(int widgetId, int x, int y) {
        NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setPosition 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y; applyLayoutForControl(c);
    }
    void setSize(int widgetId, int width, int height) {
        validateBounds(width, height); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setSize 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.width = width; c.geometry.height = height; applyLayoutForControl(c);
    }
    void setBounds(int widgetId, int x, int y, int width, int height) {
        validateBounds(width, height); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Absolute; c.geometry.x = x; c.geometry.y = y;
        c.geometry.width = width; c.geometry.height = height; applyLayoutForControl(c);
    }
    void setRelativeBounds(int widgetId, int xPercent, int yPercent, int widthPercent, int heightPercent) {
        validateRelativeBounds(xPercent, yPercent, widthPercent, heightPercent); NativeControl& c = findControl(widgetId);
        if (c.kind == ControlKind::Window) throw std::runtime_error("setRelativeBounds 的目标必须是控件，不能是窗口。");
        c.geometry.mode = LayoutMode::Relative; c.geometry.xPercent = xPercent; c.geometry.yPercent = yPercent;
        c.geometry.widthPercent = widthPercent; c.geometry.heightPercent = heightPercent; applyLayoutForControl(c);
    }
    void setOnClick(int widgetId, ClickCallback callback) {
        NativeControl& control = findControl(widgetId);
        if (control.kind != ControlKind::Button) throw std::runtime_error("onClick 只支持 button 控件。");
        control.click = std::move(callback);
    }
    void setOnChange(int widgetId, ChangeCallback callback) {
        NativeControl& control = findControl(widgetId);
        if (control.kind != ControlKind::Entry && control.kind != ControlKind::PasswordEntry && control.kind != ControlKind::TextBox &&
            control.kind != ControlKind::CheckBox && control.kind != ControlKind::RadioButton && control.kind != ControlKind::ComboBox && control.kind != ControlKind::Slider)
            throw std::runtime_error("onChange 只支持 entry、passwordEntry、textBox、checkBox、radioButton、comboBox 或 slider 控件。");
        control.change = std::move(callback);
    }
    void mainLoop() {
        if (g_windows.empty()) throw std::runtime_error("尚未创建 EzWindow 窗口。");
        ensureStarted();
        for (const auto& pair : g_windows) {
            NSWindow* window = pair.second;
            if (window) [window makeKeyAndOrderFront:nil];
        }
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
        for (auto& pair : g_controls) {
            if (pair.second.target) {
                [[NSNotificationCenter defaultCenter] removeObserver:pair.second.target];
                [pair.second.target release];
                pair.second.target = nil;
            }
        }
        for (auto& pair : g_windows) {
            if (pair.second) {
                [pair.second close];
                [pair.second release];
                pair.second = nil;
            }
        }
        g_controls.clear(); g_windows.clear(); g_contentViews.clear(); g_controlOrder.clear(); g_nextId = 1;
        [g_pool drain];
        g_pool = nil;
    }
    void quit() {
        [NSApp stop:nil];
        NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
            modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
        [NSApp postEvent:wake atStart:YES];
    }
    void message(const std::string& title, const std::string& text) {
        ensureStarted();
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:toNSString(title)];
        [alert setInformativeText:toNSString(text)];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        [alert release];
    }
}
