#!/usr/bin/env python3
# Собирает VERIFICATION_REPORT_R2.md из round-2 вердиктов (#066 round2).
# Вердикты переехали в experiments/verification/round2_verdicts/ (#070, 2026-06-02).
import json, glob
HERE='/home/localadm/projects/cpparch/experiments/ai_repo_run'
RES=f'{HERE}/../verification/round2_verdicts'; OUT=f'{HERE}/VERIFICATION_REPORT_R2.md'
FIXES=f'{HERE}/round2_new_fixes.md'

recs=[]
for f in sorted(glob.glob(f'{RES}/*.json')):
    try: recs.append(json.load(open(f)))
    except Exception: pass

n=len(recs)
conf=sum(len(r.get('confirmed',[])) for r in recs)
fp=sum(len(r.get('false_positives',[])) for r in recs)
reps_conf=sum(1 for r in recs if r.get('confirmed'))
cyc_intro=[(r['repo'],c) for r in recs if r.get('cycle') for c in (r['cycle'].get('intro_commits') or [])]
cyc_present=[r for r in recs if (r.get('cycle') or {}).get('present')]
klass={}
for r in recs:
    for x in r.get('false_positives',[]): klass[x.get('fp_class','?')]=klass.get(x.get('fp_class','?'),0)+1

L=[]
L.append('# Глазная верификация дрейфа — ROUND2 (#066, расширенный охват)\n')
L.append(f'Проверено агентами **{n} drift-реп**, расширенный охват коммитов (3×, exclude round1) + археология циклов.\n')
L.append('## Агрегат (НОВОЕ сверх round1)\n')
L.append(f'- НОВЫХ подтверждённых копипаст-случаев: **{conf}** (в {reps_conf} репах)')
L.append(f'- НОВЫХ false positives: **{fp}**')
L.append(f'- Cycle-intro коммитов (внёсших include-цикл/рост SCC): **{len(cyc_intro)}** в {len(cyc_present)} репах')
L.append(f'- Классы новых FP: ' + ', '.join(f'{k}={v}' for k,v in sorted(klass.items(), key=lambda x:-x[1])) + '\n')

L.append('## Cycle-intro коммиты (археология — когда/чем внесён цикл)\n')
for repo,c in sorted(cyc_intro, key=lambda t: t[0]):
    L.append(f'- **{repo}** `{c.get("sha","?")}` — {c.get("edge","?")} — {c.get("reason","")}')
L.append('')

L.append('## Новые подтверждённые копипаст-случаи\n')
for r in recs:
    cs=r.get('confirmed') or []
    if not cs: continue
    L.append(f'### {r["repo"]} — {r.get("commits_checked","?")} коммитов')
    for c in cs:
        L.append(f'- [{c.get("klass","?")}] `{c.get("where","?")}` @ `{c.get("sha","?")}` — {c.get("note","")}')
    L.append('')

open(OUT,'w').write('\n'.join(L))

# отдельный файл с новыми фиксами (по классам) для долива в #070
F=['## Новые предложения по фиксам FP (из round2)\n']
byk={}
for r in recs:
    for x in r.get('false_positives',[]):
        fpx=x.get('fix_proposal','').strip()
        if fpx: byk.setdefault(x.get('fp_class','other'),[]).append(fpx)
for k in sorted(byk, key=lambda x:-len(byk[x])):
    F.append(f'**{k}** ({len(byk[k])}):')
    for p in byk[k]: F.append(f'- фикс: {p}')
    F.append('')
open(FIXES,'w').write('\n'.join(F))
print(f'VERIFICATION_REPORT_R2.md: {n} реп, {conf} новых copypaste, {fp} новых FP, {len(cyc_intro)} cycle-intro')
print(f'round2_new_fixes.md: {sum(len(v) for v in byk.values())} фиксов по {len(byk)} классам')
