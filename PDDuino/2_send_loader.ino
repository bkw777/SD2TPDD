#if defined(LOADER_FILE)
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

/* TPDD2-style bootstrap */
void sendLoader() {
  byte b = 0x00;
#if DEBUG
  int c = 0;
#endif // DEBUG
  File f = SD.open(LOADER_FILE);

  DEBUG_PRINTL(F("sendLoader()"));  
  if (f) {
    SD_LED_ON
    DEBUG_PRINT(F("Sending " LOADER_FILE " "));
      while (f.available()) {
        b = f.read();
        CLIENT.write(b);
#if DEBUG
        if(++c>99) {
         DEBUG_PRINT(F("."));
         c = 0;
        }
#endif // DEBUG
        delay(LOADER_BYTE_MS);
      }
    f.close();
    SD_LED_OFF
    CLIENT.flush();
    CLIENT.write(BASIC_EOF);
#if defined(BOARD_FEATHERM0)    // some kind of bug with M0
    delay(250);                 // need to send the 0x1A twice, and need to delay between them
    CLIENT.write(BASIC_EOF);    // 2 in a row doesn't work, delay alone before sending 1 doesn't work
#endif // BOARD_FEATHERM0
    CLIENT.flush();
    CLIENT.end();
    DEBUG_PRINTL(F("DONE"));
  } else {
    DEBUG_PRINT(F("Could not find " LOADER_FILE " ..."));
  }
  restart; // go back to normal TPDD emulation mode
}
#endif // LOADER_FILE
