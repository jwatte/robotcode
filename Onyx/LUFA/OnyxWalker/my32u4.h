#if !defined(my32u4_h)
#define my32u4_h

extern void MY_Setup(void);
extern void MY_DelayUs(unsigned short us);
//  A Tick is 16 microseconds, and is the only unit of timing.
//  The maximum representable interval is just above 1 second.
extern unsigned short MY_GetTicks(void);

#endif  //  my32u4_h
