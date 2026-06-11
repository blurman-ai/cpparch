void zoo() {
    char x = L'z';
    x = '\x55';
    char16_t c = u'Ä';
    wchar_t b = L'\x1234';
    char32_t d = U'\U00010000';
    char A[] = "Bye\x0D";
    wchar_t B[] = L"Tsch\xFC\x0D";
    char* rawString = R"(
        Destination=File
        Filter="%Severity% >= ERR"
    )";
    auto integer_literal = 9'999''123;
    auto floating_point_literal = 7.250'001'9;
    auto hex_literal = 0xDEAD'beef'1234;
    int b1 = 0B111111;
    int b2 = 0b101010;
}
