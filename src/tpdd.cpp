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
 *  tpdd.cpp: Tandy Portable Disk Drive emulation layer
 *
 */


#include <SdFat.h>
#include <stdint.h>
#include "config.h"
#include "Logger.h"
#include "tpdd.h"
#include "powermgmt.h"

#if defined(USE_SDIO)
SdFatSdioEX fs;
#else
SdFat fs;
#endif

  static byte checksum = 0x00;  //Global variable for checksum calculation

  static bool DME = false; //TS-DOS DME mode flag

  static uint8_t _buffer[DATA_BUFFER_SZ]; //Data buffer for commands
  static uint8_t _length;

  static byte fileBuffer[FILE_BUFFER_SZ]; //Data buffer for file reading

  static char refFileName[FILENAME_SZ] = "";  //Reference file name for emulator
  static char refFileNameNoDir[FILENAME_SZ] = ""; //Reference file name for emulator with no ".<>" if directory
  static char tempRefFileName[FILENAME_SZ] = ""; //Second reference file name for renaming
  static byte directoryBlock = 0x00; //Current directory block for directory listing
  static char directory[DIRECTORY_SZ] = "/";
  static byte directoryDepth = 0x00;
  static char tempDirectory[DIRECTORY_SZ] = "/";
  static char dmeLabel[0x07] = "";  // 6 chars + NULL

  static sysstate_t _sysstate = SYS_IDLE;

  static openmode_t _mode = OPEN_NONE;

  static File entry; //Moving file entry for the emulator
  static File tempEntry; //Temporary entry for moving files
  static File root;  //Root file for filesystem reference

// Append a string to directory[]
static void append_dir(const char* c){
  bool t = false;
  byte i = 0x00;
  byte j = 0x00;

  LOGD_P("%s() entry",__func__);
  LOGD_P("directoryAppend(%s)", c);
  LOGD_P("directory[%s]", directory);

  LOGD_P("->");

  while(directory[i] != 0x00) i++;

  while(!t){
    directory[i++] = c[j++];
    t = c[j] == 0x00;
  }

  LOGD_P("directory[%s]", directory);
  LOGD_P("directoryAppend(%s) end", c);
  LOGD_P("%s() exit",__func__);
}

// Remove the last path element from directoy[]
static void remove_subdir(void) {
  byte j = DIRECTORY_SZ;

  LOGD_P("%s() entry",__func__);
  LOGD_P("directory[%s]", directory);

  while(directory[j] == 0x00) j--;
  if(directory[j] == '/' && j!= 0x00) directory[j] = 0x00;
  while(directory[j] != '/') directory[j--] = 0x00;

  LOGD_P("directory[%s]", directory);
  LOGD_P("%s() exit",__func__);
}

static void copy_dir(void) { //Makes a copy of the working directory to a scratchpad
  for(byte i=0x00; i<DIRECTORY_SZ; i++) tempDirectory[i] = directory[i];
}


// Fill dmeLabel[] with exactly 6 chars from s[], space-padded.
// We could just read directory[] directly instead of passng s[]
// but this way we can pass arbitrary values later. For example
// FAT volume label, RTC time, battery level, ...
static void set_label(const char* s) {
  byte z = DIRECTORY_SZ;
  byte j = z;

  LOGD_P("%s() entry",__func__);
  LOGD_P("set_label(%s", s);
  LOGD_P("directory[%s]", directory);
  LOGD_P("dmeLabel[%s]", dmeLabel);

  while(s[j] == 0x00) j--;            // seek from end to non-null
  if(s[j] == '/' && j > 0x00) j--;    // seek past trailing slash
  z = j;                              // mark end of name
  while(s[j] != '/' && j > 0x00) j--; // seek to next slash

  // copy 6 chars, up to z or null, space pad
  for(byte i=0x00 ; i<0x06 ; i++) if(s[++j]>0x00 && j<=z) dmeLabel[i] = s[j]; else dmeLabel[i] = 0x20;
  dmeLabel[0x06] = 0x00;

  LOGD_P("dmeLabel[%s]", dmeLabel);
  LOGD_P("%s() exit",__func__);
}



