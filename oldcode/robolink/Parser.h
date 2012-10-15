#if !defined(Parser_h)
#define Parser_h

#include "Packet.h"

class Parser : public IPacketDestination {
public:
  Parser();
  ~Parser();

  void on_packet(Packet *p);

  unsigned char buf[258];
  unsigned int bufsz;
};

extern int decode(unsigned char const *buf, int size);

#endif  //  Parser_h

