#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <iostream>

#include "Board.h"
#include "Voltage.h"
#include "Parser.h"


#define SYNC_BYTE ((unsigned char)0xed)


Parser::Parser()
{
}

Parser::~Parser()
{
}

void Parser::on_packet(Packet *p)
{
    /*
    std::cerr << "on_packet()";
    for (unsigned char const *ptr = (unsigned char const *)p->buffer(),
        *end = (unsigned char const *)p->buffer() + p->size();
        ptr != end; ++ptr) {
        std::cerr << " " << std::hex << (int)*ptr;
    }
    std::cerr << std::endl;
    */
    unsigned int psz = p->size();
    unsigned char const *pbuf = p->buffer();
    while (psz > 0) {
        unsigned int sz = psz;
        if (sz + bufsz > sizeof(buf)) {
            sz = sizeof(buf) - bufsz;
        }
        memcpy(&buf[bufsz], pbuf, sz);
        bufsz += sz;
        pbuf += sz;
        psz -= sz;
        if (bufsz < 3) {
            break;
        }
        unsigned int skip = 0;
        for (unsigned int i = 0; i < bufsz-2; ++i) {
            if (buf[i] == SYNC_BYTE && buf[i+1] <= (bufsz - i - 2)) {
                decode(&buf[i], buf[i+1]+2);
                i += buf[i+1] + 1;
                skip = i + 1;
            }
        }
        if (skip == 0) {
            while (skip < bufsz && buf[skip] != SYNC_BYTE) {
                ++skip;
            }
        }
        if (skip > 0) {
            if (skip < bufsz) {
                memmove(&buf[0], &buf[skip], bufsz - skip);
            }
            bufsz -= skip;
        }
    }
    p->destroy();
}

/* UART protocol:

  0xed <length> <cmdbyte> <data> -- length is length(cmdbyte + data)

 Usbboard->Host

  CMD
  O               Online
  F Node Code     Node Fatalled with Code
  D Node <data>   Data packet from Node
  N Node          Nak from Node

 */


struct format {
    char code;
    unsigned char size;
    void (*dispatch)(unsigned char const *buf, int len);
};

void do_online(unsigned char const *buf, int nsize)
{
    fprintf(stderr, "online\n");
}

void do_fatal(unsigned char const *buf, int nsize)
{
    fprintf(stderr, "node 0x%02x fatal 0x%02x\n", (unsigned char)buf[1], (unsigned char)buf[2]);
    Board *b = Board::board_by_id((BoardId)buf[1]);
    b->dead((unsigned char)buf[2]);
}

void do_nodedata(unsigned char const *buf, int nsize)
{
    Board *b = Board::board_by_id((BoardId)buf[1]);
    b->on_data(buf + 2, nsize - 2);
}

void do_nodenak(unsigned char const *buf, int nsize)
{
    std::cerr << "Nak node " << (int)buf[1] << std::endl;
    Board *b = Board::board_by_id((BoardId)buf[1]);
    b->on_nak();
}

format codes[] = {
    { 'O', 1, do_online },
    { 'F', 3, do_fatal },
    { 'D', 2, do_nodedata },
    { 'N', 2, do_nodenak },
};

int decode(unsigned char const *buf, int size)
{
    if (size < 3) {
        //  short packet
        std::cerr << "Short packet (" << size << " bytes)" << std::endl;
        return 0;
    }
    if (buf[0] != SYNC_BYTE) {
        std::cerr << "Bad sync byte: 0x" << std::hex << buf[0] << std::dec << std::endl;
        return -1;
    }
    for (size_t i = 0; i != sizeof(codes)/sizeof(codes[0]); ++i)
    {
        if (codes[i].code == buf[2])
        {
            int nsize = codes[i].size;
            if (size < nsize + 2)
            {
                std::cerr << "Bad packet size: " << size << " expected " << (nsize + 2) << " for " << buf[2] << std::endl;
                return -1;
            }
            //  dispatch will include the letter code
            codes[i].dispatch(buf+2, size-2);
            return size;
        }
    }
    //  This is unlikely to ever work -- will overflow because it's not recognized.
    //  Thus, just drop this one character and look for something I can recognize.
    std::cerr << "Unknown command: " << std::hex << buf[1] << std::dec << std::endl;
    //  skip the sync byte
    return 0;
}


