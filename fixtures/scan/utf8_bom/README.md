# scan/utf8_bom

UTF-8 BOM (`EF BB BF`) before the first preprocessor directive is silently
accepted by every mainstream compiler but historically broke
`include_scanner` (task `047_crt_scan_utf8_bom`).

- `with_bom.h` — starts with `EF BB BF`, then `#pragma once` and one `#include`.
- `without_bom.h` — identical content without the BOM.
- `other.h` — target of the include.

Expected behaviour: `with_bom.h` and `without_bom.h` produce identical
`scanIncludes()` output and identical SF.8 verdicts (both have an include
guard).
