
#include "Config.h"
#include "Ada1306.h"
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Drivers/Peripheral/TWI.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include "OnyxWalker.h"
#include "my32u4.h"
#include <avr/io.h>
#include <util/atomic.h>

#include "MyProto.h"


//  Respond every 16 miliseconds, if not more often
#define FLUSH_TICK_INTERVAL 1000

#define RAW_READ_TIMEOUT 100

#define MAX_WRITE_SIZE 64
#define MAX_READ_SIZE 64

#define DXL_READ_DATA 0x2
#define DXL_WRITE_DATA 0x3

#define ID_BROADCAST 0xfe


void Reconfig(void);

int main(void) {
    wdt_disable();
    MCUSR &= ~(1 << WDRF);
    wdt_reset();
    MCUSR |= (1 << WDRF);
    wdt_enable(WDTO_8S);
    wdt_reset();

    SetupHardware();
    //  running lights off
    for (unsigned char i = 3; i > 0; --i) {
        MY_DelayTicks(5000);
        MY_SetLed(i-1, false);
    }

    sei();

    MY_SetLedAll(false);

    while (1) {
        wdt_reset();
        USB_USBTask();
        OnyxWalker_Task();
    }
}


void SetupHardware(void) {

    MY_SetLedAll(true);

    MY_Setup();
    USB_Init();
    LCD_Setup();
    TWI_Init(TWI_BIT_PRESCALE_1, TWI_BITLENGTH_FROM_FREQ(1, 400000));
    Serial_Init(2000000, true);

    //  noisy thing
    PORTC &= ~(1 << 6);
    DDRC |= (1 << 6);

    //  don't txen
    PORTD &= ~(1 << 4);
    PORTD |= (1 << 2);  //  pull-up
    DDRD |= (1 << 4);
}

void EVENT_USB_Device_Connect(void) {
    MY_SetLed(LED_con, true);
}

