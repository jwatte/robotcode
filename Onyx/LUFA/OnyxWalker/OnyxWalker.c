
#include "Config.h"
#include <LUFA/Drivers/USB/USB.h>
#include "OnyxWalker.h"
#include <avr/io.h>
#include <util/atomic.h>

#define EMPTY_IN_TIMEOUT 8


#define CMD_RAW_MODE 0x01
//  param is baud rate
#define CMD_RAW_DATA 0x00
//  all data is data
#define CMD_SET_LEDS 0x11
//  data is byte of LED state
#define CMD_SET_REG1 0x13
//  param is id, reg, data
#define CMD_SET_REG2 0x14
//  param is id, reg, datal, datah
#define CMD_GET_STATUS 0x20
//  no param -- just return status!
#define CMD_GET_REGS 0x23
//  param is id, reg, count
#define CMD_NOP 0xf0
//  no param
#define CMD_DELAY 0x31
//  param is milliseconds
#define CMD_LERP_POS 0xf3
//  params are actually more than header indicates
//  timel/h, nids, (id,posl/h)*nids
//  This sets the goal velocity field as well as the goal pos field.

#define READ_COMPLETE 0x41
#define STATUS_COMPLETE 0x51

#define DXL_PING 1
#define DXL_REG_READ 2
#define DXL_REG_WRITE 3
#define DXL_PEND_WRITE 4
#define DXL_PEND_COMMIT 5
#define DXL_RESET 6
#define DXL_MULTIWRITE 0x83

#define DXL_REG_GOAL_POSITION 0x1E
#define DXL_REG_MOVING_SPEED 0x20

#define BLINK_CNT 2
#define RECV_TIMEOUT_TICKS 50

void Reconfig(void);
void clear_poses(void);

int main(void) {
    SetupHardware();
    clear_poses();
    MCUSR |= (1 << WDRF);
    wdt_enable(WDTO_8S);

    sei();

    while (true) {
        wdt_reset();
        USB_USBTask();
        OnyxWalker_Task();
    }
}


static unsigned char servo_stati[32];
static unsigned short target_pose[32];
static unsigned short target_prev_pose[32];

unsigned short last_in = 0;
unsigned char last_seq = 0;


void clear_poses(void) {
    for (unsigned char id = 0; id != sizeof(target_pose)/sizeof(target_pose[0]); ++id) {
        target_pose[id] = 2048;
        target_prev_pose[id] = 2048;
    }
}

//  Broadcast to turn off torque for all servos on the bus.
static const unsigned char notorque_packet[] = {
    0xff, 0xff, 0xfe, 0x4, 0x3, 0x18, 0, (unsigned char)~(0xfe + 0x4 + 0x3 + 0x18 + 0),
};

static unsigned char xbuf[DATA_RX_EPSIZE];
unsigned char xbufptr = 0;

void clear_xbuf(void) {
    xbufptr = 0;
}

unsigned char last_cmd = 0;

void add_xbuf(unsigned char const *ptr, unsigned char len) {
    if (sizeof(xbuf) - xbufptr < len) {
        //  dropped packet
        show_error(7, xbufptr);
        return;
    }
    memcpy(&xbuf[xbufptr], ptr, len);
    xbufptr += len;
}


void set_weapons(unsigned char code) {
    PORTD = (PORTD & ~(0x20 | 0x10)) | ((code << 4) & (0x20 | 0x10));
    PORTC = (PORTC & ~(0x80 | 0x40)) | ((code << 4) & (0x80 | 0x40));
}

void setup_weapons(void) {
    set_weapons(0);
    DDRC |= 0x80 | 0x40;
    DDRD |= 0x20 | 0x10;
}


void SetupHardware(void) {
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    //  weapons fire off
    setup_weapons();
    
    //  Status LEDs on
    setup_status();
    set_status(0xff, 0xff);

    clock_prescale_set(clock_div_1);
    set_status(0x7f, 0xff);

    setup_delay();
    delayms(50);
    set_status(0x3f, 0xff);

    setup_uart(0);
    delayms(50);
    set_status(0x1f, 0xff);

    send_sync(notorque_packet, sizeof(notorque_packet));    //  turn off torque on all servos
    clear_xbuf();
    delayms(50);
    set_status(0xf, 0xff);

    delayms(50);
    set_status(0x7, 0xff);
    delayms(50);
    set_status(0x3, 0xff);
    delayms(50);
    set_status(0x1, 0xff);
    delayms(50);
    set_status(0x0, 0xff);
    
    USB_Init();
}

void EVENT_USB_Device_Connect(void) {
    set_status(CONNECTED_LED, CONNECTED_LED);
}

void EVENT_USB_Device_Disconnect(void) {
    set_status(0x0, CONNECTED_LED);
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void EVENT_USB_Device_Reset(void) {
    Reconfig();
}

void Reconfig() {
    bool ConfigSuccess = true;

    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_RX_EPNUM,
        EP_TYPE_BULK, 
        DATA_RX_EPSIZE,
        2);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(
        DATA_TX_EPNUM,
        EP_TYPE_BULK, 
        DATA_TX_EPSIZE,
        1);

    if (!ConfigSuccess) {
        while (true) {
            set_status(0xff, 0xff);
            delayms(100);
            set_status(0, 0xff);
            delayms(100);
        }
    }
    clear_poses();
}

