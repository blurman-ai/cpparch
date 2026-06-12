#!/usr/bin/env python3
"""Text-based local complexity drift scorer for research runs."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


CPP_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
}

TEST_SYMBOLS = {"TEST", "TEST_F", "TEST_P", "TYPED_TEST", "BENCHMARK", "MOCK_METHOD"}
SIGNATURE_RE = re.compile(
    r"(?P<name>(?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*|operator\s*[^\s(]+)\s*\([^;{}]*\)"
)
EXCLUDED_SIGNATURE_NAMES = {
    "if",
    "for",
    "while",
    "switch",
    "catch",
    "return",
    "sizeof",
    "alignof",
    "decltype",
}
VENDORED_DIR_NAMES = {
    "thirdparty",
    "3rdparty",
    "vendor",
    "vendored",
    "vendors",
    "external",
    "externals",
    "extern",
    "deps",
    "dependencies",
    "submodules",
    "submodule",
    "nodemodules",
    "contrib",
}
VENDORED_LIB_DIRS = {
    "jpeglib",
    "libjpeg",
    "libpng",
    "zlib",
    "bzip2",
    "qhull",
    "hidapi",
    "libigl",
    "agg",
    "glulibtess",
    "mcut",
    "freetype",
    "glfw",
    "glew",
}
VENDORED_STEMS = {"qcustomplot", "sqlite3", "miniz", "tinyxml2", "pugixml", "lodepng", "cjson"}
VENDORED_EXACT = {
    "json.hpp",
    "httplib.h",
    "lua.c",
    "doctest.h",
    "catch.hpp",
    "glad.c",
    "entt.hpp",
    "miniaudio.h",
    "uni_algo.h",
    "vulkan.hpp",
    "td_api.h",
    "nuklear.h",
}
TEST_DIR_NAMES = {"test", "tests", "unittest", "unittests"}
LICENSE_MARKERS = {
    "permission is hereby granted",
    "redistribution and use in source",
    "released into the public domain",
    "licensed under the apache",
    "boost software license",
    "zlib license",
}


@dataclass(frozen=True)
class FunctionSpan:
    file: str
    symbol: str
    arity: int
    start_line: int
    end_line: int
    body: str


@dataclass(frozen=True)
class FunctionMetrics:
    file: str
    symbol: str
    arity: int
    start_line: int
    end_line: int
    meaningful_loc: int
    branch_score: int
    max_nesting_depth: int
    deep_lines_count: int
    indent_complexity_sum: int
    local_complexity_score: int


@dataclass(frozen=True)
class DriftFinding:
    repo: str
    sha: str
    parent: str
    file: str
    symbol: str
    old_score: int
    new_score: int
    delta_score: int
    delta_percent: float | None
    changed_loc: int
    branch_delta: int
    nesting_delta: int
    deep_lines_delta: int
    confidence: str
    reason: str


@dataclass(frozen=True)
class CommitSummary:
    repo: str
    sha: str
    parent: str
    changed_files: int
    skipped_files: int
    compared_functions: int
    findings: int
    positive_delta_score: int
    negative_delta_score: int
    max_new_score: int
    max_delta_score: int


def run_git(repo: Path, args: list[str]) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout


def is_cpp_path(path: str) -> bool:
    return Path(path).suffix.lower() in CPP_EXTENSIONS


def normalize_dir_segment(name: str) -> str:
    return "".join(ch.lower() for ch in name if ch not in "_- ")


def path_any_dir_segment(path: str, names: set[str]) -> bool:
    parts = path.replace("\\", "/").split("/")
    return any(normalize_dir_segment(part) in names for part in parts[:-1])


def path_has_vendored_dir(path: str) -> bool:
    names = VENDORED_DIR_NAMES | VENDORED_LIB_DIRS
    return path_any_dir_segment(path, names)


def path_has_test_dir(path: str) -> bool:
    parts = path.replace("\\", "/").split("/")
    for part in parts[:-1]:
        normalized = normalize_dir_segment(part)
        if normalized in TEST_DIR_NAMES or normalized.endswith("tests"):
            return True
    return False


def is_test_basename(path: str) -> bool:
    stem = Path(path).stem.lower()
    return stem.startswith("test_") or stem.endswith(("_test", "_tests", "_spec"))


def is_vendored_basename(path: str) -> bool:
    name = Path(path).name.lower()
    if name.startswith("stb_") or name.startswith("imgui"):
        return True
    stem = Path(name).stem
    return stem in VENDORED_STEMS or name in VENDORED_EXACT


def has_vendor_license_header(text: str) -> bool:
    head = text[:2000].lower()
    return any(marker in head for marker in LICENSE_MARKERS)


def is_author_source(path: str, old_text: str, new_text: str) -> bool:
    if path_has_vendored_dir(path) or path_has_test_dir(path):
        return False
    if is_test_basename(path) or is_vendored_basename(path):
        return False
    return not (has_vendor_license_header(old_text) or has_vendor_license_header(new_text))


def strip_comments_and_literals(text: str) -> str:
    out: list[str] = []
    i = 0
    in_block = False
    in_line = False
    in_string: str | None = None
    raw_end: str | None = None

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if raw_end is not None:
            if text.startswith(raw_end, i):
                out.extend(" " * len(raw_end))
                i += len(raw_end)
                raw_end = None
                continue
            out.append("\n" if ch == "\n" else " ")
            i += 1
            continue

        if in_line:
            out.append("\n" if ch == "\n" else " ")
            if ch == "\n":
                in_line = False
            i += 1
            continue

        if in_block:
            out.append("\n" if ch == "\n" else " ")
            if ch == "*" and nxt == "/":
                out.append(" ")
                i += 2
                in_block = False
            else:
                i += 1
            continue

        if in_string is not None:
            out.append("\n" if ch == "\n" else " ")
            if ch == "\\" and nxt:
                out.append(" ")
                i += 2
                continue
            if ch == in_string:
                in_string = None
            i += 1
            continue

        if text.startswith('R"', i):
            match = re.match(r'R"(?P<delim>[A-Za-z_0-9]*)\(', text[i:])
            if match:
                raw_end = ")" + match.group("delim") + '"'
                out.extend(" " * match.end())
                i += match.end()
                continue

        if ch == "/" and nxt == "/":
            out.extend("  ")
            i += 2
            in_line = True
            continue

        if ch == "/" and nxt == "*":
            out.extend("  ")
            i += 2
            in_block = True
            continue

        if ch in {'"', "'"}:
            out.append(" ")
            in_string = ch
            i += 1
            continue

        out.append(ch)
        i += 1

    return "".join(out)


def line_offsets(text: str) -> list[int]:
    offsets = [0]
    for index, ch in enumerate(text):
        if ch == "\n":
            offsets.append(index + 1)
    return offsets


def line_for_offset(offsets: list[int], pos: int) -> int:
    lo = 0
    hi = len(offsets)
    while lo + 1 < hi:
        mid = (lo + hi) // 2
        if offsets[mid] <= pos:
            lo = mid
        else:
            hi = mid
    return lo + 1


def matching_brace(text: str, open_pos: int) -> int | None:
    depth = 0
    for pos in range(open_pos, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return pos
    return None


def _top_level_arity(signature: str) -> int:
    """Count top-level parameters in a matched `name(...)` signature."""
    open_idx = signature.find("(")
    if open_idx < 0:
        return 0
    depth = 0
    commas = 0
    saw_param = False
    for ch in signature[open_idx:]:
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
            if depth == 0:
                break
        elif ch == "," and depth == 1:
            commas += 1
        elif depth == 1 and not ch.isspace():
            saw_param = True
    return commas + 1 if saw_param else 0


def signature_symbol(prefix: str) -> tuple[str, int] | None:
    compact = " ".join(prefix.strip().split())
    if not compact or compact.startswith(("#", "class ", "struct ", "enum ", "namespace ")):
        return None
    if "=" in compact.rsplit(")", 1)[-1]:
        return None

    matches = list(SIGNATURE_RE.finditer(compact))
    if not matches:
        return None

    match = matches[-1]
    name = match.group("name").replace(" ", "")
    short = name.split("::")[-1]
    if short in EXCLUDED_SIGNATURE_NAMES:
        return None
    if short in TEST_SYMBOLS:
        # GoogleTest/Catch/benchmark macros: every TEST_F(Suite, Name) shares the symbol
        # `TEST_F`, so matching pairs unrelated test bodies (review D6). Drop them.
        return None
    if re.search(r"\b(if|for|while|switch|catch)\s*\([^)]*\)\s*$", compact):
        return None
    return name, _top_level_arity(compact)


def discover_functions(text: str, file_name: str) -> list[FunctionSpan]:
    cleaned = strip_comments_and_literals(text)
    offsets = line_offsets(cleaned)
    spans: list[FunctionSpan] = []
    depth = 0
    statement_start = 0
    pos = 0

    while pos < len(cleaned):
        ch = cleaned[pos]
        if ch == "{":
            prefix = cleaned[statement_start:pos]
            signature = signature_symbol(prefix)
            end = matching_brace(cleaned, pos)
            if signature is not None and end is not None:
                symbol, arity = signature
                start_line = line_for_offset(offsets, pos)
                end_line = line_for_offset(offsets, end)
                body = cleaned[pos + 1 : end]
                spans.append(FunctionSpan(file_name, symbol, arity, start_line, end_line, body))
                pos = end + 1
                statement_start = pos
                depth = 0
                continue
            depth += 1
        elif ch == "}":
            depth = max(0, depth - 1)
            if depth == 0:
                statement_start = pos + 1
        elif ch == ";" and depth == 0:
            statement_start = pos + 1
        pos += 1

    return spans


_TOKEN_RE = re.compile(
    r"(?P<ws>\s+)"
    r"|(?P<word>[A-Za-z_]\w*)"
    r"|(?P<cmp>[<>=!]=)"  # ==, !=, <=, >=: keep `!=` from looking like logical-NOT
    r"|(?P<and>&&)"
    r"|(?P<or>\|\|)"
    r"|(?P<arrow>->)"
    r"|(?P<scope>::)"
    r"|(?P<punct>[(){}\[\];?=!,])"
    r"|(?P<other>.)"
)

STRUCTURAL_KEYWORDS = {"if", "for", "while", "switch", "catch"}
FUNDAMENTAL_KEYWORDS = {"goto", "co_await"}


def _tokenize(body: str) -> list[tuple[str, str, bool]]:
    """Cleaned body -> list of (kind, text, glued_left). glued_left only meaningful for &&/||."""
    tokens: list[tuple[str, str, bool]] = []
    for match in _TOKEN_RE.finditer(body):
        kind = match.lastgroup
        text = match.group()
        if kind == "ws":
            continue
        glued = False
        if kind in {"and", "or"}:
            start = match.start()
            prev = body[start - 1] if start > 0 else " "
            # rvalue-ref (`Type&&`) is glued to the preceding token; logical `&&` is spaced.
            glued = prev.isalnum() or prev == "_" or prev == ">"
        tokens.append((kind, text, glued))
    return tokens


def metric_for_span(span: FunctionSpan) -> FunctionMetrics:
    """Cognitive Complexity per Sonar spec (Campbell 2018), token-based.

    Increments: structural (if/for/while/switch/catch/?:) = +1 + nesting; hybrid
    (else / else if) = +1, no nesting bonus; fundamental (goto/co_await, each series
    of &&/|| , +1 on operator change) = +1. case/default are free, switch is +1 once.
    Nesting counts only control structures and lambdas (classified brace stack).
    """
    tokens = _tokenize(span.body)

    score = 0
    raw_increments = 0  # diagnostic: count of decision points (no nesting weight)
    nesting = 0
    max_nesting = 0
    deep_increments = 0  # diagnostic: increments applied at nesting >= 3

    paren_depth = 0
    brace_stack: list[dict[str, bool]] = []  # frames: {"control": bool, "is_do": bool}
    last_op: dict[int, str | None] = {0: None}  # last logical op per paren depth

    pending_control = False  # next `{`/braceless stmt opens a control body
    pending_is_do = False
    awaiting_header = False  # saw a control keyword, waiting for its `(`
    header_base_depth = 0  # paren depth before the control header opened
    in_header = False
    skip_next_if = False  # `else if`: the `if` was already counted by `else`
    else_armed = False  # saw `else`, deciding between `else if` and plain `else`
    do_tail_pending = False  # a do-body just closed; the next `while` is the tail
    swallow_header = False  # consume the do-while tail `while(...)` header silently
    lambda_pending = False  # saw `]`, a following `{` is a lambda body
    braceless_open = 0  # count of open braceless control bodies, dropped at `;`

    def add(structural: bool) -> None:
        nonlocal score, raw_increments, deep_increments
        score += 1 + (nesting if structural else 0)
        raw_increments += 1
        if nesting >= 3:
            deep_increments += 1

    for kind, text, glued in tokens:
        # Resolve `else` vs `else if` now that we see the following token.
        if else_armed:
            else_armed = False
            if kind == "word" and text == "if":
                skip_next_if = True  # else-if: body pends after the if's header
            else:
                pending_control = True  # plain `else`: its body starts here

        # Open a braceless control body once a real statement token follows the header.
        if pending_control and text != "{":
            nesting += 1
            max_nesting = max(max_nesting, nesting)
            braceless_open += 1
            pending_control = False
            pending_is_do = False

        if kind == "word":
            if text in {"case", "default"}:
                continue
            if text == "while" and (do_tail_pending or swallow_header):
                do_tail_pending = False
                swallow_header = True  # its `(...)` header is consumed as plain parens
                continue
            if skip_next_if and text == "if":
                skip_next_if = False
                awaiting_header = True  # body still pends after the header
                continue
            if text in STRUCTURAL_KEYWORDS:
                add(structural=True)
                awaiting_header = True
                continue
            if text == "do":
                add(structural=True)
                pending_control = True
                pending_is_do = True
                continue
            if text == "else":
                add(structural=False)  # hybrid: +1, no nesting bonus
                else_armed = True  # resolve else / else-if on the next token
                continue
            if text in FUNDAMENTAL_KEYWORDS:
                add(structural=False)
                continue
            continue

        if kind == "and" or kind == "or":
            if glued:  # rvalue-ref, not a logical operator
                continue
            op = "and" if kind == "and" else "or"
            if last_op.get(paren_depth) != op:
                add(structural=False)
                last_op[paren_depth] = op
            continue

        if text == "?":
            add(structural=True)  # ternary; does not raise nesting (rare, ignored)
            continue

        if text == "!":
            last_op[paren_depth] = None  # `!` breaks a logical series
            continue

        if text == "(":
            paren_depth += 1
            last_op[paren_depth] = None
            if awaiting_header:
                in_header = True
                header_base_depth = paren_depth - 1
                awaiting_header = False
            continue

        if text == ")":
            last_op.pop(paren_depth, None)
            paren_depth = max(0, paren_depth - 1)
            if in_header and paren_depth == header_base_depth:
                in_header = False
                if swallow_header:
                    swallow_header = False  # do-while tail header consumed, no body
                else:
                    pending_control = True  # structural-keyword body pends here
            continue

        if text == "[":
            lambda_pending = True
            continue

        if text == "=":
            lambda_pending = False  # `x[] = {…}` is an initializer, not a lambda
            continue

        if text == "{":
            if pending_control:
                brace_stack.append({"control": True, "is_do": pending_is_do})
                nesting += 1
                max_nesting = max(max_nesting, nesting)
                pending_control = False
                pending_is_do = False
            elif lambda_pending:
                brace_stack.append({"control": True, "is_do": False})
                nesting += 1
                max_nesting = max(max_nesting, nesting)
                lambda_pending = False
            else:
                brace_stack.append({"control": False, "is_do": False})
            continue

        if text == "}":
            if brace_stack:
                frame = brace_stack.pop()
                if frame["control"]:
                    nesting = max(0, nesting - 1)
                if frame["is_do"]:
                    do_tail_pending = True
            continue

        if text == ";":
            if paren_depth == 0 and braceless_open:
                nesting = max(0, nesting - braceless_open)
                braceless_open = 0
            continue

    meaningful_loc = sum(
        1 for raw in span.body.splitlines() if raw.strip() and raw.strip() not in {"{", "}"}
    )

    return FunctionMetrics(
        file=span.file,
        symbol=span.symbol,
        arity=span.arity,
        start_line=span.start_line,
        end_line=span.end_line,
        meaningful_loc=meaningful_loc,
        branch_score=raw_increments,
        max_nesting_depth=max_nesting,
        deep_lines_count=deep_increments,
        indent_complexity_sum=0,
        local_complexity_score=score,
    )


def metrics_for_text(text: str, file_name: str) -> list[FunctionMetrics]:
    return [metric_for_span(span) for span in discover_functions(text, file_name)]


def match_metrics(
    old_metrics: Iterable[FunctionMetrics], new_metrics: Iterable[FunctionMetrics]
) -> list[tuple[FunctionMetrics | None, FunctionMetrics, str]]:
    # Key on (symbol, arity): overloads of one name don't cross-pair (review D7).
    old_by_symbol: dict[tuple[str, int], list[FunctionMetrics]] = {}
    for metric in old_metrics:
        old_by_symbol.setdefault((metric.symbol, metric.arity), []).append(metric)

    matches: list[tuple[FunctionMetrics | None, FunctionMetrics, str]] = []
    for new_metric in new_metrics:
        candidates = old_by_symbol.get((new_metric.symbol, new_metric.arity), [])
        if len(candidates) == 1:
            matches.append((candidates[0], new_metric, "high"))
        elif len(candidates) > 1:
            nearest = min(candidates, key=lambda item: abs(item.start_line - new_metric.start_line))
            matches.append((nearest, new_metric, "low"))
        else:
            matches.append((None, new_metric, "new"))
    return matches


# Cognitive-complexity thresholds. 25 = Sonar C-family / clang-tidy default; K=5 per
# design-doc §5 ("стартовать с K≈5"), compared non-strictly (exactly 5 triggers).
COGNITIVE_THRESHOLD = 25
DELTA_K = 5


def finding_reason(old_score: int, new_score: int, delta: int) -> str | None:
    """LCX signal hierarchy (#101 Scoring model), strongest first. No size normalization."""
    if old_score < COGNITIVE_THRESHOLD <= new_score:
        return "crossed_25"  # LCX.1: function crossed the absolute complexity threshold
    if old_score >= COGNITIVE_THRESHOLD and delta > 0:
        return "grew_when_already_above"  # LCX.2: degradation in an already-complex function
    if delta >= DELTA_K:
        return "complexity_delta"  # LCX.3: meaningful single-PR growth
    return None


def analyze_text_pair(
    old_text: str,
    new_text: str,
    file_name: str,
    repo: str = "synthetic",
    parent: str = "baseline",
    sha: str = "current",
) -> tuple[list[DriftFinding], list[dict[str, object]]]:
    old_metrics = metrics_for_text(old_text, file_name)
    new_metrics = metrics_for_text(new_text, file_name)
    findings: list[DriftFinding] = []
    compared: list[dict[str, object]] = []

    for old_metric, new_metric, confidence in match_metrics(old_metrics, new_metrics):
        old_score = old_metric.local_complexity_score if old_metric else 0
        delta = new_metric.local_complexity_score - old_score
        old_loc = old_metric.meaningful_loc if old_metric else 0
        changed_loc = abs(new_metric.meaningful_loc - old_loc)
        delta_percent = None if old_score == 0 else round((delta / old_score) * 100.0, 2)
        branch_delta = new_metric.branch_score - (old_metric.branch_score if old_metric else 0)
        nesting_delta = new_metric.max_nesting_depth - (old_metric.max_nesting_depth if old_metric else 0)
        deep_delta = new_metric.deep_lines_count - (old_metric.deep_lines_count if old_metric else 0)

        compared.append(
            {
                "file": file_name,
                "symbol": new_metric.symbol,
                "old_score": old_score,
                "new_score": new_metric.local_complexity_score,
                "delta_score": delta,
                "changed_loc": changed_loc,
                "confidence": confidence,
            }
        )

        reason = finding_reason(old_score, new_metric.local_complexity_score, delta)
        if old_metric is not None and reason is not None:
            findings.append(
                DriftFinding(
                    repo=repo,
                    sha=sha,
                    parent=parent,
                    file=file_name,
                    symbol=new_metric.symbol,
                    old_score=old_score,
                    new_score=new_metric.local_complexity_score,
                    delta_score=delta,
                    delta_percent=delta_percent,
                    changed_loc=changed_loc,
                    branch_delta=branch_delta,
                    nesting_delta=nesting_delta,
                    deep_lines_delta=deep_delta,
                    confidence=confidence,
                    reason=reason,
                )
            )

    return findings, compared


def changed_cpp_files(repo: Path, parent: str, sha: str) -> list[str]:
    output = run_git(repo, ["diff", "--name-only", f"{parent}..{sha}", "--"])
    return [line for line in output.splitlines() if is_cpp_path(line)]


def read_git_file(repo: Path, revision: str, file_name: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), "show", f"{revision}:{file_name}"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors="replace",
    )
    if result.returncode != 0:
        return ""
    return result.stdout


