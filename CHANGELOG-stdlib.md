# Standard library statement bundles

- Replaced the previous function-wrapper experiment with directive-only `input`, `output`, and `iosystem` bundles.
- `input` imports the native `sys::read` statement; `output` imports native `sys::print`, `sys::printf`, and `sys::printfn`; `iosystem` imports all four.
- The bundles do not define functions and do not change statement syntax or semantics.
- Replaced the `$`-prefixed preprocessor spelling with plain `const`, `statement`, `copy`, `replace`, and `suppress` directives.
- Standard-library source is recursively preprocessed so its `const` declarations take effect when introduced.
