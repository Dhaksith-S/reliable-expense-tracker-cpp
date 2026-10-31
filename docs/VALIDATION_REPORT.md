# Validation report

Validation date: **2026-10-31** (Asia/Calcutta). All demonstration data is synthetic.

## Environment and actual results

| Item | Observed result |
| --- | --- |
| OS | Windows NT build 26300, x86-64 |
| Compiler | MinGW-w64 GCC 16.1.0, UCRT/POSIX, C++17 |
| Build tooling | CMake 4.3.3, Ninja 1.13.2 |
| Test interpreter selected by CMake | Python 3.14.5 |
| Debug build | Successful, no compiler warnings |
| Release build | Successful, no compiler warnings |
| Debug CTest | 2/2 suites passed |
| Release CTest | 2/2 suites passed |
| Core suite | 10,128 assertions passed, including 9,999 money format/parse round-trips |
| CLI suite | 10 test methods passed; multiple real subprocess sessions and EOF subcases |
| Desktop capture | Unavailable; native helper pipe missing on initial attempt and retry |

Commands actually executed:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure -V
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure -V
python tests/record_evidence.py build-release/expense_tracker.exe --build-dir build-release
```

[test-results.txt](test-results.txt) records real verbose CTest output. Private
workstation paths are replaced with `[PROJECT]` / `[USER_HOME]`; test outcomes
are preserved. [sample-execution.txt](sample-execution.txt) records actual CLI
stdout/stderr, exact scripted inputs, return codes, saved CSV, and a fresh-process
reload. Redirected input is listed separately because it is not echoed by the application.

## Coverage

| Area | Verified behavior |
| --- | --- |
| Add / view | IDs, valid entries, readable table, empty views |
| Category operations | Exact matching, whitespace trimming, category subtotals, missing category |
| Money | Paise precision, two-place formatting, maximum value, individual/aggregate overflow, recovery after deletion |
| Invalid amount | Zero, negatives, sign, exponent, NaN/Inf, empty, extra decimal places, whitespace inside, trailing junk |
| Dates | Gregorian century leap rules (1900 invalid, 2000 valid), day/month/year boundaries, bad formats |
| Text | Empty/whitespace, excessive category/description length, ASCII controls, oversized input drainage |
| Deletion | Valid, missing, repeated, invalid and overflowing IDs; totals updated |
| CSV | Quoted commas, doubled quotes, LF/CRLF, missing terminal newline, ID continuation |
| Malformed files | Header, blank row, field count, duplicate/invalid IDs, invalid dates/text/amounts, broken quotes, aggregate overflow |
| Bounds | 100,000-row limit, 16 MiB load and save limits, uint64 ID exhaustion |
| Failure protection | Lock conflict, existing backup conflict, missing parent, directory destination, unchanged original on failed/oversized save, staging cleanup |
| Recovery | Valid orphan backup restored; invalid backup retained; failed load leaves prior tracker state intact |
| EOF | Each interactive prompt, partial entry cancelled, completed entries saved, failed save returns nonzero |
| Exit | Explicit save, autosave, failed save cancels exit, help and bad-argument codes |

## Defect found and corrected

The initial Debug run passed the CLI suite but failed the orphan-backup recovery
assertion on Windows. `load()` attempted to rename the validated backup while
the input file stream still held it open. The fix closes that stream before
renaming. The original backup remained preserved during the failure. Debug and
Release suites passed after the fix. The recovery regression test remains in
`tests/test_core.cpp`.

Inspection also led to caching the aggregate total so loading up to 100,000
records does not repeatedly recalculate the sum. Tests verify the cache across
add, deletion, failed operations, missing-file load, successful load, and overflow.

## Evidence and limits

- No screenshot images were captured. The desktop helper reported `Computer Use native pipe is unavailable` / `os error 2` twice. See [capture status](screenshots/README.md).
- Windows GCC Debug and Release were exercised locally. WSL Ubuntu was detected, but no GCC/CMake were available there; Linux/macOS builds were not claimed as local results.
- An optional GitHub Actions matrix template covers Windows/MSVC, Ubuntu/GCC, and macOS/Clang. GitHub rejected publishing an active workflow because the existing OAuth authentication lacks the `workflow` scope. The configuration is preserved in `docs/ci.yml.example` so source publication can proceed without broader credentials. No remote CI runs are claimed.
- Tests exercise deterministic filesystem failures and interrupted-backup recovery. Disk exhaustion, process kills at every individual filesystem operation, and physical power failure were not injected.
- Standard C++17 flushing/renaming cannot promise power-loss durability. Stale locks/backups require the documented recovery procedure. Concurrent editing sessions are unsupported.
- Actual user expense files, local build outputs, credentials, and private workstation paths are excluded from committed evidence.

## Release verification

Publication succeeded at
[Dhaksith-S/reliable-expense-tracker-cpp](https://github.com/Dhaksith-S/reliable-expense-tracker-cpp).
GitHub reported `PUBLIC` visibility, a nonempty repository, and default branch
`main`. The GitHub commit API, `git ls-remote`, and the local `HEAD` agreed on the
pushed source commit. `git status --porcelain` was empty. Local Markdown links
were checked and all resolved.

The initial workflow-enabled push was rejected for missing OAuth `workflow`
scope; no branch had been published at that point. The optional workflow was
retained as `docs/ci.yml.example`, and the initial unpushed commit was amended
before a normal successful push. No force-push was used. Active CI is the only
permission-limited optional feature. To grant that scope later, run
`gh auth refresh -h github.com -s workflow` and then activate the documented template.

The README, report, transcript, test output, and release notes are committed.
Git uses the authenticated account's public GitHub noreply address rather than
the workstation's private email address. No credentials, user expense records,
or local build artifacts are tracked.
