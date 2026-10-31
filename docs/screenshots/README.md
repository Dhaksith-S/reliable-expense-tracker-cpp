# Screenshot capture status

No screenshots were captured in this environment. The Windows Computer Use
native helper failed on the initial connection and a retry with:

```text
Computer Use native pipe is unavailable: failed to connect native pipe:
The system cannot find the file specified. (os error 2)
```

No rendered transcript, mock terminal, or generated image is presented as a
screenshot. Genuine process output is available in
[sample-execution.txt](../sample-execution.txt), including menu, adding,
listing, totals, input-error recovery, reload, EOF, and malformed-file handling.

To add screenshots later, run the CLI in a real terminal and capture:

| Suggested filename | Real application state |
| --- | --- |
| `menu.png` | Initial eight-option menu |
| `add-expense.png` | Successful expense entry |
| `all-expenses.png` | Table containing sample expenses |
| `totals.png` | Category and overall totals |
| `input-recovery.png` | Invalid input followed by successful recovery |

These filenames are suggestions, not existing image artifacts.