/*
 *
 * TPDD Port misc. routines
 *
 */

static void send_byte(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  CLIENT.write(c);
  LOGV_P("O:%2.2X(%c)>%2.2X", (uint8_t)c, (c > 0x20 && (uint8_t)c < 0x80 ? c : ' '), checksum);
}

static void send_buffer(uint8_t *data, uint16_t len) {
  for(uint16_t i = 0; i < len; i++) {
    send_byte(data[i]);
  }
}

static void send_chksum(void) {  //Outputs the checksum to the TPDD port and clears the checksum
  uint8_t chk = checksum ^ 0xff;
  CLIENT.write(chk);
  LOGV_P("O:%2.2X", chk);
  checksum = 0;
}

static void send_dir_suffix(void) {
  send_byte('.');   //Tack the expected ".<>" to the end of the name
  send_byte('<');
  send_byte('>');
}


/*
 *
 * TPDD Port return routines
 *
 */

static void send_ret_normal(error_t errorCode){ //Sends a normal return to the TPDD port with error code errorCode

  LOGD_P("%s() entry",__func__);
  LOGD_P("R:Norm %2.2X", errorCode);
  send_byte(RET_NORMAL);  //Return type (normal)
  send_byte(0x01);  //Data size (1)
  send_byte(errorCode); //Error code
  send_chksum(); //Checksum
  LOGD_P("%s() exit",__func__);
}

static void send_ref(const char *name, bool isDir, uint32_t size ) {
  uint8_t i, j;

  LOGD_P("%s() entry",__func__);
  LOGI_P("N:'%s':D%d-%d", name, isDir, size);
  send_byte(RET_DIRECTORY);    //Return type (reference)
  // filename + attribute + length + free sector count
#ifdef ENABLE_TPDD_EXTENSIONS
  if(size > 65535)
    send_byte(FILENAME_SZ + 1 + 4 + 1);    //Data size (1E)
  else
#endif
    send_byte(FILENAME_SZ + 1 + 2 + 1);    //Data size (1C)
  if(name == NULL) {
    for(i = 0; i < FILENAME_SZ; i++)
      send_byte(0x00);  //Write the reference file name to the TPDD port
  } else {
    if(isDir && DME) {  // handle dirname.
      for(i = 0; (i < 6) && (name[i] != 0); i++)
        send_byte(name[i]);
      for(;i < 6; i++)
        send_byte(' '); // pad out the dir
      send_dir_suffix();
      j = 9;
    } else {
      for(i = 0; (i < 6) && (name[i] != '.'); i++) {
        send_byte(name[i]);
      }
      for(j = i; j < 6; j++) {
        send_byte(' ');
      }
      for(; j < FILENAME_SZ && (name[i] != 0); j++) {
        send_byte(name[i++]);  // send the file extension
      }
    }
    for(; j < FILENAME_SZ; j++) {
      send_byte('\0');  // pad out
    }
  }
  // other implementation send 'F' here:
  //tpddWrite(0);  //Attribute, unused
  send_byte((name == NULL ? '\0' : 'F'));
  send_byte((uint8_t)(size >> 8));  //File size most significant byte
  send_byte((uint8_t)(size)); //File size least significant byte
#ifdef ENABLE_TPDD_EXTENSIONS
  if(size > 65535) {
    send_byte('P');  //Free sectors, SD card has more than we'll ever care about
    //tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
    send_byte((uint8_t)(size >> 24));  //File size most significant byte
    send_byte((uint8_t)(size >> 16)); //File size next most significant byte
  } else
#endif
    //send_byte(0x80);  //Free sectors, SD card has more than we'll ever care about
    // Note: ts-dos only uses the value returned on the last dir entry.
    // and that entry is often empty...
    send_byte(0x9d);  //Free sectors, SD card has more than we'll ever care about
  send_chksum(); //Checksum

  LOGD_P("%s() exit",__func__);
}

