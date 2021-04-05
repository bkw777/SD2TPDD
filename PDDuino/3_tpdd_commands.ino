
// Sends a normal return to the TPDD port with error code errorCode
void return_normal(byte errorCode){ 
  DEBUG_PRINTL(F("return_normal()"));
#if DEBUG > 1
  DEBUG_PRINT("R:Norm ");
  DEBUG_PRINTIL(errorCode, HEX);
#endif
  tpddWrite(RET_NORMAL);  // Return type (normal)
  tpddWrite(0x01);  // Data size (1)
  tpddWrite(errorCode); // Error code
  tpddSendChecksum(); // Checksum
}

// Sends a reference return to the TPDD port
void returnReference(char *name, bool isDir, uint16_t size ) {
  uint8_t i, j;

  DEBUG_PRINTL(F("returnReference()"));

  tpddWrite(RET_DIRECTORY);    //Return type (reference)
  tpddWrite(0x1C);    //Data size (1C)
  if(name == NULL) {
    for(i = 0; i < FILENAME_SZ; i++)
      tpddWrite(0x00);  //Write the reference file name to the TPDD port
  } else {
    if(isDir && DME) { // handle dirname.
      for(i = 0; (i < 6) && (name[i] != 0); i++)
        tpddWrite(name[i]);
      for(;i < 6; i++)
        tpddWrite(' '); // pad out the dir
      tpddWrite('.');  //Tack the expected ".<>" to the end of the name
      tpddWrite('<');
      tpddWrite('>');
      j = 9;
    } else {
      for(i = 0; (i < 6) && (name[i] != '.'); i++) {
        tpddWrite(name[i]);
      }
      for(j = i; j < 6; j++) {
        tpddWrite(' ');
      }
      for(; j < FILENAME_SZ && (name[i] != 0); j++) {
        tpddWrite(name[i++]);  // send the file extension
      }
    }
    for(; j < FILENAME_SZ; j++) {
      tpddWrite(0);  // pad out
    }
  }
  tpddWrite(0);  //Attribute, unused
  tpddWrite((uint8_t)(size >> 8));  //File size most significant byte
  tpddWrite((uint8_t)(size & 0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

}

void return_reference() {
  DEBUG_PRINTL(F("return_reference()"));
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(tempRefFileName, entry.isDirectory(), entry.fileSize());
#if DEBUG > 1
  DEBUG_PRINTL("R:Ref");
#endif
}

// Sends a blank reference return to the TPDD port
void return_blank_reference() {
  DEBUG_PRINTL(F("return_blank_reference()"));
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(NULL, false, 0);
#if DEBUG > 1
  DEBUG_PRINTL("R:BRef");
#endif
}

void return_parent_reference(){
  DEBUG_PRINTL(F("return_parent_reference()"));
  returnReference("PARENT", true, 0);
}

/*
 *
 * TPDD Port command handler routines
 *
 */

void command_reference(){ // Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+0x1D)];  // The search form byte exists 29 bytes into the command
  byte refIndex = 0x00;  // Reference file name index

  DEBUG_PRINTL(F("command_reference()"));

#if DEBUG > 1
  DEBUG_PRINT("SF:");
  DEBUG_PRINTIL(searchForm,HEX);
#endif

  if(searchForm == 0x00){ // Request entry by name
    for(byte i=0x04; i<0x1C; i++){  // Put the reference file name into a buffer
      if(dataBuffer[(tail+i)]!=0x20){ // If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=dataBuffer[(tail+i)]; // write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex]=0x00; // Terminate the file name buffer with a null character

#if DEBUG > 1
    DEBUG_PRINT("Ref: ");
    DEBUG_PRINTL(refFileName);
#endif

    if(DME){  //        !!!Strips the ".<>" off of the reference name if we're in DME mode
      if(strstr(refFileName, ".<>") != 0x00){
        for(byte i=0x00; i<FILENAME_SZ; i++){  // Copies the reference file name to a scratchpad buffer with no directory extension if the reference is for a directory
          if(refFileName[i] != '.' && refFileName[i] != '<' && refFileName[i] != '>'){
            refFileNameNoDir[i]=refFileName[i];
          }else{
            refFileNameNoDir[i]=0x00; // If the character is part of a directory extension, don't copy it
          }
        }
      }else{
        for(byte i=0x00; i<FILENAME_SZ; i++) refFileNameNoDir[i]=refFileName[i]; // Copy the reference directly to the scratchpad buffer if it's not a directory reference
      }
    }

    directoryAppend(refFileNameNoDir);  // Add the reference to the directory buffer

    SD_LED_ON
    if(SD.exists(directory)){ // If the file or directory exists on the SD card...
      entry=SD.open(directory); // ...open it...
      return_reference(); // send a refernce return to the TPDD port with its info...
      entry.close();  // ...close the entry
    }else{  // If the file does not exist...
      return_blank_reference();
    }

    upDirectory();  // Strip the reference off of the directory buffer

  }else if(searchForm == 0x01){ // Request first directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openFirst();
  }else if(searchForm == 0x02){ // Request next directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openNext();
  }else{  // Parameter is invalid
    return_normal(ERR_PARM);  // Send a normal return to the TPDD port with a parameter error
  }
  SD_LED_OFF
}

void ref_openFirst(){
  DEBUG_PRINTL(F("ref_openFirst()"));
  directoryBlock = 0x00; // Set the current directory entry index to 0
  if(DME && directoryDepth>0x00 && directoryBlock==0x00){ // Return the "PARENT.<>" reference if we're in DME mode
    SD_LED_OFF
    return_parent_reference();
  }else{
    ref_openNext();    // otherwise we just return the next reference
  }
}

void ref_openNext(){
  DEBUG_PRINTL(F("ref_openNext()"));
  directoryBlock++; // Increment the directory entry index
  SD_LED_ON
  root.rewindDirectory(); // Pull back to the begining of the directory
  for(byte i=0x00; i<directoryBlock-0x01; i++) root.openNextFile();  // skip to the current entry offset by the index

  entry = root.openNextFile();  // Open the entry

  if(entry){  // If the entry exists it is returned
    if(entry.isDirectory() && !DME){  // If it's a directory and we're not in DME mode
      entry.close();  // the entry is skipped over
      ref_openNext(); // and this function is called again
    }

    return_reference(); // Send the reference info to the TPDD port
    entry.close();  // Close the entry
    SD_LED_OFF
  }else{
    SD_LED_OFF
    return_blank_reference();
  }
}

void command_open(){  // Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+0x04)];  // The access mode is stored in the 5th byte of the command
  DEBUG_PRINTL(F("command_open()"));
  entry.close();

  if(DME && strcmp(refFileNameNoDir, "PARENT") == 0x00){ // If DME mode is enabled and the reference is for the "PARENT" directory
    upDirectory();  // The top-most entry in the directory buffer is taken away
    directoryDepth--; // and the directory depth index is decremented
  }else{
    directoryAppend(refFileNameNoDir);  // Push the reference name onto the directory buffer
    SD_LED_ON
    if(DME && strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ // If the reference is for a directory and the directory buffer points to a directory that does not exist
      SD.mkdir(directory);  // create the directory
      upDirectory();
    }else{
      entry=SD.open(directory); // Open the directory to reference the entry
      if(entry.isDirectory()){  //      !!!Moves into a sub-directory
        entry.close();  // If the entry is a directory
        directoryAppend("/"); // append a slash to the directory buffer
        directoryDepth++; // and increment the directory depth index
      }else{  // If the reference isn't a sub-directory, it's a file
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

  if(SD.exists(directory)){ // If the file actually exists...
    SD_LED_OFF
    return_normal(ERR_SUCCESS);  // ...send a normal return with no error.
  }else{  // If the file doesn't exist...
    SD_LED_OFF
    return_normal(ERR_NOFILE);  // ...send a normal return with a "file does not exist" error.
  }
}

void command_close(){ // Closes the currently open entry
  DEBUG_PRINTL(F("command_close()"));
  entry.close();  // Close the entry
  SD_LED_OFF
  return_normal(ERR_SUCCESS);  // Normal return with no error
}

void command_read(){  // Read a block of data from the currently open entry
  DEBUG_PRINTL(F("command_read()"));
  SD_LED_ON
  byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); // Try to pull 128 bytes from the file into the buffer
  SD_LED_OFF
#if DEBUG > 1
  DEBUG_PRINT("A: ");
  DEBUG_PRINTIL(entry.available(),HEX);
#endif
  if(bytesRead > 0x00){  // Send the read return if there is data to be read
    tpddWrite(RET_READ);  // Return type
    tpddWrite(bytesRead); // Data length
    for(byte i=0x00; i<bytesRead; i++) tpddWrite(fileBuffer[i]);
    tpddSendChecksum();
  }else{
    return_normal(ERR_EOF);  // send a normal return with an end-of-file error if there is no data left to read
  }
}

void command_write(){ // Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+0x03)];

  DEBUG_PRINTL(F("command_write()"));
  SD_LED_ON
  for(byte i=0x00; i<commandDataLength; i++) entry.write(dataBuffer[(byte)(tail+0x04+i)]);
  SD_LED_OFF
  return_normal(ERR_SUCCESS);  // Send a normal return to the TPDD port with no error
}

void command_delete(){  // Delete the currently open entry
  DEBUG_PRINTL(F("command_delete()"));
  SD_LED_ON
  entry.close();  // Close any open entries
  directoryAppend(refFileNameNoDir);  // Push the reference name onto the directory buffer
  entry = SD.open(directory, FILE_READ);  // directory can be deleted if opened "READ"

  if(DME && entry.isDirectory()){
    entry.rmdir();  // If we're in DME mode and the entry is a directory, delete it
  }else{
    entry.close();  // Files can be deleted if opened "WRITE", so it needs to be re-opened
    entry = SD.open(directory, O_WRITE);
    entry.remove();
  }
  SD_LED_OFF
  upDirectory();
  return_normal(ERR_SUCCESS);  // Send a normal return with no error
}

void command_format(){  // Not implemented
  DEBUG_PRINTL(F("command_format()"));
  return_normal(ERR_SUCCESS);
}

void command_status(){  // Drive status
  DEBUG_PRINTL(F("command_status()"));
  return_normal(ERR_SUCCESS);
}

void command_condition(){ // Not implemented
  DEBUG_PRINTL(F("command_condition()"));
  return_normal(ERR_SUCCESS);
}

void command_rename(){  // Renames the currently open entry
  byte refIndex = 0x00;  // Temporary index for the reference name

  DEBUG_PRINTL(F("command_rename()"));

  directoryAppend(refFileNameNoDir);  // Push the current reference name onto the directory buffer

  SD_LED_ON

  if(entry) entry.close(); // Close any currently open entries
  entry = SD.open(directory); // Open the entry
  if(entry.isDirectory()) directoryAppend("/"); // Append a slash to the end of the directory buffer if the reference is a sub-directory

  copyDirectory();  // Copy the directory buffer to the scratchpad directory buffer
  upDirectory();  // Strip the previous directory reference off of the directory buffer

  for(byte i=0x04; i<0x1C; i++){  // Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ // If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; // ...copy the character to the temporary reference name and increment the pointer.
      }
  }

  tempRefFileName[refIndex]=0x00; // Terminate the temporary reference name with a null character

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
  SD.rename(tempDirectory,directory);  // Rename the entry

  upDirectory();
  entry.close();

  SD_LED_OFF

  return_normal(ERR_SUCCESS);  // Send a normal return to the TPDD port with no error
}

/*
 *
 * TS-DOS DME Commands
 *
 */

void command_DMEreq() {  // Send the dmeLabel

  DEBUG_PRINT(F("command_DMEReq(): dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));

  if(DME){
    if (directoryDepth>0x00) setLabel(directory); else setLabel(ROOT_DME_LABEL);
    tpddWrite(RET_NORMAL);
    tpddWrite(0x0B);
    tpddWrite(0x20);
    for (byte i=0x00 ; i<0x06 ; i++) tpddWrite(dmeLabel[i]);
    tpddWrite('.');
    tpddWrite('<');
    tpddWrite('>');
    tpddWrite(0x20);
    tpddSendChecksum();
  }else{
    return_normal(ERR_PARM);
  }
}
