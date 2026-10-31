"""Record actual CLI and CTest output using synthetic data only."""
import argparse
import pathlib
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable', type=pathlib.Path)
parser.add_argument('--build-dir', type=pathlib.Path, required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
exe = args.executable.resolve()
docs = root / 'docs'
docs.mkdir(exist_ok=True)
parts = ['Reliable Expense Tracker CLI - actual process output\n'
         'Synthetic demonstration data. Inputs are listed separately because redirected stdin does not echo.\n']


def session(title, inputs, file, expected_code=0):
    run = subprocess.run([str(exe), str(file)], input=inputs, text=True,
                         capture_output=True, timeout=15)
    if run.returncode != expected_code:
        raise RuntimeError(run.stdout + run.stderr)
    parts.append(f'\n=== {title} ===\nINPUT (one line per prompt):\n{inputs}'
                 f'\nACTUAL STDOUT:\n{run.stdout}ACTUAL STDERR:\n{run.stderr}'
                 f'EXIT CODE: {run.returncode}\n')


with tempfile.TemporaryDirectory(prefix='expense-evidence-') as directory:
    file = pathlib.Path(directory) / 'demo.csv'
    session('Add, list, filter, totals, delete and explicit save',
            '1\n2026-10-31\nFood\nLunch, "special"\n125.50\n'
            '1\n2026-10-31\nTravel\nBus fare\n40.00\n'
            '1\n2026-10-31\nFood\nTea\n0.25\n'
            '2\n3\nFood\n4\nFood\n5\n6\n2\n5\n7\n8\n', file)
    parts.append('\nACTUAL SAVED CSV (synthetic data):\n' + file.read_text())
    session('New process reloads the saved CSV', '2\n5\n8\n', file)
    session('Invalid menu, real-date validation, empty field and amount recovery',
            'oops\n1\n2026-02-29\n2024-02-29\n\nHealth\nVitamins\n-1\n0\nabc\n'
            '92233720368547758.08\n50.00\n5\n8\n', file)
    session('EOF cancels a partial entry', '1\n2026-10-31\n', file)
    broken = pathlib.Path(directory) / 'broken.csv'
    broken.write_text('id,date,category,description,amount_paise\n1,bad,Food,Tea,100\n')
    original = broken.read_bytes()
    session('Malformed file fails closed', '8\n', broken, expected_code=1)
    assert broken.read_bytes() == original
    parts.append('Verified: malformed file bytes unchanged.\n')

(docs / 'sample-execution.txt').write_text(''.join(parts), encoding='utf-8')
run = subprocess.run(['ctest', '--test-dir', str(args.build_dir.resolve()), '-V', '--output-on-failure'],
                     capture_output=True, text=True, timeout=90)
output = run.stdout + run.stderr
# Preserve real results while removing private workstation paths from published evidence.
for private, replacement in [(str(root), '[PROJECT]'), (str(root).replace('\\', '/'), '[PROJECT]'),
                             (str(pathlib.Path.home()), '[USER_HOME]'),
                             (str(pathlib.Path.home()).replace('\\', '/'), '[USER_HOME]')]:
    output = output.replace(private, replacement)
(docs / 'test-results.txt').write_text(
    'Actual CTest verbose output. Private workstation paths replaced with [PROJECT] / [USER_HOME].\n'
    + output + f'\nExit code: {run.returncode}\n', encoding='utf-8')
print('Recorded actual sessions in docs/sample-execution.txt and tests in docs/test-results.txt')
sys.exit(run.returncode)
