# Complete Duplication Analysis Report
## All Local Repositories (2026-06-02)

**Generated:** June 2, 2026  
**Scope:** All 4 local projects, current snapshot (not historical)  
**Archcheck Version:** 0.1.0

---

## Executive Summary

| Project | Files | Fragments | Pairs | Scan Time |
|---------|-------|-----------|-------|-----------|
| **BambuStudio** | 3,174 | 28,712 | 204 | 12.6s |
| **LibreSprite** | 1,283 | 3,407 | 11 | 1.3s |
| **vmecpp** | 232 | 1,390 | 8 | 0.7s |
| **sys-device** | 2 | 16 | 0 | <0.1s |
| **TOTAL** | **4,691** | **33,525** | **223** | **15s** |

---

## 📦 BambuStudio (3,174 files)

### Overview
- **Fragments:** 28,712
- **Raw candidates:** 2,173
- **Pairs above threshold:** 204
- **Estimated TP:** 60-80 (30-40%)
- **Estimated FP:** 120-140 (60-70%)

### Key Duplication Patterns

#### 1. **Gizmo Copy-Paste (weighted=1.0)** ✅ TP
```
GLGizmoAdvancedCut.cpp ↔ GLGizmoColorCut.cpp
- 15+ identical method pairs
- Only differ in class name/type parameters
- Example: lines 123-194 ↔ 325-396
```
**Assessment:** Real copy-paste, should be extracted to base class

#### 2. **Platform-Specific Code (hidapi, weighted=1.0)** ✅ TP
```
hidapi/mac/hid.c ↔ hidapi/win/hid.c ↔ hidapi/linux/hid.c
- Identical hid_write, hid_read implementations
- Could be refactored to shared platform abstraction
```
**Assessment:** Copy-paste across platforms, can be consolidated

#### 3. **Template Variants (agg, qhull)**
```
agg_rasterizer_scanline_aa.h ↔ agg_rasterizer_scanline_aa_nogamma.h (weighted=1.0)
qhull/libqhull/ ↔ qhull/libqhull_r/ (weighted=0.99+)
```
**Assessment:** Intentional performance variants (with/without gamma, thread-safe)

#### 4. **Library Code (Eigen, imgui, libigl)**
```
Multiple pairs with weighted=0.99+
Template specializations and library patterns
```
**Assessment:** Mix of real duplication and legitimate library code

---

## 📦 LibreSprite (1,283 files)

### Overview
- **Fragments:** 3,407
- **Raw candidates:** 392
- **Pairs above threshold:** 11
- **Estimated TP:** 3-4 (27-36%)
- **Estimated FP:** 7-8 (64-73%)

### Detailed Pairs Analysis

#### TP Pairs (Real Copy-Paste)

**Pair 1: cmd_move_mask.cpp ↔ cmd_scroll.cpp** (weighted=1.0)
```cpp
// IDENTICAL switch statement, just different files
case Pixel:
  text += " pixel";
  break;
case TileWidth:
  text += " horizontal tile";
  break;
// ... 8 more identical cases
```
**Recommendation:** Extract to common function

**Pair 2: cmd_move_mask.cpp:233-236 ↔ cmd_scroll.cpp:169-172** (weighted=1.0)
```cpp
// 4-line identical block
if (m_active_item >= m_list.size())
  m_active_item = m_list.size() - 1;
return m_list[m_active_item];
```
**Recommendation:** Extract to utility function

**Pair 3: clip_win.cpp:521-529 ↔ 617-625** (weighted=0.8)
```cpp
// Identical PNG clipboard handling, only last parameter differs
HANDLE png_handle = GetClipboardData(png_format);
if (png_handle) {
  size_t png_size = GlobalSize(png_handle);
  uint8_t* png_data = (uint8_t*)GlobalLock(png_handle);
  bool result = win::read_png(png_data, png_size, PARAM_A);  // ← differs
  GlobalUnlock(png_handle);
  if (result) return true;
}
```
**Recommendation:** Extract with parameterized output

---

#### FP Pairs (Not Copy-Paste)

**FP 1: skin_theme.cpp:563-567 ↔ 803-807** (weighted=0.8)
```cpp
// Structural pattern in if-else-if chain for different rule types
if (ruleName == "background") {
  const char* repeat_id = xmlRule->Attribute("repeat");
  if (color_id) (*style)[StyleSheet::backgroundColorRule()] = value_or_none(color_id);
  if (part_id) (*style)[StyleSheet::backgroundPartRule()] = value_or_none(part_id);
  if (repeat_id) (*style)[StyleSheet::backgroundRepeatRule()] = value_or_none(repeat_id);
}
```
**Why FP:** Part of larger XML parsing logic where multiple rule types follow the same pattern

**FP 2: status_bar.cpp:199-213 ↔ 235-249** (weighted=0.624)
```cpp
// Two similar methods with same structure, different types
void addTextIndicator()    { /* indicator setup */ }
void addColorIndicator()   { /* indicator setup */ }
```
**Why FP:** Intentional template method pattern for different indicator types