static void send_normal_ref(void) {  //Sends a reference return to the TPDD port

  LOGD_P("%s() entry",__func__);
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  send_ref(tempRefFileName, entry.isDirectory(), entry.fileSize());

  LOGI_P("R:Ref");
  LOGD_P("%s() exit",__func__);
}

static void send_blank_ref(void) {  //Sends a blank reference return to the TPDD port

  LOGD_P("%s() entry",__func__);
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  send_ref(NULL, false, 0);
  LOGI_P("R:BRef");
  LOGD_P("%s() exit",__func__);
}

static void send_parent_ref(void) {

  LOGD_P("%s() entry",__func__);
  send_ref("PARENT", true, 0);
  LOGI_P("R:PRef");
  LOGD_P("%s() exit",__func__);
}

/*
 *
 * TPDD Port command handler routines
 *
 */

static void ret_next_ref(void) {

  LOGD_P("%s() entry",__func__);
  if(_sysstate == SYS_ENUM) {   // We are enumerating the directory
    directoryBlock++; //Increment the directory entry index
    led_sd_on();
    root.rewindDirectory(); //Pull back to the begining of the directory
    for(uint8_t i = 0x00; i < directoryBlock - 0x01; i++)
      root.openNextFile();  //skip to the current entry offset by the index

    //Open the entry
    if(entry = root.openNextFile()) {  //If the entry exists it is returned
      if(entry.isHidden() || (entry.isDirectory() && !DME)) {
        //If it's a directory and we're not in DME mode or file/dir is hidden
        entry.close();  //the entry is skipped over
        ret_next_ref(); //and this function is called again
      } else {
        send_normal_ref(); //Send the reference info to the TPDD port
        entry.close();  //Close the entry
      }
      led_sd_off();
    } else {
      led_sd_off();
      send_blank_ref();
    }
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_DIR_SEARCH);
  }
  LOGD_P("%s() exit",__func__);
}

static void ret_first_ref(void) {

  LOGD_P("%s() entry",__func__);
  directoryBlock = 0x00; //Set the current directory entry index to 0
  if(DME && directoryDepth > 0x00) { //Return the "PARENT.<>" reference if we're in DME mode
    led_sd_off();
    send_parent_ref();
  }else{
    ret_next_ref();    //otherwise we just return the next reference
  }
  LOGD_P("%s() exit",__func__);
}

