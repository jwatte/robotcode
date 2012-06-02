
#include <avr/pgmspace.h>
#include "cmds.h"
#include "libavr.h"

char hexchar(unsigned char nybble)
{
    nybble = nybble & 0xf;
    if (nybble < 10) return '0' + nybble;
    return 'A' + (nybble - 10);
}

void nybbles(char *oData, unsigned char bsz, unsigned char const *value, unsigned char vsz)
{
    if (vsz*2+1 > bsz) {
        vsz = (bsz-1)/2;
    }
    while (vsz > 0) {
        vsz--;
        *oData++ = hexchar(*value >> 4);
        *oData++ = hexchar(*value);
        value++;
    }
    *oData = 0;
}

void format_value(void const *src, RegType type, unsigned char bufsz, char *oData)
{
    *oData = 0;
    unsigned char v = *(unsigned char *)src;
    switch (type) {
    default:
        fatal(FATAL_BAD_USAGE);
    case RegTypeUnknown:
        return;
    case RegTypeUchar:
        if (bufsz < 3) return;
        oData[0] = hexchar(v >> 4);
        oData[1] = hexchar(v & 0xf);
        oData[2] = 0;
        return;
    case RegTypeUchar16:
        if (bufsz < 4) return;
        oData[0] = hexchar(v >> 4);
        oData[1] = '.';
        oData[2] = hexchar(v & 0xf);
        oData[3] = 0;
        return;
    case RegTypeSchar:
        if (bufsz < 4) return;
        if (v & 0x80) {
            v = (v ^ ~0) + 1;
            *oData++ = '-';
        }
        *oData++ = hexchar(v >> 4);
        *oData++ = hexchar(v & 0xf);
        *oData = 0;
        return;
    case RegTypeSshort:
    case RegTypeUshort:
        if (bufsz < 5) return;
        nybbles(oData, 5, (unsigned char const *)src, 2);
        return;
    }
}


