#if !defined(istatus_h)
#define istatus_h

#include <string>

struct Message {
    Message() : timestamp(0), isError(false), message("") {}
    Message(double ts, bool e, std::string const &s) : timestamp(ts), isError(e), message(s) {}
    double timestamp;
    bool isError;
    std::string message;
};

class IStatus {
public:
    virtual void message(std::string const &str) = 0;
    virtual void error(std::string const &str) = 0;
    virtual size_t n_messages() = 0;
    virtual bool get_message(Message &o_message) = 0;
};

//  Set "local" to "true" if you want to print to stderr.
class ITime;

IStatus *mkstatus(ITime *time, bool local);

#endif  //  istatus_h
