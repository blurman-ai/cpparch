#!/usr/bin/env python3
"""Pre-classify LCX findings (#109) for the eyeball pass.

Per violation:
- collision-risk: >1 definitions of the short symbol name in the file at that
  commit (same-name cross-match family: dense listeners / overloads / shift).
- arity-change-new: 'new function' whose symbol DID exist in the parent
  version (signature change -> false "new").
- clean: everything else (goes to plain eyeball review).

Output: classified.tsv (repo, sha, file, line, kind, symbol, detail, message).
"""
import json
import re
import subprocess
import sys
from functools import lru_cache

ROOT = "/home/localadm/projects/cpparch/experiments/lcx_corpus_run"
OSS = "/home/localadm/oss"

GREW = re.compile(r"^(?P<file>.+?):(?P<line>\d+): \S+ — function '(?P<sym>.+?)' grew local complexity from \d+ to \d+ \(\+\d+(?P<reason>[^)]*)\)$")
NEW = re.compile(r"^(?P<file>.+?):(?P<line>\d+): \S+ — new function '(?P<sym>.+?)' has local complexity \d+")

def short(sym):
    return sym.rsplit("::", 1)[-1]

@lru_cache(maxsize=100000)
def def_count(repo, sha, path, name):
    """Occurrences of `name(`-ish definitions in file@sha (cheap proxy)."""
    try:
        p = subprocess.run(["git", "-C", f"{OSS}/{repo}", "show", f"{sha}:{path}"],
                           capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return -1
    if p.returncode != 0:
        return -1
    # definition-ish: name( at start-of-token, line not ending with ';' (declaration)
    n = 0
    pat = re.compile(r"(?:^|[\s:~*&])" + re.escape(name) + r"\s*\(")
    for line in p.stdout.splitlines():
        if pat.search(line) and not line.rstrip().endswith(";"):
            n += 1
    return n

def main():
    out = open(f"{ROOT}/classified.tsv", "w")
    out.write("repo\tsha\tfile\tline\tkind\tsymbol\tdetail\tmessage\n")
    stats = {}
    for raw in open(f"{ROOT}/findings.jsonl"):
        d = json.loads(raw)
        repo, sha = d["repo"], d["sha"]
        for v in d["violations"]:
            m = GREW.match(v)
            kind, sym, detail = "clean", "", ""
            if m:
                sym = m.group("sym")
                c = def_count(repo, sha, m.group("file"), short(sym))
                if c > 2:  # def + likely twin(s); >2 keeps recursion/decl noise out
                    kind, detail = "collision-risk", f"defs={c}"
            else:
                m = NEW.match(v)
                if m:
                    sym = m.group("sym")
                    pc = def_count(repo, f"{sha}^", m.group("file"), short(sym))
                    if pc > 0:
                        kind, detail = "arity-change-new", f"parent_defs={pc}"
            f, ln = (m.group("file"), m.group("line")) if m else ("?", "0")
            out.write(f"{repo}\t{sha}\t{f}\t{ln}\t{kind}\t{sym}\t{detail}\t{v}\n")
            stats[kind] = stats.get(kind, 0) + 1
    out.close()
    print(stats)

if __name__ == "__main__":
    sys.exit(main())
