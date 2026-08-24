[![progress-banner](https://backend.codecrafters.io/progress/shell/b4a05d7f-7e20-4b6e-8d47-669826065218)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

# Build Your Own Shell (C++)

A POSIX-compliant shell implementation written in C++17 for the CodeCrafters "Build Your Own Shell" project.

## Features Implemented
- **Builtin Commands**: `echo`, `exit`, `type`, `pwd`, `cd`, `history` (with `-c`, `-r`, `-w`, `-a`), `declare` (variable assignment & `-p`).
- **Executable Search**: Path resolution and execution of external binaries found in `$PATH`.
- **Quote & Escape Parsing**: Single quotes (`'...'`), double quotes (`"..."`), backslash escaping (`\`), and parameter expansion (`$VAR` and `${VAR}`).
- **I/O Redirection**: Redirecting stdout/stderr using `>`, `1>`, `2>`, `>>`, `1>>`, and `2>>`.
- **Pipelines**: Multistage command chaining via `|`.
- **Interactive REPL**: Autocomplete for builtins and binaries using GNU Readline, history management, and `HISTFILE` persistence across sessions.
