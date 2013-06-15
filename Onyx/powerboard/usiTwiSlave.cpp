/********************************************************************************

USI TWI Slave driver.

Created by Donald R. Blake
donblake at worldnet.att.net

---------------------------------------------------------------------------------

Created from Atmel source files for Application Note AVR312: Using the USI Module
as an I2C slave.

This program is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

---------------------------------------------------------------------------------

Change Activity:

    Date       Description
   ------      -------------
  16 Mar 2007  Created.
  27 Mar 2007  Added support for ATtiny261, 461 and 861.
  26 Apr 2007  Fixed ACK of slave address on a read.
  15 Jun 2013  Added ATTiny84A. Fixed rxTail bug. -- jw
  15 Jun 2013  Make it share bus with other addresses. -- jw

********************************************************************************/



/********************************************************************************

                                    includes

********************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "usiTwiSlave.h"
#include "usiTwiSlaveParts.h"



/********************************************************************************

                                   typedef's

********************************************************************************/

typedef enum
{
  USI_SLAVE_CHECK_ADDRESS                = 0x00,
  USI_SLAVE_SEND_DATA                    = 0x01,
  USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA = 0x02,
  USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA   = 0x03,
  USI_SLAVE_REQUEST_DATA                 = 0x04,
  USI_SLAVE_GET_DATA_AND_SEND_ACK        = 0x05
} overflowState_t;



/********************************************************************************

                                local variables

********************************************************************************/

static uint8_t                  slaveAddress;
static volatile overflowState_t overflowState;


static uint8_t          rxBuf[ TWI_RX_BUFFER_SIZE ];
static volatile uint8_t rxHead;
static volatile uint8_t rxTail;

static uint8_t          txBuf[ TWI_TX_BUFFER_SIZE ];
static volatile uint8_t txHead;
static volatile uint8_t txTail;




static void SET_USI_TO_SEND_ACK( void )
{
  /* prepare ACK */
  USIDR = 0;
  /* set SDA as output */
  DDR_USI |= ( 1 << PORT_USI_SDA );
  /* clear all interrupt flags, except Start Cond */
  USISR =
       ( 0 << USI_START_COND_INT ) |
       ( 1 << USIOIF ) | ( 1 << USIPF ) |
       ( 1 << USIDC )|
       /* set USI counter to shift 1 bit */
       ( 0x0E << USICNT0 );
}

static void SET_USI_TO_READ_ACK( void )
{
  /* set SDA as input */
  DDR_USI &= ~( 1 << PORT_USI_SDA );
  /* prepare ACK */
  USIDR = 0;
  /* clear all interrupt flags, except Start Cond */
  USISR =
       ( 0 << USI_START_COND_INT ) |
       ( 1 << USIOIF ) |
       ( 1 << USIPF ) |
       ( 1 << USIDC ) |
       /* set USI counter to shift 1 bit */
       ( 0x0E << USICNT0 );
}

static void SET_USI_TO_TWI_START_CONDITION_MODE( void )
{
  USICR =
       /* enable Start Condition Interrupt, disable Overflow Interrupt */
       ( 1 << USISIE ) | ( 0 << USIOIE ) |
       /* set USI in Two-wire mode, no USI Counter overflow hold */
       ( 1 << USIWM1 ) | ( 0 << USIWM0 ) |
       /* Shift Register Clock Source = External, positive edge */
       /* 4-Bit Counter Source = external, both edges */
       ( 1 << USICS1 ) | ( 0 << USICS0 ) | ( 0 << USICLK ) |
       /* no toggle clock-port pin */
       ( 0 << USITC );
  USISR =
        /* clear all interrupt flags, except Start Cond */
        ( 0 << USI_START_COND_INT ) | ( 1 << USIOIF ) | ( 1 << USIPF ) |
        ( 1 << USIDC ) | ( 0x0 << USICNT0 );
}

static void SET_USI_TO_SEND_DATA( void )
{
  /* set SDA as output */
  DDR_USI |=  ( 1 << PORT_USI_SDA );
  /* clear all interrupt flags, except Start Cond */
  USISR    =
       ( 0 << USI_START_COND_INT ) | ( 1 << USIOIF ) | ( 1 << USIPF ) |
       ( 1 << USIDC) |
       /* set USI to shift out 8 bits */
       ( 0x0 << USICNT0 );
}

static void SET_USI_TO_READ_DATA( void )
{
  /* set SDA as input */
  DDR_USI &= ~( 1 << PORT_USI_SDA );
  /* clear all interrupt flags, except Start Cond */
  USISR    =
       ( 0 << USI_START_COND_INT ) | ( 1 << USIOIF ) |
       ( 1 << USIPF ) | ( 1 << USIDC ) |
       /* set USI to shift out 8 bits */
       ( 0x0 << USICNT0 );
}



/********************************************************************************

                                local functions

********************************************************************************/



// flushes the TWI buffers

