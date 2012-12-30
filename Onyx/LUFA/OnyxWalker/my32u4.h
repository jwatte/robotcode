#if !defined(my32u4_h)
#define my32u4_h

#define BRATE_9600 3    //  really, 1/2 Mbps
#define BRATE_57600 2   //  really, 2/3 Mbps
#define BRATE_1000000 1
#define BRATE_2000000 0

void setup_uart(unsigned char brate);
void send_sync(unsigned char const *data, unsigned char size);
unsigned char recv_avail(void);
unsigned char const *recv_buf(void);
void recv_eat(unsigned char cnt);
unsigned char get_nmissed(void);
void clear_rbuf(void);
void add_rbuf(unsigned char const *buf, unsigned char cnt);

//  delay uses TIMER0
void setup_delay(void);
//  delayms() works even while interrupts are off
void delayms(unsigned short ms);
void delayus(unsigned short us);
void show_error(unsigned char err, unsigned char info);

#endif  //  my32u4_h
