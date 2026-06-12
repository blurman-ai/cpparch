#!/usr/bin/env python3
"""Lateral inter-module drift scan on per-commit graph-drift jsonl data.

Reads *_graph_drift.jsonl files and applies the lateral drift criterion
(docs/research/lateral_module_drift_criterion.md).  Output is one CSV row
per LATERAL event:

  LATERAL.CYCLE  — A→B between peer modules where B→A already existed
  LATERAL.SDP    — A→B where I(B) > I(A) + delta  (Martin SDP)
  LATERAL.NEW    — first lateral edge between sibling modules

Graph state is reconstructed by forward-only replay of the `added` edge lists
from the jsonl (removed_edges were not captured — ~5.6% phantom-edge error,
acceptable for a corpus prototype).

Usage:
    python3 lateral_drift_scan.py [--limit N] [--output FILE]
"""

from __future__ import annotations

import argparse
import collections
import csv
import glob
import json
import os
import re
import subprocess
import sys
from pathlib import Path

# ── Constants ─────────────────────────────────────────────────────────────────

MASS_TOUCH_THRESHOLD = 150    # commits with more added edges are vendor/reorg dumps
GRACE_PERIOD_COMMITS = 10     # skip first N commits for a newly-seen module
SDP_DELTA = 0.10              # Martin instability delta for LATERAL.SDP
SHARED_FID_RATIO = 0.50       # B is shared if FID(B) >= this fraction of max FID

OSS_DIR = '/home/localadm/oss'
# Same agent-author markers as agent_author_scan.py
BOT_HINTS = ('copilot', 'swe-agent', 'cursor', 'devin-ai', 'devin',
             'google-labs-jules', 'claude', 'codex', 'aider')

# ── Path helpers ──────────────────────────────────────────────────────────────

_VENDOR_RE = re.compile(
    r'(?:^|/)(?:third[_-]?party|vendor|extern(?:al)?|deps|thirdparty|ext)/',
    re.IGNORECASE,
)
_TEST_RE = re.compile(
    r'(?:^|/)(?:tests?|specs?|fixtures?|mocks?|unittests?|integrationtests?'
    r'|examples?|samples?|benchmarks?|docs?|demos?|\w+tests?)/',
    re.IGNORECASE,
)

# Cross-module edges INTO a file shadowing a libc/system header are almost
# always resolution artifacts (<string.h> landing on common/jinja/string.h,
# <omp.h> on libiomp_win/include/omp.h), not real coupling.
_SYSTEM_BASENAMES = frozenset((
    'string.h', 'math.h', 'time.h', 'signal.h', 'assert.h', 'complex.h',
    'ctype.h', 'errno.h', 'fenv.h', 'float.h', 'limits.h', 'locale.h',
    'setjmp.h', 'stdarg.h', 'stddef.h', 'stdio.h', 'stdlib.h', 'threads.h',
    'uchar.h', 'wchar.h', 'wctype.h', 'memory.h', 'malloc.h', 'io.h',
    'omp.h', 'pthread.h', 'unistd.h', 'windows.h', 'fcntl.h', 'sched.h',
))


def is_excluded(path: str) -> bool:
    return bool(_VENDOR_RE.search(path) or _TEST_RE.search(path))


_HEADER_EXTS = ('.h', '.hh', '.hpp', '.hxx', '.ipp', '.tpp', '.inl', '.inc')


def detect_parallel_trees(files: list[str]) -> set[str]:
    """Top-level dirs forming a split layout (include/X + src/X = one module X).
    Structural test, no name matching: two tops sharing >=2 child-dir names,
    where one top is (almost) headers-only and the other (almost) sources-only.
    The header/source asymmetry is what separates a true split layout from two
    sibling apps that merely use similar subdir names (xLights/ + xSchedule/
    both having controllers/ must NOT be merged)."""
    children: dict[str, set[str]] = collections.defaultdict(set)
    header_cnt: collections.Counter = collections.Counter()
    total_cnt: collections.Counter = collections.Counter()
    for f in files:
        parts = Path(f).parts
        if len(parts) >= 2:
            total_cnt[parts[0]] += 1
            if f.lower().endswith(_HEADER_EXTS):
                header_cnt[parts[0]] += 1
        if len(parts) >= 3:
            children[parts[0]].add(parts[1])

    def header_ratio(top: str) -> float:
        return header_cnt[top] / total_cnt[top] if total_cnt[top] else 0.0

    tops = list(children)
    strips: set[str] = set()
    for i, a in enumerate(tops):
        for b in tops[i + 1:]:
            if len(children[a] & children[b]) < 2:
                continue
            ra, rb = header_ratio(a), header_ratio(b)
            if (ra >= 0.8 and rb <= 0.3) or (rb >= 0.8 and ra <= 0.3):
                strips |= {a, b}
    return strips


