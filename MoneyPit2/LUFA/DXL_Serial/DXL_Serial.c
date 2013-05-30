/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the DXL_Serial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "DXL_Serial.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
    {
        .Config =
            {
                .ControlInterfaceNumber         = 0,
                .DataINEndpoint                 =
                    {
                        .Address                = CDC_TX_EPADDR,
                        .Size                   = CDC_TXRX_EPSIZE,
                        .Banks                  = 1,
                    },
                .DataOUTEndpoint                =
                    {
                        .Address                = CDC_RX_EPADDR,
                        .Size                   = CDC_TXRX_EPSIZE,
                        .Banks                  = 1,
                    },
                .NotificationEndpoint           =
                    {
                        .Address                = CDC_NOTIFICATION_EPADDR,
                        .Size                   = CDC_NOTIFICATION_EPSIZE,
                        .Banks                  = 1,
                    },
            },
    };


unsigned char send_ptr = 0;
unsigned char send_end = 0;
unsigned char send_buffer[128];
unsigned char receive_ptr = 0;
unsigned char receive_end = 0;
unsigned char receive_buffer[128];


//  timer counts in half microseconds for simplicity
void setup_hus(void)
{
    power_timer1_enable();
    TCCR1A = 0;             //  normal mode
    TCCR1B = (1 << CS11);   //  half a microsecond per tick
}

//  timer counts in half microseconds for simplicity
void delay_hus(unsigned short us)
{
    unsigned short val = TCNT1;
    while (TCNT1 - val < us)
    {
        // do nothing
    }
}

//  timer counts in half microseconds for simplicity
unsigned short clock_hus(void)
{
    return TCNT1;
}

unsigned short last_clock;


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
    SetupHardware();
    setup_hus();
	GlobalInterruptEnable();

    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
    bool txmode = false;
    unsigned short last_recv = 0;

    for (;;)
    {
        if (send_ptr < send_end)
        {
            if (!txmode)
            {
                txmode = true;
                UCSR1B = (1 << TXEN1);
                delay_hus(10);
            }
            Serial_SendByte(send_buffer[send_ptr]);
            ++send_ptr;
        }
        else if (txmode)
        {
            send_ptr = send_end = 0;
            if (UCSR1A & (1 << TXC1))
            {
                txmode = false;
                UCSR1B = (1 << RXEN1);
                delay_hus(10);
            }
        }
        else if (UCSR1A & (1 << RXC1))
        {
            unsigned char x = UDR1;
            if (receive_end < sizeof(receive_buffer))
            {
                receive_buffer[receive_end] = x;
                ++receive_end;
            }
            last_recv = clock_hus();
        }
        else if (receive_end > receive_ptr && (clock_hus() > last_recv + 2000))
        {
            Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);
            if (Endpoint_IsINReady())
            {
                uint8_t BytesToSend = MIN(receive_end - receive_ptr, (CDC_TXRX_EPSIZE - 1));
                while (BytesToSend--)
                {
                    /* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
                    if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface,
                        receive_buffer[receive_ptr]) != ENDPOINT_READYWAIT_NoError)
                    {
                        break;
                    }
                    ++receive_ptr;
                }
                CDC_Device_Flush(&VirtualSerial_CDC_Interface);
            }
        }
        else 
        {
            if (receive_end == receive_ptr)
            {
                receive_end = receive_ptr = 0;
            }
            CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
            USB_USBTask();
            uint16_t n = CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface);
            if (n > 0)
            {
                n = MIN(n, sizeof(send_buffer) - send_end);
                while (n--)
                {
                    send_buffer[send_end] = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
                    ++send_end;
                }
            }
            else
            {
                //  flush empty data every 32 milliseconds
                unsigned short lc = clock_hus();
                if (lc < last_clock)
                {
                    Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);
                    if (Endpoint_IsINReady())
                    {
                        CDC_Device_Flush(&VirtualSerial_CDC_Interface);
                    }
                }
                last_clock = lc;
            }
        }
#if 0
        /* Only try to read in bytes from the CDC interface if the transmit buffer is not full */
        if (!(RingBuffer_IsFull(&USBtoUSART_Buffer)))
        {
            int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

            /* Read bytes from the USB OUT endpoint into the USART transmit buffer */
            if (!(ReceivedByte < 0))
              RingBuffer_Insert(&USBtoUSART_Buffer, ReceivedByte);
        }

        /* Check if the UART receive buffer flush timer has expired or the buffer is nearly full */
        uint16_t BufferCount = RingBuffer_GetCount(&USARTtoUSB_Buffer);
        if (BufferCount)
        {
            Endpoint_SelectEndpoint(VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address);

            /* Check if a packet is already enqueued to the host - if so, we shouldn't try to send more data
             * until it completes as there is a chance nothing is listening and a lengthy timeout could occur */
            if (Endpoint_IsINReady())
            {
                /* Never send more than one bank size less one byte to the host at a time, so that we don't block
                 * while a Zero Length Packet (ZLP) to terminate the transfer is sent if the host isn't listening */
                uint8_t BytesToSend = MIN(BufferCount, (CDC_TXRX_EPSIZE - 1));

                /* Read bytes from the USART receive buffer into the USB IN endpoint */
                while (BytesToSend--)
                {
                    /* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
                    if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface,
                                            RingBuffer_Peek(&USARTtoUSB_Buffer)) != ENDPOINT_READYWAIT_NoError)
                    {
                        break;
                    }

                    /* Dequeue the already sent byte from the buffer now we have confirmed that no transmission error occurred */
                    RingBuffer_Remove(&USARTtoUSB_Buffer);
                }
            }
        }

        /* Load the next byte from the USART transmit buffer into the USART */
        if (!(RingBuffer_IsEmpty(&USBtoUSART_Buffer)))
          Serial_SendByte(RingBuffer_Remove(&USBtoUSART_Buffer));

        CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
        USB_USBTask();
#endif
    }
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
    /* Disable watchdog if enabled by bootloader/fuses */
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    /* Disable clock division */
    clock_prescale_set(clock_div_1);

    /* Hardware Initialization */
    LEDs_Init();
    USB_Init();
    PORTD |= 0x04;  //  pull-up
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
    DDRD |= 0x80;
    PORTD |= 0x80;
    LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
    DDRD |= 0x80;
    PORTD &= ~0x80;
    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

    LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
    CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}


/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
    uint8_t ConfigMask = 0;

    switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
    {
        case CDC_PARITY_Odd:
            ConfigMask = ((1 << UPM11) | (1 << UPM10));
            break;
        case CDC_PARITY_Even:
            ConfigMask = (1 << UPM11);
            break;
    }

    if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
      ConfigMask |= (1 << USBS1);

    switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
    {
        case 6:
            ConfigMask |= (1 << UCSZ10);
            break;
        case 7:
            ConfigMask |= (1 << UCSZ11);
            break;
        case 8:
            ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
            break;
    }

    /* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
    UCSR1B = 0;
    UCSR1A = 0;
    UCSR1C = 0;

    /* Set the new baud rate before configuring the USART */
    UBRR1  = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

    /* Reconfigure the USART in double speed mode for a wider baud rate range at the expense of accuracy */
    UCSR1C = ConfigMask;
    UCSR1A = (1 << U2X1);
    UCSR1B = (1 << RXEN1);
}

