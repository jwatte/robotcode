
#if !defined(libavr_cmds_h)
#define libavr_cmds_h

/* there really should be a librobo in addition to libavr ... */

#define ESTOP_RF_CHANNEL 48
#define ESTOP_RF_ADDRESS 1228

#define CMD_PARAMETER_VALUE 1
#define CMD_STOP_GO 2

enum ParameterType {
  TypeNone,
  TypeByte,
  TypeShort,
  TypeString,
  TypeLong,
  TypeRaw
};
enum ParameterName {
  ParamGoAllowed,
  ParamMotorPower,
  ParamSteerAngle,
  ParamEEDump
};
enum Node {
  NodeAny,
  NodeMotorPower,
  NodeEstop,
  NodeSensorInput,
  NodeUSBInterface
};

struct cmd_hdr {
  unsigned char cmd;
  unsigned char fromNode;
  unsigned char toNode;
};
struct cmd_parameter_value : cmd_hdr {
  unsigned char parameter;
  unsigned char type;
  unsigned char value[27];
};

struct cmd_stop_go : cmd_hdr {
  unsigned char go;
};

void get_param_name(ParameterName pn, unsigned char bufsz, char *oData);
void format_value(cmd_parameter_value const &pv, unsigned char bufsz, char *oData);
char hexchar(unsigned char nybble);

#endif // libavr_cmds_h