static void req_reference(void) { // File/Dir Reference command handler
  enumtype_t searchForm = (enumtype_t)_buffer[0x19];  //The search form byte exists 0x19 bytes into the command
  byte refIndex = 0x00;  //Reference file name index

  LOGD_P("%s() entry",__func__);

  LOGD_P("SF: %2.2X", searchForm);

  switch(searchForm) {
  case ENUM_PICK:  //Request entry by name
    for(uint8_t i = 0; i < FILENAME_SZ; i++) {  //Put the reference file name into a buffer
      if(_buffer[i] != ' '){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=_buffer[i];     //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex] = '\0'; //Terminate the file name buffer with a null character
    _sysstate = SYS_REF;

    LOGV_P("Ref: %s", refFileName);

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

    append_dir(refFileNameNoDir);  //Add the reference to the directory buffer

    led_sd_on();
    if(fs.exists(directory)){ //If the file or directory exists on the SD card...
      entry=fs.open(directory); //...open it...
      send_normal_ref(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry
    }else{  //If the file does not exist...
      send_blank_ref();
    }

    remove_subdir();  //Strip the reference off of the directory buffer

    break;
  case ENUM_FIRST:  //Request first directory block
    _sysstate = SYS_ENUM;
    led_sd_on();
    root.close();
    root = fs.open(directory);
    ret_first_ref();
    break;
  case ENUM_NEXT:   //Request next directory block
    led_sd_on();
    root.close();
    root = fs.open(directory);
    ret_next_ref();
    break;
  case ENUM_PREV:
    // TODO really should back up the dir one...
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_PARM);  //  For now, send some error back
    break;
  default:          //Parameter is invalid
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_PARM);  //Send a normal return to the TPDD port with a parameter error
    break;
  case ENUM_DONE:
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);  //Send a normal return to the TPDD port with a parameter error
  }
  led_sd_off();
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE if error
 */
static void req_open(void) {  //Opens an entry for reading, writing, or appending
  _mode = (openmode_t)_buffer[0];  //The access mode is stored in the 1st byte of the command payload

  LOGD_P("%s() entry",__func__);

  if(_sysstate == SYS_REF) {
    entry.close();

    if(DME && strcmp(refFileNameNoDir, "PARENT") == 0) { //If DME mode is enabled and the reference is for the "PARENT" directory
      remove_subdir();  //The top-most entry in the directory buffer is taken away
      directoryDepth--; //and the directory depth index is decremented
    } else {
      append_dir(refFileNameNoDir);  //Push the reference name onto the directory buffer
      led_sd_on();
  //    if(DME && (byte)strstr(refFileName, ".<>") != 0x00 && !fs.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      if(DME && strstr(refFileName, ".<>") != 0 && !fs.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
        fs.mkdir(directory);  //create the directory
        remove_subdir();
      } else {
        entry = fs.open(directory); //Open the directory to reference the entry
        if(entry.isDirectory()){  //      !!!Moves into a sub-directory
          entry.close();  //If the entry is a directory
          append_dir("/"); //append a slash to the directory buffer
          directoryDepth++; //and increment the directory depth index
        } else {  //If the reference isn't a sub-directory, it's a file
          entry.close();
          switch(_mode){
            case OPEN_WRITE:
              // bug: FILE_WRITE includes O_APPEND, so existing files would be opened
              //      at end.
              //entry = fs.open(directory, FILE_WRITE);

              // open for write, position at beginning of file, create if needed
              entry = fs.open(directory, O_CREAT | O_WRITE);
              _sysstate = SYS_WRITE;
              break;                // Write
            case OPEN_APPEND:
              entry = fs.open(directory, FILE_WRITE | O_APPEND);
              _sysstate = SYS_WRITE;
              break;                // Append
            case OPEN_READ:
            default:
              entry = fs.open(directory, O_READ);
              _sysstate = SYS_READ;
              break;                // Read
#ifdef ENABLE_TPDD_EXTENSIONS
            case OPEN_READ_WRITE:   // LaddieAlpha/VirtuaT extension
              entry = fs.open(directory, O_CREAT | O_RDWR);
              _sysstate = SYS_READ_WRITE;
              break;
#endif
          }
          remove_subdir();
        }
      }
    }

    if(fs.exists(directory)) { //If the file actually exists...
      led_sd_off();
      send_ret_normal(ERR_SUCCESS);  //...send a normal return with no error.
    } else {  //If the file doesn't exist...
      led_sd_off();
      send_ret_normal(ERR_NO_FILE);  //...send a normal return with a "file does not exist" error.
    }
  } else {  // wrong system state
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);  //...send a normal return with a "file does not exist" error.

  }
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  Technically, should only run from SYS_READ, SYS_WRITE, of
 *                or SYS_READ_WRITE, but we'll go ahead and be lenient.
 */
