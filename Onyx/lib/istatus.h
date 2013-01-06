#if !defined(istatus_h)
#define istatus_h

#include <string>

class IStatus {
public:
    virtual void message(std::string const &str) = 0;
    virtual void error(std::string const &str) = 0;
    virtual size_t n_messages() = 0;
    virtual bool get_message(bool &isError, std::string &oMessage) = 0;
};

IStatus *mkstatus();

#endif  //  istatus_h
