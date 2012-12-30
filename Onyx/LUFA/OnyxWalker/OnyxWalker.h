#ifndef _PacketForwarder_H_
#define _PacketForwarder_H_

		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/interrupt.h>
		#include <avr/power.h>

		#include <LUFA/Version.h>
		#include <LUFA/Drivers/USB/USB.h>

		#include "Descriptors.h"

		void SetupHardware(void);
		void OnyxWalker_Task(void);

		void EVENT_USB_Device_Connect(void);
		void EVENT_USB_Device_Disconnect(void);
		void EVENT_USB_Device_ConfigurationChanged(void);

#endif

