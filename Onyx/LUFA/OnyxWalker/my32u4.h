#if !defined(my32u4_h)
#define my32u4_h

enum {
    LED_con = 0,
    LED_act = 1,
    LED_err = 2
};
extern void MY_Failure(char const *msg, unsigned char da, unsigned char db);
extern void MY_SetLed(unsigned char led, unsigned char state);
extern void MY_SetLedAll(unsigned char state);

extern void MY_Setup(void);
extern void MY_DelayUs(unsigned short us);
//  A Tick is 16 microseconds, and is the only unit of timing.
//  The maximum representable interval is just above 1 second.
extern unsigned short MY_GetTicks(void);
extern unsigned short MY_DelayTicks(unsigned short ticks);

#endif  //  my32u4_h
