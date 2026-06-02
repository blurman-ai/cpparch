# LibreSprite Duplication Analysis (2026-06-02)

## Scan Results

**Project:** LibreSprite (1283 files)
**Fragments found:** 3407
**Raw candidates:** 392
**Pairs above threshold (similarity ≥ 0.60):** 11

## Detailed Analysis of 11 Pairs

### TRUE POSITIVES (TP) — Real Duplication

#### Pair 1: Cross-file identical code (PNG buffer handling)
```
src/clip/clip_win.cpp:521-529 <-> src/clip/clip_win.cpp:617-625
weighted=0.8
```
**Classification:** TP (Real Duplication)
**Pattern:** Identical PNG clipboard handling code
```cpp
HANDLE png_handle = GetClipboardData(png_format);
if (png_handle) {
  size_t png_size = GlobalSize(png_handle);
  uint8_t* png_data = (uint8_t*)GlobalLock(png_handle);
  bool result = win::read_png(png_data, png_size, DIFFERENT_PARAMS);
  GlobalUnlock(png_handle);
  if (result) return true;
}
```
**Difference:** Last parameter differs:
- Line 525: `&output_img, nullptr`
- Line 619: `nullptr, &spec`

**Action:** This is legitimate duplication worth reporting.

---

#### Pair 2: Cross-file identical switch statement (cmd_move_mask vs cmd_scroll)
```
src/app/commands/cmd_move_mask.cpp:204-227 <-> src/app/commands/cmd_scroll.cpp:140-163
weighted=1 (identical)
```
**Classification:** TP (Real Duplication - Copy-Paste)
**Pattern:** Identical switch-case statement
```cpp
case Pixel:
  text += " pixel";
  break;
case TileWidth:
  text += " horizontal tile";
  break;
// ... (8 more identical cases)
```
**Assessment:** Likely a copy-paste from one file to another. Should be extracted to common function.

**Action:** Worth reporting for refactoring.

---

#### Pair 3: Same-file duplication (PNG buffer, different functions)
```
src/clip/clip_win.cpp:521-529 <-> src/clip/clip_win.cpp:617-625
weighted=0.8
```
**Classification:** TP (SAME-FILE, minor variation)
**Assessment:** Two functions in same file doing nearly identical work.

---

### FALSE POSITIVES (FP) — IDIOM Pattern

#### FP 1: skin_theme.cpp background rule parsing (repeated idiom)
```
src/app/ui/skin/skin_theme.cpp:563-567 <-> src/app/ui/skin/skin_theme.cpp:803-807
weighted=0.8 (high overlap)
```
**Classification:** FP (IDIOM - Same conditional block in same file)
**Pattern:** Same code inside `if (ruleName == "background")` block appears twice
```cpp
if (ruleName == "background") {
  const char* repeat_id = xmlRule->Attribute("repeat");
  if (color_id) (*style)[StyleSheet::backgroundColorRule()] = value_or_none(color_id);
  if (part_id) (*style)[StyleSheet::backgroundPartRule()] = value_or_none(part_id);
  if (repeat_id) (*style)[StyleSheet::backgroundRepeatRule()] = value_or_none(repeat_id);
}
```
**Analysis:** This appears to be legitimate part of larger if-else-if chain. The code is repeated because the function processes multiple rule types (background, icon, text) with similar patterns.

**Why it's FP:** Legitimate XML stylesheet parsing idiom, not copy-paste.

**Guard Status:** **P0.6 should catch this** (same file, same function, high line overlap but structured repetition).

---

#### FP 2: skin_theme.cpp similar rules (multiple pairs)
```
src/app/ui/skin/skin_theme.cpp:570-577 <-> src/app/ui/skin/skin_theme.cpp:810-817
src/app/ui/skin/skin_theme.cpp:580-591 <-> src/app/ui/skin/skin_theme.cpp:820-831
src/app/ui/skin/skin_theme.cpp:1294-1325 <-> src/app/ui/skin/skin_theme.cpp:1771-1802
weighted=0.68-0.78
```
**Classification:** FP (IDIOM - repetitive rule parsing)
**Pattern:** Multiple similar rule handling patterns in same file
**Assessment:** Systematic XML parsing idiom across many rule types.

