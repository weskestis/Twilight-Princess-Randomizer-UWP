#!/usr/bin/env python3
"""Import every declared species; keep retail assets outside the repository."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import sys

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', required=True, type=Path)
    p.add_argument('--addon', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--jobs', type=int, default=2)
    a = p.parse_args()
    here = Path(__file__).resolve().parent
    roster = json.loads((here/'roster.json').read_text())['species']
    a.out.mkdir(parents=True, exist_ok=True)
    def run(mon):
        target = a.out/f"{mon['id']:03d}-{mon['asset']}"
        if (target/'model.json').is_file():
            report = json.loads((target/'model.json').read_text())
            return dict(species=mon['id'], name=mon['name'], reused=True, clips=len(report['animations']))
        log = a.out/f"{mon['id']:03d}-import.log"
        with log.open('w') as f:
            task = subprocess.run([sys.executable, str(here/'import_model.py'),
                '--source', str(a.source/f"pkx_{mon['asset']}.fsys"),
                '--addon', str(a.addon), '--out', str(target)], stdout=f, stderr=subprocess.STDOUT)
        report = json.loads((target/'model.json').read_text()) if (target/'model.json').is_file() else None
        result = dict(species=mon['id'], name=mon['name'], exit=task.returncode,
                      clips=len(report['animations']) if report else 0, log=str(log))
        print(json.dumps(result), flush=True)
        return result
    with ThreadPoolExecutor(max_workers=max(1,min(4,a.jobs))) as pool:
        results = list(pool.map(run, roster))
    (a.out/'intake-results.json').write_text(json.dumps(results,indent=2)+'\n')
    if not all(x.get('exit',0)==0 and x['clips'] for x in results):
        raise SystemExit('Roster import incomplete; inspect each failed import log')

if __name__ == '__main__':
    main()
