
#include <assert.h>
#include <string.h>

#include "Board.h"

static Board *boardIds_[bidNumBoards];

Board::Board(BoardId id)
{
    assert(id >= 0 && id < sizeof(boardIds_)/sizeof(boardIds_[0]));
    assert(boardIds_[id] == 0);
    boardIds_[id] = this;
    memset(data_, 0, sizeof(data_));
    id_ = id;
    online_ = false;
}

Board::~Board()
{
    boardIds_[id_] = 0;
}

void Board::on_data(char const *data, int nsize)
{
    online_ = true;
}

void Board::on_nak()
{
    online_ = false;
}

Board *Board::board_by_id(BoardId id)
{
    assert(id >= 0 && id < sizeof(boardIds_)/sizeof(boardIds_[0]));
    return boardIds_[id];
}

