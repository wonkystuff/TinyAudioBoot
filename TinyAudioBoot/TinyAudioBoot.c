/*
  AudioBoot - flashing a microcontroller by PC audio line out

  Originally the bootloader was made for Atmega8 and Atmega168 microcontrollers.

  Budi Prakosa a.k.a Iyok reworked the code to get it running on an Attiny85 microcontroller.

  Parts of the  * equinox-boot.c - bootloader for equinox
  from Frank Meyer and Robert Meyer are used to access the FLASH memory.

  Hardware:   Attiny85

  input pin:  should be connected to a voltage divider.
  output pin: LED for status indication of the bootloader

  The input pin is also connected by a 100nF capacitor to the PC line out

  The Atmega168 seems to have the switching voltage level at 2.2V
  The Atmega8 at 1.4V
  The switching levels of the input pins may vary a little bit from one
  MC to another.  If you to be able to adjust the voltages,
  use a 10k poti as voltage divider.


  As development platform an Arduino Diecimilla was used. Therefore you
  will find many #ifdefs for the Arduino in this code.
  If you want to optimize the bootloader further you may use an Arduino
  as development platform.


  necessary setup

  1. Project->ConfigurationOptions->Processortype
  2. Project->ConfigurationOptions->Programming Modell 'Os'
  3. Project->ConfigurationOptions->CustomOptions->LinkerOptions->see further down

  There is an article how to make an ATTINY boot loader ( German ):
  http://www.mikrocontroller.net/articles/Konzept_f%C3%BCr_einen_ATtiny-Bootloader_in_C
  ( thanks to the author of the article, very well written )


  Creating the bootloader with Atmel Studio 7
  ===========================================

  1. You have to define the bootloader sections and reset vector location

  => Toolchain/AVR_GNU_Linker/Memory Settings
  .bootreset=0x00
  .text=0xE00 // for 1KB Bootloader
  .text=0x0C00 // for 2KB Bootloader

   explanation:
  .text=0x0E00 *2 = 0x1C00 ==> this is the start address of the boot loader with 1KB size
  .text=0x0C00 *2 = 0x1800 ==> this is the start address of the boot loader with 2KB size

  2. Disable unused sections optimization in the linker
  Be sure that in the linker parameters this is not used: -Wl, --gc-sections
  disable the following check box:
  ==>Toolchain/AVR_GNU_C Compiler/Optimization/Garbage collect unused sections


  Fuse settings for the bootloader
  ================================

  There fuses have to match certain conditions.
  Mainly SELFPROGEN has to be set, Brown-Out-Detection activated and
  CKDIV8 disabled to achieve the needed F_CPU of 8MHz

  FUSES Attiny 85 ( F_CPU 16MHz with PLL )
  ========================================
  Extended: 0xFE
  HIGH:     0xDD
  LOW:      0xE1

  ************************************************************************************

  v0.1  19.6.2008 C. -H-A-B-E-R-E-R-  Bootloader for IR-Interface
  v1.0  03.9.2011 C. -H-A-B-E-R-E-R-  Bootloader for audio signal
  v1.1  05.9.2011 C. -H-A-B-E-R-E-R-  changing pin setup, comments, and exitcounter=3
  v1.2  12.5.2012 C. -H-A-B-E-R-E-R-  Atmega8 Support added, java program has to be adapted too
  v1.3  20.5.2012 C. -H-A-B-E-R-E-R-  now interrupts of user program are working
  v1.4  05.6.2012 C. -H-A-B-E-R-E-R-  signal coding changed to differential manchester code
  v2.0  13.6.2012 C. -H-A-B-E-R-E-R-  setup for various MCs
  v3.0  30.1.2017 B. -P-r-a-k-o-s-a-  first version of Attiny85 Audio Bootloader
  v3.1  04.2.2017 C. -H-A-B-E-R-E-R-  clean reset vector added, description added, pins rerouted
  v3.2  18.7.2017 C. -P-r-a-k-o-s-a-  various refactor, added eeprom write mode, makefile for compiling using arduino ide toolchain
  v3.3  03.02.2021 J. T-u-f-f-e-n-    bootloader entered if button pressed rather than time-delayed.
  v3.4  28.03.2025 J. T-u-f-f-e-n-    bootloader entered if button pressed when powered on for mmo micro-module.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  It is mandatory to keep the list of authors in this code.

*/
/*


           schematic for audio input
           =========================

                      VCC
                       |
                      | | 10K
                      | |
                       |
                       |
 audio in >-----||-----o------->  soundprog ( digital input pin )
               100nF   |
                       |
                      | | 10K
                      | |
                       |
                      GND



                              Pinout ATtiny25/45/85
                              =====================


                                     _______
                                    |   U   |
         (PCINT5/RESET/ADC0/dW) PB5-|       |- VCC
  (PCINT3/XTAL1/CLKI/OC1B/ADC3) PB3-| ATTINY|- PB2 (SCK/USCK/SCL/ADC1/T0/INT0/PCINT2)
  (PCINT4/XTAL2/CLKO/OC1B/ADC2) PB4-|   85  |- PB1 (MISO/DO/AIN1/OC0B/OC1A/PCINT1)
                                GND-|       |- PB0 (MOSI/DI/SDA/AIN0/OC0A/OC1A/AREF/PCINT0)
                                    |_______|



                                 Pinout ARDUINO
                                 ==============
                                     _______
                                    |   U   |
                          reset/PB5-|       |- VCC
    soundprog->  D3/A3          PB3-| ATTINY|- PB2       D2/A1
                 D4/A2          PB4-|   85  |- PB1       D1     -> ARDUINO_LED
                                GND-|       |- PB0       D0     -> BOOTCHECK
                                    |_______|
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

// Configuration options
#define WONKYSTUFF  (1)
#define USELED      (1)

// This value has to be adapted to the bootloader size
// If you change this, please change BOOTLOADER_ADDRESS on Makefile too

#define BOOTLOADER_ADDRESS     0x1BC0               // bootloader start address, e.g. 0x1C00 = 7168, set .text to 0x0E00

#define RJMP                   (0xC000U - 1)        // opcode of RJMP minus offset 1
#define RESET_SECTION          __attribute__((section(".bootreset"))) __attribute__((used))

// this variable seems to be unused
// but in fact it is written into the flash section
// you could find it in the *.hex file
uint16_t resetVector RESET_SECTION = RJMP + BOOTLOADER_ADDRESS / 2;

#ifdef USELED
#ifdef MMO
    #define LEDPORT      ( 1u << PB0 ); // PB0 is ATTiny85 pin 5
#else
    #define LEDPORT      ( 1u << PB1 ); // PB1 is ATTiny85 pin 6
#endif
    #define INITLED()    { DDRB |= LEDPORT; }

    #define LEDON()      { PORTB |= LEDPORT;}
    #define LEDOFF()     { PORTB &= ~LEDPORT;}
    #define TOGGLELED()  { PORTB ^= LEDPORT;}

#else

    #define INITLED()

    #define LEDON()
    #define LEDOFF()
    #define TOGGLELED()

#endif

#ifdef  WONKYSTUFF

#ifdef MMO
#define BOOTCHECKPIN    (1u << PB1)
#else
#define BOOTCHECKPIN    (1u << PB0)
#endif

#define INITBOOTCHECK() {DDRB &= ~BOOTCHECKPIN; PORTB |= BOOTCHECKPIN; } // boot-check pin is input
#else
#define INITBOOTCHECK()
#endif

#ifdef MMO
#define INPUTAUDIOPIN   (1u << PB2) // PB2 is ATTiny85 pin 7
#else
#define INPUTAUDIOPIN   (1u << PB3) // PB3 is ATTiny85 pin 2
#endif
#define PINVALUE        (PINB & INPUTAUDIOPIN)
#define INITAUDIOPORT() {DDRB &= ~INPUTAUDIOPIN;} // audio pin is input

#define WAITBLINKTIME   10000
#define BOOT_TIMEOUT    10

#define true            (1==1)
#define false           (!true)

//***************************************************************************************
// main loop
//***************************************************************************************

#define TIMER TCNT0 // we use timer1 for measuring time

// frame format definition: indices
#define COMMAND         0u
#define PAGEINDEXLOW    1u  // page address lower part
#define PAGEINDEXHIGH   2u  // page address higher part
#define LENGTHLOW       3u
#define LENGTHHIGH      4u
#define CRCLOW          5u  // checksum lower part
#define CRCHIGH         6u  // checksum higher part
#define DATAPAGESTART   7u  // start of data
#define PAGESIZE        SPM_PAGESIZE
#define FRAMESIZE       (PAGESIZE+DATAPAGESTART) // size of the data block to be received

// bootloader commands
#define NOCOMMAND       0u
#define TESTCOMMAND     1u
#define PROGCOMMAND     2u
#define RUNCOMMAND      3u
#define EEPROMCOMMAND   4u
#define EXITCOMMAND     5u

uint8_t FrameData[ FRAMESIZE ];

#define FLASH_RESET_ADDR        0x0000                // address of reset vector (in bytes)
#define BOOTLOADER_STARTADDRESS BOOTLOADER_ADDRESS    // start address:
#define BOOTLOADER_ENDADDRESS   0x2000                // end address:   0x2000 = 8192
                                                      // this is the size of the Attiny85 flash in bytes

#define LAST_PAGE (BOOTLOADER_STARTADDRESS - SPM_PAGESIZE) / SPM_PAGESIZE

#include <avr/boot.h>
#ifndef RWWSRE                                        // bug in AVR libc:
#define RWWSRE CTPB                                   // RWWSRE is not defined on ATTinys, use CTBP instead
#endif

void (*start_appl_main) (void);

#define BOOTLOADER_FUNC_ADDRESS (BOOTLOADER_STARTADDRESS - sizeof (start_appl_main))

#define sei() asm volatile("sei")
#define cli() asm volatile("cli")
#define nop() asm volatile("nop")
#define wdr() asm volatile("wdr")

//AVR ATtiny85 Programming: EEPROM Reading and Writing - YouTube
//https://www.youtube.com/watch?v=DO-D6YmRpJk

void
eeprom_write(uint16_t address, uint8_t data)
{
    while(EECR & (1<<EEPE));

    EECR = (0<<EEPM1) | (0<<EEPM0);

    if (address < 512)
    {
        EEAR = address;
    }
    else
    {
        EEAR = 511;
    }

    EEDR = data;

    EECR |= (1<<EEMPE);
    EECR |= (1<<EEPE);
}

//***************************************************************************************
// receiveFrame()
//
// This routine receives a differential manchester coded signal at the input pin.
// The routine waits for a toggling voltage level.
// It automatically detects the transmission speed.
//
// output:    uint8_t flag:     true: checksum OK
//            uint8_t FramData: global data buffer
//
//***************************************************************************************
uint8_t
receiveFrame(void)
{
    uint16_t counter = 0;
    volatile uint16_t time = 0;
    volatile uint16_t delayTime;
    uint8_t p, t;
    uint8_t k = 8;
    uint8_t dataPointer = 0;
    uint16_t n;

    //*** synchronisation and bit rate estimation **************************
    time = 0;
    // wait for edge
    p = PINVALUE;
    while (p == PINVALUE)
        ;

    p = PINVALUE;

    TIMER = 0; // reset timer
    for (n = 0; n < 16; n++)
    {
        // wait for edge
        while (p == PINVALUE)
            ;

        t = TIMER;
        TIMER = 0; // reset timer
        p = PINVALUE;

        if (n >= 8)
        {
            time += t; // time accumulator for mean period calculation only the last 8 times are used
        }
    }

    delayTime = time * 3 / 4 / 8;
    // delay 3/4 bit
    while (TIMER < delayTime)
        ;

    //****************** wait for start bit ***************************
    while (p == PINVALUE) // while not startbit ( no change of pinValue means 0 bit )
    {
        // wait for edge
        while (p == PINVALUE)
            ;
        p = PINVALUE;
        TIMER = 0;

        // delay 3/4 bit
        while (TIMER < delayTime)
            ;
        TIMER = 0;

        counter++;
    }
    p = PINVALUE;

    //****************************************************************
    //receive data bits
    k = 8;
    for (n = 0; n < (FRAMESIZE * 8); n++)
    {
        // wait for edge
        while (p == PINVALUE)
            ;

        TIMER = 0;
        p = PINVALUE;

        // delay 3/4 bit
        while (TIMER < delayTime)
            ;

        t = PINVALUE;

        counter++;

        FrameData[dataPointer] = FrameData[dataPointer] << 1;
        if (p != t) FrameData[dataPointer] |= 1;
        p = t;
        k--;
        if (k == 0)
        {
            dataPointer++;
            k = 8;
        };
    }
    return true;
}

/*-----------------------------------------------------------------------------------------------------------------------
   Flash: fill page word by word
  -----------------------------------------------------------------------------------------------------------------------
*/
#define boot_program_page_fill(byteaddr, word)      \
  {                                                 \
    uint8_t sreg;                                   \
    sreg = SREG;                                    \
    cli ();                                         \
    boot_page_fill ((uint32_t) (byteaddr), word);   \
    SREG = sreg;                                    \
  }

