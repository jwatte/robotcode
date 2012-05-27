
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "Board.h"

static Board *boardIds_[bidNumBoards];
//  This breaks the open/closed principle
static char const *boardLabels_[bidNumBoards] = {
    "None",
    "Motor/Power",
    "E-stop",
    "Sensors",
    "USB Link"
};

Board::Board(BoardId id)
{
    assert(id >= 0 && id < sizeof(boardIds_)/sizeof(boardIds_[0]));
    assert(boardIds_[id] == 0);
    boardIds_[id] = this;
    memset(data_, 0, sizeof(data_));
    id_ = id;
    online_ = false;
    code_ = 0;
}

Board::~Board()
{
    boardIds_[id_] = 0;
}

void Board::on_data(char const *data, int nsize)
{
    if (nsize > sizeof(data_))
    {
        fprintf(stderr, "on_data(): nsize %d too big!\n", nsize);
        nsize = sizeof(data_);
    }
    code_ = 0;
    online_ = true;
    memcpy(data_, data, nsize);
    dataSize_ = nsize;
    invalidate();
}

void Board::on_nak()
{
    online_ = false;
    invalidate();
}

void Board::dead(unsigned char code)
{
    online_ = false;
    code_ = code;
    invalidate();
}


Board *Board::board_by_id(BoardId id)
{
    assert(id >= 0 && id < sizeof(boardIds_)/sizeof(boardIds_[0]));
    return boardIds_[id];
}

char const *Board::label()
{
    return boardLabels_[id_];
}

