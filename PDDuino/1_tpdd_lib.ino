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

void copyDirectory(){ //Makes a copy of the working directory to a scratchpad
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
  if(s[j] == '/' && j > 0x00) j--;    // seek past trailing slash if any
  z = j;                              // mark end of name
  while(s[j] != '/' && j > 0x00) j--; // seek to next slash or start of string
  if(s[j] != '/') j++;                // don't include the slash if any

  // copy 6 chars, up to z or null, space pad
  for(byte i=0x00 ; i<0x06 ; i++) {
    if(s[j]>0x00 && j<=z) dmeLabel[i] = s[j]; else dmeLabel[i] = ' ';
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
