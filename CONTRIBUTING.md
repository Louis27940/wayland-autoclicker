# Contribution Guidelines

Thank you for your interest in contributing to Wayland Autoclicker. This document outlines the development standards, code style guidelines, and Git workflow required to contribute effectively and maintain high code quality.

---

## 1. General Principles

- **Reliability and Safety First**: This application interacts directly with the Wayland display server and emulates physical user input. Any modification must guarantee that no mouse button can become stuck down and no thread can enter an unrecoverable blocking state.
- **Surgical Approach (KISS and YAGNI)**: Modify only what is strictly required for the feature or bug fix. Avoid premature abstractions, unnecessary utility classes, or unused boilerplate.
- **Evidence-Based Verification**: Every contribution (feature or fix) must be accompanied by unit tests or reproducible evidence confirming functionality without side effects.

---

## 2. Development Standards

### 2.1 Language and Libraries
- **Standard**: Modern C++20 (`std::jthread`, `std::stop_token`, `std::condition_variable_any`, lambda expressions, `std::optional`).
- **Framework**: Qt6 (`QtCore`, `QtWidgets`, `QtDBus`).
- **Non-blocking Execution**: Never use `std::this_thread::sleep_for`, `QThread::msleep`, or busy loops in processing threads. All pauses and waits must be interruptible via `std::stop_token`.
- **RAII and Memory Safety**: Automatic resource management is strictly enforced. Scope guards (`QScopeGuard`) must be used to ensure cleanup and button releases under all exit conditions.

### 2.2 Compiler Warnings
- The project compiles with `-Wall -Wextra -Wformat -Wformat-security -fstack-protector-strong`.
- **Zero Warnings**: Code contributions must compile with zero warnings under GCC and Clang.

### 2.3 Strict Emoji Policy
To maintain professional clarity and repository sobriety, it is **strictly prohibited to include emojis** in:
- Source code (user interface text, strings, comments, variable names).
- Git commit messages.
- Pull Request titles and descriptions.
- Documentation (markdown files, specifications, guidelines).

---

## 3. Commit Message Conventions

The repository strictly enforces the **Conventional Commits** specification. Each commit message must adhere to the following structure:

```text
<type>(<optional_scope>): <imperative description without trailing period>
```

### Allowed Types
- `feat`: Introduces a new feature or capability.
- `fix`: Resolves a bug or defect.
- `docs`: Documentation-only updates.
- `test`: Adds or fixes unit tests.
- `refactor`: Code restructuring without altering existing behavior.
- `perf`: Performance or memory footprint optimizations.
- `build`: CMake build system or dependency modifications.
- `ci`: Continuous integration adjustments.
- `chore`: Maintenance tasks without runtime or binary impact.

### Git Best Practices
- Write descriptions in lowercase, imperative mood (e.g., `feat: add support for extra mouse buttons`).
- Do not commit micro-adjustments or fine-tuning iterations individually; squash and commit once the behavior is stabilized.
- Never commit build artifacts, editor caches, or generated files.

---

## 4. Git Workflow and Branch Protection

### 4.1 Protected `main` Branch
- The `main` branch represents stable, production-ready, verified code.
- **The `main` branch is strictly protected**:
  - Direct pushes to `main` are disallowed.
  - All modifications must be submitted via Pull Requests (PR).
  - The CTest test suite must pass with a 100% success rate before merging.

### 4.2 Contribution Lifecycle
1. Fork or clone the repository locally:
   ```bash
   git clone https://github.com/Louis27940/wayland-autoclicker.git
   cd wayland-autoclicker
   ```
2. Create a dedicated topic branch from `main`:
   ```bash
   git checkout -b feat/your-feature-name
   # or
   git checkout -b fix/your-fix-name
   ```
3. Implement changes following the coding and commit standards.
4. Execute the complete test suite and confirm 100% pass rate.
5. Push your topic branch to your fork and open a Pull Request targeting `main`.

---

## 5. Local Build and Test Verification

Before submitting a Pull Request, confirm that the project builds cleanly without warnings and that all automated tests pass.

### 5.1 Build with Security Hardening Flags
```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

### 5.2 Execute CTest Suite
```bash
ctest --test-dir build --output-on-failure
```

Any new behavior or bug fix must include corresponding unit tests in the `tests/` directory (`tst_TimingPolicy`, `tst_ClickController`, `tst_DBusSignatures`, or `tst_DebugInfo`).

---

## 6. Issue Reporting and Feedback

### 6.1 Bug Reports
When reporting a bug, open a GitHub Issue and provide:
- A clear, concise summary of the observed vs. expected behavior.
- Step-by-step instructions to reproduce the issue.
- Technical environment details: Linux distribution, Wayland compositor / KDE Plasma version, and Qt version.
- Relevant terminal or debug dialog logs.

### 6.2 Security Vulnerabilities
If you identify a security defect (such as privilege escalation, token leakage, or permission regression), please do not disclose it in a public issue. Use GitHub's **Private Vulnerability Reporting** feature or contact the repository maintainer directly.