/*-----------------------------------------------------------------------------------------------------------------------
   Flash: erase and write page
  -----------------------------------------------------------------------------------------------------------------------
*/
#define boot_program_page_erase_write(pageaddr)     \
  {                                                 \
    uint8_t sreg;                                   \
    eeprom_busy_wait ();                            \
    sreg = SREG;                                    \
    cli ();                                         \
    boot_page_erase ((uint32_t) (pageaddr));        \
    boot_spm_busy_wait ();                          \
    boot_page_write ((uint32_t) (pageaddr));        \
    boot_spm_busy_wait ();                          \
    boot_rww_enable ();                             \
    SREG = sreg;                                    \
  }


/*-----------------------------------------------------------------------------------------------------------------------
   write a block into flash
  -----------------------------------------------------------------------------------------------------------------------
*/
static void
pgm_write_block (uint16_t flash_addr, uint16_t * block, size_t size)
{
    uint16_t        start_addr;
    uint16_t        addr;
    uint16_t        w;
    uint8_t         idx = 0;

    start_addr = (flash_addr / SPM_PAGESIZE) * SPM_PAGESIZE;        // round down (granularity is SPM_PAGESIZE)

    for (idx = 0; idx < SPM_PAGESIZE / 2; idx++)
    {
        addr = start_addr + 2 * idx;

        if (addr >= flash_addr && size > 0)
        {
            w = *block++;
            size -= sizeof (uint16_t);
        }
        else
        {
            w = pgm_read_word (addr);
        }

        boot_program_page_fill (addr, w);
    }

    boot_program_page_erase_write(start_addr);                      // erase and write the page
}


