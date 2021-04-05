/*
 * PDDuino - Tandy Portable Disk Drive emulator on Arduino
 * github.com/bkw777/PDDuino
 * Based on github.com/TangentDelta/SD2TPDD
 */

#define SKETCH_NAME __FILE__
#define SKETCH_VERSION "0.4.1"

// 0.4.1 20200913 bkw - ENABLE_LOADER, use more IDE-supplied macros, more debug output
// 0.4.0 20200828 bkw - sendLoader()
// 0.3.0 20180921 bkw - Teensy & Feather boards, CLIENT & CONSOLE, setLabel(), sleepNow()
// 0.2   20180728 Jimmy Pettit original

// log activity to serial monitor port
// Console is set for 115200 no flow
// 0 = serial console disabled, 1 = enabled, least verbose, 2+ = enabled, more verbose
// When disabled, the port itself is disabled, which frees up significant ram and cpu cycles on some hardware.
// Example, on Teensy 3.5/3.6, with the usb port enabled at all, the sketch must be compiled to run the cpu at a minimum of 24mhz
// but with the usb port disabled, the sketch can be compiled to run at the lowest possible option of only 2Mhz.
#define DEBUG 3                 // disable unless actually debugging, usb-serial uses battery and cpu, also code waits for console at power-on.
#define DEBUG_ACTIVITY_LIGHT 1  // enable the debug led in general
#define DEBUG_SLEEP 0           // use DEBUG_LED to debug sleepNow()

#define ENABLE_LOADER 1         // 0=disable sendLoader() and ignore DSR/DTR pins, 1=enable. 
#define LOADER_FILE "LOADER.DO" // File that sendLoader() will try to send to the client.
#define LOADER_BYTE_MS 6        // pause in ms between each byte in sendLoader

#define DME_ROOT_LABEL "SD:   " // Label for root dir in TS-DOS current dir display. 6 bytes.

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
// Configs for some handy boards that are small and have sd card readers already built in

// Detect board/platform from IDE-supplied macros
// ~/arduino-*/hardware/*/*/boards.txt                  Arduino and Teensy
// ~/arduino-*/hardware/*/*/platform.txt
// ~/.arduino15/packages/*/hardware/*/*/boards.txt      Adafruit, Heltec, esp8266, others
// ~/.arduino15/packages/*/hardware/*/*/platform.txt
#if defined(ARDUINO_AVR_FEATHER32U4)
  #define BOARD_FEATHER32U4
#elif defined(ADAFRUIT_FEATHER_M0)
  #define BOARD_FEATHERM0
#elif defined(ARDUINO_TEENSY35)
  #define BOARD_TEENSY35
  #define BOARD_NAME "Teensy 3.5"
#elif defined(ARDUINO_TEENSY36)
  #define BOARD_TEENSY36
  #define BOARD_NAME "Teensy 3.6"
#else
  #define BOARD_DEFAULT
#endif

#ifdef BOARD_DEFAULT
  // User-defined platform details
  #define BOARD_NAME "Default"
  #define CONSOLE SERIAL_PORT_MONITOR      // where to send debug messages, if enabled
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN // what serial port is the TPDD client connected to
  #define DTR_PIN 5						// pin to output DTR
  #define DSR_PIN 6						// pin to input DSR
  #define SD_CS_PIN 4                   // sd card reader chip-select pin #
  //#define SD_CD_PIN 7                 // sd card reader card-detect pin #
  //#define DISABLE_SD_CS               // disable CS on SD_CS_PIN pin
  //#define USE_SDIO                    // sd card reader connected by SDIO instead of SPI (Teensy 3.5/3.6/4.1)
  #define ENABLE_SLEEP 1                // sleep() while idle for power saving
  #define WAKE_PIN 0                    // CLIENT RX pin#, interrupt is attached to wake from sleep()
  #define SLEEP_INHIBIT 5000            // Delay in ms, after idle before sleeping, 0 = sleep immediately when idle, 300000 = idle 5 minutes before sleeping
  #define DISK_ACTIVITY_LIGHT 1         // Enable led to show sd card activity
  #define PINMODE_SD_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define SD_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define SD_LED_OFF digitalWrite(LED_BUILTIN,LOW);
  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

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
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  //#define SD_CS_PIN 4
  //#define SD_CD_PIN 7
  //#define DISABLE_SD_CS
  #define USE_SDIO
  #define ENABLE_SLEEP 1
  #define WAKE_PIN 0
  #define SLEEP_INHIBIT 0       // Teensy can sleep instantly
  #define DISK_ACTIVITY_LIGHT 1
  // Main LED: PB5
  #define PINMODE_SD_LED_OUTPUT DDRB = DDRB |= 1UL << 5;
  #define SD_LED_ON PORTB |= _BV(5);
  #define SD_LED_OFF PORTB &= ~_BV(5);
  // Main LED: PB5
  #define PINMODE_DEBUG_LED_OUTPUT DDRB = DDRB |= 1UL << 5;
  #define DEBUG_LED_ON PORTB |= _BV(5);
  #define DEBUG_LED_OFF PORTB &= ~_BV(5);

