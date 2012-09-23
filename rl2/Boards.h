#if !defined(rl2_Boards_h)
#define rl2_Boards_h

#include "Board.h"

class MotorBoard : public Board {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    MotorBoard();
};

struct ir_fun;

class InputBoard : public Board {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    InputBoard(ir_fun const &tune_ir);
};

class USBBoard : public Board {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    USBBoard(double top_voltage);
};

class IMUBoard : public Board {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
private:
    IMUBoard();
};

#endif  //  rl2_Boards_h
