# Nimple Basic Diagnostics

Nimple Basic routes compiler and interpreter messages through one diagnostic model.

## Diagnostic model

Every diagnostic carries:

- **Severity**: `error`, `warning`, or `note`.
- **Phase**: infrastructure, preprocessing, lexing, parsing, type checking, or runtime.
- **Code**: a stable machine-readable identifier.
- **Location**: one-based source line and column when available.
- **Message**: concise user-facing text, independent of recovery advice.

The phase is internal structured metadata; the default terminal format remains stable:

```text
path/to/file.nb:12:5: error NB2016: print 未定义。
```

## One reporting path

Lexer, parser, type checker, preprocessor, runtime exceptions, and warnings produce the same diagnostic representation. `NimpleError` remains the control-flow exception used to stop the current invalid operation, but it wraps a structured diagnostic rather than defining a separate output format.

`DiagnosticEngine` owns collection, deterministic ordering, duplicate suppression, severity formatting, output, and the diagnostic-count limit. Runtime warnings and asynchronous EzWindow callback failures use the same formatter as static diagnostics.

## Extending diagnostics

When adding a check:

1. Assign a stable code in the relevant code range.
2. Produce a structured diagnostic with the source location and concise message.
3. Do not embed speculative fixes or module-specific instructions in generic undefined-symbol diagnostics.
4. Let `DiagnosticEngine` render the message; do not write directly to `stderr`.
5. Add a regression test for the diagnostic code, location, severity, and message where applicable.

The current implementation keeps exception-based unwinding for fatal runtime operations. This is intentional: it gives runtime operations one consistent diagnostic contract without attempting unsafe recovery after a failed side effect.
