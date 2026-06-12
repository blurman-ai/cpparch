#!/usr/bin/env python3
"""Авторский копипаст: archcheck --duplication, затем выкинуть пары, где ХОТЯ БЫ одна сторона
в вендоренном пути. Показывает raw vs authored и топ авторских пар с TYPE."""
import os
import re
import subprocess
import sys

AC = "/home/localadm/projects/cpparch/build/debug/src/archcheck"

VENDOR = re.compile(
    r"(^|/)(JUCE|tracktion|third[_-]?party|thirdparty|vendor(ed)?|external|extern|deps|"
    r"lib/|libs/|SDK|submodules?|onnx|QNN|libvorbis|libogg|AAX|VST3?_?SDK|SDL2?|imgui|"
    r"boost|eigen|opencv|googletest|gtest|catch2|zlib|libchd|lzma|spdlog|fmt|nlohmann|asio|"
    r"trilinos|kokkos|tpetra|teuchos|ROL_|Tpetra_|Kokkos_|Teuchos_|ggml|llama\.cpp|whisper\.cpp|"
    r"node_modules|\.deps|build/|cmake-build|_deps)(/|\.|_|$)", re.I)

PAIR = re.compile(r"^\s*(\S+?):[\d-]+\s+<->\s+(\S+?):[\d-]+\s+\((\w+)")


def authored(p):
    return not VENDOR.search(p)


def scan(repo_dir, timeout=300):
    try:
        r = subprocess.run([AC, "--duplication", repo_dir], capture_output=True,
                           text=True, timeout=timeout)
    except Exception:
        return None
    raw = 0
    auth = []
    for line in r.stdout.splitlines():
        m = PAIR.match(line)
        if not m:
            continue
        raw += 1
        a, b, typ = m.group(1), m.group(2), m.group(3)
        if authored(a) and authored(b):
            auth.append((a, b, typ, line.strip()))
    return raw, auth


def main():
    repos = sys.argv[1:]
    if not repos:
        root = "/home/localadm/oss/_agentic_new"
        repos = [os.path.join(root, d) for d in sorted(os.listdir(root))
                 if os.path.isdir(os.path.join(root, d))]
    print(f"{'repo':40} {'raw':>5} {'authored':>9}  (вендор отфильтрован)")
    print("-" * 70)
    out = open("/home/localadm/projects/cpparch/experiments/ai_repo_run/filtered_dup.txt", "w")
    for rd in repos:
        name = os.path.basename(rd)
        res = scan(rd)
        if res is None:
            print(f"{name:40} timeout/fail")
            continue
        raw, auth = res
        # типы авторских
        types = {}
        for _a, _b, t, _l in auth:
            types[t] = types.get(t, 0) + 1
        tstr = " ".join(f"{k}:{v}" for k, v in sorted(types.items(), key=lambda x: -x[1]))
        print(f"{name:40} {raw:>5} {len(auth):>9}  [{tstr}]")
        out.write(f"\n#### {name}  raw={raw} authored={len(auth)} ####\n")
        for a, b, t, line in auth[:12]:
            out.write("  " + line + "\n")
    out.close()
    print("\nтоп авторских пар -> filtered_dup.txt")


if __name__ == "__main__":
    main()
