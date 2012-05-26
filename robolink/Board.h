#if !defined(Board_h)
#define Board_h

enum BoardId
{
    bidMotorPower = 1,
    bidEstop,
    bidSensors,
    bidUsbLink,

    bidNumBoards
};

class Board
{
public:
    Board(BoardId id);
    virtual ~Board();
    virtual void on_data(char const *data, int nsize);
    virtual void on_nak();

    char data_[64];
    BoardId id_;
    bool online_;

    static Board *board_by_id(BoardId id);
};

#endif  //  Board_h

