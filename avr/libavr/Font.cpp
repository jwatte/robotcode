
#include "lcd.h"

Font::Font(void const *data) :
    data_(data) {
}

unsigned char Font::min_char() const {
    return ((font_desc const *)data_)->first_char;
}

unsigned char Font::max_char() const {
    return ((font_desc const *)data_)->first_char +
        ((font_desc const *)data_)->num_chars - 1;
}

unsigned char Font::height() const {
    return ((font_desc const *)data_)->height;
}

bool Font::get_char(unsigned char val, void const *&oPtr, unsigned char &ow) const {
    if (val < min_char() || val > max_char()) {
        oPtr = (char const *)data_ + sizeof(font_desc) + sizeof(unsigned short) *
            (max_char() - min_char() + 2);
        ow = 1;
        return false;
    }
    unsigned short offset = ((font_desc const *)data_)->offset[val - min_char()];
    unsigned short offset2 = ((font_desc const *)data_)->offset[val - min_char() + 1];
    oPtr = (char const *)data_ + offset;
    //  The width is a little wonky for small fonts:
    //  For a 4-height font, I can't tell width 1 or 2 apart.
    //  For a 6-height font, I can't tell width 3 or 4 apart.
    ow = (offset2 - offset) * 8 / height();
    return true;
}

