#ifndef _DESCRIPTORS_H_
#define _DESCRIPTORS_H_

#include <avr/pgmspace.h>
#include <LUFA/Drivers/USB/USB.h>
#include "AppConfig.h"
#include "LUFAConfig.h"

//  IN direction
#define DATA_RX_EPNUM        1
#define DATA_RX_EPSIZE     128
//  OUT direction
#define DATA_TX_EPNUM        2
#define DATA_TX_EPSIZE      64

typedef struct {
    USB_Descriptor_Configuration_Header_t    Config;
    USB_Descriptor_Interface_t               DATA_Interface;
    USB_Descriptor_Endpoint_t                DATA_DataInEndpoint;
    USB_Descriptor_Endpoint_t                DATA_DataOutEndpoint;
} USB_Descriptor_Configuration_t;

/*
uint16_t CALLBACK_USB_GetDescriptor(
        const uint16_t wValue,
        const uint8_t wIndex,
        const void** const DescriptorAddress)
    ATTR_WARN_UNUSED_RESULT ATTR_NON_NULL_PTR_ARG(3);
 */

#endif

