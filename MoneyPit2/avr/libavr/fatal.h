#if !defined(libavr_fatal_h)
#define libavr_fatal_h

/* fatal API */

#define F_MISC(x)       (0x00 | (x))
#define F_SERIAL(x)     (0x10 | (x))
#define F_TWI(x)        (0x20 | (x))
#define F_HARDWARE(x)   (0x30 | (x))
#define F_TIMER(x)      (0x40 | (x))
#define F_ADC(x)        (0x50 | (x))
#define F_UI(x)         (0x60 | (x))

#define FE_TOOBIG       0x1
#define FE_BADPARAM     0x2
#define FE_BADCALL      0x3
#define FE_BUSY         0x4
#define FE_UNEXPECTED   0x5
#define FE_REUSE        0x6
#define FE_TIMEOUT      0x7

enum
{
    FATAL_PURE_VIRTUAL =        F_MISC(FE_BADCALL),
    FATAL_BAD_ARGS =            F_MISC(FE_BADPARAM),
    FATAL_UNEXPECTED =          F_MISC(FE_UNEXPECTED),
    FATAL_DOUBLE_ENQUEUE =      F_MISC(FE_REUSE),

    FATAL_TOO_BIG_SERIAL =      F_SERIAL(FE_TOOBIG),
    FATAL_BAD_SERIAL =          F_SERIAL(FE_BADPARAM),
    FATAL_UNEXPECTED_SERIAL =   F_SERIAL(FE_UNEXPECTED),

    FATAL_BAD_PIN =             F_HARDWARE(FE_BADPARAM),

    FATAL_TOO_LONG_DELAY =      F_TIMER(FE_BADPARAM),
    FATAL_OUT_OF_AFTERS =       F_TIMER(FE_TOOBIG),

    FATAL_ADC_BADCALL =         F_ADC(FE_BADCALL),
    FATAL_ADC_BUSY =            F_ADC(FE_BUSY),
    FATAL_ADC_BAD_CHANNEL =     F_ADC(FE_BADPARAM),

    FATAL_TWI_BUSY =            F_TWI(FE_BUSY),
    FATAL_TWI_TOO_BIG =         F_TWI(FE_TOOBIG),
    FATAL_TWI_BAD_CALL =        F_TWI(FE_BADCALL),
    FATAL_TWI_UNEXPECTED =      F_TWI(FE_UNEXPECTED),
    FATAL_TWI_TIMEOUT =         F_TWI(FE_TIMEOUT),

    FATAL_UI_BAD_PARAM =        F_UI(FE_BADPARAM),
};

/* calling fatal stops the program cold, recording the reason code in eeprom  */
extern void fatal(int err) __attribute__((noreturn));
extern void fatal_set_blink(void (*func)(bool on));


#endif  //  libavr_fatal_h
