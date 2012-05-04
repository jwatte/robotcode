
#if !defined(nRF24L01_h)
#define nRF24L01_h

#include "pins_avr.h"
#include "nRF24L01_regs.h"
#include "spi.h"
#include "libavr.h"

template<
  bool InstallIRQ = true,
  byte IRQ = 16|6,
  byte CSN = 16|5,
  byte CE = 16|4,
  byte MAX_PAYLOAD = 32>
class nRF24L01 {
public:
  typedef enum {
    IdleMode,
    ReadMode,
    WriteMode
  } ChipMode;

  nRF24L01() {
    initVars();
  }

  inline bool getInstallIRQ() const { return InstallIRQ; }
  inline byte getPinIRQ() const { return IRQ; }
  inline byte getPinCSN() const { return CSN; }
  inline byte getPinCE() const { return CE; }
  inline byte getMaxPayload() const { return MAX_PAYLOAD; }
  inline ChipMode getMode() const { return theMode_; }

  void setup(uint8_t chan, uint16_t addrRecv) {
    {
      IntDisable idi;

      enable_spi();
      pinMode(CSN, OUTPUT);
      digitalWrite(CSN, HIGH);
      pinMode(CE, OUTPUT);
      digitalWrite(CE, LOW);
      pinMode(IRQ, INPUT);
      //  you will need to arrange for pinchange2 interrupt to 
      //  call the irq() function here.
      if (InstallIRQ) {
        pcMaskReg(IRQ) = pcMaskBit(IRQ);
        pcCtlReg(IRQ) = pcCtlBit(IRQ);
      }
      initVars();

      setRegister(NRF_FEATURE, (1 << NRF_EN_DPL));
      //  channel
      setRegister(NRF_RF_CH, chan);
      //  turn on the chip, go to standby-1 mode
      rCONFIG_ = (1 << NRF_PWR_UP) | (1 << NRF_PRIM_RX) | (1 << NRF_EN_CRC) | (1 << NRF_CRCO);
      setRegister(NRF_CONFIG, rCONFIG_);
      //  enable auto ack on pipe 0
      setRegister(NRF_EN_AA, (1 << NRF_ENAA_P0));
      //  enable address on pipe 0
      setRegister(NRF_EN_RXADDR, (1 << NRF_ERX_P0));
      //  five byte addresses
      setRegister(NRF_SETUP_AW, 3);
      //  re-transmit behavior; 750 us delay, 4 re-transmits
      setRegister(NRF_SETUP_RETR, (2 << NRF_ARD) | (4 << NRF_ARC));

      //  max power!
      setRegister(NRF_RF_SETUP, (3 << NRF_RF_PWR) | (1 << NRF_LNA_HCURR));

      //  clear any interrupts
      setRegister(NRF_STATUS, 0x70);
      //  setup rx addrs
      unsigned char addr[5] = { 0x69, 0x09, 0x28, addrRecv >> 8, addrRecv & 0xff };
      setRegister(NRF_RX_ADDR_P0, 5, addr);
      setRegister(NRF_TX_ADDR, 5, addr);
      //  enable dynamic payload length for pipe 0
      setRegister(NRF_DYNPD, (1 << NRF_DPL_P0));
    }

    //  wait for crystal to stabilize
    delay(2);

    //  start receive
    enterReadMode();
  }

  //  after teardown, you can call setup() again to 
  //  switch address, switch channel, etc
  void teardown() {
    IntDisable idi;

    enterIdleMode();
    rCONFIG_ &= ~(1 << NRF_PWR_UP);
    setRegister(NRF_CONFIG, rCONFIG_);
  }

  //  call this when you get an IRQ
  void onIRQ() {
    debugBits_ |= 8;
    if (!digitalRead(IRQ)) {
      debugBits_ |= 0x80;
      uint8_t ui = getRegister(NRF_STATUS) & 0x70;
      //  clear interrupts
      setRegister(NRF_STATUS, ui);
      //  schedule callback
      if ((ui & irqMask_) != ui) {
        if (irqMask_ == 0) {
          after(0, &irq_tramp, this);
        }
        irqMask_ |= ui;
      }
    }
  }

  bool hasLostPacket() {
    return lostPacket_;
  }

  uint8_t hasData() {
    return hasData_;
  }

  uint8_t readData(uint8_t len, void *buf) {
    if (!hasData_) {
      return 0;
    }
    if (len > hasData_) {
      len = hasData_;
    }
    memcpy(buf, packetBuf_, len);
    hasData_ -= len;
    if (hasData_ > 0) {
      memmove(&packetBuf_[0], &packetBuf_[len], hasData_);
    }
    return len;
  }


  bool canWriteData() {
    return theMode_ != WriteMode;
  }