static
void
flushTwiBuffers(
  void
)
{
  rxTail = 0;
  rxHead = 0;
  txTail = 0;
  txHead = 0;
} // end flushTwiBuffers



/********************************************************************************

                                public functions

********************************************************************************/



// initialise USI for TWI slave mode

void
usiTwiSlaveInit(
  uint8_t ownAddress
)
{

  flushTwiBuffers( );

  slaveAddress = ownAddress;

  // In Two Wire mode (USIWM1, USIWM0 = 1X), the slave USI will pull SCL
  // low when a start condition is detected or a counter overflow (only
  // for USIWM1, USIWM0 = 11).  This inserts a wait state.  SCL is released
  // by the ISRs (USI_START_vect and USI_OVERFLOW_vect).

  // Set SCL and SDA as output
  DDR_USI |= ( 1 << PORT_USI_SCL ) | ( 1 << PORT_USI_SDA );

  // set SCL high
  PORT_USI |= ( 1 << PORT_USI_SCL );

  // set SDA high
  PORT_USI |= ( 1 << PORT_USI_SDA );

  // Set SDA as input
  DDR_USI &= ~( 1 << PORT_USI_SDA );

  USICR =
       // enable Start Condition Interrupt
       ( 1 << USISIE ) |
       // disable Overflow Interrupt
       ( 0 << USIOIE ) |
       // set USI in Two-wire mode, no USI Counter overflow hold
       ( 1 << USIWM1 ) | ( 0 << USIWM0 ) |
       // Shift Register Clock Source = external, positive edge
       // 4-Bit Counter Source = external, both edges
       ( 1 << USICS1 ) | ( 0 << USICS0 ) | ( 0 << USICLK ) |
       // no toggle clock-port pin
       ( 0 << USITC );

  // clear all interrupt flags and reset overflow counter

  USISR = ( 1 << USI_START_COND_INT ) | ( 1 << USIOIF ) | ( 1 << USIPF ) | ( 1 << USIDC );

} // end usiTwiSlaveInit



// put data in the transmission buffer, wait if buffer is full

bool
usiTwiTransmitByte(
  uint8_t data
)
{

  uint8_t tmphead;

  // calculate buffer index
  tmphead = ( txHead + 1 ) & TWI_TX_BUFFER_MASK;

    unsigned short cnt = 10000;

  // wait for free space in buffer
  while ( tmphead == txTail ) {
    if (cnt-- == 0) {
        return false;
    }
  }

  // store data in buffer
  txBuf[ tmphead ] = data;

  // store new index
  txHead = tmphead;

    return true;
} // end usiTwiTransmitByte



// return a byte from the receive buffer, wait if buffer is empty

uint8_t
usiTwiReceiveByte(
  void
)
{

    unsigned char ret;

    unsigned short cnt = 10000;

  // wait for Rx data
  while ( rxHead == rxTail ) {
    if (cnt-- == 0) {
        return 0;   //  how to tell apart from data?
    }
  }

    unsigned char temptail = ( rxTail + 1 ) & TWI_RX_BUFFER_MASK;

  // return data from the buffer.
  ret = rxBuf[ temptail ];

  // calculate buffer index
  rxTail = temptail;

  return ret;

} // end usiTwiReceiveByte



// check if there is data in the receive buffer

bool
usiTwiDataInReceiveBuffer(
  void
)
{

  // return 0 (false) if the receive buffer is empty
  return rxHead != rxTail;

} // end usiTwiDataInReceiveBuffer

bool
usiTwiTransmitBufferEmpty(
    void
)
{
    //  return 0 (false) if the transmit buffer has data in it
    return txHead == txTail;
}   //  end usiTwiTransmitBufferEmpty


/********************************************************************************

                            USI Start Condition ISR

********************************************************************************/

