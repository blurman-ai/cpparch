void zoo() {
    char x = L'a';
    x = '\x30';
    char16_t c = u'ö';
    wchar_t b = L'\xFFEF';
    char32_t d = U'\U0010FFFF';
    char A[] = "Hello\x0A";
    wchar_t B[] = L"Hell\xF6\x0A";
    char* rawString = R"(
        Destination=Console
        Filter="%Severity% >= WRN"
    )";
    auto integer_literal = 1'000''000;
    auto floating_point_literal = 0.000'015'3;
    auto hex_literal = 0x0F00'abcd'6f3d;
    int b1 = 0B001101;
    int b2 = 0b000001;
}
