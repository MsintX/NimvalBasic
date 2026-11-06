# Nimval Basic 0.1.0

Nimval Basic 是一个基于 C++14 兼容层的原生解释器。执行程序前会完成预处理、词法分析、语法分析和整程序静态类型检查；发现错误时不会先执行程序副作用。

## 新版统一语法

新版语法采用 1TBS 大括号代码块、括号参数列表和 `include` 导入关键词。EasyWindow 无参数 API 使用 `(void)`，首文件入口使用 `including`。详见[新版语法说明](docs/NimvalBasic-Syntax.md)。旧语法目前保留兼容，新的代码请优先使用新版语法。

## 文件类型

- `.nb`：普通源文件。
- `.nbf`：首文件（头文件）。可以由 `statement "path/to/file.nbf"` 引入；首文件也可以单独执行 `--check`，不要求定义程序入口。
- `.nbt`：尾文件（项目配置/提示）。与 `.nb` 同名的 `.nbt` 会在检查或运行时自动校验。当前版本只校验 key/value 结构，不会自动把配置应用到 EasyWindow。

## 构建

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Windows 也可以用 Visual Studio 2015 Update 3 打开 `NimpleBasic.sln`（v140，Windows 8.1 SDK），或在 VS2015 Developer Command Prompt 中运行 `build-vs2015.bat`。核心解释器使用项目内的 `Compat14.h`，不依赖 `<filesystem>`、`<optional>` 或 `<variant>`。解释器会从自身可执行文件旁边的 `Standard library` 目录加载标准库，因此发布时需要把整个目录与 `nb.exe` 一起分发。

VS2015 构建建议使用 Release|x64。项目默认不启用 WebView2；普通 Win32 EasyWindow 控件不需要额外依赖。

## 运行与静态检查

```sh
./bin/nb path/to/Main.nb
./bin/nb --check path/to/Main.nb
./bin/nb --version
./bin/nb                    # enter interactive mode
```

不带参数运行 `nb` 时，会显示版本和版权信息，然后进入 `>>>` 交互模式。普通 Enter 立即解释当前行；Windows 控制台按 Shift+Enter 进入多行模式，继续输入后按 Ctrl+Enter 提交整个代码块。变量和方法会在当前交互会话中保留。输入 `:quit` 或 `:exit` 退出。对于无法传递 Shift/Ctrl+Enter 组合键的非 Windows 终端，可使用 `:multi` 开始多行输入、`:end` 提交。

## 作用域与 module

`::` 当前支持：

```nb
module MathOps:
    int twice with value as int:
        return value * 2
    end
end

int globalValue:
    return 12
end

int Main:
    var new result = MathOps::twice(::globalValue())
    return result - 24
end
```

`module` 是静态命名空间/函数组织单元，可以嵌套；模块内方法通过 `ModuleName::method()` 调用，也可以在模块内部直接调用同一模块的方法。`::name` 和 `global::name` 用于显式访问全局方法。

**重要设计边界：**这里的 `module` 不是可实例化的 `class` 替代品，而是有意采用的模块化、组合优先模型。它没有对象继承、基类、隐式 `this/self`、对象构造/析构、类静态字段或虚分派；因此菱形继承、脆弱基类、继承层次错配等问题不会进入这套模型。当前版本尚未实现通用对象/接口/trait 系统，也不支持 `new/delete`、嵌套类或枚举类型。不能把这些未实现能力视为已由 `module` 自动提供。

旧写法 `class` 暂时作为 `module` 的兼容别名，并产生静态检查警告：

```nb
suppress NB-W2090
class Legacy:
    void run:
        return
    end
end
```

## 系统命名空间与输入输出标准库

内置输入输出操作始终属于 `sys` 命名空间，并保持原生语句/操作语法，不会包装成普通函数：

```nb
int Main:
    sys::print "Hello" -> System
    var new age = sys::read(int) as int
    return age
end
```

标准库提供三个真正注册原生语句定义的模块：

