
#include "istatus.h"
#include <iostream>
#include <list>


class Status : public IStatus {
public:
    virtual void message(std::string const &str) {
        if (str == lastmsg_) {
            return;
        }
        lastmsg_ = str;
        std::cerr << "[msg] " << str << std::endl;
        messages_.push_back(std::pair<bool, std::string>(false, str));
    }

    virtual void error(std::string const &str) {
        std::cerr << "[ERR] " << str << std::endl;
        messages_.push_back(std::pair<bool, std::string>(true, str));
    }

    virtual size_t n_messages() {
        return messages_.size();
    }
    virtual bool get_message(bool &isError, std::string &oMessage) {
        if (messages_.empty()) {
            isError = false;
            return false;
        }
        isError = messages_.front().first;
        oMessage = messages_.front().second;
        messages_.pop_front();
        return true;
    }

    std::list<std::pair<bool, std::string>> messages_;
    std::string lastmsg_;
};

IStatus *mkstatus() {
    return new Status();
}
