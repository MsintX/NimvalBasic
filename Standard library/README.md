# Nimval Basic 标准库

标准库通过 `statement` 声明解释器内置原生语句实现。`input` 定义 `sys::read`，`output` 定义 `sys::print`、`sys::printf` 和 `sys::printfn`，`iosystem` 定义以上全部语句。

`include` 是导入关键词，取代旧的 `const` 导入写法，例如 `include <EasyWindow>`、`include "iosystem"` 和 `include namespace sys`。`statement` 仍用于声明原生语句实现或引入首文件。

EasyWindow 的无参数函数声明统一使用 `(void)`，例如 `void mainloop(void) { ... }`。
