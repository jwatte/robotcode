#if !defined(rl2_Boards_h)
#define rl2_Boards_h

#include "Board.h"
#include <stdexcept>

class MotorBoard : public cast_as_impl<Board, MotorBoard> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    MotorBoard();
};

struct ir_fun;

class InputBoard : public cast_as_impl<Board, InputBoard> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    InputBoard(ir_fun const &tune_ir);
};

class USBBoard : public cast_as_impl<Board, USBBoard> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    USBBoard(double top_voltage);
};

class IMUBoard : public cast_as_impl<Board, IMUBoard> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    IMUBoard();
};

class DisplayBoard : public cast_as_impl<Board, DisplayBoard> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);

    void draw_text(std::string const &str, unsigned short left, unsigned short top, unsigned short color, unsigned short bgcolor);
    void fill_rect(unsigned short left, unsigned short top, unsigned short right, unsigned short bottom, unsigned short color);
private:
    DisplayBoard();
    void set_colors(unsigned short front, unsigned short back);
    template<typename T> 
    void cmd(T const &t, size_t sz = sizeof(T)) {
        if (sz > 32) {
            throw std::runtime_error("Too long command in DisplayBoard::cmd()");
        }
        return_->raw_cmd(&t, sz);
    }

    unsigned short fcolor_;
    unsigned short bgcolor_;
};

//  Ugh -- a direct dependency!
extern boost::shared_ptr<Module> gUSBLink;
extern boost::shared_ptr<Module> gMotorBoard;
extern boost::shared_ptr<Module> gInputBoard;
extern boost::shared_ptr<Module> gUSBBoard;
extern boost::shared_ptr<Module> gIMUBoard;
extern boost::shared_ptr<Module> gDisplayBoard;


#endif  //  rl2_Boards_h
