#if !defined(BoardTile_h)
#define BoardTile_h

#include "Talker.h"
#include <FL/Fl_Group.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>

class Board;
class MotorPowerBoard;

class BoardTile : public Listener
{
public:
    BoardTile(Board *board);
    virtual void invalidate();
    void make_widgets();

    Board *board_;
    Fl_Group *group_;
    Fl_Box *image_;
    Fl_Box *code_;
    Fl_Text_Display *data_;
    Fl_Text_Buffer *dataBuf_;
    char codeText_[10];
    char dataText_[64 * 3 + 1];

    static Fl_Image *online_;
    static Fl_Image *offline_;

protected:
    virtual void make_widgets_inner();
};


class MotorPowerBoardTile : public BoardTile
{
public:
    MotorPowerBoardTile(MotorPowerBoard *board);

protected:
    void make_widgets_inner();
};

#endif  //  BoardTile_h

