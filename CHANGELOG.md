# v0.4.0 - 2025.06.22

- Start nkb2 and core2, add nklc for testing
- Add LLVM backend
- Implement JIT
- Reimplement some parts of ntk to simplify/optimize stuff
- Drop support for cross toolchain docker images for now
- Update CI to use native builds
- Update Readme with new build instructions

# v0.3.1 - 2025.04.18

- Some fixes in buildenv for Darwin
- Update Readme with more info
- Change package naming

# v0.3.0 - 2025.04.17

- Upgrade toolchain and build scripts
- Add native build support
- Add Darwin build support
- Update Readme
- Add nkstc -- a WIP AST compiler
- Integrate with Spall profiler
- A lot of refactoring
- Gather some common compiler logic in core

# v0.2.5 - 2023.10.22

- Improve CLI interface and diagnostics
- Improve error handling
- Implement IR compilation via the C backend
- Implement the rest of IR instructions
- Implement the ability to explicitly link libraries in IR programs
- Implement the way of controlling IR symbol visibility
- Implement running Windows tests under wine
- Improve robustness of compiler internals

# v0.2.4 - 2023.09.20

- Rename std -> core
- Add hex int literals
- Add multiline comments
- Add nkirc -- a WIP IR compiler
- Improve stability / memory allocation patterns

# v0.2.3 - 2023.07.08

- Add null keyword
- Refactor pthread stuff
- Add 64bit log2
- Fix defects

# v0.2.2 - 2023.04.15

- Add CI

# v0.2.1 - 2023.04.15

- Reorganize binary delivery packages
- Add license to the packages

# v0.2.0 - 2023.04.11

- Basic implementation

# v0.1.0 - 2023.03.11

- Initial prototype
