/*
 *  PDDuino - Arduino-based Tandy Portable Disk Drive emulator
 *  github.com/bkw777/PDDuino
 *  Based on github.com/TangentDelta/SD2TPDD
 *
 *  Copyright (C) 2020  Brian K. White
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 *  config.h: Board level configurations and general configuration
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

// Debugging settings

/*
 * Don't try to enable DEBUG and SLEEP at the same time.
 * Traffic on the usb console seems to be triggering the interrup to wake up, at least on 32u4
 * This behavior is probably different for every different board.
 * Debug sleep just with the led. (def DEBUG_LED and DEBUG_SLEEP, undef DEBUG)
 * Everything seems to be working as desired with DEBUG disabled.
 * Eject the card and the SD_LED shows that we rebooted and are waiting in initCard() again.
 * Insert a card and it inits, and 5 seconds later the DEBUG_LED comes on and stays on.
 * Request a directory listing in TS-DOS and the led goes out, and the directory listing works, and 5 seconds later the led comes back on.
 *
 * log activity to serial monitor port
 * Logger is set for 115200 no flow
 * LOG_NONE or undefined: disable serial console
 * LOG_ERROR:
 * LOG_WARN:
 * LOG_INFO: major items of note.
 * LOG_DEBUG: some minor items
 * LOG_VERBOSE: Everything
 *
 * When disabled, the port itself is disabled, which frees up ram and cpu on some hardware.
 * On Teensy 3.5/3.6, the cpu must run at 24 MHz or more to enable the usb serial console.
 * Without the usb console, the sketch can run at the lowest setting, only 2 MHz. (Tools -> CPU Speed).
 * Logging will also be disabled if the selected BOARD_* section below doesn't define any LOGGER port.
 */

// disable unless actually debugging, uses more battery, prevents full sleep, hangs at boot until connected
#define LOG_LEVEL     LOG_VERBOSE
//#define LOG_LEVEL   LOG_DEBUG

//#define DEBUG_LED   // use led to see (some) activity even if no DEBUG or CONSOLE
//#define DEBUG_SLEEP // use DEBUG_LED to debug sleepNow()

// File that sendLoader() will try to send to the client.
// Comment out to disable sendLoader() & ignore DSR_PIN
#define LOADER_FILE "LOADER.DO"



////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
//--- configs for some handy boards that are small and have sd readers built in ---

// Detect board/platform from IDE-supplied macros
//
// How to find these values.
// There is no list, and no consistent rule, every manufacturer & board makes up something.
// But in general, find the platform.txt and boards.txt for your board in the locations below (on linux)
// ~/arduino-*/hardware/*/*/boards.txt               Arduino and Teensy
// ~/arduino-*/hardware/*/*/platform.txt
// ~/.arduino*/packages/*/hardware/*/*/boards.txt    Most others, ex: Adafruit, Heltec, etc
// ~/.arduino*/packages/*/hardware/*/*/platform.txt
//
// platform.txt has gcc commandlines with "-D..." defines like "-DARDUINO_{something}"
// boards.txt has "something=AVR_UNO"
// so here we would look for "#if defined(ARDUINO_AVR_UNO)"

#if defined(ARDUINO_AVR_FEATHER32U4)
  #define BOARD_FEATHER32U4
  #define BOARD_NAME "Adafruit Feather 32u4"

#elif defined(ADAFRUIT_FEATHER_M0)
  #define BOARD_FEATHERM0
  #define BOARD_NAME "Adafruit Feather M0"

#elif defined(ARDUINO_TEENSY35)
  #define BOARD_TEENSY35
  #define BOARD_NAME "Teensy 3.5"

#elif defined(ARDUINO_TEENSY36)
  #define BOARD_TEENSY36
  #define BOARD_NAME "Teensy 3.6"

#elif defined(ARDUINO_AVR_UNO)
  #define BOARD_UNO
  #define BOARD_NAME "Arduino UNO"

#elif defined(ARDUINO_AVR_MEGA2560)
  #define BOARD_MEGA
  #define BOARD_NAME "Arduino MEGA"

//#elif defined(ARDUINO_AVR_UNO)          // example to add a new board
//  #define BOARD_UNO
//  #define BOARD_NAME "Arduino UNO"

#else
  #define BOARD_DEFAULT
  #define BOARD_NAME "Default"
#endif

