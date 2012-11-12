#ifndef _ServoController_H_
#define _ServoController_H_

		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/interrupt.h>
		#include <avr/power.h>

		#include <LUFA/Version.h>
		//#include <LUFA/Drivers/Board/LEDs.h>
		#include <LUFA/Drivers/USB/USB.h>

		#if defined(ADC)
			#include <LUFA/Drivers/Peripheral/ADC.h>
		#endif

		#include "Descriptors.h"

		#define LEDMASK_USB_NOTREADY      LEDS_LED1
		#define LEDMASK_USB_ENUMERATING  (LEDS_LED1 | LEDS_LED2)
		#define LEDMASK_USB_READY         LEDS_LED2
		#define LEDMASK_USB_ERROR         LEDS_LED1
		#define LEDMASK_BUSY             (LEDS_LED1 | LEDS_LED2)

		void SetupHardware(void);
		void ServoController_Task(void);

		void EVENT_USB_Device_Connect(void);
		void EVENT_USB_Device_Disconnect(void);
		void EVENT_USB_Device_ConfigurationChanged(void);

#endif

