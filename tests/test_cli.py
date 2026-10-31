"""Black-box sessions against the real executable; standard library only."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

EXE = str(pathlib.Path(sys.argv.pop(1)).resolve())


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="expense-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.file = self.root / "expenses.csv"

    def run_cli(self, text, path=None, code=0):
        result = subprocess.run([EXE, str(path or self.file)], input=text, text=True,
                                capture_output=True, timeout=10, cwd=self.root)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_full_lifecycle_and_reload(self):
        output = self.run_cli('1\n2026-10-31\nFood\nLunch, "special"\n125.50\n'
                              '1\n2026-10-31\nTravel\nBus\n40\n'
                              '1\n2026-10-31\nFood\nTea\n0.25\n'
                              '2\n3\nFood\n4\nFood\n5\n6\n2\n6\n999\n7\n8\n')
        for expected in ['Added expense #3.', 'Lunch, "special"', 'Category total (Food): INR 125.75',
                         'Overall total: INR 165.75', 'Expense deleted.', 'Expense ID not found.', 'Expenses saved.']:
            self.assertIn(expected, output)
        output = self.run_cli('2\n5\n8\n')
        self.assertIn('Loaded 2 expense(s)', output)
        self.assertIn('Overall total: INR 125.75', output)
        self.assertNotIn('Bus', output)
        self.assertIn('"Lunch, ""special"""', self.file.read_text())

    def test_invalid_fields_recover(self):
        output = self.run_cli('hello\n0\n9\n1\n2026-02-29\n1900-02-29\n2024-02-29\n'
                              '\nFood\n\nTea\n-1\n0\nabc\n1.999\n92233720368547758.08\n0.10\n'
                              '6\n18446744073709551616\n6\n-1\n5\n8\n')
        self.assertEqual(output.count('Invalid menu choice'), 3)
        self.assertEqual(output.count('Invalid date:'), 2)
        self.assertEqual(output.count('Invalid field:'), 2)
        self.assertEqual(output.count('Invalid amount:'), 5)
        self.assertEqual(output.count('Invalid ID:'), 2)
        self.assertIn('Overall total: INR 0.10', output)

    def test_eof_at_each_prompt(self):
        prefixes = ['', '1\n', '1\n2026-10-31\n', '1\n2026-10-31\nFood\n',
                    '1\n2026-10-31\nFood\nTea\n', '3\n', '4\n', '6\n']
        for prefix in prefixes:
            with self.subTest(prefix=prefix):
                output = self.run_cli(prefix)
                self.assertIn('End of input; incomplete entry cancelled.', output)
                self.assertFalse(self.file.exists())

    def test_eof_saves_completed_entry_and_discards_partial(self):
        output = self.run_cli('1\n2026-10-31\nFood\nTea\n0.10\n1\n2026-10-31\n')
        self.assertIn('Expenses saved.', output)
        self.assertIn('Loaded 1 expense(s)', self.run_cli('8'))

    def test_oversized_and_control_input(self):
        output = self.run_cli('x' * 5000 + '\n1\n2026-10-31\nBad\x1b\nFood\nTea\n1\n8\n')
        self.assertIn('line discarded', output)
        self.assertIn('Invalid field:', output)
        self.assertIn('Added expense #1.', output)

    def test_corrupt_file_is_never_overwritten(self):
        original = b'id,date,category,description,amount_paise\n1,bad,Food,Tea,10\n'
        self.file.write_bytes(original)
        self.assertIn('Cannot load data', self.run_cli('8\n', code=1))
        self.assertEqual(self.file.read_bytes(), original)

    def test_failed_exit_save_and_eof_nonzero(self):
        self.file.with_name(self.file.name + '.lock').mkdir()
        output = self.run_cli('1\n2026-10-31\nFood\nTea\n1\n8\n', code=1)
        self.assertIn('Exit cancelled; unsaved changes remain.', output)
        self.assertIn('Unsaved changes could not be persisted.', output)
        self.assertFalse(self.file.exists())

    def test_aggregate_overflow_recovery(self):
        output = self.run_cli('1\n2026-10-31\nLimit\nMaximum\n92233720368547758.07\n'
                              '1\n2026-10-31\nFood\nTea\n0.01\n5\n6\n1\n'
                              '1\n2026-10-31\nFood\nTea\n0.01\n5\n8\n')
        self.assertIn('Overall total would overflow', output)
        self.assertIn('Overall total: INR 92233720368547758.07', output)
        self.assertIn('Overall total: INR 0.01', output)

    def test_empty_views_and_zero_totals(self):
        output = self.run_cli('2\n3\nAbsent\n4\nAbsent\n5\n8\n')
        self.assertEqual(output.count('No expenses found.'), 2)
        self.assertIn('Category total (Absent): INR 0.00', output)
        self.assertIn('Overall total: INR 0.00', output)

    def test_arguments_and_help(self):
        result = subprocess.run([EXE, '--help'], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertIn('Usage:', result.stdout)
        result = subprocess.run([EXE, 'a', 'b'], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main(verbosity=2)
