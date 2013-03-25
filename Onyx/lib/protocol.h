#if !defined(protocol_h)
#define protocol_h

enum C2R {
    C2R_Discover = 1,
    C2R_Connect = 2,
    C2R_SetInput = 3,
    C2R_RequestVideo = 4
};

struct P_Discover {
};

struct P_Connect {
    char pilot[32];
};

enum {
    BtnFireA = 0x1,
    BtnFireB = 0x2,
};
enum {
    PoseCrouch = 0,
    PoseNormal = 1,
    PoseTall = 2
};
struct P_SetInput {
    float trot;
    float speed;
    float turn;
    float strafe;
    float aimHeading;
    float aimElevation;
    unsigned char pose;
    //  buttons should increment a counter each time they're pushed,
    //  to detect button presses even when packets are lost.
    unsigned char fire;
};

struct P_RequestVideo {
    unsigned short width;
    unsigned short height;
    unsigned short millis;
};

enum R2C {
    R2C_Info = 0x81,
    R2C_Status = 0x83,
    R2C_VideoFrame = 0x84
};

struct P_Info {
    char name[32];
    char pilot[32];
};

struct P_Status {
    unsigned char hits;
    unsigned char status;
    unsigned char battery;
    char message[32];
};

struct P_VideoFrame {
    unsigned short serial;
    unsigned short width;
    unsigned short height;
    //  MJPEG data
};

#endif  //  protocol_h