static void req_close() {  // Closes the currently open entry

  LOGD_P("%s() entry",__func__);
  entry.close();  //Close the entry
  led_sd_off();
  _sysstate = SYS_IDLE;
  send_ret_normal(ERR_SUCCESS);  //Normal return with no error
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  This can only run from SYS_READ or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
static void req_read(){  //Read a block of data from the currently open entry

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    led_sd_on();
    byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); //Try to pull 128 bytes from the file into the buffer
    led_sd_off();
    LOGV_P("A: %4X", entry.available());
    if(bytesRead > 0x00){  //Send the read return if there is data to be read
      send_byte(RET_READ);  //Return type
      send_byte(bytesRead); //Data length
      send_buffer(fileBuffer, bytesRead);
      send_chksum();
    } else { //send a normal return with an end-of-file error if there is no data left to read
      send_ret_normal(ERR_EOF);
    }
  } else if(_sysstate == SYS_WRITE) {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_FMT_MISMATCH);  // trying to read from a write/append file.
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_NAME);       // no file to reference
  }
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  This can only run from SYS_WRITE or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
static void req_write(){ //Write a block of data from the command to the currently open entry
  int32_t len;

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ_WRITE)) {
    led_sd_on();
    len = entry.write(_buffer, _length);
    led_sd_off();
    if(len == _length) {
      send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else if(len < 0) { // general error
      send_ret_normal(ERR_DATA_CRC);  // Pick someting for general error
    } else { // didn't store as many bytes as received.
      send_ret_normal(ERR_SEC_NUM);   // Send Sector Number Error (bogus, but...)
    }
  } else if(_sysstate == SYS_READ) {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_FMT_MISMATCH); // trying to write a file opened for reading.
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_NAME); // no file to reference
  }
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
static void req_delete(){  //Delete the currently open entry

  LOGD_P("%s() entry",__func__);

  if(_sysstate == SYS_REF) {
    led_sd_on();
    entry.close();  //Close any open entries
    append_dir(refFileNameNoDir);  //Push the reference name onto the directory buffer
    entry = fs.open(directory, FILE_READ);  //directory can be deleted if opened "READ"

    if(DME && entry.isDirectory()){
      entry.rmdir();  //If we're in DME mode and the entry is a directory, delete it
    }else{
      entry.close();  //Files can be deleted if opened "WRITE", so it needs to be re-opened
      entry = fs.open(directory, FILE_WRITE);
      entry.remove();
    }
    led_sd_off();
    remove_subdir();
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);  //Send a normal return with no error
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);
  }
  LOGD_P("%s() exit",__func__);
}

static void ret_not_impl(void) {
  _sysstate = SYS_IDLE;
  send_ret_normal(ERR_SUCCESS);
}

static void req_format(){  //Not implemented

  LOGD_P("%s() entry",__func__);
  ret_not_impl();
  LOGD_P("%s() exit",__func__);
}

static void req_status(){  //Drive status

  LOGD_P("%s() entry",__func__);
  send_ret_normal(ERR_SUCCESS);
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can run from any state, and does not alter state
 */
static void req_condition(){

  LOGD_P("%s() entry",__func__);
  send_byte(RET_CONDITION);  //Return type (normal)
  send_byte(0x01);  //Data size (1)
  send_byte(ERR_SUCCESS); //Error code
  send_chksum(); //Checksum
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
static void req_rename(){  //Renames the currently open entry

  LOGD_P("%s() entry",__func__);

  if(_sysstate == SYS_REF) { // we have a file reference to use.

    append_dir(refFileNameNoDir);  //Push the current reference name onto the directory buffer

    led_sd_on();

    if(entry) entry.close(); //Close any currently open entries
    entry = fs.open(directory); //Open the entry
    if(entry.isDirectory()) append_dir("/"); //Append a slash to the end of the directory buffer if the reference is a sub-directory

    copy_dir();  //Copy the directory buffer to the scratchpad directory buffer
    remove_subdir();  //Strip the previous directory reference off of the directory buffer

    uint8_t i;
    for(i = 0; i < FILENAME_SZ;i++) {
      if(_buffer[i] == 0 || _buffer[i] == ' ')
        break;
      tempRefFileName[i] = _buffer[i];
    }
    tempRefFileName[i] = '\0'; //Terminate the temporary reference name with a null character

    if(DME && entry.isDirectory()){ //      !!!If the entry is a directory, we need to strip the ".<>" off of the new directory name
      if(strstr(tempRefFileName, ".<>") != 0x00){
        for(byte i=0x00; i<FILENAME_SZ; i++){
          if(tempRefFileName[i] == '.' || tempRefFileName[i] == '<' || tempRefFileName[i] == '>'){
            tempRefFileName[i]=0x00;
          }
        }
      }
    }

    append_dir(tempRefFileName);
    if(entry.isDirectory()) append_dir("/");

    LOGD(directory);
    LOGD(tempDirectory);
    fs.rename(tempDirectory,directory);  //Rename the entry

    remove_subdir();
    entry.close();

    led_sd_off();

    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
  } else { // wrong system state
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);    // No file to rename
  }
  LOGD_P("%s() exit",__func__);
}

/*
 * Extended Commands
 */
#ifdef ENABLE_TPDD_EXTENSIONS
static void req_seek(void) {
  uint32_t pos;

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    if((_length == 5)
        && (_buffer[OFFSET_SEEK_TYPE]) // > 0
        && (_buffer[OFFSET_SEEK_TYPE] < SEEKTYPE_MAX)
       ) {
      // handle seek
      pos = (_buffer[1]
            | (_buffer[2] << 8)
            | ((uint32_t)_buffer[3] << 16)
            | ((uint32_t)_buffer[4] << 24)
           );
      switch(_buffer[OFFSET_SEEK_TYPE]) {
      case SEEKTYPE_SET:
        entry.seek(pos);
        break;
      case SEEKTYPE_CUR:
        entry.seekCur(pos);
        break;
      case SEEKTYPE_END:
        entry.seekEnd(pos);
        break;
      }
      send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else {
      // return error
      send_ret_normal(ERR_PARM);
    }
  } else {
    send_ret_normal(ERR_NO_NAME);     // no file opened for reading or writing.

  }
  LOGD_P("%s() exit",__func__);
}


static void req_tell(void) {
  uint32_t pos;

  LOGD_P("%s() entry",__func__);
  // Only tell if you have a file open
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    pos = entry.curPosition();
    send_byte((uint8_t)pos);
    send_byte((uint8_t)(pos >> 8));
    send_byte((uint8_t)(pos >> 16));
    send_byte((uint8_t)(pos >> 24));
    send_chksum();
  } else {
    send_ret_normal(ERR_NO_NAME);     // no file opened for reading or writing.
  }
  LOGD_P("%s() exit",__func__);
}
#endif