//***************************************************************************************
//  void boot_program_page (uint32_t page, uint8_t *buf)
//
//  Erase and flash one page.
//
//  input:     page address and data to be programmed
//
//***************************************************************************************
void
boot_program_page (uint32_t page, uint8_t *buf)
{
    uint16_t i;
    cli(); // disable interrupts

    boot_page_erase(page);
    boot_spm_busy_wait ();      // Wait until the memory is erased.

    for (i = 0; i < SPM_PAGESIZE; i += 2)
    {
        //read received data
        uint16_t w = *buf++; //low section
        w += (*buf++) << 8; //high section
        //combine low and high to get 16 bit

        //first page and first index is vector table... ( page 0 and index 0 )
        if (page == 0 && i == 0)
        {
            //1.save jump to application vector for later patching
            void* appl = (void *)(w - RJMP);
            start_appl_main =  ((void (*)(void)) appl);

            //2.replace w with jump vector to bootloader
            w = 0xC000 + (BOOTLOADER_ADDRESS / 2) - 1;
        }

        boot_page_fill (page + i, w);
        boot_spm_busy_wait();       // Wait until the memory is written.
    }

    boot_page_write (page);     // Store buffer in flash page.
    boot_spm_busy_wait();       // Wait until the memory is written.
}

void
resetRegister(void)
{
    DDRB = 0;
    cli();
    TCCR0B = 0; // turn off timer1
}

