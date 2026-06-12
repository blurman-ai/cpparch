#!/usr/bin/env python3
"""
Boolean-state extractor: finds structs/classes and their boolean fields.
Input: C++ source file
Output: JSON with {struct_name, file, num_bool_fields, field_names, comments}
"""

import re
import json
import sys
from pathlib import Path
from typing import List, Dict, Any


class BooleanStateExtractor:
    def __init__(self):
        # Match struct/class declaration (with optional inheritance)
        self.struct_pattern = re.compile(
            r'(?:struct|class)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::[^{]*)?{',
            re.MULTILINE
        )
        # Match bool field: 'bool fieldname;' or 'bool fieldname: bits;' or 'mutable bool ...'
        self.bool_field_pattern = re.compile(
            r'(?:mutable\s+)?bool\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;',
            re.MULTILINE
        )
        # Match C++ style comments before a field
        self.comment_pattern = re.compile(
            r'(?://[^\n]*|/\*.*?\*/)',
            re.DOTALL
        )

    def extract_struct_bodies(self, content: str) -> List[tuple]:
        """
        Extract struct/class bodies from C++ code.
        Returns: [(struct_name, start_pos, end_pos, is_class), ...]
        """
        structs = []

        for match in self.struct_pattern.finditer(content):
            struct_name = match.group(1)
            open_brace_pos = match.end() - 1

            # Find matching closing brace
            brace_count = 1
            pos = open_brace_pos + 1
            while pos < len(content) and brace_count > 0:
                if content[pos] == '{':
                    brace_count += 1
                elif content[pos] == '}':
                    brace_count -= 1
                pos += 1

            is_class = 'class' in content[match.start():match.start() + 10]
            structs.append((struct_name, open_brace_pos + 1, pos - 1, is_class))

        return structs

    def extract_bool_fields(self, body: str, struct_name: str) -> Dict[str, Any]:
        """
        Extract bool fields from struct body.
        Handles: 'bool x;', 'bool x : 1;', 'bool x, y, z;', 'bool x = false;'
        Skips: method signatures, parameters in function declarations.
        Returns: {
            'struct_name': str,
            'num_bool_fields': int,
            'fields': [
                {'name': str, 'line': int, 'has_bitfield': bool, 'comment': str},
                ...
            ]
        }
        """
        lines = body.split('\n')
        fields = []

        for line_idx, line in enumerate(lines):
            # Skip comments-only lines
            if line.strip().startswith('//') or line.strip().startswith('/*'):
                continue

            # Skip lines without 'bool'
            if 'bool' not in line:
                continue

            # CRITICAL: Skip lines with parentheses (function signatures/parameters)
            # E.g., 'void func(bool x)' or 'bool method() const'
            if '(' in line or ')' in line:
                continue

            # Extract trailing comment (before processing)
            comment_match = re.search(r'//\s*(.+?)$', line)
            trailing_comment = comment_match.group(1) if comment_match else ''

            # Handle 'bool a, b, c;' style (multiple fields on one line)
            # Match: (mutable)? bool identifier[: bitwidth][= init] [, identifier[: bitwidth][= init]]* ;
            bool_decl = re.search(r'(mutable\s+)?bool\s+([^;]+);', line)
            if bool_decl:
                declarations = bool_decl.group(2)  # e.g., 'a, b : 1, c'

                # Split by comma and extract individual identifiers
                for decl in declarations.split(','):
                    # Extract identifier and optional bitfield/init
                    # e.g., 'a : 1' or 'b' or 'c = false'
                    id_match = re.match(r'\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*\d+)?(?:\s*=\s*[^,;]+)?', decl)
                    if id_match:
                        field_name = id_match.group(1)
                        has_bitfield = ':' in decl

                        fields.append({
                            'name': field_name,
                            'line': line_idx + 1,
                            'has_bitfield': has_bitfield,
                            'comment': trailing_comment
                        })

        return {
            'struct_name': struct_name,
            'num_bool_fields': len(fields),
            'fields': fields
        }

    def extract_from_file(self, filepath: str) -> List[Dict[str, Any]]:
        """Extract all struct/class definitions and their bool fields."""
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        structs = self.extract_struct_bodies(content)
        results = []

        for struct_name, start, end, is_class in structs:
            body = content[start:end]
            extracted = self.extract_bool_fields(body, struct_name)

            if extracted['num_bool_fields'] > 0:
                extracted['file'] = filepath
                extracted['is_class'] = is_class
                results.append(extracted)

        return results


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 extractor.py <file.cpp/h> [<file2> ...]")
        sys.exit(1)

    extractor = BooleanStateExtractor()
    all_results = []

    for filepath in sys.argv[1:]:
        try:
            results = extractor.extract_from_file(filepath)
            all_results.extend(results)

            # Print to console
            for result in results:
                print(f"\n{result['file']}: {result['struct_name']} "
                      f"({'class' if result['is_class'] else 'struct'}) "
                      f"({result['num_bool_fields']} bool fields)")
                for field in result['fields']:
                    comment_str = f" // {field['comment']}" if field['comment'] else ""
                    bitfield_str = " [bitfield]" if field['has_bitfield'] else ""
                    print(f"  L{field['line']:3d}  bool {field['name']}{bitfield_str}{comment_str}")
        except Exception as e:
            print(f"Error processing {filepath}: {e}", file=sys.stderr)

    # Output JSON
    print("\n\n=== JSON OUTPUT ===")
    print(json.dumps(all_results, indent=2))


if __name__ == '__main__':
    main()
