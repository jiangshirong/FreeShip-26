"""Retained diagnostic for nondeterminism in the existing CLEMBLOU binary.

Run the same input bytes repeatedly in separate directories. This is not an
accuracy test or a replacement solver. Every invocation/output is retained.
"""
import argparse
import json
import math
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import solver_launch_probe as direct


def probe(executable, inputs, output, count=8, original=False, long_path=False):
    output.mkdir(parents=True, exist_ok=False)

    def one(index):
        folder = output/str(index)
        if original:
            folder.mkdir()
            work = folder / ('work-' + 'x'*160) if long_path else folder/'work'
            work.mkdir()
            for file in inputs.iterdir():
                if file.is_file() and file.suffix.lower() not in ('.log', '.exe', '.res'):
                    shutil.copy2(file, work/file.name)
            process = subprocess.run([str(executable)], cwd=work, input=b'',
                capture_output=True, timeout=8, creationflags=direct.common.FLAGS)
            shutil.copy2(work/'clemblou.res', folder/'clemblou.res')
            result = dict(exit_code=process.returncode,
                output_sha256={'clemblou.res': direct.common.digest_file(folder/'clemblou.res')})
            (folder/'process.log').write_bytes(process.stdout+process.stderr)
        else:
            result = direct.probe(executable, inputs, folder, output_names=('clemblou.res',))
        values = [float(token) for token in (folder/'clemblou.res').read_text('ascii').split()]
        return dict(index=index, exit_code=result.get('exit_code'),
                    finite=all(map(math.isfinite, values)), numbers=len(values),
                    output_sha256=result['output_sha256']['clemblou.res'])

    with ThreadPoolExecutor(max_workers=4) as pool:
        runs = list(pool.map(one, range(count)))
    result = dict(executable=str(executable), inputs=str(inputs), original_path=original,
                  long_workspace=long_path, runs=runs,
                  distinct_outputs=len({run['output_sha256'] for run in runs}),
                  nonfinite_runs=sum(not run['finite'] for run in runs))
    (output/'report.json').write_text(json.dumps(result, indent=2)+'\n', 'utf8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'inputs', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--count', type=int, default=8)
    parser.add_argument('--original-executable', action='store_true')
    parser.add_argument('--long-workspace', action='store_true')
    args = parser.parse_args()
    if not 2 <= args.count <= 100:
        parser.error('--count must be between 2 and 100')
    print(json.dumps(probe(args.executable.resolve(), args.inputs.resolve(),
                           args.output.resolve(), args.count,
                           args.original_executable, args.long_workspace)))
