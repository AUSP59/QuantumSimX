
#!/usr/bin/env python3
import sys, pathlib, re
root = pathlib.Path(__file__).resolve().parents[1]
ok = True
pat = re.compile(r'^// SPDX-License-Identifier: ', re.M)
for p in root.rglob('*.[ch]pp'):
    if not pat.search(p.read_text(errors='ignore')):
        print('Missing SPDX in', p)
        ok = False
sys.exit(0 if ok else 1)
