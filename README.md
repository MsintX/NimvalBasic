# Nimval Basic 0.1.0

Nimval Basic 是一个基于 C++14 兼容层的原生解释器。执行程序前会完成预处理、词法分析、语法分析和整程序静态类型检查；发现错误时不会先执行程序副作用。

## 新版统一语法

新代码统一采用 1TBS 大括号风格。左大括号与声明同行，右大括号独占一行；方法、模块、条件和循环都使用 `{}`。

```basic
module MathOps {
    int Add(int left, int right) {
        return left + right
    }
}

int Main(void) {
    var new result = MathOps::Add(20, 22)
    if result == 42 then return result - 42
    return 1
}
```

单行代码块可以省略大括号，但必须使用 `then`。分号可选，缩进仅用于可读性，不参与语法解析。

方法参数使用括号和逗号，参数格式为“类型 名称”。参数列表三种形式的含义不同：不写括号表示无参数；`(void)` 明确表示无参数；`()` 表示参数列表未指定、参数信息未知，不等同于无参数声明。EasyWindow 的无参数函数声明统一使用 `(void)`。

完整规则见[新版语法说明](docs/NimvalBasic-Syntax.md)。

## 导入与首文件

导入关键词由 `const` 改为 `include`：

```basic
include <EasyWindow>
include "local/Library.nb"
include namespace sys
include sys::print
```

`statement` 是另一项专用指令：标准库文件用 `statement sys::name` 注册原生语句实现；源文件用 `statement "path/to/file.nbf"` 引入首文件。首文件的可选约定入口由 `opening` 更名为 `including`，例如 `void including(void) { ... }`。没有 `including` 时仍会按兼容规则回退为 `copy` 并给出警告。

目前仍兼容旧的 `const` 导入、冒号代码块和 `end` 结束符，便于迁移已有代码；新项目请使用 `include`、大括号和括号参数列表。

## EasyWindow

GUI 标准库统一命名为 EasyWindow，通过 `include <EasyWindow>` 导入。声明式入口使用 `void Window(void) { ... }`，控件定义位于 `module Controls { ... }` 中。

```basic
include <EasyWindow>

void Window(void) {
    var new (auto)App = EasyWindow.Window
    App.Title = "Hello, EasyWindow"
    App.Geometry("520x320")
}

module Controls {
    void Button(void) {
        var new (auto)RunButton = EasyWindow.Button
    }
}
```

普通 API 也可以直接使用，例如 `window("App", 640, 480)`、`button(root, "Run")`、`onClick(buttonId, "callback")` 和 `mainloop()`。Windows 使用 Win32，macOS 使用 Cocoa/AppKit，Linux 通过运行时加载 GTK 3。WebView2 为可选构建功能。

## 系统输入输出

内置输入输出仍属于 `sys` 命名空间。`statement "input"` 定义 `sys::read`，`statement "output"` 定义 `sys::print`、`sys::printf`、`sys::printfn`，`statement "iosystem"` 一次定义全部输入输出语句。使用 `include sys::read` 等形式导入需要省略前缀的语句；不导入时使用完整的 `sys::` 限定名。

```basic
statement "iosystem"
include sys::print

int Main(void) {
    print "Hello" -> System
    return 0
}
```

`System` 是输入输出目标，不属于 `sys` 命名空间。

## 文件类型

- `.nb`：普通源文件。
- `.nbf`：首文件（头文件），通过 `statement "path/to/file.nbf"` 引入；首文件也可以单独执行 `--check`，不要求定义程序入口。
- `.nbt`：尾文件（项目配置/提示），与 `.nb` 同名时会在检查或运行时自动校验。

## 构建与运行

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
./bin/nb path/to/Main.nb
./bin/nb --check path/to/Main.nb
```

Windows 也可以用 Visual Studio 2015 Update 3 打开 `NimpleBasic.sln`（v140，Windows 8.1 SDK），或在 VS2015 Developer Command Prompt 中运行 `build-vs2015.bat`。解释器会从自身可执行文件旁边的 `Standard library` 目录加载标准库，发布时需要将整个目录与程序一起分发。

当前测试覆盖语法解析、类型检查、系统导入、首文件和 EasyWindow 声明式语法。GUI 窗口仍需在目标桌面系统进行人工冒烟测试。
