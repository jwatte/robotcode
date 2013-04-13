#if !defined(network_h)
#define network_h

#include <stdlib.h>
#include <sys/uio.h>
#include <boost/shared_ptr.hpp>

class INetwork {
public:
    virtual void step() = 0;
    //  receive() retrieves complete packets from senders, in order received. 
    //  It sets the "response address" to the address the packet was received 
    //  from. If address is locked, only messages from the locked address 
    //  are paid attention to.
    virtual bool receive(size_t &size, void const *&packet) = 0;
    //  broadcast() sends to the locked address. If not locked, will broadcast, 
    //  if the network was created with "scan()" rather than "listen()." It is 
    //  an error to broadcast() without a locked address if network was created 
    //  with "listen()."
    virtual void broadcast(size_t size, void const *packet) = 0;
    //  respond() responds to the packet last retrieved from receive().
    virtual void respond(size_t size, void const *packet) = 0;
    //  vsend() is more efficient if constructing a packet of many pieces.
    //  'response' is false if you want to broadcast.
    virtual void vsend(bool response, size_t cnt, iovec const *vecs) = 0;
    //  lock_address locks the send address to the peer that sent the packet 
    //  last returned by receive(), and filters out messages from others.
    //  If no messages are received within timeout time, address is automatically 
    //  unlocked.
    virtual void lock_address(double timeout) = 0;
    //  unlock_address() starts listening to / returning packets from all remote 
    //  peers without waiting for the timeout.
    virtual void unlock_address() = 0;
    //  Am I locked?
    virtual bool is_locked() = 0;
    //  Have I seen send overflows?
    virtual bool check_clear_overflow() = 0;
    //  Have I seen receive loss?
    virtual void check_clear_loss(int &lost, int &received) = 0;
};

class ITime;
class IStatus;
class ISockets;

INetwork *listen(ISockets *socks, ITime *time, IStatus *status);
INetwork *scan(ISockets *socks, ITime *time, IStatus *status);

//  IPacketizer sits on top of INetwork and provides sub-chunking of network's payload
class IPacketizer {
public:
    virtual void step() = 0;
    virtual bool receive(unsigned char &code, size_t &size, void const *&data) = 0;
    virtual void broadcast(unsigned char code, size_t size, void const *data) = 0;
    virtual void respond(unsigned char code, size_t size, void const *data) = 0;
    virtual void vrespond(unsigned char code, size_t cnt, iovec const *vecs) = 0;
};

IPacketizer *packetize(INetwork *net, IStatus *status);

struct sockaddr_in;

class ISocket {
public:
    virtual size_t peek(void *buf, size_t maxSize) = 0;
    virtual void recvd(size_t maxSize) = 0;
    virtual size_t send(void const *buf, size_t size) = 0;
    virtual bool step() = 0;
    virtual ~ISocket() {}
};

class ISockets {
public:
    virtual int recvfrom(void *buf, size_t sz, sockaddr_in &addr) = 0;
    virtual int sendto(void const *buf, size_t sz, sockaddr_in const &addr) = 0;
    virtual boost::shared_ptr<ISocket> connect(sockaddr_in const &addr) = 0;
    virtual ~ISockets() {}
};

ISockets *mksocks(unsigned short port, IStatus *status);
std::string ipaddr(sockaddr_in const &sin);

#endif  //  network_h

