# Local Complexity Drift Corpus Prototype Report

## Synthetic suite

- Synthetic cases: 13/13 passed.

## Corpus coverage

- Repositories with scanned commits: 100
- Scanned commits: 1614
- Findings: 524
- Skipped oversized/excess files: 9608

## Noise notes

- Findings where the existing function previously had score 0: 41
- Low-confidence symbol matches: 43
- Findings in likely vendor/generated paths: 10
- Reason counts: {'body_and_complexity_growth': 3, 'complexity_delta': 477, 'relative_complexity_delta': 12, 'zero_to_complex': 32}
- Meaningful LOC delta is report context only; LOC-only growth is not a finding.
- Early read: useful signal exists, but report buckets still need manual TP/FP review.
- Concrete before/after examples: `docs/research/local_complexity_drift_examples.md`.

## Top findings

| repo | sha | file | symbol | old | new | delta | loc | reason |
|------|-----|------|--------|-----|-----|-------|-----|--------|
| lock042_siril | 0de15584bbac | src/io/siril_pythoncommands.c | process_connection | 2184 | 2486 | 302 | 178 | complexity_delta |
| zeroc-ice_ice | 511e3edeb053 | cpp/src/Slice/DocCommentParser.cpp | DocCommentParser::parseDocCommentFor | 1 | 291 | 290 | 166 | complexity_delta |
| alphaonex86_CatchChallenger | dd86efc14854 | server/base/MapManagement/Map_server_MapVisibility_Simple_StoreOnSender.cpp | Map_server_MapVisibility_Simple_StoreOnSender::min_network | 0 | 215 | 215 | 82 | zero_to_complex |
| community-shaders_skyrim-community-shaders | 37c759c4d0b2 | src/Menu.cpp | Menu::DrawSettings | 558 | 761 | 203 | 86 | complexity_delta |
| Mudlet_Mudlet | c3f397552c06 | src/dlgPackageExporter.cpp | dlgPackageExporter::slot_exportPackage | 41 | 240 | 199 | 100 | complexity_delta |
| Mudlet_Mudlet | c4ad7e43e0f5 | src/TBuffer.cpp | TBuffer::decodeOSC | 355 | 546 | 191 | 92 | complexity_delta |
| Netatalk_netatalk | 8ca9a54a7727 | etc/afpd/fork.c | afp_openfork | 244 | 395 | 151 | 84 | complexity_delta |
| Mudlet_Mudlet | c4ad7e43e0f5 | src/TBuffer.cpp | TBuffer::translateToPlainText | 715 | 864 | 149 | 53 | complexity_delta |
| zeroc-ice_ice | 70fd3b371c7e | cpp/src/slice2py/PythonUtil.cpp | Slice::Python::CodeVisitor::writeDocstring | 37 | 186 | 149 | 85 | complexity_delta |
| fernandotonon_QtMeshEditor | 97676c6944e3 | src/TransformOperator.cpp | TransformOperator::mouseReleaseEvent | 11 | 136 | 125 | 42 | complexity_delta |
| community-shaders_skyrim-community-shaders | d19fd3b61fa6 | src/Menu.cpp | Menu::DrawSettings | 817 | 940 | 123 | 34 | complexity_delta |
| Netatalk_netatalk | cc0c751e3356 | etc/afpd/afp_dsi.c | afp_over_dsi | 262 | 372 | 110 | 46 | complexity_delta |
| Mudlet_Mudlet | 6a6f55563750 | src/TDetachedWindow.cpp | TDetachedWindow::slot_showMapperDialog | 0 | 104 | 104 | 102 | zero_to_complex |
| ramensoftware_windhawk-mods | cdca30ff8a2b | mods/taskbar-content-presenter-injector.wh.cpp | InjectForElement | 6 | 108 | 102 | 37 | complexity_delta |
| domoticz_domoticz | 0fb20ab21cac | main/dzVents.cpp | CdzVents::processLuaCommand | 49 | 148 | 99 | 36 | complexity_delta |
| GregorGullwi_FlashCpp | abe3aed435eb | src/Parser.cpp | Parser::try_instantiate_class_template | 162 | 260 | 98 | 86 | complexity_delta |
| chrxh_alien | 8a66aece4125 | source/EngineTests/ConstructorTests.cpp | TEST_F | 6 | 101 | 95 | 71 | complexity_delta |
| GregorGullwi_FlashCpp | 2b7cd91870b0 | src/IRConverter.h | handleFunctionCall | 0 | 93 | 93 | 42 | zero_to_complex |
| chrxh_alien | 8a66aece4125 | source/EngineTests/ConstructorTests.cpp | TEST_F | 14 | 104 | 90 | 48 | complexity_delta |
| lock042_siril | 68ff54073583 | src/io/ser.c | get_thumbnail_from_ser | 165 | 254 | 89 | 60 | complexity_delta |

## Recommendation for #101

- Recommendation: `revise` before shipping. Keep the scorer direction, keep the author-code filtering, and add manual TP/FP review plus better test/generated edge-case suppression before integrating into `archcheck --diff`.