def git_file_size(repo: Path, revision: str, file_name: str) -> int | None:
    result = subprocess.run(
        ["git", "-C", str(repo), "cat-file", "-s", f"{revision}:{file_name}"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if result.returncode != 0:
        return None
    try:
        return int(result.stdout.strip())
    except ValueError:
        return None


def scan_commit(
    repo: Path,
    sha: str,
    parent: str | None = None,
    max_changed_files: int = 80,
    max_file_bytes: int = 1_000_000,
) -> tuple[CommitSummary, list[DriftFinding]]:
    if parent is None:
        parent = run_git(repo, ["rev-parse", f"{sha}^"]).strip()

    all_findings: list[DriftFinding] = []
    compared_functions = 0
    positive_delta = 0
    negative_delta = 0
    max_new_score = 0
    max_delta_score = 0
    all_files = changed_cpp_files(repo, parent, sha)
    files = all_files[:max_changed_files]
    skipped_files = max(0, len(all_files) - len(files))

    for file_name in files:
        old_size = git_file_size(repo, parent, file_name)
        new_size = git_file_size(repo, sha, file_name)
        if (old_size and old_size > max_file_bytes) or (new_size and new_size > max_file_bytes):
            skipped_files += 1
            continue
        old_text = read_git_file(repo, parent, file_name)
        new_text = read_git_file(repo, sha, file_name)
        if not is_author_source(file_name, old_text, new_text):
            skipped_files += 1
            continue
        findings, compared = analyze_text_pair(
            old_text,
            new_text,
            file_name,
            repo=str(repo),
            parent=parent,
            sha=sha,
        )
        all_findings.extend(findings)
        compared_functions += len(compared)
        for item in compared:
            delta = int(item["delta_score"])
            max_new_score = max(max_new_score, int(item["new_score"]))
            max_delta_score = max(max_delta_score, delta)
            if delta > 0:
                positive_delta += delta
            elif delta < 0:
                negative_delta += delta

    summary = CommitSummary(
        repo=str(repo),
        sha=sha,
        parent=parent,
        changed_files=len(files),
        skipped_files=skipped_files,
        compared_functions=compared_functions,
        findings=len(all_findings),
        positive_delta_score=positive_delta,
        negative_delta_score=negative_delta,
        max_new_score=max_new_score,
        max_delta_score=max_delta_score,
    )
    return summary, all_findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path)
    parser.add_argument("--sha")
    parser.add_argument("--parent")
    parser.add_argument("--max-changed-files", type=int, default=80)
    parser.add_argument("--max-file-bytes", type=int, default=1_000_000)
    parser.add_argument("--old-file", type=Path)
    parser.add_argument("--new-file", type=Path)
    parser.add_argument("--file-name", default="example.cpp")
    args = parser.parse_args()

    if args.old_file and args.new_file:
        findings, compared = analyze_text_pair(
            args.old_file.read_text(errors="replace"),
            args.new_file.read_text(errors="replace"),
            args.file_name,
        )
        print(json.dumps({"findings": [asdict(f) for f in findings], "compared": compared}, indent=2))
        return 0

    if not args.repo or not args.sha:
        parser.error("provide --repo/--sha or --old-file/--new-file")

    summary, findings = scan_commit(args.repo, args.sha, args.parent, args.max_changed_files, args.max_file_bytes)
    print(json.dumps({"summary": asdict(summary), "findings": [asdict(f) for f in findings]}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
