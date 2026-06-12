#!/usr/bin/env python3
"""Один baseline -> одна TSV-строка метрик: max_fanin, god_header, cyclic_sccs, mutual, chain, nodes."""
import sys
import os
sys.path.insert(0, "/home/localadm/projects/cpparch/experiments/ai_repo_run")
import graph_probe as gp

gb = sys.argv[1]
try:
    nodes, edges = gp.parse(gb)
except Exception:
    print("0\t\t0\t0\t0\t0")
    sys.exit()
n = len(nodes)
if n == 0:
    print("0\t\t0\t0\t0\t0")
    sys.exit()
succ = [[] for _ in range(n)]
indeg = [0] * n
eset = set()
for a, b in edges:
    succ[a].append(b)
    indeg[b] += 1
    eset.add((a, b))
comps = gp.tarjan(n, succ)
comp_id = [0] * n
for cid, comp in enumerate(comps):
    for v in comp:
        comp_id[v] = cid
cyclic = sum(1 for c in comps if len(c) > 1 or (len(c) == 1 and c[0] in succ[c[0]]))
mutual = sum(1 for (a, b) in eset if a < b and (b, a) in eset)
fi = max(range(n), key=lambda i: indeg[i])
try:
    chain = len(gp.longest_chain(n, succ, comp_id, len(comps)))
except Exception:
    chain = 0
print(f"{indeg[fi]}\t{os.path.basename(nodes[fi])}\t{cyclic}\t{mutual}\t{chain}\t{n}")
