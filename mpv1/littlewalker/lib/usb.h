#if !defined(littlewalker_usb_h)
#define littlewalker_usb_h

void init_usb();
void send_usb(void const *data, size_t size);
void geterrcnt(char const *name);

#endif  //  littlewalker_usb_h
