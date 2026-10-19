// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <iostream>

#include "import/support/text_encoding.h"

int main()
{
    using Bank = musx::dom::others::FontDefinition::CharacterSetBank;
    using finale_mus_reader::text::toUtf8;

    if (toUtf8("C\x8elino", Bank::MacOS, 0) != "Célino" || toUtf8("C\x8elino", Bank::Windows, 0) != "CŽlino"
        || toUtf8("\x81\x82", Bank::MacOS, 6) != "¹²" || toUtf8("\x80\x81\x82", Bank::MacOS, 7) != "АБВ"
        || toUtf8("\x81\x82", Bank::MacOS, 29) != "Āā"
        || toUtf8("\x83\x71\x83\x89\x83\x4d\x83\x6d\x96\xbe\x92\xa9 Pro W3", Bank::MacOS, 1) != "ヒラギノ明朝 Pro W3") {
        std::cout << "Legacy text conversion failed\n";
        return 1;
    }
    return 0;
}