  bool writeData(uint8_t len, void const *buf) {
    if (theMode_ == WriteMode) {
      return false;
    }
    enterWriteMode();
    lostPacket_ = false;
    writePayload(len, buf);
    pulseCE();
    return true;
  }

  uint8_t readClearDebugBits() {
    IntDisable idi;
    uint8_t ret = debugBits_;
    debugBits_ = 0;
    return ret;
  }

private:
  void pulseCE() {
    digitalWrite(CE, HIGH);
    udelay(10);
    digitalWrite(CE, LOW);
  }

  static void irq_tramp(void *that) {
    ((nRF24L01 *)that)->irq_service();
  }

  void irq_service() {
    uint8_t irqBits;
    {
      IntDisable idi;
      irqBits = irqMask_;
      irqMask_ = 0;
    }

    ChipMode newMode = theMode_;
    if (irqBits & (1 << NRF_MAX_RT)) {
      debugBits_ |= 0x10;
      lostPacket_ = true;
      newMode = ReadMode;
      sendCommand(NRF_FLUSH_TX);
    }
    if (irqBits & (1 << NRF_TX_DS)) {
      debugBits_ |= 0x20;
      //  I wrote successfully
      newMode = ReadMode;
    }
    if (irqBits & (1 << NRF_RX_DR)) {
      debugBits_ |= 0x40;
      //  I received data -- turn off enable
      hasData_ = getRegister(NRF_RX_PW_P0);
      readPayload(hasData_, packetBuf_);
      if (newMode == ReadMode) {
        //  don't receive when having pending data, but don't 
        //  screw with a write mode that is pending
        theMode_ = IdleMode;
      }
    }
    enterMode(newMode);
  }

  void setRegister(uint8_t reg, uint8_t value) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(reg | 0x20);
    shift_spi(value);
    digitalWrite(CSN, HIGH);
  }
  void setRegister(uint8_t reg, uint8_t valLen, void const *data) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(reg | 0x20);
    for (uint8_t i = 0; i < valLen; ++i) {
      shift_spi(((uint8_t const *)data)[i]);
    }
    digitalWrite(CSN, HIGH);
  }

  uint8_t getRegister(uint8_t reg) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(reg);
    uint8_t ret = shift_spi(0);
    digitalWrite(CSN, HIGH);
    return ret;
  }

  void writePayload(uint8_t len, void const *data) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(NRF_W_TX_PAYLOAD);
    for (uint8_t i = 0; i < len; ++i) {
      shift_spi(((uint8_t const *)data)[i]);
    }
    digitalWrite(CSN, HIGH);
  }

  void readPayload(uint8_t len, void *data) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(NRF_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < len; ++i) {
      ((uint8_t *)data)[i] = shift_spi(0);
    }
    digitalWrite(CSN, HIGH);
  }

  void sendCommand(uint8_t cmd) {
    IntDisable idi;
    digitalWrite(CSN, LOW);
    shift_spi(cmd);
    digitalWrite(CSN, HIGH);
  }

  void initVars() {
    theMode_ = IdleMode;
    lostPacket_ = false;
    hasData_ = 0;
    rCONFIG_ = 0;
    irqMask_ = 0;
    debugBits_ = 0;
  }

  void enterReadMode() {
    debugBits_ |= 1;
    theMode_ = ReadMode;
    digitalWrite(CE, LOW);
    rCONFIG_ |= (1 << NRF_PRIM_RX) | (1 << NRF_PWR_UP);
    setRegister(NRF_CONFIG, rCONFIG_);
    digitalWrite(CE, HIGH);
  }

  void enterIdleMode() {
    debugBits_ |= 2;
    theMode_ = IdleMode;
    digitalWrite(CE, LOW);
  }

  void enterWriteMode() {
    debugBits_ |= 4;
    digitalWrite(CE, LOW);
    rCONFIG_ &= ~(1 << NRF_PRIM_RX);
    rCONFIG_ |= (1 << NRF_PWR_UP);
    setRegister(NRF_CONFIG, rCONFIG_);
    theMode_ = WriteMode;
  }

  void enterMode(ChipMode m) {
    if (m != theMode_) {
      switch (m) {
        case ReadMode: enterReadMode(); break;
        case WriteMode: enterWriteMode(); break;
        case IdleMode: enterIdleMode(); break;
      }
    }
  }

  volatile ChipMode theMode_;
  volatile bool lostPacket_;
  volatile uint8_t hasData_;
  volatile uint8_t rCONFIG_;
  volatile uint8_t irqMask_;
  volatile uint8_t debugBits_;
  char packetBuf_[MAX_PAYLOAD];
};


#endif  //  nRF24L01_h
