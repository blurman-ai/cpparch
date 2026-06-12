#!/usr/bin/env python3
"""Generate synthetic local complexity drift cases."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
CASES_DIR = ROOT / "synthetic_cases"


CASES = [
    (
        "flat_to_nested_if",
        "must_trigger",
        """
int classify(int value)
{
    if (value > 0) return 1;
    return 0;
}
""",
        """
int classify(int value)
{
    if (value > 0)
    {
        if (value > 10)
        {
            if ((value % 2) == 0 && value < 100)
            {
                return 3;
            }
            return 2;
        }
        return 1;
    }
    return 0;
}
""",
    ),
    (
        "harmless_append_statement",
        "must_not_trigger",
        """
int render(int value)
{
    int result = value + 1;
    return result;
}
""",
        """
int render(int value)
{
    int result = value + 1;
    result += 2;
    return result;
}
""",
    ),
    (
        "large_linear_body_growth",
        "must_not_trigger",
        """
int build(int input)
{
    int value = input;
    return value;
}
""",
        """
int build(int input)
{
    int value = input;
    value += 1;
    value += 2;
    value += 3;
    value += 4;
    value += 5;
    value += 6;
    value += 7;
    value += 8;
    value += 9;
    value += 10;
    value += 11;
    value += 12;
    value += 13;
    value += 14;
    value += 15;
    value += 16;
    value += 17;
    value += 18;
    value += 19;
    value += 20;
    value += 21;
    value += 22;
    value += 23;
    value += 24;
    value += 25;
    value += 26;
    value += 27;
    value += 28;
    value += 29;
    value += 30;
    return value;
}
""",
    ),
    (
        "else_if_chain_growth",
        "must_trigger",
        """
int mapCode(int code)
{
    if (code == 1) return 10;
    return 0;
}
""",
        """
int mapCode(int code)
{
    if (code == 1) return 10;
    else if (code == 2) return 20;
    else if (code == 3) return 30;
    else if (code == 4) return 40;
    else if (code == 5) return 50;
    else if (code == 6) return 60;
    return 0;
}
""",
    ),
    (
        # Cognitive complexity: a flat switch is +1 regardless of case count (Sonar
        # calibration). Growing it by adding flat cases is defect D1 — delta = 0, no
        # finding. Nested growth is covered by flat_to_nested_if / loop_inside_loop.
        "switch_case_explosion",
        "must_not_trigger",
        """
int tokenCost(int token)
{
    switch (token)
    {
    case 1: return 1;
    default: return 0;
    }
}
""",
        """
int tokenCost(int token)
{
    switch (token)
    {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 5;
    case 5: return 8;
    case 6: return 13;
    case 7: return 21;
    default: return 0;
    }
}
""",
    ),
    (
        "loop_inside_loop",
        "must_trigger",
        """
int sum(const int* values, int count)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
    {
        total += values[i];
    }
    return total;
}
""",
        """
int sum(const int* values, int count)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
    {
        for (int j = 0; j < count; ++j)
        {
            if (values[i] > values[j])
            {
                total += values[i] - values[j];
            }
        }
    }
    return total;
}
""",
    ),
    (
        "lambda_nesting_growth",
        "must_trigger",
        """
int visit(int value)
{
    auto apply = [value]() { return value + 1; };
    return apply();
}
""",
        """
int visit(int value)
{
    auto apply = [value]() {
        if (value > 0)
        {
            return [value]() {
                if (value > 10)
                {
                    return value + 2;
                }
                return value + 1;
            }();
        }
        return 0;
    };
    return apply();
}
""",
    ),
    (
        "guard_clause_refactor",
        "should_reduce",
        """
int parse(int state, int token)
{
    if (state > 0)
    {
        if (token > 0)
        {
            if (token < 10)
            {
                return state + token;
            }
        }
    }
    return 0;
}
""",
        """
int parse(int state, int token)
{
    if (state <= 0) return 0;
    if (token <= 0) return 0;
    if (token >= 10) return 0;
    return state + token;
}
""",
    ),
    (
        "comments_and_strings_noise",
        "must_not_trigger",
        """
int load()
{
    return 1;
}
""",
        r'''
int load()
{
    const char* text = "if (x) { for (;;) while (true) switch (x) }";
    // if (debug) { while (true) { return 2; } }
    /* for (;;) { if (x && y) return 3; } */
    return 1;
}
''',
    ),
    (
        "macro_heavy_file",
        "low_confidence_allowed",
        """
int call(int value)
{
    return value;
}
""",
        """
#define CHECK_AND_RETURN(x) if ((x) > 0) { return (x); }
int call(int value)
{
    CHECK_AND_RETURN(value)
    return 0;
}
""",
    ),
    (
        "formatting_only_change",
        "must_not_trigger",
        """
int normalize(int value)
{
    if (value > 10) return 10;
    if (value < 0) return 0;
    return value;
}
""",
        """
int normalize(int value)
{
        if (value > 10)
            return 10;
        if (value < 0)
            return 0;
        return value;
}
""",
    ),
    (
        "function_move_without_growth",
        "must_not_trigger",
        """
int first()
{
    return 1;
}

int stable(int value)
{
    if (value > 0) return value;
    return 0;
}
""",
        """
int stable(int value)
{
    if (value > 0) return value;
    return 0;
}

int first()
{
    return 1;
}
""",
    ),
    (
        "extract_helper_reduces_complexity",
        "should_reduce",
        """
int handle(int value)
{
    if (value > 0)
    {
        if (value < 10)
        {
            if ((value % 2) == 0)
            {
                return value * 2;
            }
        }
    }
    return 0;
}
""",
        """
int isSmallEven(int value)
{
    return value > 0 && value < 10 && (value % 2) == 0;
}

int handle(int value)
{
    if (!isSmallEven(value)) return 0;
    return value * 2;
}
""",
    ),
]


def main() -> int:
    CASES_DIR.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, expected, baseline, current in CASES:
        case_dir = CASES_DIR / name
        (case_dir / "baseline").mkdir(parents=True, exist_ok=True)
        (case_dir / "current").mkdir(parents=True, exist_ok=True)
        (case_dir / "baseline" / "example.cpp").write_text(baseline.strip() + "\n")
        (case_dir / "current" / "example.cpp").write_text(current.strip() + "\n")
        manifest.append({"name": name, "file": "example.cpp", "expected": expected})

    (CASES_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"generated {len(manifest)} synthetic cases in {CASES_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
