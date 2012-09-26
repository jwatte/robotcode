#include "libavr.h"
#include "pins_avr.h"

//  How long to settle the ADC after switching the input mux
#define SETTLE_US 10

void adc_setup(bool use_aref) {
    power_adc_enable();
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);  //  enable, prescaler @ 0.25 MHz @ 16 MHz
    ADMUX = (use_aref ? 0 : (1 << REFS0)) | (1 << ADLAR);
}

void (*_adc_cb)(unsigned char val) = 0;

void _adc_result(void *cb) {
    void (*comp)(unsigned char) = _adc_cb;
    _adc_cb = 0;
    (*comp)(ADCH);
}

ISR(ADC_vect) {
    if (!_adc_cb) {
        fatal(FATAL_ADC_BADCALL);
    }
    after(0, _adc_result, 0);
    ADCSRA = ADCSRA & ~((1 << ADSC) | (1 << ADATE) | (1 << ADIE));
}

bool adc_busy() {
    unsigned char adcsra = ADCSRA;
    return (_adc_cb != 0) || ((adcsra & (1 << ADSC)) != 0);
}

void adc_read(unsigned char channel, void (*cb)(unsigned char val)) {
    if (adc_busy()) {
        fatal(FATAL_ADC_BUSY);
    }
    if (channel > 5) {
        fatal(FATAL_ADC_BAD_CHANNEL);
    }
    _adc_cb = cb;
    ADMUX = (ADMUX & 0xf0) | channel;
    udelay(SETTLE_US); //  give the charge cap some time to settle
    DIDR0 |= (1 << channel);  //  disable ADC input as digital
    ADC_DDR &= ~(1 << channel);
    ADC_PORT &= ~(1 << channel);
    ADCSRA = ADCSRA | (1 << ADSC) | (1 << ADIF) | (1 << ADIE);  //  start conversion, clear interrupt flag, enable interrupt
}