#if defined(BOARD_DEFAULT)
  // User-defined platform details
  #define LOGGER                        SERIAL_PORT_MONITOR      // where to send debug messages, if enabled. (Serial)
  #define CLIENT                        SERIAL_PORT_HARDWARE_OPEN // what serial port is the TPDD client connected to (Serial1)
  #define SD_CS_PIN                     SS    // sd card reader chip-select pin #, usually automatic
  //#define SD_CD_PIN                     7   // sd card reader card-detect pin #, interrupt & restart if card ejected
  //#define DISABLE_CS                    10  // Disable other SPI device on this pin, usully none, assume SD is only SPI device
  //#define SD_SPI_MHZ                    12  // override default SPI clock for SD card reader
  //#define USE_SDIO                          // sd card reader is connected by SDIO instead of SPI (Teensy 3.5/3.6/4.1)
  #define ENABLE_SLEEP                        // sleepNow() while idle for power saving
  //#define USE_ALP                           // Use ArduinoLowPower instead of avr/sleep.h, SAMD only (Feather M0)
  #define CLIENT_RX_PIN                 0     // CLIENT RX pin#, interrupt is attached to wake from sleepNow()
  //#define SLEEP_DELAY                   5000  // Delay in ms before sleeping  #define LED_SD_ACT_PIN                LED_BUILTIN
  #define LED_SD_ACT_INIT               pinMode(LED_SD_ACT_PIN, OUTPUT);
  #define LED_SD_ACT_ON                 digitalWrite(LED_SD_ACT_PIN,HIGH);
  #define LED_SD_ACT_OFF                digitalWrite(LED_SD_ACT_PIN,LOW);
  #define LED_DEBUG_PIN                 LED_BUILTIN
  #define LED_DEBUG_INIT                DDRC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_ON                  PORTC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_OFF                 PORTC &= ~_BV(LED_DEBUG_PIN);
  #define DTR_PIN                       5 // pin to output DTR, github.com/bkw777/MounT uses pin 5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6 // pin to input DSR, github.com/bkw777/MounT uses pin6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
   digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
}

// Teensy3.5, Teensy3.6
// https://www.pjrc.com/store/teensy35.html
// https://www.pjrc.com/store/teensy36.html
// Teensy 3.6 has no CD pin and no good alternative way to detect card.
// https://forum.pjrc.com/threads/43422-Teensy-3-6-SD-card-detect
// Might be able to use comment #10 idea.
//   #define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
//   #define CPU_RESTART_VAL 0x5FA0004
//   #define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);
//   ...
//   if (!sd.exists("settings.txt")) { CPU_RESTART }
//
#elif defined(BOARD_TEENSY35) || defined(BOARD_TEENSY36)
  #define LOGGER                        SERIAL_PORT_MONITOR
  #define CLIENT                        SERIAL_PORT_HARDWARE_OPEN
  //#define SD_CS_PIN                     4
  //#define SD_CD_PIN                     7
  //#define DISABLE_CS                    10
  //#define SD_SPI_MHZ                    12
  #define USE_SDIO
  #define ENABLE_SLEEP
  //#define USE_ALP
  #define CLIENT_RX_PIN                 0
  //#define SLEEP_DELAY                   5000
  // Main LED: PB5
  #define LED_SD_ACT_PIN                5
  #define LED_SD_ACT_INIT               DDRB |= _BV(LED_SD_ACT_PIN);
  #define LED_SD_ACT_ON                 PORTB |= _BV(LED_SD_ACT_PIN);
  #define LED_SD_ACT_OFF                PORTB &= ~_BV(LED_SD_ACT_PIN);
  // Main LED: PB5
  #define LED_DEBUG_PIN                 5
  #define LED_DEBUG_INIT                DDRC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_ON                  PORTC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_OFF                 PORTC &= ~_BV(LED_DEBUG_PIN);
  #define DTR_PIN                       5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
}


// Adafruit Feather 32u4 Adalogger
// https://learn.adafruit.com/adafruit-feather-32u4-adalogger
//
#elif defined(BOARD_FEATHER32U4)
  #define LOGGER                        SERIAL_PORT_MONITOR
  #define CLIENT                        SERIAL_PORT_HARDWARE_OPEN
  #define SD_CS_PIN                     4
  #define SD_CD_PIN                     7
  //#define DISABLE_CS 10
  //#define SD_SPI_MHZ 12
  //#define USE_SDIO
  #define ENABLE_SLEEP
  //#define USE_ALP
  #define CLIENT_RX_PIN                 0
  #define SLEEP_DELAY 5000          // Adalogger 32u4 needs a few seconds before sleeping
  // Green LED near card reader: PB4
  #define LED_SD_ACT_PIN                4
  #define LED_SD_ACT_INIT               DDRB |= _BV(LED_SD_ACT_PIN);
  #define LED_SD_ACT_ON                 PORTB |= _BV(LED_SD_ACT_PIN);
  #define LED_SD_ACT_OFF                PORTB &= ~_BV(LED_SD_ACT_PIN);
  // Main LED: PC7
  #define LED_DEBUG_PIN                 7
  #define LED_DEBUG_INIT                DDRC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_ON                  PORTC |= _BV(LED_DEBUG_PIN);
  #define LED_DEBUG_OFF                 PORTC &= ~_BV(LED_DEBUG_PIN);
  #define DTR_PIN                       5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
}