ISR( USI_START_VECTOR )
{
  // set default starting conditions for new TWI package
  overflowState = USI_SLAVE_CHECK_ADDRESS;

  // set SDA as input
  DDR_USI &= ~( 1 << PORT_USI_SDA );

    unsigned short cnt = 10000;

  // wait for SCL to go low to ensure the Start Condition has completed (the
  // start detector will hold SCL low ) - if a Stop Condition arises then leave
  // the interrupt to prevent waiting forever - don't use USISR to test for Stop
  // Condition as in Application Note AVR312 because the Stop Condition Flag is
  // going to be set from the last TWI sequence
  while (
       // SCL his high
       ( PIN_USI & ( 1 << PIN_USI_SCL ) ) &&
       // and SDA is low
       !( ( PIN_USI & ( 1 << PIN_USI_SDA ) ) )
  ) {
    if (cnt-- == 0) {
        goto stop_condition;
    }
  }


  if ( !( PIN_USI & ( 1 << PIN_USI_SDA ) ) )
  {

    // a Stop Condition did not occur

    USICR =
         // keep Start Condition Interrupt enabled to detect RESTART
         ( 1 << USISIE ) |
         // enable Overflow Interrupt
         ( 1 << USIOIE ) |
         // set USI in Two-wire mode, hold SCL low on USI Counter overflow
         ( 1 << USIWM1 ) | ( 1 << USIWM0 ) |
         // Shift Register Clock Source = External, positive edge
         // 4-Bit Counter Source = external, both edges
         ( 1 << USICS1 ) | ( 0 << USICS0 ) | ( 0 << USICLK ) |
         // no toggle clock-port pin
         ( 0 << USITC );

  }
  else
  {
stop_condition:

    USICR =
         // enable Start Condition Interrupt
         ( 1 << USISIE ) |
         // disable Overflow Interrupt
         ( 0 << USIOIE ) |
         // set USI in Two-wire mode, no USI Counter overflow hold
         ( 1 << USIWM1 ) | ( 0 << USIWM0 ) |
         // Shift Register Clock Source = external, positive edge
         // 4-Bit Counter Source = external, both edges
         ( 1 << USICS1 ) | ( 0 << USICS0 ) | ( 0 << USICLK ) |
         // no toggle clock-port pin
         ( 0 << USITC );

  } // end if

  USISR =
      // clear interrupt flags - resetting the Start Condition Flag will
      // release SCL
      ( 1 << USI_START_COND_INT ) | ( 1 << USIOIF ) |
      ( 1 << USIPF ) |( 1 << USIDC ) |
      // set USI to sample 8 bits (count 16 external SCL pin toggles)
      ( 0x0 << USICNT0);

} // end ISR( USI_START_VECTOR )



/********************************************************************************

                                USI Overflow ISR

Handles all the communication.

Only disabled when waiting for a new Start Condition.

********************************************************************************/

ISR( USI_OVERFLOW_VECTOR )
{
    unsigned short temphead;
    unsigned short temptail;

  switch ( overflowState )
  {

    // Address mode: check address and send ACK (and next USI_SLAVE_SEND_DATA) if OK,
    // else reset USI
    case USI_SLAVE_CHECK_ADDRESS:
      if ( ( USIDR == 0 ) || ( ( USIDR >> 1 ) == slaveAddress) )
      {
          if ( USIDR & 0x01 )
        {
            if (txHead != txTail)
            {
              overflowState = USI_SLAVE_SEND_DATA;
            }
            else
            {
                //  I have no data; not acking!
                SET_USI_TO_TWI_START_CONDITION_MODE();
                break;
            }
        }
        else
        {
          overflowState = USI_SLAVE_REQUEST_DATA;
        } // end if
        SET_USI_TO_SEND_ACK( );
      }
      else
      {
        SET_USI_TO_TWI_START_CONDITION_MODE( );
      }
      break;

    // Master write data mode: check reply and goto USI_SLAVE_SEND_DATA if OK,
    // else reset USI
    case USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA:
      if ( USIDR )
      {
        // if NACK, the master does not want more data
        SET_USI_TO_TWI_START_CONDITION_MODE( );
        goto end_of_func;
      }
      // from here we just drop straight into USI_SLAVE_SEND_DATA if the
      // master sent an ACK

    // copy data from buffer to USIDR and set USI to shift byte
    // next USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA
    case USI_SLAVE_SEND_DATA:
      // Get data from Buffer
      if ( txHead != txTail )
      {
        temptail = ( txTail + 1 ) & TWI_TX_BUFFER_MASK;
        USIDR = txBuf[ temptail ];
        txTail = temptail;
      }
      else
      {
        // the buffer is empty
        SET_USI_TO_TWI_START_CONDITION_MODE( );
        goto end_of_func;
      } // end if
      overflowState = USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA;
      SET_USI_TO_SEND_DATA( );
      break;

    // set USI to sample reply from master
    // next USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA
    case USI_SLAVE_REQUEST_REPLY_FROM_SEND_DATA:
      overflowState = USI_SLAVE_CHECK_REPLY_FROM_SEND_DATA;
      SET_USI_TO_READ_ACK( );
      break;

    // Master read data mode: set USI to sample data from master, next
    // USI_SLAVE_GET_DATA_AND_SEND_ACK
    case USI_SLAVE_REQUEST_DATA:
      overflowState = USI_SLAVE_GET_DATA_AND_SEND_ACK;
      SET_USI_TO_READ_DATA( );
      break;

    // copy data from USIDR and send ACK
    // next USI_SLAVE_REQUEST_DATA
    case USI_SLAVE_GET_DATA_AND_SEND_ACK:
      // put data into buffer
      // Not necessary, but prevents warnings
      temphead = ( rxHead + 1 ) & TWI_RX_BUFFER_MASK;
      rxBuf[ temphead ] = USIDR;
      rxHead = temphead;
      // next USI_SLAVE_REQUEST_DATA
      overflowState = USI_SLAVE_REQUEST_DATA;
      SET_USI_TO_SEND_ACK( );
      break;

  } // end switch

end_of_func:
    return;
} // end ISR( USI_OVERFLOW_VECTOR )
