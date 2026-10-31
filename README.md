# Reliable Expense Tracker CLI

A dependable, menu-driven expense tracker written in **standard C++17**.
Record expenses in INR, inspect category spending, and keep data locally in a
validated CSV file. All calculations use **integer paise**, so `0.10 + 0.20`
is exactly `0.30`.

## Features

- Add expenses with an ID, real calendar date, category, description, and amount.
- Browse a readable table or filter by category; calculate category and overall totals.
- Delete an expense by ID and save explicitly from the menu.
- Reload saved records on startup; automatically save completed changes on exit or EOF.
- Recover from invalid menu choices, amounts, dates, fields, oversized input, and missing IDs.
- Reject corrupt files without overwriting them; stage saves and retain the previous file during replacement.
- Run without networking, API keys, paid services, or third-party C++ libraries.

## Technologies

C++17 standard library, CMake 3.16+, CTest, and a C++17 compiler (GCC, Clang, or
MSVC). Python 3.8+ is used only for black-box tests and evidence recording.
The application itself has no Python dependency.

## Build and run

### Windows: Visual Studio 2022 / MSVC

Install the C++ desktop workload, CMake, and Python 3, then open a Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\expense_tracker.exe
```

### Windows: MinGW-w64 and Ninja

Put GCC, CMake, Ninja, and Python on `PATH`:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
.\build\expense_tracker.exe
```

### Linux / macOS

