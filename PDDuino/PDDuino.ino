/*
 * PDDuino - Tandy Portable Disk Drive emulator on Arduino
 * github.com/bkw777/PDDuino
 * Based on github.com/TangentDelta/SD2TPDD
 */

#define SKETCH_NAME __FILE__
#define SKETCH_VERSION "0.4.2"
// 0.4.2 20210405 bkw - fix setLabel(), configurable sendLoader() speed, replace some magic numbers
// 0.4.1 20200913 bkw - more IDE-supplied macros to un-hardcode where possible, more debug output
// 0.4.0 20200828 bkw - sendLoader()
// 0.3.0 20180921 bkw - Teensy & Feather boards, CLIENT & CONSOLE, setLabel(), sleepNow()
// 0.2   20180728 Jimmy Pettit original

// Debugging settings

// Don't try to enable DEBUG and SLEEP at the same time.
// Traffic on the usb console seems to be triggering the interrup to wake up, at least on 32u4
// This behavior is probably different for every different board.
// Debug sleep just with the led. (def DEBUG_LED and DEBUG_SLEEP, undef DEBUG)
// Everything seems to be working as desired with DEBUG disabled.
// Eject the card and the SD_LED shows that we rebooted and are waiting in initCard() again.
// Insert a card and it inits, and 5 seconds later the DEBUG_LED comes on and stays on.
// Request a directory listing in TS-DOS and the led goes out, and the directory listing works, and 5 seconds later the led comes back on.

// Console is 115200 no flow
// DEBUG: 0 or undef = disable console, 1 = enabled, least verbose, 2+ = enabled, more verbose (max 3)
// When disabled, the port itself is disabled, which frees up ram and cpu on some hardware.
// On Teensy 3.5/3.6, the cpu must run at 24 MHz or more to enable the usb serial console.
// Without the usb console, the sketch can run at the lowest setting, only 2 MHz. (Tools -> CPU Speed).
// DEBUG will also be disabled if board_*.h doesn't define CONSOLE.
//#define DEBUG 2     // disable unless actually debugging, uses more battery, prevents full sleep, hangs at boot until connected
//#define DEBUG_LED   // use led to see (some) activity even if no DEBUG or CONSOLE (waiting for console, sleeping)
//#define DEBUG_SLEEP // use DEBUG_LED to debug sleepNow()

// File that sendLoader() will try to send to the client.
// Comment out to disable sendLoader() & ignore DSR_PIN
#define LOADER_FILE "LOADER.DO"

// Wait this many ms between each byte in sendLoader()
// 5 is good for most things, but rxcini.DO (REXCPM bootstrapper) needs 6
#define LOADER_BYTE_MS 6

// TS-DOS label for root dir - 6 bytes
#define ROOT_DME_LABEL "SD:   "

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

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
  #include "board_Feather_32u4.h"
#elif defined(ADAFRUIT_FEATHER_M0)
  #include "board_Feather_M0.h"
#elif defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY36) || defined(ARDUINO_TEENSY41)
  #include "board_Teensy_35_36_41.h"
#endif

#include "board_Defaults.h"
#include "cfg_behavior.h"

#if !defined(USE_SDIO)
#include <SPI.h>
#endif
#include <SdFat.h>
#include <sdios.h>
#include <SdFatConfig.h>
// #include <SysCall.h>  // used to require, now it's an error after a libraries or board support update

#if defined(USE_SDIO)
SdFatSdioEX SD;
#else
SdFat SD;
#endif

#if defined(ENABLE_SLEEP)
 #if defined(USE_ALP)
  #include <ArduinoLowPower.h>
 #else
  #include <avr/sleep.h>
 #endif // USE_ALP
#endif // ENABLE_SLEEP

#define DATA_BUFFER_SZ  0x0100  // 256 bytes, 2 full tpdd packets? 0xFF did not work.
#define FILE_BUFFER_SZ  0x80    // 128 bytes at at time to/from files
#define DIRECTORY_SZ    0x40    // size of directory[] which holds full paths
#define FILENAME_SZ     0x18    // TPDD protocol spec 1C, minus 4 for ".<>"+NULL