def normalize(file_path: str, strips: set[str]) -> str:
    parts = Path(file_path).parts
    if len(parts) > 1 and parts[0] in strips:
        return '/'.join(parts[1:])
    return file_path


def get_module(file_path: str, depth: int) -> str:
    parts = Path(file_path).parts
    if len(parts) > depth:
        return '/'.join(parts[:depth])
    return str(Path(file_path).parent)


def are_siblings(mod_a: str, mod_b: str) -> bool:
    """True iff mod_a and mod_b share a parent and neither is ancestor of other."""
    if mod_a == mod_b:
        return False
    pa, pb = Path(mod_a), Path(mod_b)
    try:
        pa.relative_to(pb)
        return False
    except ValueError:
        pass
    try:
        pb.relative_to(pa)
        return False
    except ValueError:
        pass
    return pa.parent == pb.parent


def detect_module_depth(all_files: list[str]) -> int:
    """Return shallowest k where ≥3 sibling dirs each contain a C++ source file."""
    max_depth = max((len(Path(f).parts) for f in all_files), default=2)
    for depth in range(1, max_depth + 1):
        source_dirs: set[str] = set()
        for f in all_files:
            if is_excluded(f):
                continue
            parts = Path(f).parts
            if len(parts) > depth and f.endswith(('.cpp', '.c', '.cc', '.cxx', '.h', '.hpp')):
                source_dirs.add('/'.join(parts[:depth]))
        if len(source_dirs) >= 3:
            return depth
    return 1


def parse_edge_line(s: str) -> tuple[str, str] | None:
    """Parse '  + from  ->  to' → (from, to), or None."""
    m = re.match(r'^[+\-]\s+(.+?)\s+->\s+(.+)$', s.strip())
    return (m.group(1).strip(), m.group(2).strip()) if m else None


def touched_files(repo_dir: str, sha: str) -> set[str] | None:
    """Files modified by commit `sha`, or None if git is unavailable."""
    try:
        out = subprocess.run(
            ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'show',
             '--no-renames', '--name-only', '--format=', sha],
            capture_output=True, text=True, timeout=60, check=True,
        ).stdout
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return None
    return {l.strip() for l in out.splitlines() if l.strip()}


_SOURCE_EXTS = ('.cpp', '.c', '.cc', '.cxx') + _HEADER_EXTS
_INCLUDE_RE = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')


