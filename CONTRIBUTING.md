# Contributing

Vyra is licensed under GPL-3.0-only. By contributing, you agree that your
contributions may be distributed under that license.

## Development

1. Install CMake 3.22+, a C++20 compiler, Git, and your platform's desktop/audio
   development dependencies.
2. Configure and build using the commands in [README.md](README.md).
3. Run `ctest --test-dir build --output-on-failure` (add `-C Release` on
   multi-configuration generators).
4. Keep file I/O, allocation, locks, UI, and database work out of audio
   callbacks. Add a focused test for behavior changes.

For audio bugs, include the OS, audio backend/device, sample rate, buffer size,
file format, and steps to reproduce. Do not attach copyrighted music or private
library data.