// Adafruit Feather M0 Adalogger
// https://learn.adafruit.com/adafruit-feather-m0-adalogger
//
#elif defined(BOARD_FEATHERM0)
  #define LOGGER                        SERIAL_PORT_MONITOR
  #define CLIENT                        SERIAL_PORT_HARDWARE_OPEN
  #define SD_CS_PIN                     4
  #define SD_CD_PIN                     7
  //#define DISABLE_CS                    10
  #define SD_SPI_MHZ                    12  // https://github.com/adafruit/ArduinoCore-samd/pull/186
  //#define USE_SDIO
  #define ENABLE_SLEEP
  #define USE_ALP
  #define CLIENT_RX_PIN                 0
  #define SLEEP_DELAY                   250
  // Green LED near card reader: PA6 / pin 8
  #define LED_SD_ACT_PIN                8
  #define LED_SD_ACT_INIT               pinMode(LED_SD_ACT_PIN, OUTPUT);
  #define LED_SD_ACT_ON                 digitalWrite(LED_SD_ACT_PIN,HIGH);
  #define LED_SD_ACT_OFF                digitalWrite(LED_SD_ACT_PIN,LOW);
  // Main LED: PA17
  #define LED_DEBUG_PIN                 LED_BUILTIN
  #define LED_DEBUG_INIT                pinMode(LED_DEBUG_PIN, OUTPUT);
  #define LED_DEBUG_ON                  digitalWrite(LED_DEBUG_PIN, HIGH);
  #define LED_DEBUG_OFF                 digitalWrite(LED_DEBUG_PIN, LOW);
  #define DTR_PIN                       5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
}

#elif defined(BOARD_UNO)
#ifndef HAVE_HWSERIAL1
  #define LOGGER                        mySerial
  #include <SoftwareSerial.h>
  #define LOGGER_DECL                   SoftwareSerial mySerial(7,8); // RX, TX;
#else
  #define LOGGER                        SERIAL_PORT_HARDWARE_OPEN
  #define LOGGER                        SERIAL_PORT_HARDWARE_OPEN
#endif
  #define CLIENT                        SERIAL_PORT_MONITOR
  #define SD_CS_PIN                     10
  //#define SD_CD_PIN                     7
  // User-defined platform details
  //#define ENABLE_SLEEP
  //#define USE_ALP
  //#define CLIENT_RX_PIN               0
  //#define SLEEP_DELAY 5000
  #define LED_SD_ACT_PIN                9
  #define LED_SD_ACT_INIT               pinMode(LED_SD_ACT_PIN, OUTPUT);
  #define LED_SD_ACT_ON                 digitalWrite(LED_SD_ACT_PIN,HIGH);
  #define LED_SD_ACT_OFF                digitalWrite(LED_SD_ACT_PIN,LOW);
  #define LED_DEBUG_PIN                 4
  #define LED_DEBUG_INIT                pinMode(LED_DEBUG_PIN, OUTPUT);
  #define LED_DEBUG_ON                  digitalWrite(LED_DEBUG_PIN, HIGH);
  #define LED_DEBUG_OFF                 digitalWrite(LED_DEBUG_PIN, LOW);
  #define DTR_PIN                       5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
}

#elif defined(BOARD_MEGA)
  #define LOGGER                        SERIAL_PORT_HARDWARE_OPEN
  #define CLIENT                        SERIAL_PORT_MONITOR
  #define DTR_PIN                       5
  #define DSR_PIN                       6
  #define SD_CS_PIN                     53
  //#define SD_CD_PIN                     7
  // User-defined platform details
  //#define ENABLE_SLEEP
  //#define USE_ALP
  //#define CLIENT_RX_PIN               0
  //#define SLEEP_DELAY 5000
  #define LED_SD_ACT_PIN                9
  #define LED_SD_ACT_INIT               pinMode(LED_SD_ACT_PIN, OUTPUT);
  #define LED_SD_ACT_ON                 digitalWrite(LED_SD_ACT_PIN, HIGH);
  #define LED_SD_ACT_OFF                digitalWrite(LED_SD_ACT_PIN, LOW);
  #define LED_DEBUG_PIN                 4
  #define LED_DEBUG_INIT                pinMode(LED_DEBUG_PIN, OUTPUT);
  #define LED_DEBUG_ON                  digitalWrite(LED_DEBUG_PIN, HIGH);
  #define LED_DEBUG_OFF                 digitalWrite(LED_DEBUG_PIN, LOW);
  #define DTR_PIN                       5
  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
  #define DSR_PIN                       6
  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