void
exitBootloader(void)
{
    memcpy_P (&start_appl_main, (PGM_P) BOOTLOADER_FUNC_ADDRESS, sizeof (start_appl_main));

    if (start_appl_main)
    {
        resetRegister();
        (*start_appl_main) ();
    }
}

void
runProgramm(void)
{
    // reintialize registers to default
    resetRegister();

    pgm_write_block (BOOTLOADER_FUNC_ADDRESS, (uint16_t *) &start_appl_main, sizeof (start_appl_main));

    start_appl_main();
}

//***************************************************************************************
// main loop
//***************************************************************************************
static inline void
a_main(void)
{
    uint8_t p;
    uint16_t time = WAITBLINKTIME;

    p = PINVALUE;

#ifdef WONKYSTUFF
    // wait whilst the reset button is held down (and turn on the LED to say that we're waiting)
    uint32_t lPress=0;
#ifdef MMO
    while ((PINB & BOOTCHECKPIN))
#else
    while (!(PINB & BOOTCHECKPIN))
#endif
    {
        LEDON();           // Switch on the LED
        if (++lPress > 3000000)         // pretty arbitrary count
        {
            // Wait for audio bootloader shenanigans
            break;
        }
    }
    LEDOFF();

    if (lPress < 3000000)
    {
        // leave bootloader and run program
        exitBootloader();
    }

#else   // !WONKYSTUFF

  uint8_t timeout = BOOT_TIMEOUT;

  p = PINVALUE;

  //*************** wait for toggling input pin or timeout ******************************
  uint8_t exitcounter = 3;
  while (1)
  {
    if (TIMER > 100) // timedelay ==> frequency @16MHz= 16MHz/8/100=20kHz
    {
      TIMER = 0;
      time--;
      if (time == 0)
      {
        TOGGLELED();

        time = WAITBLINKTIME;
        timeout--;
        if (timeout == 0)
        {
          LEDOFF(); // timeout,
          // leave bootloader and run program
          exitBootloader();
        }
      }
    }
    if (p != PINVALUE)
    {
      p = PINVALUE;
      exitcounter--;
    }
    if (exitcounter == 0) break; // signal received, leave this loop and go on
  }

#endif  // !WONKYSTUFF

    //*************** start command interpreter *************************************

    while (1)
    {
        if (!receiveFrame())
        {
            //*****  if data transfer error: blink fast, press reset to restart *******************
            while (1)
            {
                if (TIMER > 100) // timerstop ==> frequency @16MHz= 16MHz/8/100=20kHz
                {
                    TIMER = 0;
                    time--;
                    if (time == 0)
                    {
                        TOGGLELED();
                        time = 1000;
                    }
                }
            }
        }
        else // succeed
        {
            switch (FrameData[COMMAND])
            {
                case PROGCOMMAND:
                {
                    uint16_t pageNumber = (((uint16_t)FrameData[PAGEINDEXHIGH]) << 8) + FrameData[PAGEINDEXLOW];
                    uint16_t address=SPM_PAGESIZE * pageNumber;

                    if( address < BOOTLOADER_ADDRESS) // prevent bootloader form self killing
                    {
                        boot_program_page(address, FrameData + DATAPAGESTART);  // erase and program page
                        TOGGLELED();
                    }
                }
                break;

                case RUNCOMMAND:
                {
                    // after programming leave bootloader and run program
                    runProgramm();
                }
                break;

                case EEPROMCOMMAND:
                {
                    uint8_t pageNumber = FrameData[PAGEINDEXLOW];
                    uint8_t data_length = FrameData[LENGTHLOW];
                    uint8_t address = SPM_PAGESIZE * pageNumber;

                    uint8_t *buf = FrameData + DATAPAGESTART;

                    for (uint8_t i = 0; i < data_length; i++)
                    {
                        //write received data to EEPROM
                        uint8_t w = *buf++;
                        eeprom_write(address + i, w);
                    }

                    //Leave bootloader after eeprom signal received
                    //todo: wait until all data sent > spm pagesize (64)
                    //fix this!!!!
                    LEDOFF();
                    exitBootloader();
                }
                break;
            }
            FrameData[COMMAND] = NOCOMMAND; // delete command
        }
    }
}

int
main(void)
{
    INITLED();
    INITAUDIOPORT();
    INITBOOTCHECK();

    // Timer 2 normal mode, clk/8, count up from 0 to 255
    // ==> frequency @16MHz= 16MHz/8/256=7812.5Hz
    TCCR0B = _BV(CS01);

    a_main(); // start the main function
}
