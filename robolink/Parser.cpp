#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <iostream>

#include "Board.h"
#include "Voltage.h"
#include "Parser.h"


Parser::Parser()
{
}

Parser::~Parser()
{
}

void Parser::on_packet(Packet *p)
{
    decode(p->buffer(), p->size());
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

#define SYNC_BYTE ((unsigned char)0xed)


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


