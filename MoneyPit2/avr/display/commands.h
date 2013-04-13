#if !defined(display_commands_h)
#define display_commands_h

/*
  For now, just drive the display directly.
 */
enum {
    cmdSetColors = 0x21,
    cmdDrawText = 0x22,
    cmdFillRect = 0x23
};

struct cmd_SetColors {
    unsigned char cmd;
    unsigned short front;
    unsigned short back;
};

struct cmd_DrawText {
    unsigned char cmd;
    unsigned char x;
    unsigned char y;
    //  the high bit of 'len' is the lowest bit of y
    unsigned char len;
    char text[26];
};

enum {
    fillFlagFront = 1,
    fillFlagHeight = 0x40,
    fillFlagYCoord = 0x80
};
struct cmd_FillRect {
    unsigned char cmd;
    unsigned char x;
    unsigned char y;
    unsigned char w;
    unsigned char h;
    unsigned char flags;   //  high bit is low bit of y, low bit is front/back
};


#endif  //  display_commands_h