**Guard Status:** Same-file, should be filtered by **P0.1** or line-overlap guard.

---

#### FP 3: status_bar.cpp indicator pattern
```
src/app/ui/status_bar.cpp:199-213 <-> src/app/ui/status_bar.cpp:235-249
weighted=0.624
```
**Classification:** FP (IDIOM - parallel method implementations)
**Pattern:** Two methods with identical structure:
- `addTextIndicator()` - creates TextIndicator
- `addColorIndicator()` - creates ColorIndicator

Same boilerplate for different indicator types:
```cpp
if (m_iterator != m_indicators.end()) {
  if ((*m_iterator)->indicatorType() == TYPE) {
    static_cast<XYZIndicator*>(*m_iterator)->updateIndicator(...);
    ++m_iterator;
    return;
  }
  else removeAllNextIndicators();
}
auto indicator = new XYZIndicator(...);
m_indicators.push_back(indicator);
m_iterator = m_indicators.end();
addChild(indicator);
```

**Why it's FP:** Legitimate template pattern for indicator management, not copy-paste duplication.

**Guard Status:** weighted=0.624 < 0.75 but >= 0.60. P0.6 would PASS because line overlap likely >= 0.50. **Should be filtered by P1.2 (boilerplate-density)** or other classifier.

---

#### FP 4: Library code template (safe_list.h, observable)
```
third_party/observable/obs/safe_list.h:292-322 <-> third_party/observable/obs/safe_list.h:339-370
weighted=0.763
```
**Classification:** FP (IDIOM - template method with different type parameters)
**Pattern:** Same template-like code with different iterator/value types

---

#### FP 5: Generic pointer template (shared_ptr.h)
```
src/base/shared_ptr.h:121-131 <-> src/base/shared_ptr.h:137-147
weighted=0.746
```
**Classification:** FP (IDIOM - template specializations)
**Pattern:** Similar template implementations with different parameter sets

---

#### FP 6: Convolution matrix filter (complex algorithm)
```
src/filters/convolution_matrix_filter.cpp:159-206 <-> src/filters/convolution_matrix_filter.cpp:281-338
weighted=0.656
```
**Classification:** Uncertain (possibly TP, possibly FP)
**Pattern:** Two similar algorithm implementations
**Need:** Manual inspection to determine if real duplication or necessary variation

---

## Summary Statistics

| Category | Count | Notes |
|----------|-------|-------|
| TP (real duplication) | 2-3 | Cross-file, worth reporting |
| FP (idiom/template) | 6-7 | Legitimate repetition patterns |
| Uncertain | 1-2 | Requires deeper analysis |
| **Total pairs** | **11** | |

**Precision (from this sample):** ~22% (2/11 clear TP)

## Patterns Identified

### 1. **XML/Stylesheet Parsing Idiom** (skin_theme.cpp)
Multiple rule types (background, icon, text, etc.) processed with nearly identical structure.
- Expected pattern, not duplication
- Should be recognized as idiom

### 2. **Template Method Idiom** (status_bar.cpp, shared_ptr.h)
Similar skeleton code with different type instantiations.
- Legitimate template pattern
- Not copy-paste

### 3. **Cross-file Real Duplication**
- PNG buffer handling code (cmd_move_mask.cpp vs cmd_scroll.cpp)
- Clear opportunity for extraction to common function

## Recommendations

1. **For #070 guard improvements:**
   - P1.2 (boilerplate-density) should catch status_bar pattern (weighted=0.624)
   - P0.1 should catch most same-file idioms (except maybe weighted=0.78+)
   - Need better detection for "legitimate template repetition"

2. **For future work:**
   - Consider AST-level analysis to detect template/idiom patterns
   - Track method signatures to recognize intentional duplication