Use a modern GCC or Clang compiler with CMake and Python 3 installed
(on macOS, install the Xcode command-line tools for Clang):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/expense_tracker
```

For an application-only build, configure with `-DBUILD_TESTING=OFF`; Python is
then unnecessary. Compilers must implement C++17 `std::filesystem` (GCC 9+ recommended).

Pass a custom CSV path as the only argument, or use `--help`:

```powershell
.\build\expense_tracker.exe "C:\existing-folder\my-expenses.csv"
```

```sh
./build/expense_tracker ./my-expenses.csv
```

The default is `expenses.csv` in the current working directory. Parent
directories must already exist. Keep financial data outside the source repository;
CSV files, backup files, staging directories, builds, and common secrets are ignored by Git.

## Usage

```text
Reliable Expense Tracker CLI
1. Add Expense
2. View All Expenses
3. View Expenses by Category
4. View Category Total
5. View Overall Total
6. Delete Expense
7. Save Expenses
8. Exit
```

Choose `1`, enter `2026-10-31`, category `Food`, description `Lunch`, and amount
`125.50`. Choose `2` to inspect the table, `4` and `Food` for its category total,
or `5` for overall spending. Choose `6` and the displayed ID to delete a record.
`7` saves immediately; `8` saves pending changes and exits.

Categories use exact, case-sensitive matching after trimming surrounding
whitespace: `Food` and `food` are distinct. Category/description columns shorten
long text with `...` for display; full values remain stored. IDs are unique within
the dataset. A running process never reuses a deleted ID; after reload, allocation
resumes above the highest remaining ID.

An actual captured session using synthetic sample expenses produced:

```text
Added expense #1.
Added expense #2.
Added expense #3.
Category total (Food): INR 125.75
Overall total: INR 165.75
Expense deleted.
Overall total: INR 125.75
Expenses saved.
```

See [the full real execution transcript](docs/sample-execution.txt) for the exact
inputs, raw output, saved CSV, reload in a new process, and error recovery.

## Input validation

| Input | Accepted behavior |
| --- | --- |
| Menu | A single choice `1` through `8`; invalid input returns to the menu |
| Date | `YYYY-MM-DD`, years `0001`–`9999`, valid month/day, Gregorian leap-year rules |
| Category | 1–80 bytes after trimming, no ASCII control characters |
| Description | 1–500 bytes after trimming, no ASCII control characters |
| Amount | Positive decimal INR, whole digits and optional 1–2 fractional digits |
| Money limits | At most `92233720368547758.07` INR, including the aggregate total |
| ID | Positive unsigned 64-bit integer; nonexistent IDs are reported |
| Input line | At most 4096 bytes; longer lines are drained and rejected |
| Dataset | At most 100,000 expenses and a 16 MiB CSV |

Amounts reject zero, negative values, signs, exponents, separators, `NaN`,
infinity, trailing junk, excessive decimal places, and integer overflow. Examples:
`12`, `12.3`, and `0.01` are valid; `-1`, `0`, `.50`, `1.`, and `1.001` are invalid.
Boundary whitespace is trimmed. UTF-8 text can be stored, but terminal encoding
and wide-character alignment depend on the terminal; length limits count bytes.

EOF (Ctrl+Z then Enter on Windows; Ctrl+D on Unix) cancels the partial entry and
saves any completed changes. If saving fails during explicit exit, the program
keeps running so the failure can be resolved and saving retried. A save failure
at EOF returns exit code `1`, since interaction cannot continue. Usage errors
return `2`; normal exit returns `0`.

## Persistence and recovery

The CSV schema is versioned by its exact header:

```text
id,date,category,description,amount_paise
1,2026-10-31,"Food","Lunch, ""special""",12550
```

Amounts in the file are integer paise. Quoted fields escape quotes by doubling
them, and preserve commas. LF and CRLF files are accepted, including a final
record without a newline. Fields must occupy one physical line; embedded newlines
and control characters are deliberately rejected. Incorrect headers, invalid
fields, duplicate IDs, overflow, malformed quoting, and extra/missing columns
abort the entire load. Existing in-memory state and invalid source bytes remain intact.

Saving creates an exclusive `<path>.lock` directory and writes a complete
`pending.csv` inside it. After flushing and closing successfully, the previous
CSV is renamed to `<path>.bak`, and the staged file is moved into place. A failed
replacement attempts to restore the backup. Normal completion removes the backup
and staging directory. When startup finds a missing primary file and a valid
backup, it restores that backup only after validation.

Use **one running tracker per data file**. The lock serializes saves, but does
not provide concurrent-session merging or prevent stale writers. Saves are
failure-protected, but standard C++17 streams do not offer power-loss durability
or a portable atomic replacement of an existing file. Abrupt termination may
leave recovery artifacts:

1. Stop all tracker processes and copy the CSV, `.bak`, and `.lock/pending.csv` to a safe location.
2. If only `.bak` remains, restart to validate and restore it. A stale `.lock` must be removed manually once its contents are preserved.
3. If both CSV and `.bak` exist, inspect preserved copies and decide which is correct. Move the old backup aside before retrying a save; the app refuses to overwrite it.
4. If the file is corrupt, repair a copy or select another CSV path. The app refuses to continue from invalid data.

Data is local plaintext. No real expense records or credentials are included in this repository.

## Project structure

```text
include/Expense.hpp          Expense model, money/date/text validation
include/ExpenseTracker.hpp   Tracker and persistence interface
src/Expense.cpp             Validation and exact money formatting
src/ExpenseTracker.cpp      Operations, CSV parsing, protected persistence
src/main.cpp                Interactive CLI and EOF recovery
tests/test_core.cpp         Core assertions and filesystem failure cases
tests/test_cli.py           Real-process CLI integration tests
tests/record_evidence.py    Reproduce transcripts and test evidence
docs/sample-execution.txt   Actual synthetic demonstration sessions
docs/test-results.txt       Actual CTest output with private paths redacted
docs/VALIDATION_REPORT.md   Results, coverage, fix, and limitations
docs/screenshots/README.md  Capture status and suggested screenshot states
docs/ci.yml.example         Optional Windows, Linux, macOS CI template
CMakeLists.txt              Build, warning flags, CTest, install target
LICENSE                     MIT license
```

## Testing and evidence

```sh
ctest --test-dir build --output-on-failure -V
python tests/record_evidence.py build/expense_tracker --build-dir build
```

For Windows/Ninja, use `build/expense_tracker.exe` in the second command.
For MSVC, use `build/Release/expense_tracker.exe`; add `-C Release` when running
CTest directly. The evidence recorder targets a single-configuration build.

Local Windows Debug and Release builds passed **both CTest suites**:
**10,128 core assertions** plus **10 black-box CLI test methods**, with no
compiler warnings. Coverage includes exact money arithmetic and limits, real
dates, deletion, filtering, CSV round-trips/escaping, corruption, recovery,
oversized files/lines, save conflicts, EOF at every prompt, and subprocess reload.
See [validation report](docs/VALIDATION_REPORT.md) and
[actual test output](docs/test-results.txt). An optional three-OS GitHub Actions
template is included at `docs/ci.yml.example`. Existing GitHub authentication
does not grant the `workflow` scope, so the template is preserved outside the
active workflows directory. To enable it later, an authorized maintainer can
copy it to `.github/workflows/ci.yml`. No remote CI results are claimed.

## Screenshot gallery

Desktop capture was unavailable: the native Windows capture helper failed to
connect on two attempts. No screenshots have been fabricated or substituted.
The [screenshot directory](docs/screenshots/README.md) documents the failure
and the five requested capture states. The [real transcript](docs/sample-execution.txt)
provides textual evidence for all five states.

## Requirement compliance

- [x] C++17 with organized source files, headers, tests, and documentation
- [x] Add, list, category filter, category total, overall total, delete, save, exit
- [x] Expense ID, date, category, description, amount
- [x] Integer-paise money and overflow protection
- [x] Calendar dates, invalid input, empty fields, EOF, and recovery
- [x] Local CSV persistence, escaping, malformed-file rejection, protected saves
- [x] CMake warnings, meaningful CTest suites, actual CLI sessions
- [x] README, sample output, validation report, MIT license, data/secret exclusions
- [x] Reproducible evidence committed with the source
- [ ] Genuine screenshot images — native desktop capture unavailable

## License

[MIT](LICENSE).
