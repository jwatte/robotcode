
#include <libavr.h>
#include <pins_avr.h>


void read_ping(void *);

class Ping4 {
public:
  Ping4(unsigned char pinTrigger, unsigned char pinPulse) :
    pinTrigger_(pinTrigger),
    pinPulse_(pinPulse)
  {
  }
  unsigned char pinTrigger_;
  unsigned char pinPulse_;
  unsigned short value_;
  unsigned short readTimer_;
  void startRead() {
    //  todo: hook up pin change interrupt
    //  todo: trigger read signal
  }
  static void pin_change(void *p) {
    if (digitalRead(((Ping4 *)p)->pinPulse_)) {
      ((Ping4 *)p)->readTimer_ = uread_timer();
    }
    else {
      ((Ping4 *)p)->value_ = uread_timer() - ((Ping4 *)p)->readTimer_;
      //  todo: unhook pin change interrupt
      after(50, read_ping, 0);
    }
  }
};

Ping4 pings[3] = {
  Ping4(16|5, 16|2),  //  front
  Ping4(16|6, 16|3),  //  left
  Ping4(16|7, 16|4)   //  right
};
unsigned char nPing = 0;

void read_ping(void *)
{
  nPing = (nPing + 1);
  if (nPing == 3) {
    nPing = 0;
  }
  pings[nPing].startRead();
}


unsigned char state = 0;

void blink_laser(void *p)
{
  static unsigned char pins[8] = {
    0, 1, 3, 7, 7, 6, 4, 0
  };
  unsigned char val = pins[state];
  state = (state + 1) & 7;
  digitalWrite(0, (val & 1) ? HIGH : LOW);
  digitalWrite(6, (val & 2) ? HIGH : LOW);
  digitalWrite(7, (val & 4) ? HIGH : LOW);
  after(200, blink_laser, 0);
}

void setup() {
  pinMode(0, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  blink_laser(0);
  read_ping(0);
}