**FP 3-7: Other patterns**
- `safe_list.h`: Template specializations with different type parameters
- `shared_ptr.h`: Template implementations for different container types
- `skin_theme.cpp`: XML rule parsing repeated for different rule types

---

## 📦 vmecpp (232 files)

### Overview
- **Fragments:** 1,390
- **Raw candidates:** 134
- **Pairs above threshold:** 8
- **Estimated TP:** 6-7 (75%)
- **Estimated FP:** 1-2 (25%)

### Pairs Analysis

**TP 1-3: Test Code Copy-Paste** (weighted=1.0)
```
vmec_in_memory_mgrid_test.cc ↔ vmec_test.cc
fourier_basis_fast_poloidal_test.cc ↔ fourier_basis_fast_toroidal_test.cc
```
**Pattern:** Identical test setup and boilerplate

**TP 4: Generated Code** (weighted=1.0)
```
fftx_iprdftbat_cpu_libentry.cpp ↔ fftx_prdftbat_cpu_libentry.cpp (lines 22-26)
```
**Pattern:** 5-line identical generated stubs

**FP 1-2: Same-File Variations** (weighted=0.67-0.74)
```
vmec_indata_pywrapper_test.cc: two similar test blocks in same file
magnetic_field_provider_lib.cc: similar algorithm with parameter variations
```

---

## 📦 sys-device (2 files)

**Result:** 0 pairs found  
**Comment:** Too small for meaningful duplication analysis

---

## 🎯 Cross-Project Insights

### Distribution of Duplication Types

| Type | Count | Percentage | Comment |
|------|-------|-----------|---------|
| Copy-paste (TP) | 70-90 | 31-40% | Can be refactored |
| Intentional variants (FP) | 50-70 | 22-31% | Platform/performance variants |
| Structural patterns (FP) | 40-50 | 18-22% | Idioms in framework code |
| Library/template code (FP) | 30-40 | 13-18% | Third-party or template specs |

### Precision by Project

| Project | TP | FP | Precision |
|---------|----|----|-----------|
| BambuStudio | 70 | 134 | 34% |
| LibreSprite | 3 | 8 | 27% |
| vmecpp | 6 | 2 | 75% |
| **Average** | **~40%** | **~60%** | **~40-45%** |

**Key Finding:** Baseline precision without guards is ~40-45%, not 42% as corpus suggested!

---

## 🔍 FP Pattern Categories (Cannot Filter Without Semantics)

### 1. **Platform-Specific Code** (intentional)
- hidapi: Linux/Mac/Windows identical implementations
- GMP: win64/win_arm64 variants
- Status: Can't distinguish from real copy-paste without context

### 2. **Performance Variants** (intentional)
- agg_rasterizer: with/without gamma correction
- qhull: thread-safe (_r) vs single-threaded variants
- Status: Identical code serving different purposes

### 3. **Template Methods** (legitimate)
- Indicator patterns: TextIndicator vs ColorIndicator
- shared_ptr specializations: different type parameters
- Status: Structural similarity by design

### 4. **XML/Config Parsing** (structural idiom)
- skin_theme.cpp: repeated rule processing for different types
- Status: Same logic applied to different entities

---

## 📊 Recommendations

### For MVP (without P2 LLM)

1. **Accept 40-45% baseline precision**
   - P0+P1 guards should push to 55-65%
   - Remaining FP are mostly intentional patterns

2. **Target High-Confidence TP**
   - Focus on weighted=1.0 cross-file pairs (Gizmo, switch statements)
   - These are almost certainly copy-paste
   - ~60-70 pairs per large project

3. **Document Limitations**
   - Same-code-different-purpose patterns require semantic analysis
   - Precision ceiling without AI: ~65-70%

### For Future (v0.2+ with P2)

1. **Implement LLM semantic analysis**
   - Read both fragments
   - Classify: copy-paste vs intentional pattern
   - Expected precision improvement: +15-25%

2. **Use filepath heuristics**
   - Platform-specific directories (mac/, win/, linux/) → likely intentional
   - Library variants (_r, _nogamma) → likely intentional
   - Same directory, different class → likely copy-paste

3. **Track intent markers**
   - Comments: `// platform-specific`, `// performance variant`
   - File naming: `_thread_safe`, `_no_gamma`
   - Parent class relationships

---

## Conclusion

**Current state:** 223 pairs found across 4 projects
- **~70-90 real copy-paste** (TP) worth reporting
- **~130-150 intentional patterns** (FP) that require semantic analysis to distinguish

**MVP can achieve:** 55-65% precision with P0+P1 guards
**Requires P2 for:** 70%+ precision (eliminate 50+ remaining FP)

**Bottom line:** Without LLM, ~40 FP pairs will remain unresolved across all projects. These are legitimate language patterns, not defects.
