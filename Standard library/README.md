# Nimple Basic standard library

The standard library keeps native I/O statements in three independently introducible modules:

- `input` defines `sys::read`.
- `output` defines `sys::print`, `sys::printf`, and `sys::printfn`.
- `iosystem` defines all four statements above.

Use `statement "input"`, `statement "output"`, or `statement "iosystem"` before source statements.

## Statement definitions vs. `const`

A line such as `statement sys::read` is a native-statement definition declaration. It registers the corresponding built-in parser/runtime implementation for use by the source that imports the module with `statement "module"`. These are not functions and do not generate function-call wrappers. Their runtime behavior remains the native implementation in the interpreter, preserving `read(type) <- System` and `print`/`printf`/`printfn ... -> System` syntax.

`const` remains a separate source-file directive. It controls whether an explicitly imported native statement may omit its `sys::` prefix; it does not define or implement a statement. Without `const`, use the qualified `sys::` form.
