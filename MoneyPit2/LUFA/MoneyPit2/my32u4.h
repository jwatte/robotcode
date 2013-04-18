#if !defined(my32u4_h)
#define my32u4_h

#include "AppConfig.h"
#include "LUFAConfig.h"


#define CONNECTED_LED 0x1
#define RECEIVED_LED 0x2
#define POLLING_LED 0x4
#define TWI_ERROR_LED 0x8
#define NUMLEDS 4


//  delay uses TIMER0
void setup_delay(void);
//  delayms() works even while interrupts are off
void delayms(unsigned short ms);
void delayus(unsigned short us);
unsigned short getms(void);
void show_error(unsigned char err, unsigned char info);
void setup_status(void);
void set_status(unsigned char value, unsigned char mask);
void set_status_override(unsigned char value, unsigned char mask);

void setup_adc(void);

#endif  //  my32u4_h
