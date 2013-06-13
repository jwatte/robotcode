#if !defined (OnyxWalker_MyProto_h)
#define OnyxWalker_MyProto_h

enum Opcode {
    OpSetStatus = 0x00,     //  Target, <actual data>
    OpGetStatus = 0x10,     //  Target, Count
    OpWriteServo = 0x20,    //  ID, Reg, <actual data>
    OpReadServo = 0x30,     //  ID, Reg, Count
    OpOutText = 0x40,       //  <actual text>
};

enum Target {
    TargetPower = 0x0,
};

#endif  //  OnyxWalker_MyProto_h