// Adafruit Feather 32u4 Adalogger
// https://learn.adafruit.com/adafruit-feather-32u4-adalogger
//
#elif defined(BOARD_FEATHER32U4)
  #define BOARD_NAME "Feather 32u4"
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  #define SD_CS_PIN 4
  //#define SD_CD_PIN 7
  //#define DISABLE_SD_CS
  //#define USE_SDIO
  #define ENABLE_SLEEP 1
  #define WAKE_PIN 0
  #define SLEEP_INHIBIT 5000          // Adalogger 32u4 needs a few seconds before sleeping
  #define DISK_ACTIVITY_LIGHT 1
  // Green LED near card reader: PB4
  #define PINMODE_SD_LED_OUTPUT DDRB = DDRB |= 1UL << 4;
  #define SD_LED_ON PORTB |= _BV(4);
  #define SD_LED_OFF PORTB &= ~_BV(4);
  // Main LED: PC7
  #define PINMODE_DEBUG_LED_OUTPUT DDRC = DDRC |= 1UL << 7;
  #define DEBUG_LED_ON PORTC |= _BV(7);
  #define DEBUG_LED_OFF PORTC &= ~_BV(7);

// Adafruit Feather M0 Adalogger
// https://learn.adafruit.com/adafruit-feather-m0-adalogger
//
#elif defined(BOARD_FEATHERM0)
  #define BOARD_NAME "Feather M0"
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  #define SD_CS_PIN 4
  //#define SD_CD_PIN 7
  //#define DISABLE_SD_CS
  //#define USE_SDIO
  #define ENABLE_SLEEP 1
  #define WAKE_PIN 0
  #define SLEEP_INHIBIT 5000
  #define DISK_ACTIVITY_LIGHT 1
  // Green LED near card reader: PA6
  // SAMD must map pins differently than AVR  "'DDRA'was not declared in this scope"
  //#define PINMODE_SD_LED_OUTPUT DDRA = DDRA |= 1UL << 6;
  //#define SD_LED_ON PORTA |= _BV(6);
  //#define SD_LED_OFF PORTA &= ~_BV(6);
  #define PINMODE_SD_LED_OUTPUT pinMode(8,OUTPUT);
  #define SD_LED_ON digitalWrite(8,HIGH);
  #define SD_LED_OFF digitalWrite(8,LOW);
  // Main LED: PA17
  //#define PINMODE_DEBUG_LED_OUTPUT DDRA = DDRA |= 1UL << 17;
  //#define DEBUG_LED_ON PORTA |= _BV(17);
  //#define DEBUG_LED_OFF PORTA &= ~_BV(17);
  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);
  // only needed if you don't have the Adafruit SAMD board support installed
  //#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
  // // Required for Serial on Zero based boards
  // #define Serial SERIAL_PORT_USBVIRTUAL
  //#endif

#endif // BOARD_*

//
// end of config section
//
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

#define BASIC_EOF 0x1A


//#include <BlockDriver.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <SdFat.h>
#include <SdFatConfig.h>
//#include <SysCall.h>
#include <sdios.h>

#if defined(USE_SDIO)
SdFatSdioEX SD;
#else
SdFat SD;
#endif

#if !DISK_ACTIVITY_LIGHT
  #define PINMODE_SD_LED_OUTPUT
  #define SD_LED_ON
  #define SD_LED_OFF
#endif

#if !DEBUG_ACTIVITY_LIGHT
 #define PINMODE_DEBUG_LED_OUTPUT
 #define DEBUG_LED_ON
 #define DEBUG_LED_OFF
#endif

#if DEBUG && defined(CONSOLE)
 #define DEBUG_PRINT(x)     CONSOLE.print (x)
 #define DEBUG_PRINTI(x,y)  CONSOLE.print (x,y)
 #define DEBUG_PRINTL(x)    CONSOLE.println (x)
 #define DEBUG_PRINTIL(x,y) CONSOLE.println (x,y)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTI(x,y)
 #define DEBUG_PRINTL(x)
 #define DEBUG_PRINTIL(x,y)
#endif

#define DATA_BUFFER_SZ 0x0100  // 256 bytes, 2 full tpdd packets? 0xFF did not work.
#define FILE_BUFFER_SZ 0x80    // 128 bytes at at time to/from files
#define DIRECTORY_SZ 0x40      // size of directory[] which holds full paths
#define FILENAME_SZ 0x18       // TPDD protocol spec 1C, minus 4 for ".<>"+NULL


File root;  //Root file for filesystem reference
File entry; //Moving file entry for the emulator
File tempEntry; //Temporary entry for moving files

byte head = 0x00;  //Head index
byte tail = 0x00;  //Tail index

byte checksum = 0x00;  //Global variable for checksum calculation

byte state = 0x00; //Emulator command reading state
bool DME = false; //TS-DOS DME mode flag

byte dataBuffer[DATA_BUFFER_SZ]; //Data buffer for commands
byte fileBuffer[FILE_BUFFER_SZ]; //Data buffer for file reading

