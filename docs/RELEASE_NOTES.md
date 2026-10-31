# v1.0.0

Initial release of Reliable Expense Tracker CLI.

- Eight-option menu for add, list, category filtering/totals, overall total, delete, save, and exit.
- Exact integer-paise calculations with individual and aggregate overflow checks.
- Real calendar dates, bounded fields/input, invalid-input recovery, and EOF handling.
- Local CSV persistence with quote escaping, full-file validation, staged saves, and backup recovery.
- Standard C++17 core/application, CMake warning flags, CTest suites, MIT license, and documented Windows/Linux/macOS build commands.
- Actual synthetic execution transcript and privacy-redacted test results included.

Windows GCC 16.1.0 Debug and Release builds passed 10,128 core assertions and
10 black-box CLI test methods. See `docs/VALIDATION_REPORT.md` for actual local
coverage and limitations. Screenshot capture was unavailable because the native
desktop capture helper could not connect. No screenshot images are claimed.

This is a source release. Build instructions are in `README.md`. The application
requires no network access or API keys. Use one process per CSV; recovery and
power-loss limitations are documented.
