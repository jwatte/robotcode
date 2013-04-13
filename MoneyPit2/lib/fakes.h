#if !defined(fakes_h)
#define fakes_h

#include "inetwork.h"
#include "istatus.h"
#include "itime.h"
#include <list>
#include <vector>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 

class Fakenet : public INetwork {
public:
    Fakenet();
    virtual void step();
    virtual bool receive(size_t &size, void const *&packet);
    virtual void broadcast(size_t size, void const *packet);
    virtual void respond(size_t size, void const *packet);
    virtual void vsend(bool response, size_t cnt, iovec const *vecs);
    virtual void lock_address(double timeout);
    virtual void unlock_address();
    virtual bool is_locked();
    virtual bool check_clear_overflow();
    virtual void check_clear_loss(int &, int &);

    size_t stepCnt_;
    std::list<std::pair<size_t, void const *>> toReceive_;
    std::list<std::pair<bool, std::vector<char>>> wereSent_;
    bool wasLocked_;
    bool wasUnlocked_;
    bool locked_;
    double timeout_;
};

class Fakestatus : public IStatus {
public:
    virtual void message(std::string const &str);
    virtual void error(std::string const &str);
    virtual size_t n_messages();
    virtual bool get_message(Message &om);

    std::list<Message> messages_;
};

class Fakesockets;

class Fakesocket : public ISocket {
public:
    virtual size_t peek(void *buf, size_t maxSize);
    virtual void recvd(size_t maxSize);
    virtual size_t send(void const *buf, size_t size);
    virtual bool step();
    ~Fakesocket();

    sockaddr_in addr_;
    std::list<std::vector<char>> toRecv_;
    std::list<std::vector<char>> sent_;
    Fakesockets *owner_;
    bool connected_;
};

class Fakesockets : public ISockets {
public:
    virtual boost::shared_ptr<ISocket> connect(sockaddr_in const &addr);
    std::list<Fakesocket *> sockets_;

    virtual int recvfrom(void *buf, size_t sz, sockaddr_in &addr);
    virtual int sendto(void const *buf, size_t sz, sockaddr_in const &addr);
    struct rec {
        rec() {
            ret = 0;
            memset(&addr, 0, sizeof(addr));
        }
        int ret;
        sockaddr_in addr;
        std::vector<char> data;
    };
    std::list<rec> toReceive_;
    std::list<rec> wasSent_;

};

class Faketime : public ITime {
public:
    Faketime();
    virtual double now();
    virtual void sleep(double dt);

    double timeSlept_;
    double time_;
    double dTime_;
};

#endif  //  fakes_h

