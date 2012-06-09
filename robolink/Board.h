#if !defined(Board_h)
#define Board_h

#include "Talker.h"
#include "Value.h"

enum BoardId
{
    bidMotorPower = 1,
    bidEstop,
    bidSensors,
    bidUsbLink,

    bidNumBoards
};

class Board : public Talker
{
public:
    Board(BoardId id);
    virtual ~Board();
    virtual void on_data(char const *data, int nsize);
    virtual void on_nak();
    char const *label();
    virtual void dead(unsigned char code);

    char data_[64];
    BoardId id_;
    bool online_;
    unsigned char code_;
    unsigned char dataSize_;

    static Board *board_by_id(BoardId id);
};

class MotorPowerBoard : public Board
{
public:
    MotorPowerBoard();
    virtual void on_data(char const *data, int nsize);

    Value<unsigned char> voltage_;
    Value<char> power_;
    Value<char> steering_;
    Value<bool> allowed_;
};

class UsbLinkBoard : public Board
{
public:
    UsbLinkBoard();
    virtual void on_data(char const *data, int nsize);

    Value<unsigned char> voltage_;
};

class SensorBoard : public Board
{
public:
    SensorBoard();
    virtual void on_data(char const *data, int nsize);

    Value<unsigned char> cliffDetect_;
    Value<unsigned char> leftDetect_;
    Value<unsigned char> rightDetect_;
    Value<unsigned char> leftWedge_;
    Value<unsigned char> rightWedge_;
    Value<unsigned char> backWedge_;
};

#endif  //  Board_h

