#include "BoardTile.h"
#include "Board.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Shared_Image.H>


#define TILE_WIDTH      250
#define TILE_HEIGHT     150


Fl_Image *BoardTile::online_;
Fl_Image *BoardTile::offline_;

BoardTile::BoardTile(Board *b) :
    board_(b)
{
    b->add_listener(this);
    if (!online_)
    {
        online_ = Fl_Shared_Image::get("/usr/local/src/robotcode/robolink/online.png");
    }
    if (!offline_)
    {
        offline_ = Fl_Shared_Image::get("/usr/local/src/robotcode/robolink/offline.png");
    }
}

void BoardTile::invalidate()
{
    image_->image(board_->online_ ? online_ : offline_);
    image_->redraw();
    sprintf(codeText_, "0x%02x", board_->code_);
    code_->label(codeText_);
    code_->labelcolor(board_->code_ ? FL_RED : FL_BLACK);
    for (size_t i = 0; i != board_->dataSize_; ++i)
    {
        sprintf(&dataText_[i * 3], "%02x%c", (unsigned char)board_->data_[i],
            ((i & 7) == 7) ? '\n' : ' ');
    }
    dataBuf_->text(dataText_);
}

void BoardTile::make_widgets()
{
    group_ = new Fl_Group(0, 0, TILE_WIDTH, TILE_HEIGHT, board_->label());
    group_->box(FL_BORDER_BOX);
    image_ = new Fl_Box(5, 0, 25, 25);
    image_->image(offline_);
    code_ = new Fl_Box(35, 0, 35, 25);
    code_->labelfont(FL_COURIER);
    data_ = new Fl_Text_Display(75, 1, TILE_WIDTH - 76, 30);
    data_->textsize(10);
    data_->textfont(FL_COURIER);
    data_->color(FL_GRAY);
    data_->box(FL_BORDER_FRAME);
    dataBuf_ = new Fl_Text_Buffer();
    data_->buffer(dataBuf_);

    make_widgets_inner();

    group_->end();
}

void BoardTile::make_widgets_inner()
{
}






MotorPowerBoardTile::MotorPowerBoardTile(MotorPowerBoard *mpb) :
    BoardTile(mpb)
{
}

void MotorPowerBoardTile::make_widgets_inner()
{
}