char refFileName[FILENAME_SZ] = "";  //Reference file name for emulator
char refFileNameNoDir[FILENAME_SZ] = ""; //Reference file name for emulator with no ".<>" if directory
char tempRefFileName[FILENAME_SZ] = ""; //Second reference file name for renaming
char entryName[FILENAME_SZ] = "";  //Entry name for emulator
byte directoryBlock = 0x00; //Current directory block for directory listing
char directory[DIRECTORY_SZ] = "/";
byte directoryDepth = 0x00;
char tempDirectory[DIRECTORY_SZ] = "/";
char dmeLabel[0x07] = "";  // 6 chars

/*
 *
 * General misc. routines
 *
 */

#if DEBUG
void printDirectory(File dir, byte numTabs) {
  char fileName[FILENAME_SZ] = "";

  SD_LED_ON
  while (true) {
    entry = dir.openNextFile();
    if (!entry) break;
    entry.getName(fileName,FILENAME_SZ);
    for (byte i = 0x00; i < numTabs; i++) DEBUG_PRINT(F("\t"));
    DEBUG_PRINT(fileName);
    if (entry.isDirectory()) {
      DEBUG_PRINTL(F("/"));
      DEBUG_PRINT(F("--- printDirectory(")); DEBUG_PRINT(fileName); DEBUG_PRINT(F(",")); DEBUG_PRINT(numTabs+0x01); DEBUG_PRINTL(F(") start ---"));
      printDirectory(entry, numTabs + 0x01);
      DEBUG_PRINT(F("--- printDirectory(")); DEBUG_PRINT(fileName); DEBUG_PRINT(F(",")); DEBUG_PRINT(numTabs+0x01); DEBUG_PRINTL(F(") end ---"));
    } else {
      DEBUG_PRINT(F("\t\t")); DEBUG_PRINTIL(entry.fileSize(), DEC);
    }
    entry.close();
  }
  SD_LED_OFF
}
#endif // DEBUG

#if ENABLE_SLEEP
#if defined(ARDUINO_SAMD_ZERO)
 #include <ArduinoLowPower.h>
#else
 #include <avr/sleep.h>
#endif // SAMD

#if defined(ARDUINO_SAMD_ZERO)
#else
  const byte wakeInterrupt = digitalPinToInterrupt(WAKE_PIN);
#endif // SAMD

#if SLEEP_INHIBIT
  unsigned long now = millis();
  unsigned long idleSince = now;
#endif // SLEEP_INHIBIT

void wakeNow () {
}

void sleepNow() {
#if SLEEP_INHIBIT
  now = millis();
  if ((now-idleSince)<SLEEP_INHIBIT) return;
  idleSince = now;
#endif // SLEEP_INHIBIT
#if defined(ARDUINO_SAMD_ZERO)
  LowPower.attachInterruptWakeup(WAKE_PIN, wakeNow, CHANGE);
  LowPower.sleep();
#else
// prevent the built-in usb-serial device from powering-off if we're using it
 #if DEBUG && defined(CONSOLE)
  set_sleep_mode(SLEEP_MODE_IDLE);
 #else
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
 #endif // DEBUG && CONSOLE
 #if DEBUG_SLEEP
  DEBUG_LED_ON
 #endif // DEBUG_SLEEP

  attachInterrupt(wakeInterrupt,wakeNow,CHANGE);
  sleep_mode();
  detachInterrupt(wakeInterrupt);
#endif // ARDUINO_SAMD_ZERO
#if DEBUG_SLEEP
  DEBUG_LED_OFF
#endif // DEBUG_SLEEP
}
#endif // ENABLE_SLEEP

