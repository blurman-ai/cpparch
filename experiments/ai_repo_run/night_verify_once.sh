#!/usr/bin/env bash
# #066/#067: ОДНОРАЗОВАЯ обёртка ночной верификации. Снимает свою cron-запись
# при старте (чтобы не сработать повторно), затем зовёт night_verify.sh.
export HOME=~
export PATH=~/.local/bin:/usr/local/bin:/usr/bin:/bin
HERE=~/projects/cpparch/experiments/ai_repo_run

# самоудаление cron.d-записи (one-shot): убираем файл до запуска работы
sudo -n rm -f /etc/cron.d/airepo_night_verify 2>/dev/null

echo "=== night_verify_once запуск $(date '+%F %T') (cron.d-запись снята) ===" >> "$HERE/night_verify.log"
exec /bin/bash "$HERE/night_verify.sh"