def confirm_backedge(repo_dir: str, sha: str, mod_a: str, mod_b: str, depth: int,
                     strips: set[str], cache: dict) -> bool | None:
    """Ground-truth check that module `mod_b` really depends on `mod_a` at
    `<sha>~1`, i.e. some file in mod_b #includes a file in mod_a.

    The replay graph's `mod_edges` can retain a PHANTOM reverse edge: removed
    lists are lost (forward-only replay) and archcheck's bare-name resolution
    can synthesise an edge with no matching #include. A CYCLE grade therefore
    must be confirmed against real source at the event's parent commit; the
    two eyeball FPs (#115 §8.3) were leaf targets with no such back-edge.

    Returns True/False, or None when unverifiable (repo or sha unavailable) —
    callers keep the replay verdict on None so events are never lost to a
    missing clone.
    """
    if not os.path.isdir(repo_dir):
        return None
    key = (sha, mod_a, mod_b)
    if key in cache:
        return cache[key]
    parent = sha + '~1'
    try:
        names = subprocess.run(
            ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'ls-tree', '-r',
             '--name-only', parent],
            capture_output=True, text=True, timeout=60, check=True,
        ).stdout.splitlines()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        cache[key] = None
        return None
    b_files: list[tuple[str, str]] = []   # (raw path, normalized path)
    a_norm_paths: set[str] = set()        # normalized paths of mod_a source files
    a_basenames: set[str] = set()
    for raw in names:
        raw = raw.strip()
        if not raw or not raw.endswith(_SOURCE_EXTS):
            continue
        nf = normalize(raw, strips)
        mod = get_module(nf, depth)
        if mod == mod_b:
            b_files.append((raw, nf))
        elif mod == mod_a:
            a_norm_paths.add(nf)
            a_basenames.add(Path(nf).name)
    if not b_files or not a_norm_paths:
        cache[key] = False
        return False
    for raw, nf_b in b_files:
        try:
            content = subprocess.run(
                ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'show', f'{parent}:{raw}'],
                capture_output=True, text=True, timeout=30, check=True,
            ).stdout
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            continue
        for inc in _INCLUDE_RE.findall(content):
            # Resolve the include the way archcheck does and check it lands in
            # mod_a:
            #  - relative (`../math/x.h`, `./x.h`): normalise against the
            #    including file's directory, then exact-match a mod_a path;
            #  - root-relative (`engine/x.h`): suffix-match a mod_a path;
            #  - bare (`x.h`): basename match.
            if inc.startswith(('./', '../')):
                cand = os.path.normpath(os.path.join(os.path.dirname(nf_b), inc))
                if cand in a_norm_paths:
                    cache[key] = True
                    return True
                continue
            if any(p == inc or p.endswith('/' + inc) for p in a_norm_paths):
                cache[key] = True
                return True
            if '/' not in inc and inc in a_basenames:
                cache[key] = True
                return True
    cache[key] = False
    return False


_COAUTHOR_RE = re.compile(
    r'co-authored-by:.*\b(claude|copilot|cursor|devin|aider|codex|jules)\b', re.IGNORECASE)


def classify_commit_author(repo_dir: str, sha: str) -> str:
    """'agent' if author/committer matches BOT_HINTS or an AI co-author trailer
    is present, else 'human'."""
    try:
        out = subprocess.run(
            ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'log', '-1',
             '--format=%an|%ae|%cn|%ce%n%b', sha],
            capture_output=True, text=True, timeout=30, check=True,
        ).stdout
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return 'unknown'
    ident = out.splitlines()[0].lower() if out else ''
    if any(h in ident for h in BOT_HINTS):
        return 'agent'
    return 'agent' if _COAUTHOR_RE.search(out) else 'human'