void initCard () {
  while(true){
    DEBUG_PRINT(F("Opening SD card..."));
    SD_LED_ON
#if defined(USE_SDIO)
#if DEBUG > 2
    DEBUG_PRINT(F("(SDIO)"));
#endif
    if (SD.begin()) {
#else  // USE_SDIO
#if DEBUG > 2
    DEBUG_PRINT(F("(SPI)"));
#endif
    //if (SD.begin(SD_CS_PIN,SD_SCK_MHZ(50))) {
    if (SD.begin(SD_CS_PIN)) {
#endif  // USE_SDIO
      DEBUG_PRINTL(F("OK."));
#if DISK_ACTIVITY_LIGHT
      SD_LED_OFF
      delay (0x60);
      SD_LED_ON
      delay (0x60);
      SD_LED_OFF
      delay (0x60);
      SD_LED_ON
      delay (0x60);
#endif
      break;
    } else {
      DEBUG_PRINTL(F("No SD card."));
#if DISK_ACTIVITY_LIGHT
      SD_LED_OFF
      delay(1000);
#endif
    }
  }

  SD.chvol();

  // TODO - get the FAT volume label and use it in place of rootLabel
  // ... non-trivial

  // Always do this open() & close(), even if we aren't doing printDirectory().
  // It's needed to get the SdFat library to put the sd card to sleep.
  root = SD.open(directory);
#if DEBUG
  DEBUG_PRINTL(F("--- printDirectory(root,0) start ---"));
  printDirectory(root,0x00);
  DEBUG_PRINTL(F("--- printDirectory(root,0) end ---"));
#endif
  root.close();

  SD_LED_OFF
}


#if ENABLE_LOADER
/*
 * sendLoader() and restart()
 *
 * At power-on the Model 100 rs232 port sets all data & control pins to -5v.
 * On RUN "COM:98N1E", pins 4 (RTS) and 20 (DTR) go to +5v.
 * github.com/bkw777/MounT connects GPIO pin 5 through MAX3232 to RS232 DSR (DB25 pin 6) and to RS232 DCD (DB25 pin 8)
 * and connects RS232 DTR (DB25 pin 20) through MAX3232 to GPIO pin 6.
 * To assert DTR (to RS232 DSR), raise or lower GPIO pin 5.
 * To read DSR (from RS232 DTR), read GPIO pin 6.
 */

/* reboot */
void(* restart) (void) = 0;

/* TPDD2-style bootstrap */
void sendLoader() {
  byte b = 0x00;
  int c = 0;
  File f = SD.open(LOADER_FILE);
  DEBUG_PRINTL(F("sendLoader()"));
  if (f) {
    SD_LED_ON
    DEBUG_PRINTL(F("Sending " LOADER_FILE));
      while (f.available()) {
        b = f.read();
        CLIENT.write(b);
        if(++c>99) {
         DEBUG_PRINT(F("."));
         c = 0;
        }
        delay(LOADER_BYTE_MS);
      }
    f.close();
    SD_LED_OFF
    CLIENT.write(BASIC_EOF);
    CLIENT.flush();
    CLIENT.end();
    DEBUG_PRINTL(F("DONE"));
  } else {
    DEBUG_PRINTL(F("Could not find " LOADER_FILE " ..."));
  }
  restart(); // go back to normal TPDD emulation mode
}
#endif // ENABLE_LOADER

// Append a string to directory[]
void directoryAppend(char* c){
  bool t = false;
  byte i = 0x00;
  byte j = 0x00;

  DEBUG_PRINT(F("directoryAppend(")); DEBUG_PRINT(c); DEBUG_PRINTL(F(")"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));

  DEBUG_PRINTL(F("->"));

  while(directory[i] != 0x00) i++;

  while(!t){
    directory[i++] = c[j++];
    t = c[j] == 0x00;
  }

  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
  DEBUG_PRINT(F("directoryAppend(")); DEBUG_PRINT(c); DEBUG_PRINTL(F(") end"));
}

// Remove the last path element from directoy[]
void upDirectory(){
  byte j = DIRECTORY_SZ;

  DEBUG_PRINTL(F("upDirectory()"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));

  while(directory[j] == 0x00) j--;
  if(directory[j] == '/' && j!= 0x00) directory[j] = 0x00;
  while(directory[j] != '/') directory[j--] = 0x00;

  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
}

void copyDirectory(){ // Makes a copy of the working directory to a scratchpad
  for(byte i=0x00; i<DIRECTORY_SZ; i++) tempDirectory[i] = directory[i];
}


// Fill dmeLabel[] with exactly 6 chars from s[], space-padded.
// We could just read directory[] directly instead of passng s[]
// but this way we can pass arbitrary values later. For example
// FAT volume label, RTC time, battery level, ...
void setLabel(char* s) {
  byte z = DIRECTORY_SZ;
  byte j = z;

  DEBUG_PRINT(F("setLabel(")); DEBUG_PRINT(s); DEBUG_PRINTL(F(")"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
  DEBUG_PRINT(F("dmeLabel["));  DEBUG_PRINT(dmeLabel);  DEBUG_PRINTL(F("]"));

  while(s[j] == 0x00) j--;            // seek from end to non-null
  if(s[j] == '/' && j > 0x00) j--;    // seek past trailing slash, if any
  z = j;                              // mark end of name
  while(s[j] != '/' && j > 0x00) j--; // seek to next slash or start of string
  if (s[j] == '/') j++;               // don't include the slash itself, if any

  // copy 6 chars, up to z or null, space pad
  for(byte i=0x00 ; i<0x06 ; i++) {
    if(s[j]>0x00 && j<=z) dmeLabel[i] = s[j]; else dmeLabel[i] = 0x20;
    j++;
  }
  dmeLabel[0x06] = 0x00;

  DEBUG_PRINT(F("dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));
}



/*
 *
 * TPDD Port misc. routines
 *
 */

void tpddWrite(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  CLIENT.write(c);
}

void tpddWriteString(char* c){  //Outputs a null-terminated char array c to the TPDD port
  byte i = 0x00;
  while(c[i]!=0x00){
    checksum += c[i];
    CLIENT.write(c[i]);
    i++;
  }
}

void tpddSendChecksum(){  //Outputs the checksum to the TPDD port and clears the checksum
  CLIENT.write(checksum^0xFF);
  checksum = 0x00;
}


/*
 *
 * TPDD Port return routines
 *
 */

void return_normal(byte errorCode){ //Sends a normal return to the TPDD port with error code errorCode
  DEBUG_PRINTL(F("return_normal()"));
#if DEBUG > 1
  DEBUG_PRINT("R:Norm ");
  DEBUG_PRINTIL(errorCode, HEX);
#endif
  tpddWrite(0x12);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void return_reference(){  //Sends a reference return to the TPDD port
  byte term = 0x06;
  bool terminated = false;
  DEBUG_PRINTL(F("return_reference()"));

  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  for(byte i=0x00;i<FILENAME_SZ;++i) tempRefFileName[i] = 0x00;

  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer

  if(DME && entry.isDirectory()){ //      !!!Tacks ".<>" on the end of the return reference if we're in DME mode and the reference points to a directory
    for(byte i=0x00; i < 0x07; i++){ //Find the end of the directory's name by looping through the name buffer
      if(tempRefFileName[i] == 0x00) term = i; //and setting a termination index to the offset where the termination is encountered
    }
    tempRefFileName[term++] = '.';  //Tack the expected ".<>" to the end of the name
    tempRefFileName[term++] = '<';
    tempRefFileName[term++] = '>';
    for(byte i=term; i<FILENAME_SZ; i++) tempRefFileName[i] = 0x00;
    term = 0x06; //Reset the termination index to prepare for the next check
  }



  for(byte i=0x00; i<0x06; i++){ //      !!!Pads the name of the file out to 6 characters using space characters
    if(term == 0x06){  //Perform these checks if term hasn't changed
      if(tempRefFileName[i]=='.'){
        term = i;   //If we encounter a '.' character, set the temrination pointer to the current offset and output a space character instead
        tpddWrite(' ');
      }else{
        tpddWrite(tempRefFileName[i]);  //If we haven't encountered a period character, output the next character
      }
    }else{
      tpddWrite(' '); //If we did find a period character, write a space character to pad the reference name
    }
  }

  for(byte i=0x00; i<0x12; i++) tpddWrite(tempRefFileName[i+term]);  //      !!!Outputs the file extension part of the reference name starting at the offset found above

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite((byte)((entry.fileSize()&0xFF00)>>0x08));  //File size most significant byte
  tpddWrite((byte)(entry.fileSize()&0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#if DEBUG > 1
  DEBUG_PRINTL("R:Ref");
#endif
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port
  DEBUG_PRINTL(F("return_blank_reference()"));
  tpddWrite(0x11);    //Return type (reference)
  tpddWrite(0x1C);    //Data size (1C)

  for(byte i=0x00; i<FILENAME_SZ; i++) tpddWrite(0x00);  //Write the reference file name to the TPDD port

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#if DEBUG > 1
  DEBUG_PRINTL("R:BRef");
#endif
}

void return_parent_reference(){
  DEBUG_PRINTL(F("return_parent_reference()"));
  tpddWrite(0x11);    // return type
  tpddWrite(0x1C);    // data size

  tpddWriteString("PARENT.<>");
  for(byte i=0x09; i<FILENAME_SZ; i++) tpddWrite(0x00);

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
}

/*
 *
 * TPDD Port command handler routines
 *
 */

void command_reference(){ //Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+0x1D)];  //The search form byte exists 29 bytes into the command
  byte refIndex = 0x00;  //Reference file name index

  DEBUG_PRINTL(F("command_reference()"));

#if DEBUG > 1
  DEBUG_PRINT("SF:");
  DEBUG_PRINTIL(searchForm,HEX);
#endif

  if(searchForm == 0x00){ //Request entry by name
    for(byte i=0x04; i<0x1C; i++){  //Put the reference file name into a buffer
      if(dataBuffer[(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=dataBuffer[(tail+i)]; //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character

#if DEBUG > 1
    DEBUG_PRINT("Ref: ");
    DEBUG_PRINTL(refFileName);
#endif

    if(DME){  //        !!!Strips the ".<>" off of the reference name if we're in DME mode
      if(strstr(refFileName, ".<>") != 0x00){
        for(byte i=0x00; i<FILENAME_SZ; i++){  //Copies the reference file name to a scratchpad buffer with no directory extension if the reference is for a directory
          if(refFileName[i] != '.' && refFileName[i] != '<' && refFileName[i] != '>'){
            refFileNameNoDir[i]=refFileName[i];
          }else{
            refFileNameNoDir[i]=0x00; //If the character is part of a directory extension, don't copy it
          }
        }
      }else{
        for(byte i=0x00; i<FILENAME_SZ; i++) refFileNameNoDir[i]=refFileName[i]; //Copy the reference directly to the scratchpad buffer if it's not a directory reference
      }
    }

    directoryAppend(refFileNameNoDir);  //Add the reference to the directory buffer

    SD_LED_ON
    if(SD.exists(directory)){ //If the file or directory exists on the SD card...
      entry=SD.open(directory); //...open it...
      return_reference(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry
    }else{  //If the file does not exist...
      return_blank_reference();
    }

    upDirectory();  //Strip the reference off of the directory buffer

  }else if(searchForm == 0x01){ //Request first directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openFirst();
  }else if(searchForm == 0x02){ //Request next directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openNext();
  }else{  //Parameter is invalid
    return_normal(0x36);  //Send a normal return to the TPDD port with a parameter error
  }
  SD_LED_OFF
}

void ref_openFirst(){
  DEBUG_PRINTL(F("ref_openFirst()"));
  directoryBlock = 0x00; //Set the current directory entry index to 0
  if(DME && directoryDepth>0x00 && directoryBlock==0x00){ //Return the "PARENT.<>" reference if we're in DME mode
    SD_LED_OFF
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
}

void ref_openNext(){
  DEBUG_PRINTL(F("ref_openNext()"));
  directoryBlock++; //Increment the directory entry index
  SD_LED_ON
  root.rewindDirectory(); //Pull back to the begining of the directory
  for(byte i=0x00; i<directoryBlock-0x01; i++) root.openNextFile();  //skip to the current entry offset by the index

  entry = root.openNextFile();  //Open the entry

  if(entry){  //If the entry exists it is returned
    if(entry.isDirectory() && !DME){  //If it's a directory and we're not in DME mode
      entry.close();  //the entry is skipped over
      ref_openNext(); //and this function is called again
    }

    return_reference(); //Send the reference info to the TPDD port
    entry.close();  //Close the entry
    SD_LED_OFF
  }else{
    SD_LED_OFF
    return_blank_reference();
  }
}

void command_open(){  //Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+0x04)];  //The access mode is stored in the 5th byte of the command
  DEBUG_PRINTL(F("command_open()"));
  entry.close();

  if(DME && strcmp(refFileNameNoDir, "PARENT") == 0x00){ //If DME mode is enabled and the reference is for the "PARENT" directory
    upDirectory();  //The top-most entry in the directory buffer is taken away
    directoryDepth--; //and the directory depth index is decremented
  }else{
    directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
    SD_LED_ON
//    if(DME && (byte)strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
    if(DME && strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      SD.mkdir(directory);  //create the directory
      upDirectory();
    }else{
      entry=SD.open(directory); //Open the directory to reference the entry
      if(entry.isDirectory()){  //      !!!Moves into a sub-directory
        entry.close();  //If the entry is a directory
        directoryAppend("/"); //append a slash to the directory buffer
        directoryDepth++; //and increment the directory depth index
      }else{  //If the reference isn't a sub-directory, it's a file
        entry.close();
        switch(rMode){
          case 0x01: entry = SD.open(directory, O_WRITE); break;             // Write
          case 0x02: entry = SD.open(directory, O_WRITE | O_APPEND); break;  // Append
          case 0x03: entry = SD.open(directory, O_READ); break;              // Read
        }
        upDirectory();
      }
    }
  }

  if(SD.exists(directory)){ //If the file actually exists...
    SD_LED_OFF
    return_normal(0x00);  //...send a normal return with no error.
  }else{  //If the file doesn't exist...
    SD_LED_OFF
    return_normal(0x10);  //...send a normal return with a "file does not exist" error.
  }
}

void command_close(){ //Closes the currently open entry
  DEBUG_PRINTL(F("command_close()"));
  entry.close();  //Close the entry
  SD_LED_OFF
  return_normal(0x00);  //Normal return with no error
}

void command_read(){  //Read a block of data from the currently open entry
  DEBUG_PRINTL(F("command_read()"));
  SD_LED_ON
  byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); //Try to pull 128 bytes from the file into the buffer
  SD_LED_OFF
#if DEBUG > 1
  DEBUG_PRINT("A: ");
  DEBUG_PRINTIL(entry.available(),HEX);
#endif
  if(bytesRead > 0x00){  //Send the read return if there is data to be read
    tpddWrite(0x10);  //Return type
    tpddWrite(bytesRead); //Data length
    for(byte i=0x00; i<bytesRead; i++) tpddWrite(fileBuffer[i]);
    tpddSendChecksum();
  }else{
    return_normal(0x3F);  //send a normal return with an end-of-file error if there is no data left to read
  }
}

void command_write(){ //Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+0x03)];

  DEBUG_PRINTL(F("command_write()"));
  SD_LED_ON
  for(byte i=0x00; i<commandDataLength; i++) entry.write(dataBuffer[(byte)(tail+0x04+i)]);
  SD_LED_OFF
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

void command_delete(){  //Delete the currently open entry
  DEBUG_PRINTL(F("command_delete()"));
  SD_LED_ON
  entry.close();  //Close any open entries
  directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
  entry = SD.open(directory, O_READ);  //directory can be deleted if opened "READ"

  if(DME && entry.isDirectory()){
    entry.rmdir();  //If we're in DME mode and the entry is a directory, delete it
  }else{
    entry.close();  //Files can be deleted if opened "WRITE", so it needs to be re-opened
    entry = SD.open(directory, O_WRITE);
    entry.remove();
  }
  SD_LED_OFF
  upDirectory();
  return_normal(0x00);  //Send a normal return with no error
}

void command_format(){  //Not implemented
  DEBUG_PRINTL(F("command_format()"));
  return_normal(0x00);
}

void command_status(){  //Drive status
  DEBUG_PRINTL(F("command_status()"));
  return_normal(0x00);
}

void command_condition(){ //Not implemented
  DEBUG_PRINTL(F("command_condition()"));
  return_normal(0x00);
}

void command_rename(){  //Renames the currently open entry
  byte refIndex = 0x00;  //Temporary index for the reference name

  DEBUG_PRINTL(F("command_rename()"));

  directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer

  SD_LED_ON

  if(entry) entry.close(); //Close any currently open entries
  entry = SD.open(directory); //Open the entry
  if(entry.isDirectory()) directoryAppend("/"); //Append a slash to the end of the directory buffer if the reference is a sub-directory

  copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
  upDirectory();  //Strip the previous directory reference off of the directory buffer

  for(byte i=0x04; i<0x1C; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ //If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
  }

  tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character

  if(DME && entry.isDirectory()){ //      !!!If the entry is a directory, we need to strip the ".<>" off of the new directory name
    if(strstr(tempRefFileName, ".<>") != 0x00){
      for(byte i=0x00; i<FILENAME_SZ; i++){
        if(tempRefFileName[i] == '.' || tempRefFileName[i] == '<' || tempRefFileName[i] == '>'){
          tempRefFileName[i]=0x00;
        }
      }
    }
  }

  directoryAppend(tempRefFileName);
  if(entry.isDirectory()) directoryAppend("/");

  DEBUG_PRINTL(directory);
  DEBUG_PRINTL(tempDirectory);
  SD.rename(tempDirectory,directory);  //Rename the entry

  upDirectory();
  entry.close();

  SD_LED_OFF

  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

/*
 *
 * TS-DOS DME Commands
 *
 */

void command_DMEReq() {  //Send the dmeLabel

  DEBUG_PRINT(F("command_DMEReq(): dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));

  if (DME) {
    if (directoryDepth>0x00) setLabel(directory); else setLabel(DME_ROOT_LABEL);
    tpddWrite(0x12);
    tpddWrite(0x0B);
    tpddWrite(0x20);
    for (byte i=0x00 ; i<0x06 ; i++) tpddWrite(dmeLabel[i]);
    tpddWrite('.');
    tpddWrite('<');
    tpddWrite('>');
    tpddWrite(0x20);
    tpddSendChecksum();
  }else{
    return_normal(0x36);
  }
}


/*
 *
 * Power-On
 *
 */

void setup() {
  PINMODE_SD_LED_OUTPUT
  PINMODE_DEBUG_LED_OUTPUT
  //pinMode(WAKE_PIN, INPUT_PULLUP);  // typical, but don't do on RX
  SD_LED_OFF
  DEBUG_LED_OFF
  digitalWrite(LED_BUILTIN,LOW);  // turn main led off, regardless of SD_LED or DEBUG_LED settings

  // DSR/DTR
#if defined(DTR_PIN)
  pinMode(DTR_PIN, OUTPUT);
  digitalWrite(DTR_PIN,HIGH);  // tell client we're not ready
#endif // DTR_PIN
#if ENABLE_LOADER && defined(DSR_PIN)
  pinMode(DSR_PIN, INPUT_PULLUP);
#endif // ENABLE_LOADER && DSR_PIN

// if debug console enabled, blink led and wait for console to be attached before proceeding
#if DEBUG && defined(CONSOLE)
  CONSOLE.begin(115200);
    while(!CONSOLE){
      DEBUG_LED_ON
      delay(0x60);
      DEBUG_LED_OFF
      delay(0x60);
    }
  CONSOLE.flush();
#endif // DEBUG && CONSOLE

  CLIENT.begin(19200);
  CLIENT.flush();

  DEBUG_PRINTL(F("\r\n-----------[ " SKETCH_NAME " " SKETCH_VERSION " ]------------"));
  DEBUG_PRINTL(F("setup()"));
  DEBUG_PRINTL(F("BOARD_NAME: " BOARD_NAME));
#if ENABLE_LOADER
  DEBUG_PRINT(F("sendLoader() enabled, DSR:"));
#if defined(DSR_PIN)
  DEBUG_PRINT(DSR_PIN);
#endif // DSR_PIN
  DEBUG_PRINT(" DTR:");
#if defined(DTR_PIN)
  DEBUG_PRINTL(DTR_PIN);
#endif DTR_PIN
#else
  DEBUG_PRINTL(F("sendLoader() disabled"));
#endif // ENABLE_LOADER

  for(byte i=0x00;i<FILE_BUFFER_SZ;++i) dataBuffer[i] = 0x00;


#if defined(USE_SDIO)
 #if DEBUG > 2
 DEBUG_PRINTL(F("Using SDIO"));
 #endif DEBUG
#else
 #if defined(SD_CS_PIN)
  #if defined(DISABLE_SD_CS)
    #if DEBUG > 2
    DEBUG_PRINT(F("Disabling SPI device on pin "));
    DEBUG_PRINTL(SD_CS_PIN);
    #endif // DEBUG
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
  #else
    #if DEBUG > 2
    DEBUG_PRINTL(F("Assuming the SD is the only SPI device."));  
    #endif
  #endif // DISABLE_SD_CS
  #if DEBUG > 2
  DEBUG_PRINT(F("Using SD chip select pin: "));
  DEBUG_PRINTL(SD_CS_PIN);
  #endif // DEBUG
 #endif // SD_CS_PIN
#endif // USE_SDIO

  initCard();

#if defined(DTR_PIN)
  digitalWrite(DTR_PIN,LOW); // tell client we're ready
#endif

#if ENABLE_LOADER && defined(DSR_PIN)
  if(!digitalRead(DSR_PIN)) sendLoader(); // client wants bootstrap
#endif // ENABLE_LOADER
}


/*
 *
 * Main code loop
 *
 */

void loop() {
  byte rType = 0x00; // Current request type (command type)
  byte rLength = 0x00; // Current request length (command length)
  byte diff = 0x00;  // Difference between the head and tail buffer indexes

  DEBUG_PRINTL(F("loop(): start"));
  state = 0x00; // 0 = waiting for command, 1 = waiting for full command, 2 = have full command

//#if ENABLE_SLEEP
//  sleepNow();
//#endif // ENABLE_SLEEP
  while(state<0x02){ // While waiting for a command...
#if ENABLE_SLEEP
    sleepNow();
#endif // ENABLE_SLEEP
    while (CLIENT.available() > 0x00){ // While there's data to read from the TPDD port...
#if ENABLE_SLEEP
    #if SLEEP_INHIBIT
      idleSince = millis();
    #endif
#endif // ENABLE_SLEEP
      dataBuffer[head++]=(byte)CLIENT.read();  //...pull the character from the TPDD port and put it into the command buffer, increment the head index...
      if(tail==head)tail++; //...if the tail index equals the head index (a wrap-around has occoured! data will be lost!)
                            //...increment the tail index to prevent the command size from overflowing.

#if DEBUG > 1
      DEBUG_PRINTI((byte)(head-1),HEX);
      DEBUG_PRINT("-");
      DEBUG_PRINTI(tail,HEX);
      DEBUG_PRINTI((byte)(head-tail),HEX);
      DEBUG_PRINT(":");
      DEBUG_PRINTI(dataBuffer[head-1],HEX);
      DEBUG_PRINT(";");
      DEBUG_PRINTL((dataBuffer[head-1]>=0x20)&&(dataBuffer[head-1]<=0x7E)?(char)dataBuffer[head-1]:' ');
#endif
    }

    diff=(byte)(head-tail); //...set the difference between the head and tail index (number of bytes in the buffer)

    if(state == 0x00){ //...if we're waiting for a command...
      if(diff >= 0x04){  //...if there are 4 or more characters in the buffer...
        if(dataBuffer[tail]=='Z' && dataBuffer[(byte)(tail+0x01)]=='Z'){ //...if the buffer's first two characters are 'Z' (a TPDD command)
          rLength = dataBuffer[(byte)(tail+0x03)]; //...get the command length...
          rType = dataBuffer[(byte)(tail+0x02)]; //...get the command type...
          state = 0x01;  //...set the state to "waiting for full command".
        }else if(dataBuffer[tail]=='M' && dataBuffer[(byte)(tail+0x01)]=='1'){ //If a DME command is received
          DME = true; //set the DME mode flag to true
          tail=tail+0x02;  //and skip past the command to the DME request command
        }else{  //...if the first two characters are not 'Z'...
          tail=tail+(tail==head?0x00:0x01); //...move the tail index forward to the next character, stop if we reach the head index to prevent an overflow.
        }
      }
    }

    if(state == 0x01){ //...if we're waiting for the full command to come in...
      if(diff>rLength+0x04){ //...if the amount of data in the buffer satisfies the command length...
          state = 0x02;  //..set the state to "have full command".
        }
    }
  }

#if DEBUG > 1
  DEBUG_PRINTI(tail,HEX); // show the tail index in the buffer where the command was found...
  DEBUG_PRINT("=");
  DEBUG_PRINT("T:"); //...the command type...
  DEBUG_PRINTI(rType, HEX);
  DEBUG_PRINT("|L:");  //...and the command length.
  DEBUG_PRINTI(rLength, HEX);
  DEBUG_PRINTL(DME?'D':'.');
#endif

  switch(rType){  // Select the command handler routine to jump to based on the command type
    case 0x00: command_reference(); break;
    case 0x01: command_open(); break;
    case 0x02: command_close(); break;
    case 0x03: command_read(); break;
    case 0x04: command_write(); break;
    case 0x05: command_delete(); break;
    case 0x06: command_format(); break;
    case 0x07: command_status(); break;
    case 0x08: command_DMEReq(); break; // DME Command
    case 0x0C: command_condition(); break;
    case 0x0D: command_rename(); break;
    default: return_normal(0x36); break;  // Send a normal return with a parameter error if the command is not implemented
  }

#if DEBUG > 1
  DEBUG_PRINTI(head,HEX);
  DEBUG_PRINT(":");
  DEBUG_PRINTI(tail,HEX);
  DEBUG_PRINT("->");
#endif

  tail = tail+rLength+0x05;  // Increment the tail index past the previous command

#if DEBUG > 1
  DEBUG_PRINTIL(tail,HEX);
#endif
}