unsigned char pbuf[16];

unsigned char cksum(unsigned char const *ptr, unsigned char n) {
    unsigned char ck = 0;
    unsigned char const *end = ptr + n;
    while (end != ptr) {
        ck += *ptr;
        ++ptr;
    }
    return ~ck;
}

void reg_write(unsigned char id, unsigned char reg, unsigned char const *buf, unsigned char cnt, unsigned char lookaside) {
    if (cnt > 9) {
        show_error(5, cnt);
        cnt = 9;
    }
    if (lookaside) {
        if ((reg <= DXL_REG_GOAL_POSITION) && (reg+cnt >= DXL_REG_GOAL_POSITION + 2) &&
            (id < sizeof(target_pose)/sizeof(target_pose[0]))) {
            //  half updates of goal register are not look-aside snooped...
            unsigned char offs = DXL_REG_GOAL_POSITION - reg;
            target_pose[id] = ((buf[offs] + ((unsigned short)buf[offs + 1] << 8)) & 4095);
        }
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = cnt + 3;
    pbuf[4] = DXL_REG_WRITE;
    pbuf[5] = reg;
    memcpy(&pbuf[6], buf, cnt);
    pbuf[6 + cnt] = cksum(&pbuf[2], cnt + 4);
    send_sync(pbuf, cnt + 7);
    //  assume servos do not ack writes
}

unsigned char recv_packet(unsigned char *dst, unsigned char maxsz) {
    unsigned char cnt = 0;
    UCSR1B = (1 << RXEN1);
    unsigned char tc = TCNT0;
    unsigned char ntc = tc;
    while (cnt < maxsz) {
        while (!(UCSR1A & (1 << RXC1))) {
            //  don't spend more than X microseconds waiting for something that won't come
            ntc = TCNT0;
            if (ntc - tc > RECV_TIMEOUT_TICKS) {
                show_error(8, cnt);
                break;
            }
        }
        tc = ntc;
        dst[cnt] = UDR1;
        ++cnt;
    }
    UCSR1B = (1 << RXEN1) | (1 << RXCIE1);
    return cnt;
}

void reg_read(unsigned char id, unsigned char reg, unsigned char cnt) {
    if (cnt > 9) {
        show_error(6, cnt);
        cnt = 9;
    }
    pbuf[0] = 0xff;
    pbuf[1] = 0xff;
    pbuf[2] = id;
    pbuf[3] = 4;
    pbuf[4] = DXL_REG_READ;
    pbuf[5] = reg;
    pbuf[6] = cnt;
    pbuf[7] = cksum(&pbuf[2], 5);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        send_sync(pbuf, 8);
        cnt = recv_packet(pbuf, 6 + cnt);
        if (pbuf[2] < sizeof(servo_stati)) {
            servo_stati[pbuf[2]] = pbuf[4];
        }
        pbuf[1] = READ_COMPLETE;
        pbuf[2] = id;
        pbuf[3] = reg;
        pbuf[4] = cnt - 6;
        if (cnt > 6) {
            add_xbuf(&pbuf[1], cnt - 2);
        }
    }
}


bool rawmode = false;
unsigned char sbuf[DATA_TX_EPSIZE];
unsigned char sbuflen;

void wait_for_idle(void) {
    while (true) {
        unsigned char av = recv_avail();
        unsigned char ts = TCNT0;
        //  Wait for 40 us without any data -- that's 80 bits; plenty of time 
        //  for the bus to clear!
        while ((unsigned char)(TCNT0 - ts) < 10) {
            ; // do nothing
        }
        if (av == recv_avail()) {
            break;
        }
    }
}

unsigned char lerp_pos(unsigned char const *data, unsigned char sz) {
    if (sz < 3) {
        //  bad command
        return sz;
    }
    unsigned short dt = data[0] + ((unsigned short)data[1] << 8);
    if (dt < 5) {
        show_error(3, 3);
        //  don't allow lerping faster than at 200 Hz
        dt = 5;
    }
    unsigned char nid = data[2];
    if (nid > 32 || sz < nid * 3 + 3) {
        //  bad command
        return sz;
    }
    unsigned char ptr = 3;
    while (nid > 0) {
        unsigned char id = data[ptr];
        if (id >= sizeof(target_pose)/sizeof(target_pose[0])) {
            //  bad parameter
            show_error(3, 5);
        }
        else {
            unsigned short pos = data[ptr+1] + ((unsigned short)data[ptr+2] << 8);
            target_prev_pose[id] = target_pose[id];
            target_pose[id] = pos;
            //  calculate velocity
            short distance = target_prev_pose[id] - target_pose[id];
            if (distance < 0) {
                distance = -distance;
            }
            distance = distance << 4;   //  max range for short
            unsigned short ticks_per_ms_16 = distance / dt;
            //  The value is scaled by 16.
            //  A value of 1023 means 117 rpm, which we'll round to 120, which 
            //  in turn is 8192 ticks per second.
            //  That's about 8 ticks per millisecond.
            //  Multiply by 16, and you get 128 as the max value after the division.
            //  This means I need to scale by another 8.
            unsigned short spdval = ticks_per_ms_16 << 3;
            //  kick some off to avoid oscillation (introduces mush instead)
            spdval -= (spdval >> 3);    //  gets us 7/8th of the calculated value
            spdval = spdval > 1023 ? 1023 : spdval;
            unsigned char bf[4] = { pos & 0xff, (pos >> 8) & 0xff, spdval & 0xff, (spdval >> 8) & 0xff };
            //  write both goal position and speed, which is right after
            reg_write(id, DXL_REG_GOAL_POSITION, bf, 4, 0);
        }
        ptr += 3;
        --nid;
    }
    return ptr;
}

