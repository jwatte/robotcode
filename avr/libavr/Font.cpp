
#include "lcd.h"


char Font::buf_[sizeof(Font::buf_)];

Font::Font(void const *data) :
    data_(data) {
    memcpy_PF(&desc_, (uint_farptr_t)data_, sizeof(desc_));
}

unsigned char Font::min_char() const {
    return desc_.min_char;
}

unsigned char Font::max_char() const {
    return desc_.max_char;
}

unsigned char Font::height() const {
    return desc_.height;
}

bool Font::get_char(unsigned char val, void const *&oPtr, unsigned char &ow) const {
    oPtr = buf_;
    bool ret = true;
    if (val < min_char() || val > max_char()) {
        val = min_char();
        ret = false;
    }
    unsigned short offset = pgm_read_word(&((font_desc const *)data_)->offset[val - min_char()]);
    unsigned short offset2 = pgm_read_word(&((font_desc const *)data_)->offset[val - min_char() + 1]);
    //  Don't overwrite past buf_ -- better to get corrupt data on screen, 
    //  but not smash other memory.
    if (offset2 > offset + sizeof(buf_)) {
        offset2 = offset + sizeof(buf_);
    }
    memcpy_PF(buf_, (uint_farptr_t)((char const *)data_ + offset), offset2 - offset);
    //  The width is a little wonky for small fonts. For example:
    //  For a 4-height font, I can't tell width 1 or 2 apart.
    //  For a 6-height font, I can't tell width 3 or 4 apart.
    //  For a 7-height font, I can't tell width 7 or 8 apart.
    //  For 8-height and better, I'm good.
    ow = ret ? (offset2 - offset) * 8 / height() : 1;
    return ret;
}

