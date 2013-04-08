
#include "istatus.h"
#include "logger.h"
#include <stdexcept>


class LogStatus : public IStatus {
public:
    LogStatus(IStatus *chain) :
        chain_(chain)
    {
    }
    void message(std::string const &str) {
        chain_->message(str);
    }
    void error(std::string const &str) {
        log(LogKeyError, str.c_str(), str.size());
        chain_->message(str);
    }
    size_t n_messages() {
        return chain_->n_messages();
    }
    bool get_message(Message &o_message) {
        return chain_->get_message(o_message);
    }

    IStatus *chain_;
};

IStatus *mk_logstatus(IStatus *chain) {
    if (chain == nullptr) {
        throw std::runtime_error("logstatus requires chained status");
    }
    return new LogStatus(chain);
}