/*
 *
 * TS-DOS DME Commands
 *
 */

/*
 * System State: This can run from any state, and does not alter state
 */
static void req_dme_label() {  //Send the dmeLabel

  LOGD_P("%s() entry",__func__);
  LOGD_P("dmeLabel[%s]", dmeLabel);

  /* as per
   * http://bitchin100.com/wiki/index.php?title=Desklink/TS-DOS_Directory_Access#TPDD_Service_discovery
   * The mere inclusion of this command implies Directory Mode Extensions, so enable
   */
  DME = true;
  // prepend "/" to the root dir label just because my janky-ass set_label() assumes it
  if (directoryDepth > 0) set_label(directory); else set_label("/SD:   ");
  send_byte(RET_NORMAL);
  send_byte(0x0B);
  send_byte(0x20);
  for (byte i=0x00 ; i<0x06 ; i++) send_byte(dmeLabel[i]);
  send_dir_suffix();
  send_byte(' ');
  send_chksum();
  LOGD_P("%s() exit",__func__);
}

/*
 * Unknown commands
 */

#ifdef ENABLE_TPDD_EXTENSIONS
//  https://www.mail-archive.com/m100@lists.bitchin100.com/msg11247.html
static void req_unknown_1(void) {

  LOGD_P("%s() entry",__func__);
  send_byte(RET_TSDOS_UNK_1);
  send_byte(0x01);  //Data size (1)
  send_byte(ERR_SUCCESS);
  send_chksum();
  LOGD_P("%s() exit",__func__);
}

static void req_unknown_2(void) {
  const uint8_t data[] = {0x41, 0x10, 0x01, 0x00, 0x50, 0x05, 0x00, 0x02,
                          0x00, 0x28, 0x00, 0xE1, 0x00, 0x00, 0x00
                         };


  LOGD_P("%s() entry",__func__);
  send_byte(RET_UNKNOWN_2);
  send_byte(sizeof(data));
  send_buffer((uint8_t *)data, sizeof(data));
  send_chksum();
  LOGD_P("%s() exit",__func__);
}
#endif