def load_baseline(path: str) -> list[tuple[str, str]] | None:
    """Parse archcheck --save-graph-baseline YAML → file edge list, or None."""
    if not os.path.isfile(path):
        return None
    nodes: list[str] = []
    edges: list[tuple[str, str]] = []
    section = ''
    node_re = re.compile(r'^\s*-\s*"(.*)"\s*$')
    edge_re = re.compile(r'^\s*-\s*\[(\d+),\s*(\d+)\]\s*$')
    with open(path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('nodes:'):
                section = 'nodes'
            elif line.startswith('edges:'):
                section = 'edges'
            elif section == 'nodes':
                m = node_re.match(line)
                if m:
                    nodes.append(m.group(1))
            elif section == 'edges':
                m = edge_re.match(line)
                if m:
                    edges.append((nodes[int(m.group(1))], nodes[int(m.group(2))]))
    return edges


# ── Incremental graph ─────────────────────────────────────────────────────────

class IncrementalGraph:
    """Accumulates file edges; tracks module-level FID / FOD / instability."""

    def __init__(self, depth: int) -> None:
        self.depth = depth
        self._file_edges: set[tuple[str, str]] = set()
        self.mod_edges: set[tuple[str, str]] = set()
        self.fid: collections.Counter = collections.Counter()  # fan-in diversity
        self.fod: collections.Counter = collections.Counter()  # fan-out diversity
        # Rename detection (jsonl has no removed-edge lists, so moves are
        # detected on the adding side):
        # - moved SOURCE: (basename(from), to) already seen from another module;
        # - moved TARGET: from_f already had an edge to a file with the same
        #   basename (compat.h -> compat/compat.h re-points every consumer).
        self.edge_signature: dict[tuple[str, str], set[str]] = collections.defaultdict(set)
        self.target_basenames: dict[str, set[str]] = collections.defaultdict(set)

    def looks_like_move(self, from_f: str, to_f: str) -> bool:
        sig = (Path(from_f).name, to_f)
        mods = self.edge_signature.get(sig)
        my_mod = get_module(from_f, self.depth)
        if mods and any(m != my_mod for m in mods):
            return True
        return Path(to_f).name in self.target_basenames.get(from_f, ())

    def add(self, from_f: str, to_f: str) -> None:
        if (from_f, to_f) in self._file_edges:
            return
        self._file_edges.add((from_f, to_f))
        self.edge_signature[(Path(from_f).name, to_f)].add(get_module(from_f, self.depth))
        self.target_basenames[from_f].add(Path(to_f).name)
        mod_a = get_module(from_f, self.depth)
        mod_b = get_module(to_f, self.depth)
        if mod_a == mod_b:
            return
        pair = (mod_a, mod_b)
        if pair not in self.mod_edges:
            self.mod_edges.add(pair)
            self.fid[mod_b] += 1
            self.fod[mod_a] += 1

    def instability(self, mod: str) -> float:
        ca, ce = self.fid.get(mod, 0), self.fod.get(mod, 0)
        return ce / (ca + ce) if (ca + ce) > 0 else 0.5

    def is_shared(self, mod: str) -> bool:
        if not self.fid:
            return False
        max_fid = max(self.fid.values())
        if max_fid == 0:
            return False
        fod_vals = sorted(self.fod.values())
        median_fod = fod_vals[len(fod_vals) // 2] if fod_vals else 0
        return (
            self.fid.get(mod, 0) >= SHARED_FID_RATIO * max_fid
            and self.fod.get(mod, 0) <= median_fod
        )

    def lakos_level(self) -> dict[str, int]:
        """Approximate Lakos level: longest path in module DAG (ignores SCCs)."""
        adj: dict[str, set[str]] = collections.defaultdict(set)
        in_deg: collections.Counter = collections.Counter()
        all_mods: set[str] = set()
        for a, b in self.mod_edges:
            adj[a].add(b)
            in_deg[b] += 1
            all_mods |= {a, b}
        level = {m: 0 for m in all_mods}
        queue = [m for m in all_mods if in_deg.get(m, 0) == 0]
        while queue:
            node = queue.pop(0)
            for nb in adj.get(node, set()):
                if level.get(nb, 0) < level[node] + 1:
                    level[nb] = level[node] + 1
                in_deg[nb] -= 1
                if in_deg[nb] == 0:
                    queue.append(nb)
        return level


# ── Per-repo scan ─────────────────────────────────────────────────────────────

def scan_repo(jsonl_path: str, agentic_map: dict[str, int], baseline_dir: str) -> list[dict] | None:
    """Returns events, [] for degenerate repos, None when no baseline exists."""
    local_name = Path(jsonl_path).name.replace('_graph_drift.jsonl', '')
    baseline_edges = load_baseline(
        os.path.join(baseline_dir, f'{local_name}_window_baseline.yml'))
    if baseline_edges is None:
        return None

    records: list[dict] = []
    with open(jsonl_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    if len(records) < 2:
        return []

    raw_files: set[str] = set()
    for a, b in baseline_edges:
        raw_files.update((a, b))
    for rec in records:
        for s in rec.get('added', []):
            edge = parse_edge_line(s)
            if edge:
                raw_files.update(edge)
    if not raw_files:
        return []

    strips = detect_parallel_trees(list(raw_files))
    all_files = {normalize(f, strips) for f in raw_files}
    depth = detect_module_depth(list(all_files))

    active_mods = {get_module(f, depth) for f in all_files if not is_excluded(f)}
    if len(active_mods) < 4:
        return []

    gh_repo = local_name.replace('_', '/', 1)
    agentic = agentic_map.get(gh_repo, -1)
    repo_kind = {1: 'agentic', 0: 'human'}.get(agentic, 'unknown')

    graph = IncrementalGraph(depth)
    mod_pair_first: dict[tuple[str, str], str] = {}
    mod_first_idx: dict[str, int] = {}
    # Seed pre-window state: existing module pairs are not "first contact",
    # established modules get no grace period.
    for a, b in baseline_edges:
        na, nb = normalize(a, strips), normalize(b, strips)
        graph.add(na, nb)
        mod_a, mod_b = get_module(na, depth), get_module(nb, depth)
        mod_first_idx.setdefault(mod_a, -GRACE_PERIOD_COMMITS)
        mod_first_idx.setdefault(mod_b, -GRACE_PERIOD_COMMITS)
        if mod_a != mod_b:
            mod_pair_first[(mod_a, mod_b)] = 'baseline'
    # (event_idx, modA, modB) → filled with levels after final graph is built
    event_records: list[dict] = []
    repo_dir = os.path.join(OSS_DIR, local_name)
    touched_cache: dict[str, set[str] | None] = {}
    backedge_cache: dict[tuple[str, str, str], bool | None] = {}

    for idx, rec in enumerate(records):
        sha = rec['sha']
        date = rec['date']
        added_list = rec.get('added', [])

        if len(added_list) > MASS_TOUCH_THRESHOLD:
            for s in added_list:
                edge = parse_edge_line(s)
                if edge:
                    graph.add(normalize(edge[0], strips), normalize(edge[1], strips))
            continue

        commit_pairs: set[tuple[str, str]] = set()
        for s in added_list:
            edge = parse_edge_line(s)
            if not edge:
                continue
            if is_excluded(edge[0]) or is_excluded(edge[1]):
                continue
            from_f = normalize(edge[0], strips)
            to_f = normalize(edge[1], strips)

            mod_a = get_module(from_f, depth)
            mod_b = get_module(to_f, depth)
            if mod_a == mod_b:
                continue
            commit_pairs.add((mod_a, mod_b))

            for mod in (mod_a, mod_b):
                mod_first_idx.setdefault(mod, idx)

            if (idx - mod_first_idx[mod_a] < GRACE_PERIOD_COMMITS
                    or idx - mod_first_idx[mod_b] < GRACE_PERIOD_COMMITS):
                continue

            if not are_siblings(mod_a, mod_b):
                continue

            # Pair must be genuinely new: absent from the replay graph (covers
            # baseline + every prior commit INCLUDING mass-touch ones, whose
            # edges enter the graph without passing through event detection)
            # and not already emitted this window (mod_pair_first dedups).
            pair = (mod_a, mod_b)
            if pair in mod_pair_first or pair in graph.mod_edges:
                continue

            if graph.is_shared(mod_b):
                continue

            if graph.looks_like_move(from_f, to_f):
                continue

            if Path(to_f).name.lower() in _SYSTEM_BASENAMES:
                continue

            # Resolution side-effect guard: a "new" edge from a file the commit
            # never touched means a same-basename header appeared elsewhere and
            # got re-resolved (FastLED.h -> examples/.../OctoWS2811.h).
            if sha not in touched_cache:
                touched_cache[sha] = (touched_files(repo_dir, sha)
                                      if os.path.isdir(repo_dir) else None)
            touched = touched_cache[sha]
            if touched is not None and edge[0] not in touched:
                continue

            rev_pair = (mod_b, mod_a)
            i_a = graph.instability(mod_a)
            i_b = graph.instability(mod_b)

            # A commit adding both directions at once creates the cycle as a
            # whole, so the reverse pair counts even before the graph update.
            # A pre-existing replay back-edge must be confirmed against real
            # source (mod_edges can hold phantoms — see confirm_backedge);
            # an unverifiable result (None) keeps the replay verdict.
            if rev_pair in commit_pairs:
                grade = 'LATERAL.CYCLE'
            elif (rev_pair in graph.mod_edges and confirm_backedge(
                    repo_dir, sha, mod_a, mod_b, depth, strips, backedge_cache) is not False):
                grade = 'LATERAL.CYCLE'
            elif i_b > i_a + SDP_DELTA:
                grade = 'LATERAL.SDP'
            else:
                grade = 'LATERAL.NEW'

            event_records.append({
                'repo': gh_repo,
                'sha': sha[:12],
                'date': date,
                'author_kind': '',          # per-commit, filled below
                'repo_kind': repo_kind,
                'grade': grade,
                'moduleA': mod_a,
                'moduleB': mod_b,
                'FID_B': graph.fid.get(mod_b, 0),
                'level_A': -1,   # filled after full scan
                'level_B': -1,
                'I_A': round(i_a, 3),
                'I_B': round(i_b, 3),
                'example_edge': f'{from_f} -> {to_f}',
            })
            mod_pair_first[pair] = sha

        for s in added_list:
            edge = parse_edge_line(s)
            if edge:
                graph.add(normalize(edge[0], strips), normalize(edge[1], strips))

    if not event_records:
        return []

    # Fill Lakos levels from final graph (approximate: end-of-window levels)
    levels = graph.lakos_level()
    author_cache: dict[str, str] = {}
    for ev in event_records:
        ev['level_A'] = levels.get(ev['moduleA'], 0)
        ev['level_B'] = levels.get(ev['moduleB'], 0)
        sha = ev['sha']
        if sha not in author_cache:
            author_cache[sha] = (classify_commit_author(repo_dir, sha)
                                 if os.path.isdir(repo_dir) else 'unknown')
        ev['author_kind'] = author_cache[sha]

    return event_records


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--jsonl-dir',
        default='/home/localadm/projects/cpparch/experiments/ai_repo_run',
    )
    parser.add_argument(
        '--master',
        default='/home/localadm/projects/cpparch/experiments/ai_repo_run/cpp_master.tsv',
    )
    parser.add_argument('--baseline-dir', default='')
    parser.add_argument('--output', default='lateral_drift_new.csv')
    parser.add_argument('--limit', type=int, default=0, help='limit repos (0=all)')
    args = parser.parse_args()
    baseline_dir = args.baseline_dir or os.path.join(args.jsonl_dir, 'baselines')

    agentic_map: dict[str, int] = {}
    try:
        with open(args.master) as f:
            reader = csv.DictReader(f, delimiter='\t')
            for row in reader:
                agentic_map[row['repo']] = int(row['agentic'])
    except FileNotFoundError:
        print(f'Warning: {args.master} not found', file=sys.stderr)

    jsonl_files = sorted(glob.glob(os.path.join(args.jsonl_dir, '*_graph_drift.jsonl')))
    if args.limit:
        jsonl_files = jsonl_files[: args.limit]

    print(f'Processing {len(jsonl_files)} repos...', file=sys.stderr)

    all_events: list[dict] = []
    grade_counts: collections.Counter = collections.Counter()
    no_baseline = 0
    scanned = 0

    for i, jf in enumerate(jsonl_files):
        if i % 50 == 0:
            print(f'  {i}/{len(jsonl_files)} repos, {len(all_events)} events', file=sys.stderr)
        evts = scan_repo(jf, agentic_map, baseline_dir)
        if evts is None:
            no_baseline += 1
            continue
        scanned += 1
        all_events.extend(evts)
        for e in evts:
            grade_counts[e['grade']] += 1

    out_path = os.path.join(args.jsonl_dir, args.output)
    with open(out_path, 'w', newline='', encoding='utf-8') as f:
        if all_events:
            writer = csv.DictWriter(f, fieldnames=list(all_events[0].keys()))
            writer.writeheader()
            writer.writerows(all_events)

    print(f'\nDone. Scanned {scanned} repos ({no_baseline} skipped: no baseline).',
          file=sys.stderr)
    print(f'{len(all_events)} LATERAL events:', file=sys.stderr)
    for grade, cnt in sorted(grade_counts.items()):
        print(f'  {grade}: {cnt}', file=sys.stderr)
    print(f'Output: {out_path}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
