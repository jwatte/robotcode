#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "Parser.h"
#include "Commands.h"

Parser::Parser()
{
    memset(buf_, 0, sizeof(buf_));
    bufptr_ = 0;
}

Parser::~Parser()
{
}

void Parser::on_char(char ch)
{
    buf_[bufptr_++] = ch;
    int d = check_buf();
    if (d > 0)
    {
        assert(d <= bufptr_);
        memmove(buf_, &buf_[d], bufptr_ - d);
        bufptr_ -= d;
    }
    if (bufptr_ == sizeof(buf_))
    {
        fprintf(stderr, "Too long command buffer; flushing\n");
        bufptr_ = 0;
        memset(buf_, 0, sizeof(buf_));
    }
}

int Parser::check_buf()
{
    return decode(buf_, bufptr_);
}

