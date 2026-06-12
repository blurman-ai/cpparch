#!/usr/bin/env python3
"""
PER-STRUCT drift: attribute each CURRENT bool field to its enclosing struct, then
git-blame each field to the commit that introduced it. A struct whose bool fields
entered via MANY DISTINCT commits over time = real incremental drift (constraint
decay) — attributed to ONE struct, fields only (no method sigs / locals).
"""
import os
import re
import subprocess
from collections import defaultdict
from datetime import date

OSS = "/home/localadm/oss"
LIST = "/tmp/all_local.txt"
AGENTIC = "/tmp/agentic_local.txt"
HDR_EXT = (".h", ".hpp", ".hh", ".hxx")
MIN_FIELDS = 4         # only structs with >=4 bool fields are worth blaming
MIN_COMMITS = 4        # drift = bools entered via >=4 distinct commits
VEND = re.compile(r'/lib/|/third_party/|/extern/|/vendor/|espasync|imgui|sdl_|rapidjson|/examples?/', re.I)

STRUCT_RE = re.compile(r'(?:struct|class)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^{;]*)?\{')
BOOL_RE = re.compile(r'^\s*(?:mutable\s+)?bool\s+([A-Za-z_]\w*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;\s*(?://.*)?$')
CFG_NAME = re.compile(r'(config|configdata|settings|options|opts|preferences|prefs|params|parameters|args|arguments)$', re.I)


def find_body(text, start):
    depth, in_body, bs = 0, False, 0
    for i in range(start, len(text)):
        c = text[i]
        if c == '{':
            if not in_body:
                in_body, bs = True, i + 1
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0 and in_body:
                return bs, i
    return None


def struct_fields(text):
    """-> list of (struct_name, field_name, lineno) for direct bool members."""
    out = []
    for m in STRUCT_RE.finditer(text):
        name = m.group(1)
        body = find_body(text, m.start())
        if not body:
            continue
        bs, be = body
        base_line = text.count('\n', 0, bs) + 1
        depth = 0
        for off, line in enumerate(text[bs:be].split('\n')):
            if depth == 0 and '(' not in line and ')' not in line:
                fm = BOOL_RE.match(line)
                if fm:
                    out.append((name, fm.group(1), base_line + off))
            depth += line.count('{') - line.count('}')
            if depth < 0:
                depth = 0
    return out


def blame_dates(repo, relpath):
    """-> {lineno: (sha10, 'YYYY-MM-DD')} via one porcelain blame."""
    try:
        out = subprocess.run(["git", "-C", repo, "blame", "--line-porcelain", "--", relpath],
                             capture_output=True, text=True, errors="ignore", timeout=120).stdout
    except (subprocess.TimeoutExpired, OSError):
        return {}
    res = {}
    sha = None
    ctime = None
    final_line = None
    for line in out.splitlines():
        if re.match(r'^[0-9a-f]{40} ', line):
            parts = line.split()
            sha = parts[0][:10]
            final_line = int(parts[2])
        elif line.startswith("committer-time "):
            ctime = int(line.split()[1])
        elif line.startswith("\t"):
            if final_line is not None and ctime is not None:
                res[final_line] = (sha, date.fromtimestamp(ctime).isoformat())
    return res


EXAMINED = []

def scan_repo(repo):
    rd = os.path.join(OSS, repo)
    drifts = []
    for root, _, files in os.walk(rd):
        for f in files:
            if not f.endswith(HDR_EXT):
                continue
            fp = os.path.join(root, f)
            if VEND.search(fp):
                continue
            try:
                if os.path.getsize(fp) > 2_000_000:
                    continue
                text = open(fp, encoding="utf-8", errors="ignore").read()
            except OSError:
                continue
            if "bool" not in text:
                continue
            fields = struct_fields(text)
            if not fields:
                continue
            # structs with >=MIN_FIELDS bool fields
            by_struct = defaultdict(list)
            for s, fld, ln in fields:
                by_struct[s].append((fld, ln))
            big = {s: v for s, v in by_struct.items() if len(v) >= MIN_FIELDS}
            if not big:
                continue
            rel = os.path.relpath(fp, rd)
            bl = blame_dates(rd, rel)
            if not bl:
                continue
            for s, flds in big.items():
                EXAMINED.append(repo)
                commits = {}
                for fld, ln in flds:
                    if ln in bl:
                        sha, d = bl[ln]
                        commits.setdefault(sha, d)
                ncommits = len(commits)
                if ncommits >= MIN_COMMITS:
                    ds = sorted(commits.values())
                    drifts.append({
                        "repo": repo, "file": rel, "struct": s,
                        "nfields": len(flds), "ncommits": ncommits,
                        "span": (date.fromisoformat(ds[-1]) - date.fromisoformat(ds[0])).days,
                        "first": ds[0], "last": ds[-1],
                        "is_cfg": bool(CFG_NAME.search(s)),
                        "path": fp,
                    })
    return drifts



def main():
    agentic = set(r.strip() for r in open(AGENTIC) if r.strip())
    repos = [r.strip() for r in open(LIST) if r.strip()]
    alld = []
    scanned = 0
    for i, repo in enumerate(repos, 1):
        if not os.path.isdir(os.path.join(OSS, repo, ".git")):
            continue
        scanned += 1
        for d in scan_repo(repo):
            d["agentic"] = repo in agentic
            alld.append(d)
        if i % 50 == 0:
            print(f"[{i}/{len(repos)}] drifts={len(alld)}", flush=True)

    real = [d for d in alld if not d["is_cfg"]]
    def grp(lst, ag): return [d for d in lst if d["agentic"] == ag]

    from collections import Counter
    ex_by_repo = Counter(EXAMINED)
    # examined pool per group
    ex_ag = sum(v for r, v in ex_by_repo.items() if r in agentic)
    ex_non = sum(v for r, v in ex_by_repo.items() if r not in agentic)
    rA = len(set(d["repo"] for d in grp(real, True)))
    rN = len(set(d["repo"] for d in grp(real, False)))
    nA = len([r for r in agentic if os.path.isdir(os.path.join(OSS, r, ".git"))])
    nN = scanned - nA

    print(f"\n=== ALL {scanned} repos: per-struct drift (content, >={MIN_FIELDS}bool, >={MIN_COMMITS} commits) ===")
    print(f"{'':22} {'repos':>6} {'examined':>9} {'drift':>6} {'drift/exam':>11} {'repos w/drift':>14}")
    for label, ag, nrepo, exa in [("AGENTIC", True, nA, ex_ag), ("NON-AGENTIC", False, nN, ex_non)]:
        d = grp(real, ag)
        rw = len(set(x["repo"] for x in d))
        rate = f"{100*len(d)/exa:.1f}%" if exa else "-"
        print(f"{label:22} {nrepo:>6} {exa:>9} {len(d):>6} {rate:>11} {rw:>5}/{nrepo} ({100*rw//max(nrepo,1)}%)")

    csv = "experiments/boolean_state/perstruct_drift_all.csv"
    with open(csv, "w") as fh:
        fh.write("agentic,ncommits,nfields,span_days,first,last,is_config,struct,repo,file\n")
        for d in alld:
            fh.write(f'{int(d["agentic"])},{d["ncommits"]},{d["nfields"]},{d["span"]},{d["first"]},{d["last"]},{int(d["is_cfg"])},{d["struct"]},{d["repo"]},"{d["file"]}"\n')
    print(f"\nCSV: {csv}")

if __name__ == "__main__":
    main()
