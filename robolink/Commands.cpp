
#include <stdio.h>

#include "Commands.h"
#include "Board.h"
#include "Voltage.h"


/* UART protocol:

 Usbboard->Host
 O                      On, running
 F <code>               Fatal death, will reboot in 8 seconds
 D <node> <len> <data>  Data polled from node
 N <node>               Nak from node when polling
 R <sensor> <distance>  Distance reading from ranging sensor
 V <value>              Local voltage value, ff == 16V, 00 == 8V
 X <len> <text>         Debug text

 Host->Usbboard


 */

struct format {
    char code;
    unsigned char size;
    unsigned char sizeoffset;
    void (*dispatch)(char const *buf, int len);
};

void do_online(char const *buf, int nsize)
{
    fprintf(stderr, "online\n");
}

void do_fatal(char const *buf, int nsize)
{
    fprintf(stderr, "fatal 0x%02x\n", (unsigned char)buf[1]);
    Board *b = Board::board_by_id((BoardId)bidUsbLink);
    b->dead((unsigned char)buf[1]);
}

void do_nodedata(char const *buf, int nsize)
{
    Board *b = Board::board_by_id((BoardId)buf[1]);
    b->on_data(buf + 3, nsize - 3);
    if (buf[1] == bidMotorPower)
    {
        //  todo: this function shouldn't know about a particular board
        voltage.on_motor_voltage((unsigned char)buf[3] / 16.0f);
    }
}

void do_nodenak(char const *buf, int nsize)
{
    Board *b = Board::board_by_id((BoardId)buf[1]);
    b->on_nak();
}

void do_range(char const *buf, int nsize)
{
}

void do_voltage(char const *buf, int nsize)
{
    float v = (unsigned char)buf[1] / 16.0f;
    fprintf(stderr, "voltage %.2f\n", v);
    voltage.on_main_voltage(v);
}

void do_debug(char const *buf, int nsize)
{
    fprintf(stderr, "debug %.*s", nsize-2, buf+2);
}



format codes[] = {
    { 'O', 1, 0, do_online },
    { 'F', 2, 0, do_fatal },
    { 'D', 3, 2, do_nodedata },
    { 'N', 2, 0, do_nodenak },
    { 'R', 3, 0, do_range },
    { 'V', 2, 0, do_voltage },
    { 'X', 2, 1, do_debug }
};

int decode(char const *buf, int size)
{
    if (size < 1)
    {
        return 0;
    }
    for (size_t i = 0; i != sizeof(codes)/sizeof(codes[0]); ++i)
    {
        if (codes[i].code == buf[0])
        {
            int nsize = codes[i].size;
            if (size < nsize)
            {
                return 0;
            }
            if (codes[i].sizeoffset != 0)
            {
                nsize += (unsigned char)buf[codes[i].sizeoffset];
                if (size < nsize)
                {
                    return 0;
                }
            }
            codes[i].dispatch(buf, nsize);
            return nsize;
        }
    }
    //  This is unlikely to ever work -- will overflow because it's not recognized.
    //  Thus, just drop this one character and look for something I can recognize.
    fprintf(stderr, "unknown command: 0x%02x\n", (unsigned char)buf[0]);
    return 1;
}