static inline void board_init(void) {
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
}

// example to add a new board
//#elif defined(BOARD_UNO)
//  // User-defined platform details
//  #define                               LOGGER SERIAL_PORT_MONITOR
//  #define                               CLIENT SERIAL_PORT_HARDWARE_OPEN
//  #define SD_CS_PIN                     4
//  //#define SD_CD_PIN                   7
//  //#define DISABLE_CS                  10
//  //#define SD_SPI_MHZ 12
//  //#define USE_SDIO
//  #define ENABLE_SLEEP
//  //#define USE_ALP
//  #define CLIENT_RX_PIN                 0
//  //#define SLEEP_DELAY                 5000
//  #define LED_SD_ACT_PIN                LED_BUILTIN
//  #define LED_SD_ACT_INIT               pinMode(LED_SD_ACT_PIN, OUTPUT);
//  #define LED_SD_ACT_ON                 digitalWrite(LED_SD_ACT_PIN, HIGH);
//  #define LED_SD_ACT_OFF                digitalWrite(LED_SD_ACT_PIN, LOW);
//  #define LED_DEBUG_PIN                 LED_BULTIN
//  #define LED_DEBUG_INIT                pinMode(LED_DEBUG_PIN, OUTPUT);
//  #define LED_DEBUG_ON                  digitalWrite(LED_DEBUG_PIN, HIGH);
//  #define LED_DEBUG_OFF                 digitalWrite(LED_DEBUG_PIN, LOW);
//  #define DTR_PIN                       5
//  #define DTR_PIN_INIT                  pinMode(DTR_PIN, OUTPUT);
//  #define DTR_PIN_HI                    digitalWrite(DTR_PIN,HIGH);
//  #define DTR_PIN_LO                    digitalWrite(DTR_PIN,LOW);
//  #define DSR_PIN                       6
//  #define DSR_PIN_INIT                  pinMode(DSR_PIN, INPUT_PULLUP);
//  #define DSR_PIN_HIZ                   digitalWrite(DSR_PIN,HIGH);
//  #define DSR_PIN_IN                    !digitalRead(DSR_PIN);

//static inline void board_init(void) {
//  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros
//}


#endif // BOARD_*

#if !defined(LED_SD_ACT_ON)  || !defined(LED_SD_ACT_ON) || !defined(LED_SD_ACT_OFF)
  #define led_sd_init()                   do {} while(0)
  #define led_sd_on()                     do {} while(0)
  #define led_sd_off()                    do {} while(0)
  #define LED_SD 0
#else
  #define LED_SD 1
  static inline void led_sd_init(void)    { LED_SD_ACT_INIT }
  static inline void led_sd_on(void)      { LED_SD_ACT_ON }
  static inline void led_sd_off(void)     { LED_SD_ACT_OFF }

#endif

#if !defined(LED_DEBUG_INIT)  || !defined(LED_DEBUG_ON) || !defined(LED_DEBUG_OFF)
  #define led_debug_init()                do {} while(0)
  #define led_debug_on()                  do {} while(0)
  #define led_debug_off()                 do {} while(0)
#else
  static inline void led_debug_init(void) { LED_DEBUG_INIT }
  static inline void led_debug_on(void)   { LED_DEBUG_ON }
  static inline void led_debug_off(void)  { LED_DEBUG_OFF }
#endif

#define LOADER_SEND_DELAY                 5 // ms
#define TOKEN_BASIC_END_OF_FILE           0x1A

#ifdef DTR_PIN
  static inline void dtr_init(void)       {
                                            DTR_PIN_INIT
                                            DTR_PIN_HI    // tell client we're not ready
                                          }
static inline void dtr_ready(void)        { DTR_PIN_LO }
static inline void dtr_not_ready(void)    { DTR_PIN_HI }
#else
  #define DTR_PIN                         -1
  #define dtr_init()                      do {} while(0)
#endif // DTR_PIN

#ifdef DSR_PIN
static inline void dsr_init(void)         {
                                            DTR_PIN_INIT
                                          }
static inline bool dsr_is_ready(void)     { return DSR_PIN_IN }
#else
  #define DSR_PIN                         -1
  #define dsr_init()                      do {} while(0)
  #define dsr_is_ready()                  {0} // always return 0
#endif // DSR_PIN

#endif /* CONFIG_H_ */