- `statement "input"`：定义 `sys::read`。
- `statement "output"`：定义 `sys::print`、`sys::printf`、`sys::printfn`。
- `statement "iosystem"`：一次定义以上全部输入输出语句。

标准库文件中的 `statement sys::name` 是原生语句定义声明，绑定解释器内置的解析和运行时实现；它不是 `const` 导入，也不是普通函数。`const` 是源文件侧独立机制，只决定已经定义/可用的语句是否允许省略 `sys::` 前缀。例如：

```nb
statement "iosystem"
include sys::read
include sys::print
int Main:
    print "Hello" -> System
    var new age = read(int) as int
    return age
end
```

不使用 `const` 时，继续写 `sys::print`、`sys::read` 等完整限定名。`System` 是输出/输入目标，不属于 `sys` 命名空间。

## 首文件与预处理

```nb
statement "headers/Utilities.nbf"
```

支持的预处理指令为 `replace`、`copy`、`suppress`、`const` 和 `statement`，不再使用 `$` 前缀。`statement "模块名"` 引进模块；标准库文件内部的 `statement sys::name` 则声明原生语句定义。旧的 `introduce` 关键词已移除。打开 `.nbf` 时，解释器会检查其中是否存在任意返回类型的 `including` 方法声明。如果存在，按首文件导入处理；如果不存在，会发出 `NB-W2101` 警告，并按隐式 `copy` 展开。`copy` 与 `statement` 仅应对可信源码使用。

## 尾文件

与 `App.nb` 同目录的 `App.nbt` 可写为：

```ini
title = "Example application"
width = 640
height = 480
icon = "assets/app.ico"
prompt = "Project notes"
```

当前版本校验字符串、布尔值、数字、重复键，以及 `width`/`height` 的正整数约束。尾文件暂不参与运行时窗口配置，图标字段目前只是可保存的项目元数据。

## EasyWindow

EasyWindow 使用现有原生控件后端。应用入口可以使用 `void Window:`，不再要求另写 `Main`；`Window` 必须无参数且返回 `void`。普通命令行程序仍使用 `int Main` 或 `void Main`。

窗口函数支持默认参数：标题默认为 `EasyWindow Application`，尺寸默认为 200×200；也可以显式传入标题和尺寸：

```nb
statement "EasyWindow"

window "Hello, Nimple" 520 320 as root
label root "Hello, Nimval Basic" as greeting
entry root "Your name" as name
button root "Say hello" as submit

void Window:
    pack(greeting)
    pack(name)
    pack(submit)
    show root
    mainloop()
    return
end
```

EasyWindow 还提供声明式语法糖：`window "标题" [宽 高] as 名称` 创建窗口；`label/button/entry/textBox/checkBox 窗口 "文本或提示" as 名称` 创建控件；`on 控件 click|change 方法` 绑定事件；`show 窗口` 显示窗口，`run` 进入事件循环。控件仍可使用原有完整 API 进行属性、布局和高级操作。

或 `window("Custom title", 640, 480)`。当前仍采用显式控件 ID/父窗口参数，控件 API 与 Python Tkinter 的基本控件概念相近，但尚未实现完整 Tkinter 式布局 DSL。窗口函数的第 4 个可选参数可以指定图标路径，例如 `window("App", 640, 480, "assets/app.ico")`；相对图标路径以 `.nb` 源文件所在目录为基准。Windows 使用 `.ico`，Linux 使用 GTK 支持的图像格式，macOS 将图像设为应用图标。`.nbt` 的 `icon` 字段目前只做元数据保存，不会自动覆盖代码中显式传入的图标。

平台后端：Windows 使用 Win32，macOS 使用 Cocoa/AppKit，Linux 通过运行时加载 GTK 3。Linux 运行 GUI 程序仍需要 GTK 3 运行库和可用桌面会话。

### WebView2（可选）

WebView2 需要单独安装 Microsoft WebView2 SDK，并在构建时设置 `WebView2SDKDir`（VS 项目）或 `WEBVIEW2_SDK_DIR`（CMake）并启用 `NIMPLE_ENABLE_WEBVIEW2`。为兼容 Windows 7 SP1、Windows 8 和 Windows 8.1，必须使用 **WebView2 SDK 1.0.1518.46 或更早的兼容版本**；1.0.1519.0 及更高版本不再支持这些旧系统。

