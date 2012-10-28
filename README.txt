
avr: contains the AVR support library, and firmware for the various AVR boards
  commusb: the atmega part of the Arduino UNO that talks to the Linux host.
  motorbard: the board that drives steering servo, locomotion H-bridge, and 
    e-stop radio, as well as senses 7.4V power voltage
  sensorboard: the board that polls the 3 ping and 3 IR sensors, as well as 
    blinks the lasers

LUFA: contains the "PacketForwarder" code that runs on the 8u2 part of the 
  Arduino board to talk to the host. Translates to the serial link from the 
  8u2 t the atmega.

rl2: contains the Linux host program, using FLTK for a GUI.

