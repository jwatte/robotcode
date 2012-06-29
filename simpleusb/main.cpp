
#include <libusb.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>

class ER {
public:
    ER & operator=(int err) {
        if (err < 0) {
            std::cerr << "USB Error: " << libusb_error_name(err) << std::endl;
            exit(1);
        }
        return *this;
    }
    ER &operator=(libusb_device_handle *h) {
        if (h == 0) {
            std::cerr << "USB Device not found!" << std::endl;
            exit(1);
        }
        return *this;
    }
    ER &operator=(libusb_transfer *t) {
        if (t == 0) {
            std::cerr << "USB transfer allocate failed!" << std::endl;
            exit(1);
        }
        return *this;
    }
};
ER er;


void start_transfer(libusb_transfer *);

void in_cb(libusb_transfer *xin) {
    if (xin->actual_length > 0) {
        std::cout << "flags: " << std::hex << (unsigned int)xin->flags << ", ";
        std::cout << "endpoint: " << std::hex << (unsigned int)xin->endpoint << ", ";
        std::cout << "type: " << std::hex << (unsigned int)xin->type << ", ";
        std::cout << "length: " << std::dec << xin->length << ", ";
        std::cout << "actual_length: " << std::dec << xin->actual_length << ", ";
        std::cout << std::endl;
        std::cout << "Got data: " << xin->actual_length << ": ";
        for (unsigned char *p = xin->buffer; p != xin->buffer + xin->actual_length; ++p) {
            std::cout << std::hex << (unsigned int)*p << std::dec << " ";
        }
        std::cout << std::endl;
    }
    usleep(10000);
    start_transfer(xin);
}

#define EP_INFO 0x81
#define EP_DATAIN 0x82
#define EP_DATAOUT 0x03

static unsigned char bufin[64];

void start_transfer(libusb_transfer *xin) {
    libusb_fill_bulk_transfer(xin, xin->dev_handle, EP_DATAIN, bufin, 64, in_cb, 0, 0);
    er = libusb_submit_transfer(xin);
}

int main() {
    libusb_context *ctx = 0;
    er = libusb_init(&ctx);
    libusb_device_handle *dh = libusb_open_device_with_vid_pid(ctx, 0xf000, 0x0001);
    er = dh;

    libusb_device_descriptor ldd;
    er = libusb_get_device_descriptor(libusb_get_device(dh), &ldd);
    std::cout << "Device: ";
    std::cout << "bLength: " << (unsigned int)ldd.bLength << ", ";
    std::cout << "bDescriptorType: " << (unsigned int)ldd.bDescriptorType << ", ";
    std::cout << "bMaxPacketSize0: " << (unsigned int)ldd.bMaxPacketSize0 << ", ";
    std::cout << "bNumConfigurations: " << (unsigned int)ldd.bNumConfigurations << ", ";
    std::cout << std::endl;

    er = libusb_set_configuration(dh, 1);

    libusb_config_descriptor *lcd;
    er = libusb_get_active_config_descriptor(libusb_get_device(dh), &lcd);
    std::cout << "Config: ";
    std::cout << "bLength: " << (unsigned int)lcd->bLength << ", ";
    std::cout << "wTotalLength: " << (unsigned int)lcd->wTotalLength << ", ";
    std::cout << "bNumInterfaces: " << (unsigned int)lcd->bNumInterfaces << ", ";
    std::cout << "bConfigurationValue: " << (unsigned int)lcd->bConfigurationValue << ", ";
    std::cout << "extra_length: " << (unsigned int)lcd->extra_length << ", ";
    std::cout << std::endl;
    libusb_interface const *lcif = lcd->interface;
    std::cout << "Interface: ";
    std::cout << "num_altsetting: " << (unsigned int)lcif->num_altsetting << ", ";
    std::cout << std::endl;

    libusb_interface_descriptor const *lida = lcif->altsetting;
    for (int i = 0; i != lcif->num_altsetting; ++i) {
        libusb_interface_descriptor const *lid = lida + i;
        std::cout << "Interface Description " << i << ": ";
        std::cout << "bLength: " << (unsigned int)lid->bLength << ", ";
        std::cout << "bDescriptorType: " << (unsigned int)lid->bDescriptorType << ", ";
        std::cout << "bInterfaceNumber: " << (unsigned int)lid->bInterfaceNumber << ", ";
        std::cout << "bAlternateSetting: " << (unsigned int)lid->bAlternateSetting << ", ";
        std::cout << "iInterface: " << (unsigned int)lid->iInterface << ", ";
        std::cout << "extra_length: " << (unsigned int)lid->extra_length << ", ";
        std::cout << "bNumEndpoints: " << (unsigned int)lid->bNumEndpoints << ", ";
        std::cout << std::endl;
        for (int j = 0; j != lid->bNumEndpoints; ++j) {
            libusb_endpoint_descriptor const *led = lid->endpoint + j;
            std::cout << "  Endpoint " << j << ": ";
            std::cout << "bLength: " << (unsigned int)led->bLength << ", ";
            std::cout << "bDescriptorType: " << (unsigned int)led->bDescriptorType << ", ";
            std::cout << "bEndpointAddress: " << (unsigned int)led->bEndpointAddress << ", ";
            std::cout << "wMaxPacketSize: " << (unsigned int)led->wMaxPacketSize << ", ";
            std::cout << "extra_length: " << (unsigned int)led->extra_length;
            std::cout << std::endl;
        }
    }
    libusb_free_config_descriptor(lcd);

    er = libusb_claim_interface(dh, 0);
    libusb_transfer *xin = libusb_alloc_transfer(0);
    er = xin;
    xin->dev_handle = dh;
    libusb_transfer *xout = libusb_alloc_transfer(0);
    er = xout;
    xout->dev_handle = dh;

    start_transfer(xin);
    while (true) {
        er = libusb_handle_events(ctx);
    }
    libusb_exit(ctx);
    return 0;
}

