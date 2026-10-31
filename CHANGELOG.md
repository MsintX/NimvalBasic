# Changelog

## Nimple Basic 0.1.0 — follow-up update

- Renamed the module-import keyword from `introduce` to `statement`; `statement "module"` imports a module while `statement sys::name` declares a native statement.
- Standard-library `sys` statements now require the `sys::` prefix after importing `input`, `output`, or `iosystem`, unless explicitly elided with `const`.
- Empty and comment-only source files now exit cleanly without invoking the whole-program entry-point check; non-empty files still receive static analysis.
- Added concise EzWindow declarations for windows, controls, event bindings, showing windows, and starting the event loop.

## Nimple Basic 0.1.0

- Renamed the interpreter identity, CMake target, Visual Studio solution/project, examples and tests to Nimple Basic.
- Migrated source files to `.nb`, introduced `.nbf` header imports and `.nbt` tail metadata validation.
- Added `::` tokenization, module-qualified function calls, nested modules, `::global` and `global::global` lookup.
- Added `sys::` built-in input/output/pause calls and `const namespace sys` / individual `const sys::name` imports.
- Added `class` compatibility alias to `module`, warning `NB-W2090`, and `suppress NB-W2090`.
- Added `.nbf` `opening` detection with `NB-W2101` fallback warning.
- Added optional literal default parameters; EzWindow `window()` defaults to title `EzWindow Application` and size 200×200, with optional icon-path support.
- Added `void Window` as a no-argument, void-returning GUI entry point; `Main` remains the command-line entry point fallback.
- Added regression tests; 47/47 CTest tests pass in the current Linux build.

## Known limitations

- `module` is a namespace-oriented function grouping construct, not an object/class system. Inheritance, instances, interfaces/traits, nested classes/enums, class static fields, and allocation operators are not implemented.
- The `.nbt` file is validated but its values are not yet applied to runtime/window settings. `.nbt` icon metadata is not automatically applied; use the optional fourth `window()` argument to set an icon.
- The current EzWindow surface still uses explicit widget IDs and parent window IDs; it is not yet a full Tkinter-style declarative/layout DSL.
- GUI rendering was not manually smoke-tested in this Linux build container.


## Unreleased — native statement definitions

- Replaced the I/O standard-library `const` bundles with native `statement sys::...` definition declarations.
- Kept `const` as a separate source-level prefix-elision mechanism.
- `input`, `output`, and `iosystem` now register only their respective native I/O statements.
- Added tests for definition/import separation, module scoping, and invalid native statement definitions.

## Unreleased — require output statement definitions

- Qualified `sys::print`, `sys::printf`, and `sys::printfn` are now rejected unless the current source imports a module that defines the corresponding native statement (for example, `statement "output"` or `statement "iosystem"`).
- `const namespace sys` and individual `const sys::...` imports no longer make an undefined native output statement available; `const` only controls prefix elision.
- Added a regression test proving `sys::print` fails without an output statement definition, and updated positive fixtures to import the appropriate standard-library module.

## Static-checker symbol resolution update

- Native statement availability is resolved through the registered statement-name set instead of module-specific error branches.
- Undefined native statements now report the symbol itself (for example, `print 未定义。`) without prescribing an import or implementation path.
- System-prefix diagnostics use concise undefined-symbol wording rather than embedding `const` or module-import instructions.
- Added an exact-output regression test for an undefined `print` statement.

## Diagnostic framework refactor

- Added a shared structured diagnostic model with severity, phase, stable code, source location, and message.
- Centralized ordering, duplicate suppression, output formatting, and the diagnostic limit in `DiagnosticEngine`.
- Routed lexer deprecation warnings, runtime conversion/division/loop warnings, preprocessor warnings, and EzWindow callback failures through the same reporting API.
- Preserved stable diagnostic codes and the existing terminal format for compatibility.
- Documented the diagnostic extension contract in `docs/Diagnostics.md`.


## EzWindow declarative syntax (work-in-progress baseline)

- Added `const <EzWindow>` as the unified EzWindow import form.
- Added `//` single-line comments; existing `/* ... */` comments remain supported.
- Added the declarative `void Window : ... end` entry syntax with default title, 400x300 geometry, optional icon, custom instance names, and automatic show/main-loop startup.
- Added `module Controls` declarations for button controls and the `loction(distance, top, bottom, left, right)` edge layout form. The first argument selects `%` or `px`; `size(px, length, width)` sets explicit pixel dimensions. PasswordEntry now uses `*` as its mask character.
- Added EzWindow-owned static diagnostic when the required window instance declaration is missing.
- Preserved the previous compact EzWindow syntax and its regression tests.


## EzWindow declarative controls and style resource validation

- Expanded `module Controls` declarations to instantiate the matching native control kind instead of silently creating a button for every declaration.
- Added name-prefix recognition for multiple controls such as `Button1`, `LabelStatus`, and `EntryName`.
- Preserved control-body statements after instance creation, allowing existing text/value/event APIs to configure declared controls.
- Enforced the `.css` suffix for declarative `Window.Style` resource paths with an EzWindow-specific static diagnostic.
- Added regression coverage for all supported native control declarations, suffixed control names, and valid/invalid `.css` extensions.
- Documented that `.ewsty` content syntax and cross-platform visual style application remain undefined rather than pretending a CSS or theme format is supported.


### EzWindow CSS and explicit control instances

- Switched the declarative `Window.Style` resource extension from `.ewsty` to `.css`.
- Changed `module Controls` so each control must explicitly instantiate itself with `var new (auto)Name = EzWindow.ControlType` as the first statement; declarations without an instance no longer synthesize controls.
- Updated the declarative example to use CSS paths and explicit widget instances.
- Extended `const` imports: angle brackets resolve standard-library modules; quoted paths resolve relative to the importing source file.
- CSS parsing and native visual application remain unimplemented and are explicitly documented as such.

### EzWindow control emission order fix
- Fixed a declaration-order bug where `void Window` was emitted before a following `module Controls` had been parsed, causing explicitly instantiated controls to be omitted at runtime.
- EzWindow now collects the complete declarative control module before generating the runtime window body; controls declared after the window block are included.
- Added a regression test that places an intentionally undefined call inside a control declared after `void Window`, proving that the control body reaches the normal checker/runtime generation path.