void do_get_status(void) {
    unsigned char cmd[3] = { STATUS_COMPLETE, 1 + sizeof(servo_stati), get_nmissed() };
    add_xbuf(cmd, 3);
    add_xbuf(servo_stati, sizeof(servo_stati));
}

void dispatch(unsigned char const *sbuf, unsigned char offset, unsigned char end) {
    while (offset < end) {
        if (rawmode && sbuf[offset] == CMD_RAW_DATA) {
            wait_for_idle();
            send_sync(&sbuf[offset+1], end - offset - 1);
            break;
        }
        unsigned char cmdSize = (sbuf[offset] & 0x7) + 1;
        if ((end - offset) < cmdSize) {
            //  missing data!
            show_error(1, cmdSize);
            break;
        }
        switch (sbuf[offset]) {
            case CMD_RAW_MODE:
                rawmode = true;
                setup_uart(sbuf[offset+1]);
                break;
            case CMD_SET_LEDS:
                set_status(sbuf[offset+1], 0xff);
                break;
            case CMD_SET_REG1:
                rawmode = false;
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 1, 1);
                break;
            case CMD_SET_REG2:
                rawmode = false;
                reg_write(sbuf[offset+1], sbuf[offset+2], &sbuf[offset+3], 2, 1);
                break;
            case CMD_GET_REGS:
                rawmode = false;
                reg_read(sbuf[offset+1], sbuf[offset+2], sbuf[offset+3]);
                break;
            case CMD_GET_STATUS:
                do_get_status();
                break;
            case CMD_DELAY:
                rawmode = false;
                delayms(sbuf[offset+1]);
                wait_for_idle();
                break;
            case CMD_NOP:
                break;
            case CMD_LERP_POS:
                cmdSize = lerp_pos(sbuf+offset+1, end-offset-1) + 1;
                break;
            default:
                rawmode = false;
                //  unknown command
                show_error(2, sbuf[offset]);
                return;
        }
        if (cmdSize == 0) {
            show_error(4, sbuf[offset]);
        }
        offset += cmdSize;
    }
}

unsigned char epic;
unsigned char epiir;
unsigned char epirwa;

unsigned short clear_received;

void OnyxWalker_Task(void) {
    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }

    unsigned short now = getms();
    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    epic = Endpoint_IsConfigured();
    epiir = epic && Endpoint_IsINReady();
    epirwa = epiir && Endpoint_IsReadWriteAllowed();
    if (epirwa) {
        unsigned char gg = xbufptr;
        unsigned char m = recv_avail();
        if (gg || m || (now - last_in > EMPTY_IN_TIMEOUT)) {
            last_in = now;
            Endpoint_Write_8(last_seq);
            if (gg) {
                for (unsigned char q = 0; q < gg; ++q) {
                    Endpoint_Write_8(xbuf[q]);
                }
                clear_xbuf();
            }
            ++gg;   //  for the seq
            if (m) {
                unsigned char nm = m;
                if (rawmode) {
                    if ((unsigned short)m + gg > DATA_RX_EPSIZE) {
                        m = DATA_RX_EPSIZE - gg;
                    }
                    nm = m;
                    unsigned char const *ptr = recv_buf();
                    while (m > 0) {
                        Endpoint_Write_8(*ptr);
                        ++ptr;
                        --m;
                    }
                }
                recv_eat(nm);
            }
            Endpoint_ClearIN();
        }
    }

    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
        set_status(RECEIVED_LED, RECEIVED_LED);
        clear_received = now + BLINK_CNT;
        uint8_t n = Endpoint_BytesInEndpoint();
        if (n > sizeof(sbuf)) {
            n = sizeof(sbuf);
        }
        if (n) {
            last_seq = Endpoint_Read_8();
            --n;
            for (unsigned char c = 0; c < n; ++c) {
                sbuf[c] = Endpoint_Read_8();
            }
        }
        Endpoint_ClearOUT();
        if (n) {
            sbuflen = n;
            dispatch(sbuf, 0, n);
        }
    }

    if ((short)(now - clear_received) > 0) {
        set_status(0, RECEIVED_LED);
        clear_received = now;
    }
}