void tpdd_scan(void) {
  cmdstate_t state = IDLE;
  uint8_t i = 0;
  uint8_t data;
  uint8_t cmd = 0; // make the compiler happy
  uint8_t chk = 0;
  unsigned long idleSince = 0;


  LOGD_P("%s() entry",__func__);
  dtr_ready();
  while(true) {
  #if defined(ENABLE_SLEEP)
    sleepNow();
  #endif // ENABLE_SLEEP
    if(millis() - idleSince > 500) { // interval between cmd element > ./5s
      state = IDLE; // go back to IDLE state.
      idleSince = millis();
    }
    // should check for a timeout...
    while(CLIENT.available()) {
      idleSince = millis();  // reset timer.
      data = (uint8_t)CLIENT.read();
      LOGV_P("S:%2.2d|I:%3.3d|D:%2.2x(%c)", state, i, data, (data > 0x20 && data < 0x7f ? data : ' '));
      // handle this one here, since if someone does ZM, the M should be handled
      // as a new command, same with Za or ZR
      if(state == FOUND_Z) {
        if(data == 'Z') {
          state = FOUND_OPCODE;
        } else
          state = IDLE;
      }
      if(!((state == FOUND_OPCODE) && (data == 'Z'))) { // if the above did not match
        switch (state) {
        case IDLE:
          switch(data) {
          case 'Z':
            state = FOUND_Z;
            break;
          case 'M':
            state = FOUND_MODE;
            break;
          case 'R':
            state = IDLE;  // should be FOUND_R;
            break;
          default:
            //if(data >= 'a' && data <= 'z') {
              // VirtualT goes into command line mode here...
              // TODO This is where one would add the WifiModem stuff..
            //}
            break;
          }
          break;
        case FOUND_Z:
          // this is handled above.
          break;
        case FOUND_OPCODE:
          cmd = data;
          chk = data;
          state = FOUND_CMD;
          break;
        case FOUND_CMD:
          _length = data;
          chk += data;
          i = 0;
          if(_length)
            state = FOUND_LEN;
          else
            state = FOUND_DATA;
          break;
        case FOUND_LEN:
          if(i < _length) {
            chk += data;
            _buffer[i++] = data;
          }
          if(i == _length) {  // cmd is complete.  get checksum
            state = FOUND_DATA;
          }
          break;
        case FOUND_DATA: // got checksum.  Check and exec
          if ((chk ^ 0xff) == data) {
            LOGV_P("T:%2.2X|L:%2.2X|%c", cmd, _length, (DME ? 'D' : '.'));
            switch(cmd){  // Select the command handler routine to jump to based on the command type
              case CMD_REFERENCE:   req_reference(); break;
              case CMD_OPEN:        req_open(); break;
              case CMD_CLOSE:       req_close(); break;
              case CMD_READ:        req_read(); break;
              case CMD_WRITE:       req_write(); break;
              case CMD_DELETE:      req_delete(); break;
              case CMD_FORMAT:      req_format(); break;
              case CMD_STATUS:      req_status(); break;
              case CMD_DMEREQ:      req_dme_label(); break; // DME Command
              case CMD_CONDITION:   req_condition(); break;
              case CMD_RENAME:      req_rename(); break;
#ifdef ENABLE_TPDD_EXTENSIONS
              case CMD_SEEK_EXT:    req_seek(); break;
              case CMD_TELL_EXT:    req_tell(); break;
              case CMD_TSDOS_UNK_1: req_unknown_1(); break;
              case CMD_TSDOS_UNK_2: req_unknown_2(); break;
#endif
              default:              send_ret_normal(ERR_PARM); break;  // Send a normal return with a parameter error if the command is not implemented
            }
          } else {
            LOGW_P("Checksum Error: calc(%2.2X) != sent(%2.2X)", chk ^ 0xff, data);
            send_ret_normal(ERR_ID_CRC);  // send back checksum error
          }
          state = IDLE;
          break;
        case FOUND_MODE:
          // 1 = operational mode
          // 0 = FDC emulation mode
          // (ignore for now).
          state = FOUND_MODE_DATA;
          break;
        case FOUND_MODE_DATA:
          // eat the 0x0d that is sent after it.
          _sysstate = SYS_IDLE;
          state = IDLE;
          break;
        default:
          // not sure how you'd get here, but...
          _sysstate = SYS_IDLE;
          state = IDLE;
          break;
        }
      }
    }
  }
}
