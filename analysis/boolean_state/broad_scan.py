#!/usr/bin/env python3
"""
BROAD corpus scan — NO name filters (no state-word whitelist, no *Config/*Cache
blacklist). Only correctness kept: depth-0 counting (real struct fields, not
local bools in method bodies). Scans ALL repos in /home/localadm/oss.

Goal: maximum information. Every struct/class with >= MIN_BOOLS real bool fields
is reported, tagged (vendored? state-like names?) but NEVER dropped.
"""
import os
import re
import sys

OSS = "/home/localadm/oss"
MIN_BOOLS = 5

# Informational tags only — NOT used to filter.
STATE_WORDS = {
    "started", "starting", "running", "paused", "stopped", "stopping", "failed",
    "completed", "complete", "cancelled", "canceled", "ready", "active", "inactive",
    "idle", "loaded", "loading", "connected", "connecting", "disconnected",
    "initialized", "pending", "done", "finished", "aborted", "waiting", "busy",
    "opened", "closing", "closed",
}
VENDOR_DIRS = ("/third_party/", "/thirdparty/", "/3rdparty/", "/vendor/", "/vendored/",
               "/extern/", "/external/", "/_deps/", "/deps/", "/tclap/", "/submodules/",
               "/.git/", "/node_modules/")
VENDOR_FILES = ("catch.hpp", "catch_config.hpp", "cgltf.h", "peglib.h", "rapidjson.h",
                "json.hpp", "stb_image.h", "miniaudio.h", "imgui.h")

STRUCT_RE = re.compile(r'(?:struct|class)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::[^{;]*)?\{')
BOOL_RE = re.compile(r'\bbool\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;')


def is_vendored(path):
    p = "/" + path.lower()
    return any(d in p for d in VENDOR_DIRS) or any(p.endswith(f) for f in VENDOR_FILES)


def looks_state(name):
    n = name.lower()
    core = n[2:] if n.startswith("m_") else (n[1:] if n.startswith("_") else n)
    if core.startswith("is_"):
        core = core[3:]
    if core in STATE_WORDS:
        return True
    return any(n.endswith("_" + w) or n.endswith(w) for w in STATE_WORDS)


def depth_zero(body):
    out, depth = [], 0
    for c in body:
        if c == "{":
            depth += 1
        elif c == "}":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(c)
    return "".join(out)


def member_slice(source, start):
    depth, in_body, bstart, bend, i = 0, False, 0, 0, start
    n = len(source)
    while i < n:
        c = source[i]
        if c == "{":
            if not in_body:
                in_body, bstart = True, i + 1
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and in_body:
                bend = i
                break
        i += 1
    if not in_body or bend <= bstart:
        return ""
    return depth_zero(source[bstart:bend])


def analyze(source, path):
    out = []
    for m in STRUCT_RE.finditer(source):
        name = m.group(1)
        members = member_slice(source, m.start())
        if not members:
            continue
        names = []
        for fm in BOOL_RE.finditer(members):
            pos = fm.start()
            ls = members.rfind("\n", 0, pos)
            le = members.find("\n", pos)
            line = members[(ls + 1):(le if le >= 0 else len(members))]
            if "(" in line or ")" in line:
                continue
            names.append(fm.group(1))
        if len(names) >= MIN_BOOLS:
            state = sum(1 for x in names if looks_state(x))
            out.append((name, len(names), state, names, path))
    return out


def main():
    results = []
    repos = sorted(os.listdir(OSS))
    for idx, repo in enumerate(repos, 1):
        rp = os.path.join(OSS, repo)
        if not os.path.isdir(rp):
            continue
        for root, _, files in os.walk(rp):
            for f in files:
                if not f.endswith((".h", ".hpp", ".hh", ".hxx")):
                    continue
                fp = os.path.join(root, f)
                try:
                    with open(fp, "r", encoding="utf-8", errors="ignore") as fh:
                        src = fh.read()
                except OSError:
                    continue
                if "bool" not in src:
                    continue
                results.extend(analyze(src, fp))
        if idx % 50 == 0:
            print(f"[{idx}/{len(repos)}] {repo}... candidates: {len(results)}", flush=True)

    results.sort(key=lambda x: -x[1])
    print(f"\n=== BROAD SCAN: {len(results)} structs with {MIN_BOOLS}+ real bool fields "
          f"across {len(repos)} repos ===\n")

    # Distribution
    from collections import Counter
    dist = Counter(r[1] for r in results)
    print("Distribution by bool-field count:")
    for k in sorted(dist):
        print(f"  {k:3d} bools: {dist[k]}")

    # Write CSV (machine-readable for next analysis) + markdown (clickable)
    csv_path = "/home/localadm/projects/cpparch/experiments/boolean_state/broad_scan.csv"
    with open(csv_path, "w") as fh:
        fh.write("bools,state_like,vendored,struct,file,fields\n")
        for name, n, state, names, path in results:
            fh.write(f'{n},{state},{int(is_vendored(path))},{name},"{path}","{";".join(names)}"\n')

    md = ["# Boolean-State — широкий прогон (без name-фильтров)\n",
          f"**Кандидатов:** {len(results)} структур с {MIN_BOOLS}+ реальными bool-полями "
          f"в {len(repos)} репозиториях.\n",
          "**Фильтры:** только глубина-0 (реальные поля, не локальные переменные). "
          "Name-whitelist и blacklist УБРАНЫ. Колонки `vendor`/`state-like` — пометки, не фильтр.\n",
          "| Bools | State-like | Vendor | Struct | File |",
          "|---|---|---|---|---|"]
    for name, n, state, names, path in results[:150]:
        v = "⚠️" if is_vendored(path) else ""
        link = f"[{os.path.basename(path)}](file://{path})"
        md.append(f"| {n} | {state} | {v} | `{name}` | {link} |")
    md.append(f"\n*(показаны топ-150 из {len(results)}; полный список — broad_scan.csv)*\n")
    md_path = "/home/localadm/projects/cpparch/docs/research/boolean_state_broad_scan.md"
    with open(md_path, "w") as fh:
        fh.write("\n".join(md) + "\n")

    print(f"\nReport:  {md_path}")
    print(f"CSV:     {csv_path}")
    print(f"\nTop 30:")
    for name, n, state, names, path in results[:30]:
        v = " [vendor]" if is_vendored(path) else ""
        print(f"  {n:3d} bool | {state:2d} state | {name:35s}{v}")


if __name__ == "__main__":
    main()
