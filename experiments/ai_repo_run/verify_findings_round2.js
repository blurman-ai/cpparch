export const meta = {
  name: 'verify-drift-round2',
  description: 'Round2: 3x охват drift-коммитов (exclude round1) + археология коммитов, вносящих include-циклы/сложность графа. Пишет verify_results2/<repo>.json',
  phases: [
    { title: 'Discover', detail: 'drift-репы из summary.tsv' },
    { title: 'Audit2', detail: 'агент/репа: 15-30 коммитов копипаста + cycle-intro археология' },
  ],
}

const HERE = '~/projects/cpparch/experiments/ai_repo_run'
const ARCH = '~/projects/cpparch/build/debug/src/archcheck'
const DETAIL = `${HERE}/CORPUS_CHECK_DETAIL.md`
const TSV = `${HERE}/corpus_check_summary.tsv`
const RESULTS = `${HERE}/verify_results2`
const STATE = `~/oss/_aidev_state`
const EXCLUDE = `${STATE}/round1_shas.txt`

const REPOS_SCHEMA = {
  type: 'object',
  properties: {
    repos: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          repo: { type: 'string' }, dir: { type: 'string' },
          sccs: { type: 'integer' }, cc: { type: 'integer' },
        },
        required: ['repo', 'dir', 'sccs', 'cc'],
      },
    },
  },
  required: ['repos'],
}

const VERDICT_SCHEMA = {
  type: 'object',
  properties: {
    repo: { type: 'string' },
    commits_checked: { type: 'integer' },
    confirmed: { type: 'integer', description: 'НОВЫХ подтверждённых случаев копипаста (не из round1)' },
    fp: { type: 'integer', description: 'НОВЫХ ложных срабатываний' },
    cycle_intro_commits: { type: 'integer', description: 'коммитов, реально вносящих include-цикл/рост SCC' },
    cycle_confirmed: { type: 'boolean' },
    note: { type: 'string' },
  },
  required: ['repo', 'commits_checked', 'confirmed', 'fp', 'cycle_intro_commits', 'cycle_confirmed'],
}

phase('Discover')
const disc = await agent(
  `Прочитай TSV ${TSV} (колонки: repo, commits_postmay, copypaste_hits, within, cross, sccs_cyclic, largest_scc, verdict). ` +
  `Верни ВСЕ строки с verdict=="drift" как объекты {repo, dir, sccs, cc}, где ` +
  `dir = "~/oss/_aidev_dense/" + repo с первым "/" заменённым на "_", sccs=sccs_cyclic, cc=commits_postmay. ` +
  `Используй Bash/Read. Это весь список для аудита.`,
  { schema: REPOS_SCHEMA, label: 'discover-drift' }
)
const allRepos = (disc && disc.repos) ? disc.repos : []

await agent(`Bash: mkdir -p ${RESULTS}`, { label: 'mkdir-results2' })

// RESUME: пропустить репы, по которым уже есть verify_results2/<repo>.json (переживает reboot)
const doneRes = await agent(
  `Bash: \`ls ${RESULTS}/*.json 2>/dev/null | xargs -r -n1 basename | sed 's/\\.json$//'\` (может быть пусто). ` +
  `Верни JSON {done:[...]} — список basenames без .json (это repo с '/' заменённым на '_').`,
  { schema: { type: 'object', properties: { done: { type: 'array', items: { type: 'string' } } }, required: ['done'] }, label: 'resume-scan' }
)
const doneSet = new Set((doneRes && doneRes.done) ? doneRes.done : [])
const repos = allRepos.filter((r) => !doneSet.has(r.repo.replace('/', '_')))
log(`drift-реп всего ${allRepos.length}, уже сделано ${doneSet.size}, к аудиту осталось ${repos.length}`)