// TPDD commands
#define CMD_REFERENCE 0x00
#define CMD_OPEN      0x01
#define CMD_CLOSE     0x02
#define CMD_READ      0x03
#define CMD_WRITE     0x04
#define CMD_DELETE    0x05
#define CMD_FORMAT    0x06
#define CMD_STATUS    0x07
#define CMD_DMEREQ    0x08
#define CMD_CONDITION 0x0c
#define CMD_RENAME    0x0d

// TPDD returns
#define RET_READ        0x10
#define RET_DIRECTORY   0x11
#define RET_NORMAL      0x12
#define RET_CONDITION   0x15

// TPDD errors
#define ERR_SUCCESS       0x00
#define ERR_NOFILE        0x10
#define ERR_EXISTS        0x11
#define ERR_NO_NAME       0x30
#define ERR_DIR_SEARCH    0x31
#define ERR_BANK          0x35
#define ERR_PARM          0x36
#define ERR_FMT_MISMATCH  0x37
#define ERR_EOF           0x3f
#define ERR_NO_START      0x40
#define ERR_ID_CRC        0x41
#define ERR_SEC_LEN       0x42
#define ERR_FMT_VERIFY    0x44
#define ERR_FMT_INTRPT    0x46
#define ERR_ERASE_OFFSET  0x47
#define ERR_DATA_CRC      0x49
#define ERR_SEC_NUM       0x4a
#define ERR_READ_TIMEOUT  0x4b
#define ERR_SEC_NUM2      0x4d
#define ERR_WRITE_PROTECT 0x50
#define ERR_DISK_NOINIT   0x5e
#define ERR_DIR_FULL      0x60
#define ERR_DISK_FULL     0x61
#define ERR_FILE_LEN      0x6e
#define ERR_NO_DISK       0x70
#define ERR_DISK_CHG      0x71

// main loop states
#define ST_WAITING 0x00
#define ST_PARSING 0x01
#define ST_WORKING 0x02

#define BASIC_EOF 0x1A

File root;  // Root file for filesystem reference
File entry; // Moving file entry for the emulator
File tempEntry; // Temporary entry for moving files

byte head = 0x00;  // Head index
byte tail = 0x00;  // Tail index

byte checksum = 0x00;  // Global variable for checksum calculation

byte state = ST_WAITING; // Emulator command reading state
bool DME = false; // TS-DOS DME mode flag

byte dataBuffer[DATA_BUFFER_SZ]; // Data buffer for commands
byte fileBuffer[FILE_BUFFER_SZ]; // Data buffer for file reading

char refFileName[FILENAME_SZ] = "";  // Reference file name for emulator
char refFileNameNoDir[FILENAME_SZ] = ""; // Reference file name for emulator with no ".<>" if directory
char tempRefFileName[FILENAME_SZ] = ""; // Second reference file name for renaming
char entryName[FILENAME_SZ] = "";  // Entry name for emulator
byte directoryBlock = 0x00; // Current directory block for directory listing
char directory[DIRECTORY_SZ] = "/";
byte directoryDepth = 0x00;
char tempDirectory[DIRECTORY_SZ] = "/";
char dmeLabel[0x07] = "";  // 6 chars + NULL

#if defined(ENABLE_SLEEP)
 #if defined(SLEEP_DELAY)
  unsigned long now = millis();
  unsigned long idleSince = now;
 #endif // SLEEP_DELAY
 #if !defined(USE_ALP)
  const byte rxInterrupt = digitalPinToInterrupt(CLIENT_RX_PIN);
 #endif // !USE_ALP
#endif // ENABLE_SLEEP

#if defined(SD_CD_PIN)
  const byte cdInterrupt = digitalPinToInterrupt(SD_CD_PIN);
#endif // SD_CD_PIN

#if defined(DSR_PIN)
  byte dsrState = HIGH;
#endif // DSR_PIN

//void setup() {}
//void loop() {}
