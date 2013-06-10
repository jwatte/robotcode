
#include "Config.h"
#include "Ada1306.h"
#include <LUFA/Drivers/USB/USB.h>
#include "OnyxWalker.h"
#include "my32u4.h"
#include <avr/io.h>
#include <util/atomic.h>


//  Respond every 16 miliseconds, if not more often
#define FLUSH_TICK_INTERVAL 1000

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
        MY_DelayTicks(1000);
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


unsigned char in_packet[DATA_RX_EPSIZE];
unsigned char in_packet_ptr;
unsigned char out_packet[DATA_TX_EPSIZE];
unsigned char out_packet_ptr;


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
            sz = out_packet[ptr];
            ++ptr;
        }
        if (ptr > out_packet_ptr || ptr + sz > out_packet_ptr) {
            MY_Failure("Too big recv", ptr + sz, out_packet_ptr);
        }
        switch (sz & 0xf0) {
        default:
            MY_Failure("Unknown op", sz & 0xf0, 0);
        }
        ptr += sz;
    }
}

unsigned short last_flush = 0;

void OnyxWalker_Task(void) {

    unsigned short now = MY_GetTicks();
    if (now < lastTicks) {
        ++numWraps;
        LCD_DrawUint(numWraps, WIDTH-7, 3);
    }
    //LCD_DrawUint(now, WIDTH-13, 3);
    lastTicks = now;
    LCD_Flush();

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
        MY_SetLed(LED_act, false);
    }
}

