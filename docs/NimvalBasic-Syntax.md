# Nimval Basic 统一语法约定

本文记录已确认的新语法规则。实现目前保留部分旧写法兼容；新代码应优先使用本文形式。

## 代码块

所有多行代码块统一使用大括号并采用 1TBS：左大括号与声明同行，右大括号独占一行。适用于方法、模块、条件和循环。

```basic
int Main(void) {
    if true {
        return 0
    }
}

module MathOps {
    int Add(int left, int right) {
        return left + right
    }
}
```

单行代码块允许省略大括号，但必须使用 `then`：

```basic
if age >= 12 then print("Hello")
```

分号仍然可选；缩进只用于可读性，不参与语法解析。

## 方法声明与参数列表

方法参数使用括号，参数格式为“类型 名称”，参数之间使用逗号：

```basic
int Add(int left, int right) {
    return left + right
}
```

参数列表有三种不同含义：

- 不写括号：明确无参数。
- `(void)`：明确无参数。
- `()`：参数列表未指定，参数信息未知；不等同于无参数声明。

EasyWindow 的无参数函数统一要求写成 `(void)`。

## 导入

导入关键词由 `const` 改为 `include`：

```basic
include <EasyWindow>
include "local/Library.nb"
include namespace sys
include sys::print
```

## EasyWindow

GUI 标准库名称统一为 **EasyWindow**。声明式入口采用 `void Window(void) { ... }`，控件定义位于 `module Controls { ... }` 中。EasyWindow 的一般无参数 API 声明使用 `(void)`。

```basic
include <EasyWindow>

void Window(void) {
    var new (auto)App = EasyWindow.Window
    App.Title = "Hello"
}

module Controls {
    void Button(void) {
        var new (auto)RunButton = EasyWindow.Button
    }
}
```

## 首文件入口

首文件（`.nbf`）约定的入口方法由 `opening` 更名为 `including`：

```basic
void including(void) {
    return
}
```
