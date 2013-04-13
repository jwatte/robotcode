
#include "istatus.h"
#include "itime.h"
#include <iostream>
#include <list>

class Status : public IStatus {
public:
    Status(ITime *time, bool local) {
        time_ = time;
        local_ = local;
    }
    virtual void message(std::string const &str) {
        if (str == lastmsg_) {
            return;
        }
        lastmsg_ = str;
        double now = time_->now();
        if (local_) {
            std::cerr << "[msg] " << now << " " << str << std::endl;
        }
        messages_.push_back(Message(now, false, str));
    }

    virtual void error(std::string const &str) {
        double now = time_->now();
        if (local_) {
            std::cerr << "[ERR] " << now << " " << str << std::endl;
        }
        messages_.push_back(Message(now, true, str));
    }

    virtual size_t n_messages() {
        return messages_.size();
    }
    virtual bool get_message(Message &oMessage) {
        if (messages_.empty()) {
            return false;
        }
        oMessage = messages_.front();
        messages_.pop_front();
        return true;
    }

    ITime *time_;
    bool local_;
    std::list<Message> messages_;
    std::string lastmsg_;
};

IStatus *mkstatus(ITime *time, bool local) {
    return new Status(time, local);
}