3. **False positive reduction target:**
   - Current: ~22% precision
   - With current guards: likely 55-65% (from MVP projection)
   - Remaining FP are mostly legitimate idioms

---

# BambuStudio Full Project Analysis (3174 files)

## Scan Results

**Project:** BambuStudio
**Fragments found:** 28,712
**Raw candidates:** 2,173
**Pairs above threshold (similarity ≥ 0.60):** 204

## New FP Patterns Discovered

### Pattern 1: Platform-Specific Implementations (hidapi)
```
src/hidapi/mac/hid.c:521-551 <-> src/hidapi/win/hid.c:524-554 (weighted=1)
src/hidapi/linux/hid.c:714-744 <-> src/hidapi/win/hid.c:524-554 (weighted=1)
src/hidapi/linux/hid.c:714-744 <-> src/hidapi/mac/hid.c:521-551 (weighted=1)
```
**Classification:** FP (IDIOM - Platform-specific code)
**Analysis:** Same functionality implemented identically across Linux/Mac/Windows. This is expected code duplication for platform portability.
**Guard Status:** Should be filtered by P0.4 (function-boundary anchor) or detected as intentional pattern.

---

### Pattern 2: Template Code with Variations (qhull, AGG)
```
src/qhull/src/libqhull/geom.c:1117-1137 <-> src/qhull/src/libqhull_r/geom_r.c:1117-1137
src/agg/agg_rasterizer_scanline_aa.h:205-257 <-> src/agg/agg_rasterizer_scanline_aa_nogamma.h:208-260
```
**Classification:** FP (IDIOM - Intentional variant generation)
**Analysis:** Two code variants:
- qhull: single-threaded vs thread-safe (_r suffix)
- agg: with gamma correction vs without

These are intentionally maintained as separate implementations for performance/feature reasons.

---

### Pattern 3: UI Component Cloning (Gizmo implementations)
```
src/slic3r/GUI/Gizmos/GLGizmoAdvancedCut.cpp (many methods) <-> 
src/slic3r/GUI/Gizmos/GLGizmoColorCut.cpp (similar methods)
```
**Classification:** Mixed TP/FP
- Some pairs (weighted=1) are clear copy-paste
- Some pairs might be intentional similar API design

**Example TP (clear copy-paste):**
```
GLGizmoAdvancedCut.cpp:123-194 <-> GLGizmoColorCut.cpp:325-396 (weighted=1)
GLGizmoAdvancedCut.cpp:1439-1481 <-> GLGizmoColorCut.cpp:1886-1928 (weighted=1)
```

---

### Pattern 4: Cross-library Implementations  
```
src/libigl/igl/copyleft/cgal/outer_facet.cpp:104-147 <-> src/libigl/igl/outer_element.cpp:227-270
```
**Classification:** Likely TP (Real duplication across library)

---

## Overall FP Distribution Estimate

From sample of BambuStudio 204 pairs:

| Category | Est. Count | Reason |
|----------|-----------|--------|
| TP (real copy-paste) | 50-80 | Cross-file duplication, can be extracted |
| FP (platform-specific) | 20-30 | Intentional: hidapi, qhull, agg variants |
| FP (UI template pattern) | 30-50 | Gizmo implementations with similar structure |
| FP (library idiom) | 40-60 | Third-party code patterns (Eigen, imgui, igl) |
| **Total** | **204** | |

**Estimated precision:** 25-40% (mostly TP)

---

## Cross-Project Summary

| Project | Files | Fragments | Pairs | Estimated Precision |
|---------|-------|-----------|-------|-------------------|
| vmecpp | 232 | 1,390 | 8 | ~75% (generated code) |
| LibreSprite | 1,283 | 3,407 | 11 | ~22% (many idioms) |
| BambuStudio | 3,174 | 28,712 | 204 | ~30% (mixed patterns) |

**Average precision across 3 projects:** ~42% (before guards)
**Expected after P0+P1 guards:** 55-65% (MVP target)

