
#if !defined(UsbComm_h)
#define UsbComm_h

#include <string>
#include "Packet.h"
#include "Signal.h"

namespace boost {
    class thread;
}
struct libusb_context;
struct libusb_device_handle;

class UsbComm : public IPacketDestination {
public:
  UsbComm(std::string const &devName, IPacketDestination *dst);
  ~UsbComm();

  /* start receiving packets */
  bool open();
  /* stop accepting new packets */
  void close();
  /* transmit received packets to the destination */
  void transmit();
  /* send a packet to the USB board */
  void on_packet(Packet *p);

private:

  void read_func();

  std::string name_;
  IPacketDestination *dest_;
  int fd_;
  bool running_;
  boost::thread *thread_;
  libusb_context *ctx_;
  libusb_device_handle *dh_;
  boost::mutex lock_;
  Pipe<Packet, 64> received_;
};

#endif  //  UsbComm_h