void EVENT_USB_Device_Disconnect(void) {
    MY_SetLed(LED_con, false);
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void EVENT_USB_Device_Reset(void) {
    Reconfig();
}

void Reconfig() {
    unsigned char ConfigSuccess = 0;

    if (Endpoint_ConfigureEndpoint(
        DATA_RX_EPNUM,
        EP_TYPE_BULK, 
        DATA_RX_EPSIZE,
        2)) {
        ConfigSuccess |= 1;
    }
    if (Endpoint_ConfigureEndpoint(
        DATA_TX_EPNUM,
        EP_TYPE_BULK, 
        DATA_TX_EPSIZE,
        1)) {
        ConfigSuccess |= 2;
    }

    if (ConfigSuccess != 3) {
        MY_Failure("Reconfig", ConfigSuccess, 0);
    }
}


unsigned char epic;
unsigned char epiir;
unsigned char epirwa;

unsigned short lastTicks;
unsigned short numWraps;

#define MAX_SERVOS 24

unsigned char servo_stati[MAX_SERVOS+1];

unsigned char in_packet[DATA_RX_EPSIZE];
unsigned char in_packet_ptr;
unsigned char out_packet[DATA_TX_EPSIZE];
unsigned char out_packet_ptr;

char const err_TMR[] = "Too much response";

void add_response_sz(unsigned char code, unsigned char sz, unsigned char const *data) {
    unsigned char adj = (sz >= 15) ? 2 : 1;
    if (sizeof(in_packet) - in_packet_ptr - adj < sz) {
        MY_Failure(err_TMR, sz, sizeof(in_packet) - in_packet_ptr - adj);
    }
    if (sz < 15) {
        code = code | sz;
    }
    else {
        code = code | 0xf;
    }
    in_packet[in_packet_ptr++] = code;
    if (sz >= 15) {
        in_packet[in_packet_ptr++] = sz;
    }
    memcpy(&in_packet[in_packet_ptr], data, sz);
    in_packet_ptr += sz;
}

/* power */

#define POWER_WRITE_ADDR 0x12
#define POWER_READ_ADDR 0x13

/* these must live together in RAM */
struct {
    unsigned short p_cvolts;
    unsigned char p_status;
    unsigned char p_failure;
}
_power_data = { 0 };
#define power_cvolts _power_data.p_cvolts
#define power_status _power_data.p_status
#define power_failure _power_data.p_failure
/* end together */

unsigned short last_cvolts;

void power_tick(void) {
    unsigned char dummy;
    if (TWI_ReadPacket(POWER_READ_ADDR, 1, &dummy, 0, (unsigned char *)&_power_data, 4) != TWI_ERROR_NoError) {
        power_cvolts = 0;
    }
    else {
        power_cvolts = (unsigned short)(1.99f * power_cvolts);
    }
}

void set_status_power(unsigned char sz, unsigned char const *ptr) {
    if (sz == 0) {
        return;
    }
    TWI_WritePacket(POWER_WRITE_ADDR, 1, ptr, 0, ptr, 1);
}

void get_status_power(void) {
    unsigned char d[5] = {
        TargetPower,
        power_cvolts & 0xff,
        (power_cvolts >> 8) & 0xff,
        power_status,
        power_failure
    };
    add_response_sz(OpGetStatus, 5, d);
}

void get_status_servos(void) {
    servo_stati[0] = TargetServos;
    add_response_sz(OpGetStatus, MAX_SERVOS + 1, servo_stati);
}

void invalid_target(unsigned char target) {
    MY_Failure("Invalid target", target, 0);
}

void set_status(unsigned char target, unsigned char sz, unsigned char const *ptr) {
    switch (target) {
    case TargetPower:
        set_status_power(sz, ptr);
        break;
    default:
        invalid_target(target);
    }
}

void get_status(unsigned char target) {
    switch (target) {
    case TargetPower:
        get_status_power();
        break;
    case TargetServos:
        get_status_servos();
        break;
    default:
        invalid_target(target);
    }
}

void servo_cmd(unsigned char id, unsigned char pre_len, unsigned char const *pre, unsigned char len, unsigned char const *ptr) {
    PORTD |= (1 << 4);
    unsigned char cs = 0;
    Serial_SendByte(0xff);
    Serial_SendByte(0xff);
    Serial_SendByte(id);
    cs += id;
    Serial_SendByte(len + pre_len + 1);
    cs += len + pre_len + 1;
    for (unsigned char ix = 0; ix != pre_len; ++ix) {
        Serial_SendByte(pre[ix]);
        cs += pre[ix];
    }
    for (unsigned char ix = 0; ix != len; ++ix) {
        Serial_SendByte(ptr[ix]);
        cs += ptr[ix];
    }
    Serial_SendByte(~cs);
    while (!(UCSR1A & (1 << UDRE1)))
        ;
    _delay_us(5);
    PORTD &= ~(1 << 4);
}

void write_servo(unsigned char id, unsigned char reg, unsigned char sz, unsigned char const *ptr) {

    cli();

    if (sz > MAX_WRITE_SIZE) {
        MY_Failure("Write Size", sz, MAX_WRITE_SIZE);
    }
    if (id >= MAX_SERVOS && id != ID_BROADCAST) {
        MY_Failure("Servo ID", id, MAX_SERVOS-1);
    }
    unsigned char cmd[2] = { DXL_WRITE_DATA, reg };
    servo_cmd(id, 2, cmd, sz, ptr);

    sei();
}

unsigned char read_servo_buf[MAX_READ_SIZE + 9];

bool waitchar(unsigned char ptr) {
    unsigned short ctr = 15000;
    while (!(Serial_IsCharReceived())) {
        if (ctr-- == 0) {
            return false;
        }
    }
    read_servo_buf[ptr] = UDR1;
    return true;
}

enum {
    ERR_NOTPRESENT = 0,
    ERR_BAD_SYNC1 = 1,
    ERR_BAD_SYNC2 = 2,
    ERR_BAD_ID = 3,
    ERR_BAD_LENGTH = 4,
    ERR_BAD_STATUS = 5,
    ERR_READ_ERROR = 6,
    ERR_BAD_CHECKSUM = 7,
};

void error_recv(unsigned char id, unsigned char kind, unsigned char offset) {
    LCD_DrawUint(offset, WIDTH-6, 1);
    LCD_DrawUint(kind, WIDTH-9, 1);
    LCD_DrawUint(id, WIDTH-12, 1);
    LCD_DrawString("RcvErr", WIDTH-17, 1, 0);
    LCD_DrawHex(read_servo_buf, offset+1, 1, 2);
}


void read_servo(unsigned char id, unsigned char reg, unsigned char sz) {

    cli();
    UCSR1B &= ~(1 << RXEN1);    //  reset receiver
    _delay_us(1);
    UCSR1B |= (1 << RXEN1);    //  reset receiver

    if (sz > MAX_READ_SIZE) {
        MY_Failure("Read Size", sz, MAX_READ_SIZE);
    }
    if (id >= MAX_SERVOS) { //  can't read from broadcast address
        MY_Failure("Servo ID", id, MAX_SERVOS-1);
    }
    unsigned char cs = 0;
    unsigned char ptr = 0;
    unsigned char cmd[3] = { DXL_READ_DATA, reg, sz };


    servo_cmd(id, 3, cmd, 0, cmd);
    //  empty the receive pipe
    while (Serial_IsCharReceived()) {
        read_servo_buf[0] = UDR1;
    }

    //  now read response
    read_servo_buf[0] = 0xff;
    if (!waitchar(ptr) || read_servo_buf[ptr] != 0xff) {
        error_recv(id, read_servo_buf[ptr] == 0xff ? ERR_NOTPRESENT : ERR_BAD_SYNC1, ptr);
        goto out;
    }
    if (!waitchar(ptr) || read_servo_buf[ptr] != 0xff) {
        error_recv(id, ERR_BAD_SYNC2, ptr);
        goto out;
    }
    //  id
    if (!waitchar(ptr) || read_servo_buf[ptr] != id) {
        error_recv(id, ERR_BAD_ID, ptr);
        goto out;
    }
    //  save the ID
    ++ptr;
    //  length
    if (!waitchar(ptr) || read_servo_buf[ptr] != sz + 2) {
        error_recv(id, ERR_BAD_LENGTH, ptr);
        goto out;
    }
    cs = id + sz + 2;
    if (!waitchar(ptr)) {
        error_recv(id, ERR_BAD_STATUS, ptr);
        goto out;
    }
    servo_stati[id] = read_servo_buf[ptr];
    cs += read_servo_buf[ptr];
    read_servo_buf[ptr] = reg;
    //  return also what the length is
    ++ptr;
    for (unsigned char ix = 0; ix != sz; ++ix) {
        if (!waitchar(ptr)) {
            error_recv(id, ERR_READ_ERROR, ptr);
            goto out;
        }
        cs += read_servo_buf[ptr];
        //  save the actual data
        ++ptr;
    }
    cs = ~cs;
    if (!waitchar(ptr) || cs != read_servo_buf[ptr]) {
        error_recv(id, ERR_BAD_CHECKSUM, ptr);
        goto out;
    }
    //  buf is id + reg + data
    add_response_sz(OpReadServo, ptr, read_servo_buf);

out:

    sei();
}

void out_text(unsigned char sz, unsigned char const *ptr) {
    for (unsigned char p = 0; p != WIDTH; ++p) {
        if (p < sz) {
            LCD_DrawChar(p, 0, ptr[p]);
        }
        else {
            LCD_DrawChar(p, 0, ' ');
        }
    }
}

void raw_write(unsigned char sz, unsigned char const *data) {

    cli();
    PORTD |= (1 << 4);

    unsigned char const *end = data + sz;
    while (data < end) {
        Serial_SendByte(*data);
        data++;
    }

    //  wait for completion
    while (!(UCSR1A & (1 << UDRE1)))
        ;
    _delay_us(5);

    PORTD &= ~(1 << 4);
    sei();
}

void raw_read(unsigned char sz) {

    cli();

    if (sz > MAX_READ_SIZE) {
        MY_Failure("RawReadSize", sz, MAX_READ_SIZE);
    }

    unsigned short ticks = MY_GetTicks();
    unsigned char offset = 0;
    while (MY_GetTicks() - ticks < RAW_READ_TIMEOUT && offset < sz) {
        if (Serial_IsCharReceived()) {
            read_servo_buf[offset] = UDR1;
            ++offset;
        }
    }

    sei();

    add_response_sz(OpRawRead, offset, read_servo_buf);
}


static const PROGMEM unsigned char min_size[] = {
    0x2,    //  set status
    0x1,    //  get status
    0x3,    //  write servo
    0x3,    //  read servo
    0x2,    //  text out
    0x1,    //  raw write
    0x1,    //  raw read
};

static void dispatch_out(void) {
    //  update latest serial received
    if (out_packet_ptr) {
        in_packet[0] = out_packet[0];
        if (in_packet_ptr == 0) {
            in_packet_ptr = 1;
        }
    }
    unsigned char ptr = 1;
    while (ptr < out_packet_ptr) {
        unsigned char code = out_packet[ptr];
        ++ptr;
        unsigned char sz = code & 0xf;
        if (sz == 15) {
            if (ptr == out_packet_ptr) {
                goto too_big;
            }
            sz = out_packet[ptr];
            ++ptr;
        }
        if (ptr + sz > out_packet_ptr || ptr + sz < ptr) {
too_big:
            MY_Failure("Too big recv", ptr + sz, out_packet_ptr);
        }
        unsigned char code_ix = (code & 0xf0) >> 4;
        if (code_ix >= sizeof(min_size)) {
unknown_op:
            MY_Failure("Unknown op", code & 0xf0, sz);
        }
        unsigned char msz = 0;
        memcpy_P(&msz, &min_size[code_ix], 1);
        if (sz < msz) {
            MY_Failure("Too small data", sz, code);
        }
        unsigned char *base = &out_packet[ptr];
        switch (code & 0xf0) {
        case OpSetStatus:
            set_status(base[0], sz-1, base+1);
            break;
        case OpGetStatus:
            get_status(base[0]);
            break;
        case OpWriteServo:
            write_servo(base[0], base[1], sz-2, base+2);
            break;
        case OpReadServo:
            read_servo(base[0], base[1], base[2]);
            break; 
        case OpOutText:
            out_text(sz, base);
            break;
        case OpRawWrite:
            raw_write(sz, base);
            break;
        case OpRawRead:
            raw_read(base[0]);
            break;
        default:
            goto unknown_op;
        }
        ptr += sz;
    }
}

unsigned short last_flush = 0;
unsigned short lastVolts = 0;
unsigned char voltBlink = 0;

void OnyxWalker_Task(void) {

    unsigned short now = MY_GetTicks();
    if (now < lastTicks) {
        ++numWraps;
        LCD_DrawUint(numWraps, WIDTH-7, 3);
    }
    if (now - lastVolts > 15000) {
        lastVolts = now;
        ++voltBlink;
        if ((power_cvolts > 1320 && !power_failure) || (voltBlink & 1)) {
            LCD_DrawFrac(power_cvolts, 2, 0, 3);
            LCD_DrawChar(' ', 6, 3);
            LCD_DrawChar('V', 7, 3);
            PORTC &= ~(1 << 6);
        }
        else {
            LCD_DrawChar(' ', 0, 3);
            for (unsigned char i = 1; i != 8; ++i) {
                LCD_DrawChar('-', i, 3);
            }
            if (!(voltBlink & 15) && power_cvolts > 0) {
                PORTC |= (1 << 6);
            }
            else {
                PORTC &= ~(1 << 6);
            }
        }
    }
    lastTicks = now;
    LCD_Flush();
    if (lastTicks - last_cvolts > 10000) {
        power_tick();
        last_cvolts = lastTicks;
    }

    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }

    /* see if host has requested data */
    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    epic = Endpoint_IsConfigured();
    epiir = epic && Endpoint_IsINReady();
    epirwa = epiir && Endpoint_IsReadWriteAllowed();
    if (epirwa && (in_packet_ptr || (now - last_flush > FLUSH_TICK_INTERVAL))) {
        last_flush = now;
        if (in_packet_ptr == 0) {
            in_packet_ptr = 1;  //  repeat the last received serial
        }
        //  send packet in
        for (unsigned char ch = 0; ch < in_packet_ptr; ++ch) {
            Endpoint_Write_8(in_packet[ch]);
        }
        Endpoint_ClearIN();
        in_packet_ptr = 0;
    }

    /* see if there's data from the host */
    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    MY_SetLed(LED_act, false);
    if (Endpoint_IsConfigured() && 
        Endpoint_IsOUTReceived() && 
        Endpoint_IsReadWriteAllowed()) {
        uint8_t n = Endpoint_BytesInEndpoint();
        if (n > sizeof(out_packet)) {
            MY_Failure("OUT too big", n, sizeof(out_packet));
        }
        out_packet_ptr = 0;
        MY_SetLed(LED_act, true);
        while (n > 0) {
            epic = Endpoint_Read_8();
            out_packet[out_packet_ptr++] = epic;
            --n;
        }
        Endpoint_ClearOUT();
        dispatch_out();
    }
}

