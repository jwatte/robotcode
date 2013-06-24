#if !defined (OnyxWalker_MyProto_h)
#define OnyxWalker_MyProto_h

enum Opcode {
    OpSetStatus = 0x00,     //  Target, <actual data>
    OpGetStatus = 0x10,     //  Target, Count
    OpWriteServo = 0x20,    //  ID, Reg, <actual data>
    OpReadServo = 0x30,     //  ID, Reg, Count
    OpOutText = 0x40,       //  <actual text>
    OpRawWrite = 0x50,      //  <actual bytes>
    OpRawRead = 0x60        //  MaxBytes
};

enum Target {
    TargetPower = 0x0,
    TargetServos = 0x1
};

#define STATE_PWR 0x1
#define STATE_SERVOS 0x2
#define STATE_FANS 0x4
#define STATE_GUNS 0x8

#endif  //  OnyxWalker_MyProto_h
