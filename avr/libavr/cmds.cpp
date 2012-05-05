
#include <avr/pgmspace.h>
#include "cmds.h"
#include "libavr.h"

char const pnGoOK[] PROGMEM = "GoOK";
char const pnMPower[] PROGMEM = "MPower";
char const pnSteer[] PROGMEM = "Steer";
char const pnEEDump[] PROGMEM = "EEDump";
char const pnTuneSteering[] PROGMEM = "TSteer";

void get_param_name(ParameterName pn, unsigned char bufsz, char *oData)
{
  char const *p = 0;
  switch (pn) {
    case ParamGoAllowed: p = pnGoOK; break;
    case ParamMotorPower: p = pnMPower; break;
    case ParamSteerAngle: p = pnSteer; break;
    case ParamEEDump: p = pnEEDump; break;
    case ParamTuneSteering: p = pnTuneSteering; break;
    default: fatal(FATAL_BAD_PARAM); break;
  }
  strncpy_P(oData, p, bufsz);
  oData[bufsz-1] = 0;
}

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

void format_value(cmd_parameter_value const &pv, unsigned char bufsz, char *oData)
{
  *oData = 0;
  switch (pv.type) {
  case TypeNone: return;
  case TypeString: strncpy(oData, (char const *)pv.value, bufsz); break;
  case TypeRaw: nybbles(oData, bufsz, &pv.value[1], pv.value[0]); break;
  default: nybbles(oData, bufsz, pv.value, pv.type); break;
  }
  oData[bufsz-1] = 0;
}

