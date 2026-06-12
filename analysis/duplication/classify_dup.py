#!/usr/bin/env python3
"""Классификация копипаст-пар по природе (отчётный уровень, тул не трогаем). v3.

Три ортогональные оси, сведённые в приоритетную лестницу. Для каждой пары фрагментов:
  generated — кодогенерёнка (никто не правит) — FP
  vendored  — путь (BSP/HAL/known-lib) ИЛИ копирайт НЕ на владельца репы — не код автора
  file_dup  — тот же файл скопирован: совпал basename ИЛИ структурный twin
              (chip/platform/vendor) — это «дублирование файла», НЕ копипаст куска
  authored  — фрагмент в РАЗНОИМЁННОМ не-вендорном файле = чистый авторский копипаст (нижняя оценка)

Приоритет сверху вниз: generated > vendored > file_dup > authored.
"""
import csv
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

OSS = "/home/localadm/oss"
AC = "/home/localadm/projects/cpparch/build/debug/src/archcheck"
HERE = "/home/localadm/projects/cpparch/experiments/ai_repo_run"
OUT = os.path.join(HERE, "classified_dup.tsv")
SAMPLES = os.path.join(HERE, "authored_samples.txt")
PAR = 6

PAIR = re.compile(r"^\s+(\S+?):\d+-\d+\s+<->\s+(\S+?):\d+-\d+\s+\((\w+)")
CR_LINE = re.compile(r"copyright[^\n]*", re.I)

GENERATED = (".pb.", "_generated", ".generated", "/generated/", "moc_", "/ui_", "qrc_", ".tab.c",
             "lex.yy", "_wrap.c", "_wrap.cxx", ".g.cpp", "cython", ".pyx", "/__generated", ".pb-c.")

VENDOR_DIRS = {"thirdparty", "3rdparty", "third_party", "vendor", "vendored", "vendors", "external",
               "externals", "extern", "deps", "dependencies", "submodules", "submodule", "node_modules",
               "nodemodules", "contrib", "3rd_party", "ext", "bsp"}
VENDOR_SUBSTR = ("/hal/", "_hal_", "_hal.", "/cmsis", "freertos", "mbedtls", "mbed-tls", "/coreclr/",
                 "bcl2fastq", "tree-sitter", "treesitter", "platform/ext/", "/boost/", "/eigen/",
                 "/abseil", "/absl/", "/zlib", "/openssl/", "/lwip", "drivers/net/wireless/",
                 "drivers/staging/", "/arch/arm/", "/arch/arm64/", "/arch/x86/", "/arch/riscv/",
                 "/board/", "/soc/", "/u-boot", "/uboot", "/linux-", "/kernel/")
VENDOR_STEMS = ("qcustomplot", "sqlite3", "miniz", "tinyxml", "pugixml", "lodepng", "cjson", "json",
                "httplib", "doctest", "catch", "glad", "entt", "miniaudio", "uni_algo", "vulkan",
                "nuklear", "lua", "zlib", "libpng", "jpeg", "freetype", "expat", "pcre", "zstd", "lz4",
                "curl", "libuv", "stb", "imgui", "glfw", "glm", "fmt", "spdlog", "gtest", "gmock",
                "benchmark", "flatbuffers", "nanopb", "yaml", "yyjson", "rapidjson", "utf8", "base64",
                "sha256", "sha1", "md5", "xxhash", "wepoll", "inih", "ini", "toml", "concurrentqueue",
                "robin_hood", "ankerl", "khash", "uthash", "klib", "kvec")
VENDOR_PREFIX = ("stb_", "imgui", "lib", "ggml", "stm32", "nrf", "esp_", "freertos")

PLATFORM_TOKENS = {"sse2", "sse3", "ssse3", "sse41", "sse42", "sse4", "sse", "avx512", "avx2", "avx",
                   "neon", "sve", "scalar", "altivec", "vsx", "mmx", "x86_64", "x86", "amd64", "i686",
                   "i386", "aarch64", "arm64", "arm32", "arm", "ppc64", "ppc", "powerpc", "riscv64",
                   "riscv", "mips", "wasm", "s390x", "loongarch", "macosx", "macos", "darwin", "osx",
                   "mac", "linux", "win64", "win32", "win", "windows", "android", "ios", "freebsd",
                   "openbsd", "netbsd", "bsd", "posix", "unix", "qnx", "emscripten", "wasi", "msvc",
                   "gcc", "clang", "generic", "common", "portable"}
SILICON_VENDORS = {"broadcom", "mellanox", "marvell", "qualcomm", "nvidia", "amd", "nephos", "clounix",
                   "centec", "barefoot", "innovium", "cavium", "nxp", "microchip", "atmel", "espressif",
                   "allwinner", "rockchip", "amlogic", "mediatek", "realtek", "atheros", "intel", "stm",
                   "st", "ti", "renesas", "infineon", "xilinx", "altera", "lattice", "ftdi"}
_chip = re.compile(r"stm32[a-z0-9]+|mt\d{2,}[a-z0-9]*|nrf\d+[a-z0-9]*|esp32[a-z0-9]*|samd\d+[a-z0-9]*"
                   r"|rp2040|xr\d+[a-z]*|qca\d+[a-z]*|rtl\d+[a-z]*|bcm\d+[a-z]*")


def baseN(p):
    return p.rsplit("/", 1)[-1]


def norm_variant(path):
    p = _chip.sub("CHIP", path.lower())
    return "/".join("PLAT" if s in PLATFORM_TOKENS or s in SILICON_VENDORS else s for s in p.split("/"))


