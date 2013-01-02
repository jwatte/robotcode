#include "Descriptors.h"

const USB_Descriptor_Device_t PROGMEM DeviceDescriptor =
{
	.Header                 = {
        .Size = sizeof(USB_Descriptor_Device_t),
        .Type = DTYPE_Device
    },
	.USBSpecification       = VERSION_BCD(01.10),
	.Class                  = USB_CSCP_VendorSpecificClass,
	.SubClass               = USB_CSCP_NoDeviceSubclass,
	.Protocol               = USB_CSCP_NoDeviceProtocol,
	.Endpoint0Size          = ENDPOINT_CONTROLEP_DEFAULT_SIZE,
	.VendorID               = 0xf000,
	.ProductID              = 0x0002,
	.ReleaseNumber          = VERSION_BCD(01.01),
	.ManufacturerStrIndex   = 0x01,
	.ProductStrIndex        = 0x02,
	.SerialNumStrIndex      = USE_INTERNAL_SERIAL,
	.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

const USB_Descriptor_Configuration_t PROGMEM ConfigurationDescriptor =
{
	.Config = {
        .Header                 = {
            .Size = sizeof(USB_Descriptor_Configuration_Header_t),
            .Type = DTYPE_Configuration},
			.TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
			.TotalInterfaces        = 1,
			.ConfigurationNumber    = 1,
			.ConfigurationStrIndex  = NO_DESCRIPTOR,
			.ConfigAttributes       = (USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_SELFPOWERED),
			.MaxPowerConsumption    = USB_CONFIG_POWER_MA(100)
		},

	.DATA_Interface = {
			.Header                 = {
                .Size = sizeof(USB_Descriptor_Interface_t),
                .Type = DTYPE_Interface
            },
			.InterfaceNumber        = 0,
			.AlternateSetting       = 0,
			.TotalEndpoints         = 2,
			.Class                  = USB_CSCP_VendorSpecificClass,
			.SubClass               = USB_CSCP_NoDeviceSubclass,
			.Protocol               = USB_CSCP_NoDeviceProtocol,
			.InterfaceStrIndex      = NO_DESCRIPTOR
		},

    .DATA_DataOutEndpoint = {
        .Header                 = {
            .Size = sizeof(USB_Descriptor_Endpoint_t),
            .Type = DTYPE_Endpoint
        },
        .EndpointAddress        = (ENDPOINT_DIR_OUT | DATA_TX_EPNUM),
        .Attributes             = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize           = DATA_TX_EPSIZE,
        .PollingIntervalMS      = 0x0
    },

    .DATA_DataInEndpoint = {
        .Header                 = {
            .Size = sizeof(USB_Descriptor_Endpoint_t),
            .Type = DTYPE_Endpoint
        },
        .EndpointAddress        = (ENDPOINT_DIR_IN | DATA_RX_EPNUM),
        .Attributes             = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize           = DATA_RX_EPSIZE,
        .PollingIntervalMS      = 0x0
    },

};

const USB_Descriptor_String_t PROGMEM LanguageString = {
    .Header                 = {
        .Size = USB_STRING_LEN(1),
        .Type = DTYPE_String
    },
    .UnicodeString          = {
        LANGUAGE_ID_ENG
    },
};

const USB_Descriptor_String_t PROGMEM ManufacturerString = {
    .Header                 = {
        .Size = USB_STRING_LEN(6),
        .Type = DTYPE_String
    },
    .UnicodeString          = L"jwatte",
};

const USB_Descriptor_String_t PROGMEM ProductString = {
    .Header                 = {
        .Size = USB_STRING_LEN(10),
        .Type = DTYPE_String
    },
    .UnicodeString          = L"OnyxWalker"
};

uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
        const uint8_t wIndex,
        const void** const DescriptorAddress)
{
    const uint8_t  DescriptorType   = (wValue >> 8);
    const uint8_t  DescriptorNumber = (wValue & 0xFF);

    const void* Address = NULL;
    uint16_t    Size    = NO_DESCRIPTOR;

    switch (DescriptorType)
    {
        case DTYPE_Device:
            Address = &DeviceDescriptor;
            Size    = sizeof(USB_Descriptor_Device_t);
            break;
        case DTYPE_Configuration:
            Address = &ConfigurationDescriptor;
            Size    = sizeof(USB_Descriptor_Configuration_t);
            break;
        case DTYPE_String:
            switch (DescriptorNumber)
            {
                case 0x00:
                    Address = &LanguageString;
                    Size    = pgm_read_byte(&LanguageString.Header.Size);
                    break;
                case 0x01:
                    Address = &ManufacturerString;
                    Size    = pgm_read_byte(&ManufacturerString.Header.Size);
                    break;
                case 0x02:
                    Address = &ProductString;
                    Size    = pgm_read_byte(&ProductString.Header.Size);
                    break;
            }

            break;
    }

    *DescriptorAddress = Address;
    return Size;
}

