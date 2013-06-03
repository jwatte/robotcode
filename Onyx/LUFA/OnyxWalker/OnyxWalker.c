
#include "Config.h"
#include "Ada1306.h"
#include <LUFA/Drivers/USB/USB.h>
#include "OnyxWalker.h"
#include "my32u4.h"
#include <avr/io.h>
#include <util/atomic.h>

void Reconfig(void);

int main(void) {
    SetupHardware();
    MCUSR |= (1 << WDRF);
    wdt_enable(WDTO_8S);

    sei();

    while (1) {
        wdt_reset();
        USB_USBTask();
        OnyxWalker_Task();
    }
}


void SetupHardware(void) {
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    LCD_Setup();
    USB_Init();
}

void EVENT_USB_Device_Connect(void) {
}

void EVENT_USB_Device_Disconnect(void) {
}

void EVENT_USB_Device_ConfigurationChanged(void) {
    Reconfig();
}

void EVENT_USB_Device_Reset(void) {
    Reconfig();
}

void Reconfig() {
    bool ConfigSuccess = 1;

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
        while (1) {
        }
    }
}


unsigned char epic;
unsigned char epiir;
unsigned char epirwa;

void OnyxWalker_Task(void) {

    LCD_Flush();

    if (USB_DeviceState != DEVICE_STATE_Configured) {
        return;
    }

    Endpoint_SelectEndpoint(DATA_RX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_IN);
    epic = Endpoint_IsConfigured();
    epiir = epic && Endpoint_IsINReady();
    epirwa = epiir && Endpoint_IsReadWriteAllowed();
    if (epirwa) {
        Endpoint_ClearIN();
    }

    /* see if there's data from the host */
    Endpoint_SelectEndpoint(DATA_TX_EPNUM);
    Endpoint_SetEndpointDirection(ENDPOINT_DIR_OUT);
    if (Endpoint_IsConfigured() && 
        Endpoint_IsOUTReceived() && 
        Endpoint_IsReadWriteAllowed()) {
        uint8_t n = Endpoint_BytesInEndpoint();
        while (n > 0) {
            epic = Endpoint_Read_8();
            --n;
        }
        Endpoint_ClearOUT();
    }
}