def seg_is_variant(seg):
    s = seg.lower()
    return any(c.isdigit() for c in s) or s in PLATFORM_TOKENS or s in SILICON_VENDORS


def is_twin(a, b):
    if a == b:
        return False
    sa, sb = a.split("/"), b.split("/")
    if len(sa) != len(sb):
        return baseN(a) == baseN(b) and norm_variant(a) == norm_variant(b)
    diff = [(x, y) for x, y in zip(sa, sb) if x != y]
    return bool(diff) and all(seg_is_variant(x) and seg_is_variant(y) for x, y in diff)


def has_vendor_path(path):
    low = path.lower()
    segs = set(s.replace("-", "").replace("_", "") for s in low.split("/")[:-1])
    if segs & VENDOR_DIRS or any(s in low for s in VENDOR_SUBSTR):
        return True
    base = baseN(low)
    if base.split(".", 1)[0] in VENDOR_STEMS:
        return True
    return any(base.startswith(v) for v in VENDOR_PREFIX)


# Копирайт срабатывает ТОЛЬКО на известных вендорах по имени (word-boundary), а не на
# «отсутствии owner» — иначе личное имя автора (open5gs: "Copyright by Sukchan Lee")
# ложно метит авторскую репу как вендор (капкан #081).
CR_VENDORS = ("stmicroelectronics", "mediatek", "qualcomm", "broadcom", "nvidia", "clounix", "nephos",
              "centec", "marvell", "realtek", "atheros", "microchip", "espressif", "allwinner",
              "rockchip", "amlogic", "renesas", "infineon", "xilinx", "freescale", "cypress",
              "instant802", "devicescape", "ralink", "mellanox", "cavium", "innovium", "barefoot",
              "texas instruments", "analog devices", "silicon labs", "synopsys", "cadence", "intel corporation",
              "advanced micro devices", "samsung", "huawei", "hisilicon", "rohm", "nordic semiconductor")
_cr_re = re.compile(r"\b(" + "|".join(re.escape(v) for v in CR_VENDORS) + r")\b")


def cr_foreign(abspath, owner, cache):
    if abspath in cache:
        return cache[abspath]
    res = False
    try:
        with open(abspath, "rb") as fh:
            head = fh.read(8000).decode("latin-1", "ignore").lower()
        for line in CR_LINE.findall(head):
            v = _cr_re.search(line)
            if v:
                vn = v.group(1).replace(" ", "")
                if vn not in owner and owner not in vn:  # вендор назван, и это НЕ сам owner репы
                    res = True
                    break
    except Exception:
        res = False
    cache[abspath] = res
    return res


def classify(a, b, owner, repo_root, crc):
    la, lb = a.lower(), b.lower()
    if any(m in la or m in lb for m in GENERATED):
        return "generated"
    if has_vendor_path(a) or has_vendor_path(b):
        return "vendored"
    if cr_foreign(os.path.join(repo_root, a), owner, crc) or cr_foreign(os.path.join(repo_root, b), owner, crc):
        return "vendored"
    if baseN(a) == baseN(b) or is_twin(a, b):
        return "file_dup"
    return "authored"


def run(repo):
    root = os.path.join(OSS, repo)
    owner = re.sub(r"[^a-z0-9]", "", repo.split("_", 1)[0].lower())
    try:
        out = subprocess.run([AC, "--duplication", root], capture_output=True, text=True, timeout=480).stdout
    except Exception:
        return repo, {}, []
    counts = {"generated": 0, "vendored": 0, "file_dup": 0, "authored": 0}
    crc, samples = {}, []
    for ln in out.splitlines():
        m = PAIR.match(ln)
        if not m:
            continue
        cls = classify(m.group(1), m.group(2), owner, root, crc)
        counts[cls] += 1
        if cls == "authored" and len(samples) < 5:
            samples.append(f"{m.group(1)} <-> {m.group(2)} ({m.group(3)})")
    return repo, counts, samples


def main():
    keys = []
    for r in csv.DictReader(open(os.path.join(HERE, "recensus.tsv")), delimiter="\t"):
        if r["dir"] == "_cpp_archcheck":
            continue
        try:
            if int(r["dup_pairs"]) > 0 and os.path.isdir(os.path.join(OSS, r["dir"])):
                keys.append(r["dir"])
        except ValueError:
            pass
    done = set()
    if os.path.exists(OUT):
        for ln in open(OUT, encoding="utf-8"):
            done.add(ln.split("\t")[0])
    keys = [k for k in keys if k not in done]
    print(f"к классификации (дозапуск): {len(keys)} реп, уже готово {len(done) - 1}", flush=True)
    new = not os.path.exists(OUT)
    f = open(OUT, "a", encoding="utf-8")
    if new:
        f.write("dir\ttotal\tgenerated\tvendored\tfile_dup\tauthored\n")
    sf = open(SAMPLES, "a", encoding="utf-8")
    n = 0
    with ThreadPoolExecutor(max_workers=PAR) as ex:
        futs = {ex.submit(run, k): k for k in keys}
        for fut in as_completed(futs):
            repo, c, samples = fut.result()
            if not c:
                continue
            tot = sum(c.values())
            f.write(f"{repo}\t{tot}\t{c['generated']}\t{c['vendored']}\t{c['file_dup']}\t{c['authored']}\n")
            f.flush()
            if samples:
                sf.write(f"### {repo} (authored={c['authored']})\n" + "\n".join(samples) + "\n\n")
                sf.flush()
            n += 1
            if n % 50 == 0:
                print(f"  {n}/{len(keys)}", flush=True)
    f.close()
    sf.close()
    print(f"ГОТОВО -> {OUT}", flush=True)


if __name__ == "__main__":
    main()
