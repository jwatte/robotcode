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

struct cmd_FillRect {
    unsigned char cmd;
    unsigned char x;
    unsigned char y;
    unsigned char y2;
};

#endif  //  display_commands_h