Windows 10/11 使用系统安装的 Evergreen Runtime。Windows 7/8/8.1 使用最后支持的 Runtime 109；程序优先查找可选的应用目录 `WebView2Runtime109`，也可通过环境变量 `NIMPLE_WEBVIEW2_FIXED_RUNTIME` 指定 Fixed Version Runtime 根目录。若该目录不存在，则使用系统已安装的 Runtime 109。Runtime 109 已停止安全更新，不应将其用于访问不可信网页。发布时如选择捆绑 Fixed Version Runtime，需将其解压到 `nb.exe` 同级的 `WebView2Runtime109` 目录；该运行时体积较大，故未默认打包。

## 测试

当前 CTest 套件包含 71 项测试，覆盖原有类型检查和 EasyWindow API，以及空文件/纯注释文件、EasyWindow 声明式语法、`module` 限定调用、全局作用域、`sys::` 导入、原生语句定义与 `const` 分离、标准库模块作用域隔离、`.nbf` including/回退、`class` 弃用警告抑制、`Window` 入口、窗口默认参数和 `.nbt` 校验。

GUI 窗口本身尚未在本次 Linux 构建环境中进行人工桌面冒烟测试；Windows/macOS 也需要在目标系统实际运行确认。

## 标准库

`Standard library/input`、`Standard library/output` 和 `Standard library/iosystem` 使用 `statement sys::...` 声明注册原生语句定义。实际解析与执行由解释器内置实现提供；标准库不创建包装函数。`const` 不参与定义，只控制是否省略 `sys::` 前缀。


## EasyWindow declarative syntax

The declarative EasyWindow surface is enabled with `include <EasyWindow>`. It supports a required `void Window` entry block, optional title/icon/geometry/style settings, and a `module Controls` block. The required `var new (auto)Name = EasyWindow.Window` declaration creates the window instance. The default title is `EasyWindow Application`; the default geometry is `400x300`; omitted style follows the operating-system theme. Style resource paths use the `.css` extension. Each control declaration must explicitly instantiate its widget as the first statement, for example `var new (auto)RunButton = EasyWindow.Button`; declarations without this line do not create a widget. EasyWindow creates and places instantiated controls automatically; `.pack()` is not required. `include <Module>` imports a standard-library module (including a first file in the standard-library directory), while `const "path"` imports a file by path relative to the current source file. CSS file parsing and native visual-property application are not yet implemented; the `.css` suffix is validated, but setting `Window.Style` does not yet apply CSS rules.

Each `void ControlName:` block declares one native control. Recognized names include `Button`, `Label`, `Entry`, `TextBox`, `CheckBox`, `PasswordEntry`, `ComboBox`, `RadioButton`, `Slider`, `ProgressBar`, and `WebView`; names may carry a suffix such as `Button1` or `LabelStatus`. Control logic is emitted after its instance has been created, so it can use the ordinary EasyWindow API and bind callbacks. `loction(distance, top, bottom, left, right)` sets the four edge offsets; `distance` is `%` or `px`, and all four offsets use that unit. `%` is relative to the initial window dimensions; `px` is an absolute pixel distance. `size(px, length, width)` optionally sets the explicit pixel dimensions, where `length` is horizontal width and `width` is vertical height. When `size` is omitted, the remaining area between the opposing edges determines the control size. `PasswordEntry` masks input with `*`.

```nimple
include <EasyWindow>

void Window :
    var new (auto)Window = EasyWindow.Window
    Window.Title = "Title"
    Window.Geometry("300x300")
end

module Controls :
    void Button :
        loction(%, 10, 20, 30, 40)
        setText(Button, "Run")
    end
    void LabelStatus :
        loction(px, 10, 20, 30, 40)
        size(px, 160, 32)
        setText(LabelStatus, "Ready")
    end
end
```
