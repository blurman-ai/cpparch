# Local Complexity Drift Corpus Prototype Report

## Synthetic suite

- Synthetic cases: 13/13 passed.

## Corpus coverage

- Repositories with scanned commits: 100
- Scanned commits: 1612
- Findings: 403
- Skipped oversized/excess files: 9656

## Noise notes

- Findings where the existing function previously had score 0: 31
- Low-confidence symbol matches: 21
- Findings in likely vendor/generated paths: 5
- Reason counts: {'complexity_delta': 127, 'crossed_25': 66, 'grew_when_already_above': 210}
- Triage buckets: {'high_confidence_existing_growth': 354, 'low_confidence_match': 21, 'zero_baseline_existing_growth': 28}
- Score = Sonar Cognitive Complexity (Campbell 2018), token-based; linear code is 0 by construction.
- Signals: `crossed_25` (LCX.1), `grew_when_already_above` (LCX.2), `complexity_delta` Δ>=5 (LCX.3).
- v2 vs v1: flat-switch parsers and `TEST_F` bodies no longer dominate the top (review D1/D6 fixed).
- Open noise: `grew_when_already_above` fires on any Δ>0 above 25; the Δ1-2 tail is low-value (see recommendation).
- Concrete before/after examples: `docs/research/local_complexity_drift_examples.md`.

## Top findings

| repo | sha | file | symbol | old | new | delta | loc | reason |
|------|-----|------|--------|-----|-----|-------|-----|--------|
| zeroc-ice_ice | 511e3edeb053 | cpp/src/Slice/DocCommentParser.cpp | DocCommentParser::parseDocCommentFor | 1 | 110 | 109 | 166 | crossed_25 |
| alphaonex86_CatchChallenger | dd86efc14854 | server/base/MapManagement/Map_server_MapVisibility_Simple_StoreOnSender.cpp | Map_server_MapVisibility_Simple_StoreOnSender::min_network | 0 | 107 | 107 | 82 | crossed_25 |
| lock042_siril | 0de15584bbac | src/io/siril_pythoncommands.c | process_connection | 574 | 681 | 107 | 178 | grew_when_already_above |
| Mudlet_Mudlet | c4ad7e43e0f5 | src/TBuffer.cpp | TBuffer::decodeOSC | 83 | 179 | 96 | 92 | grew_when_already_above |
| community-shaders_skyrim-community-shaders | 37c759c4d0b2 | src/Menu.cpp | Menu::DrawSettings | 205 | 298 | 93 | 86 | grew_when_already_above |
| Mudlet_Mudlet | c3f397552c06 | src/dlgPackageExporter.cpp | dlgPackageExporter::slot_exportPackage | 16 | 108 | 92 | 100 | crossed_25 |
| ciaranm_glasgow-constraint-solver | f223605ec811 | gcs/constraints/min_max.cc | ArrayMinMax::install | 146 | 233 | 87 | 15 | grew_when_already_above |
| Mudlet_Mudlet | c4ad7e43e0f5 | src/TBuffer.cpp | TBuffer::translateToPlainText | 458 | 540 | 82 | 53 | grew_when_already_above |
| zeroc-ice_ice | 70fd3b371c7e | cpp/src/slice2py/PythonUtil.cpp | Slice::Python::CodeVisitor::writeDocstring | 23 | 100 | 77 | 85 | crossed_25 |
| Mudlet_Mudlet | 6a6f55563750 | src/TDetachedWindow.cpp | TDetachedWindow::slot_showMapperDialog | 0 | 75 | 75 | 102 | crossed_25 |
| fernandotonon_QtMeshEditor | 97676c6944e3 | src/TransformOperator.cpp | TransformOperator::mouseReleaseEvent | 7 | 81 | 74 | 42 | crossed_25 |
| lock042_siril | 68ff54073583 | src/io/ser.c | get_thumbnail_from_ser | 104 | 169 | 65 | 60 | grew_when_already_above |
| community-shaders_skyrim-community-shaders | d19fd3b61fa6 | src/Menu.cpp | Menu::DrawSettings | 289 | 350 | 61 | 34 | grew_when_already_above |
| GregorGullwi_FlashCpp | abe3aed435eb | src/Parser.cpp | Parser::try_instantiate_class_template | 90 | 147 | 57 | 86 | grew_when_already_above |
| domoticz_domoticz | 0fb20ab21cac | main/dzVents.cpp | CdzVents::processLuaCommand | 28 | 81 | 53 | 36 | grew_when_already_above |
| community-shaders_skyrim-community-shaders | b225568e3228 | src/Upscaling.cpp | Upscaling::DrawSettings | 42 | 93 | 51 | 42 | grew_when_already_above |
| Mudlet_Mudlet | 6a6f55563750 | src/TDetachedWindow.cpp | TDetachedWindow::~TDetachedWindow | 7 | 58 | 51 | 25 | crossed_25 |
| ramensoftware_windhawk-mods | cdca30ff8a2b | mods/taskbar-content-presenter-injector.wh.cpp | InjectForElement | 2 | 50 | 48 | 37 | crossed_25 |
| community-shaders_skyrim-community-shaders | 3791d86aa91a | src/Features/WetnessEffects.cpp | WetnessEffects::DrawSettings | 49 | 96 | 47 | 66 | grew_when_already_above |
| Netatalk_netatalk | cc0c751e3356 | etc/afpd/afp_dsi.c | afp_over_dsi | 77 | 124 | 47 | 46 | grew_when_already_above |

## Recommendation for #101

- Recommendation: `revise`, leaning `ship` for the scorer core.
- The metric itself is ready to port: it is Sonar Cognitive Complexity (authority-backed),
  passes the synthetic suite 13/13, fixes every defect from the scorer review (D1-D7), and
  preserves all 6 manually-confirmed true positives while dropping the switch-parser/`TEST_F` noise.
- What still needs tuning before it ships as a default rule (threshold work, not scorer work):
  - `grew_when_already_above` (LCX.2) currently triggers on any Δ>0 over the 25 threshold;
    the Δ1-2 tail on already-huge functions is not actionable. Add a floor (e.g. Δ>=K) or
    report it as advisory-only, separate from the harder `crossed_25` / `complexity_delta` signals.
  - Confirm the K=5 delta threshold on a second corpus slice; it is a calibration start, not a derived value.