phase('Audit2')
const auditPrompt = (r) => {
  const safe = r.repo.replace('/', '_')
  const depth = r.cc > 1500 ? 30 : 15
  return (
`Ты аудитор копипаста и архитектурного дрейфа в AI-написанном C++ репозитории "${r.repo}" (dir: ${r.dir}).
Это ВТОРОЙ проход (round2): нужен РАСШИРЕННЫЙ охват, БЕЗ повтора уже проверенного в round1.

EXCLUDE: в файле ${EXCLUDE} — SHA коммитов, по которым round1 уже дал находки. НЕ аудируй
коммит, чей хэш совпадает (по префиксу, в любую сторону) с любой строкой оттуда. Прочитай его
(Read/Bash) и держи как стоп-лист.

=== ЧАСТЬ A: копипаст кусков (РАСШИРЕННО) ===
ЦЕЛЬ: реальные случаи, где КУСОК кода (функция/блок) скопирован и частично изменён — ВНУТРИ
файла (главный интерес) или между файлами. Игнорируй копии целых вендорных файлов и генерёнку
(protobuf/moc/ggml — шапка-маркер).

Для КАЖДОГО FP опиши (а) ЧТО именно обмануло детектор по реальному коду (бойлерплейт класса?
header↔impl? data-таблица? идиома? общий словарь?), и (б) КОНКРЕТНЫЙ фикс #056/archcheck
(порог/фильтр/эвристика). FP без разбора и фикса не пиши.

Шаги A:
A1. Подсказки чекера: \`grep -A60 '^## ${r.repo} —' ${DETAIL}\` — пары ADDED⟵BASE + SHA. Проверь их.
A2. Отбери ${depth} коммитов для проверки — ШИРОКО ПО ИСТОРИИ, не только топ-жирные (их брал
    round1). Считай жирность ТОЛЬКО по C++-строкам (.cpp/.cc/.h/.hpp/.cxx):
    \`git -C ${r.dir} log --since=2025-05-01 --no-merges --numstat --pretty=@@%H\` → сумма add+del
    по C++-файлам. Возьми коммиты РАЗНЫХ рангов (средние и крупные, разбросанные по времени),
    ПРОПУСКАЯ всё из EXCLUDE. Цель — НОВЫЕ коммиты, которых не было в round1.
A3. Для каждого: \`git -C ${r.dir} show <sha> -- '*.cpp' '*.cc' '*.h' '*.hpp'\`. Ищи перенос
    кусков (почти одинаковые функции/блоки с переименованными id) и ЧАСТИЧНЫЕ совпадения.

${r.sccs > 0 ? `=== ЧАСТЬ B: археология циклов / роста сложности графа ===
ЦЕЛЬ: найти КОММИТЫ, которые ВНЕСЛИ include-цикл или нарастили сложность графа, и проверить их.
B1. \`${ARCH} --graph ${r.dir}\` — подтверди sccs_cyclic>0, выпиши файлы циклического SCC и
    рёбра-include, ЗАМЫКАЮЩИЕ цикл (A.h включает B.h, B.h обратно A.h).
B2. Для КАЖДОГО замыкающего ребра найди коммит, добавивший этот include:
    \`git -C ${r.dir} log --oneline -S'#include "<имя>"' -- <файл-замыкатель>\` (или blame строки include).
    Это и есть cycle-introducing commit. Прочитай его (\`git show\`), пойми ПОЧЕМУ внесён
    (рефактор-сплит монолита? back-compat include? круговая зависимость по неосторожности?).
B3. Проверь, что цикл в АВТОРСКОМ коде, не в вендоренной под нестандартным именем библиотеке
    (imgui/MacKernelSDK/фрагмент движка) — если вендор, present=false, в evidence укажи папку.
B4. Если SCC размера >2 — отметь, рос ли он постепенно (несколько коммитов добавляли рёбра).` : '=== ЧАСТЬ B: пропущена (sccs_cyclic=0) ==='}

Запиши findings в ${RESULTS}/${safe}.json (Write tool):
{ "repo": "${r.repo}", "commits_checked": <N>,
  "confirmed": [ {"sha":"...","klass":"within-file-chunk|cross-file-chunk|partial-edited","where":"file:line-line","note":"<фраза>"} ],
  "false_positives": [ {"sha":"...","where":"file:line","fp_class":"coincidental|whole-file|generated|header-impl|data-table|idiom|other",
     "what_happened":"<что обмануло детектор>","fix_proposal":"<конкретный фикс #056/archcheck>"} ],
  "cycle": ${r.sccs > 0 ? '{"present":<bool>,"desc":"A.h → B.h → A.h","scc_size":<int>,"intro_commits":[{"sha":"...","edge":"A.h adds #include B.h","reason":"<почему внесён>"}],"evidence":"<файлы/вендор-папка>"}' : 'null'} }

Верни структурный итог: {repo, commits_checked, confirmed(число НОВЫХ), fp(число НОВЫХ), cycle_intro_commits(число), cycle_confirmed, note}.`
  )
}

const results = await parallel(
  repos.map((r) => () =>
    agent(auditPrompt(r), { schema: VERDICT_SCHEMA, label: `audit2:${r.repo}`, phase: 'Audit2' })
      .then((v) => v || { repo: r.repo, commits_checked: 0, confirmed: 0, fp: 0, cycle_intro_commits: 0, cycle_confirmed: false, note: 'agent-null' })
      .catch((e) => ({ repo: r.repo, commits_checked: 0, confirmed: 0, fp: 0, cycle_intro_commits: 0, cycle_confirmed: false, note: 'agent-error: ' + String(e && e.message || e) }))
  )
)

const ok = results.filter(Boolean)
const agg = {
  repos_audited: ok.length,
  repos_with_new_copypaste: ok.filter((v) => v.confirmed > 0).length,
  total_new_confirmed: ok.reduce((s, v) => s + (v.confirmed || 0), 0),
  total_new_fp: ok.reduce((s, v) => s + (v.fp || 0), 0),
  cycles_confirmed: ok.filter((v) => v.cycle_confirmed).length,
  cycle_intro_commits: ok.reduce((s, v) => s + (v.cycle_intro_commits || 0), 0),
  commits_checked: ok.reduce((s, v) => s + (v.commits_checked || 0), 0),
}
log(`ROUND2 ГОТОВО: реп=${agg.repos_audited}, коммитов=${agg.commits_checked}, НОВ.копипаст=${agg.total_new_confirmed} (реп ${agg.repos_with_new_copypaste}), НОВ.FP=${agg.total_new_fp}, cycle-intro коммитов=${agg.cycle_intro_commits}, реп с циклом=${agg.cycles_confirmed}`)
return agg
