#if !defined(littlewalker_defs_h)
#define littlewalker_defs_h

#include <stdlib.h>
#include <netinet/in.h>

#define CONTROL_PORT 7331

#define VIDEO_WIDTH 432
#define VIDEO_HEIGHT 240

#define RIGHT_CENTER 3200
#define LEFT_CENTER 3200
#define CENTER_CENTER 3100

//  in half-microseconds
#define PWM_FREQ 30000

#define WALK_EXTENT 560
#define LIFT_EXTENT 800


#define DATA_INFO_EPNUM 0x81
#define DATA_IN_EPNUM 0x82
#define DATA_OUT_EPNUM 0x03

#define CMD_DDR 1
#define CMD_POUT 2
#define CMD_PIN 3
#define CMD_TWOBYTEARG 8
#define CMD_PWMRATE CMD_TWOBYTEARG
#define CMD_SETPWM 9
#define CMD_WAIT 10
#define CMD_TWOARGS 16
#define CMD_LERPPWM CMD_TWOARGS
#define CMD_TWOTWOBYTEARG 24
#define CMD_SETMINMAX CMD_TWOTWOBYTEARG



struct packet_hdr {
    unsigned char cmd;
};

class PacketHandler {
public:
    virtual void onPacket(packet_hdr const *hdr, size_t size, sockaddr_in const *from) = 0;
};

enum ctr_id {
    ctrFastErrors = 0,
    ctrSlowErrors = 1,
    ctrPackets = 2,
    ctrPings = 3,
    ctrForward = 4,
    ctrTurn = 5
};

enum ledtype {
    lRunning = 0,
    lConnected = 1,
    lForward = 2,
    lBackward = 3
};

enum {
    cPing = 1,
    cPong = 2,
    cControl = 3,
    cFire = 5,
    cReports = 6,
    cCamera = 7,
    cPower = 8,
    cReport = 9,
    cFrame = 10,
    cTune = 11,
    cGetAllTune = 12,
    cAllTune = 13,
    cSetMinMax = 14,
    cSetPWM = 15
};

enum {
    typeController = 1,
    typeRobot = 2
};

struct cmd_ping : public packet_hdr {
    unsigned char ping_type;
    unsigned char slen;
};

struct cmd_pong : public packet_hdr {
    unsigned char pong_type;
    unsigned char slen;
};

struct cmd_control : public packet_hdr {
    //  negative means backwards
    //  4096 is "nominal"
    short speed;
    short rate;
};

struct cmd_fire : public packet_hdr {
    //  number of shots to fire
    unsigned char num_shots;
};

struct cmd_reports : public packet_hdr {
    //  milliseconds, 0 for off
    unsigned short interval;
    //  after num_reports, will shut off reporting
    unsigned char num_reports;
};

struct cmd_camera : public packet_hdr {
    //  0 for off; after num_frames, will shut off streaming
    unsigned char num_frames;
};

struct cmd_power : public packet_hdr {
    //  0 for off, 255 for on
    unsigned char power;
};

struct tune_value {
    int value;
    unsigned char slen;
    char name[0];
};
struct cmd_tune : public packet_hdr {
    tune_value value;
};

struct cmd_gettune : public packet_hdr {
};

struct cmd_alltune : public packet_hdr {
    unsigned char cnt;
    tune_value values[0];
};


enum {
    ptString = 1,
    ptByte = 2,
    ptShort = 3,
    ptFloat = 4,
};

struct cmd_report_param {
    unsigned char type; //  data type of param
    unsigned char code; //  "name" of param
    unsigned char data[0];  //  actual data, based on type
};

struct cmd_report : public packet_hdr {
    unsigned char num_params;
    cmd_report_param params[0];
};

struct cmd_frame : public packet_hdr {
    //  correlate milliseconds
    unsigned short millis;
    //  MJPEG data
    unsigned char data[0];   //  actually, often very big
};

struct cmd_setminmax : public packet_hdr {
    unsigned char channel;
    unsigned short min;
    unsigned short max;
};

struct cmd_setpwm : public packet_hdr {
    unsigned char channel;
    unsigned short value;
};

#endif  //  littlewalker_defs_h
