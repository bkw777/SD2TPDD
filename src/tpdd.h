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
 *  tpdd.h: Portable Disk Drive emulation layer definitions and low level functions
 *
 */

#ifndef TPDD_H
#define TPDD_H

#define DIRECTORY_SZ 0x40       // size of directory[] which holds full paths
#define FILENAME_SZ 0x18        // TPDD protocol spec 1C, minus 4 for ".<>"+NULL
#define DATA_BUFFER_SZ 0x0100   // 256 bytes, 2 full tpdd packets? 0xFF did not work.
#define FILE_BUFFER_SZ 0x80     // 128 bytes at at time to/from files

#define ENABLE_TPDD_EXTENSIONS  // If you want to enable the extensions

typedef enum command_e {
  CMD_REFERENCE =     0x00,
  CMD_OPEN =          0x01,
  CMD_CLOSE =         0x02,
  CMD_READ =          0x03,
  CMD_WRITE =         0x04,
  CMD_DELETE =        0x05,
  CMD_FORMAT =        0x06,
  CMD_STATUS =        0x07,
  CMD_DMEREQ =        0x08,
#ifdef ENABLE_TPDD_EXTENSIONS
  CMD_SEEK_EXT =      0x09,
  CMD_TELL_EXT =      0x0a,
#endif
  CMD_SET_EXT =       0x0b,
  CMD_CONDITION =     0x0c,
  CMD_RENAME =        0x0d,
  CMD_REQ_QUERY_EXT = 0x0e,
  CMD_COND_LIST =     0x0f,

  RET_READ =          0x10,
  RET_DIRECTORY =     0x11,
  RET_NORMAL =        0x12,
#ifdef ENABLE_TPDD_EXTENSIONS
  RET_UNKNOWN_2 =     0x14,
#endif
  RET_CONDITION =     0x15,
#ifdef ENABLE_TPDD_EXTENSIONS
  CMD_TSDOS_UNK_2 =   0x23,
  CMD_UNKNOWN_48 =    0x30, // https://www.mail-archive.com/m100@lists.bitchin100.com/msg12129.html
  CMD_TSDOS_UNK_1 =   0x31, // https://www.mail-archive.com/m100@lists.bitchin100.com/msg11244.html
  RET_TSDOS_UNK_1 =   0x38
#endif
} command_t;

typedef enum error_e {
  ERR_SUCCESS =       0x00,
  ERR_NO_FILE =       0x10,
  ERR_EXISTS =        0x11,
  ERR_NO_NAME =       0x30, // command.tdd calls this EOF
  ERR_DIR_SEARCH =    0x31,
  ERR_BANK =          0x35,
  ERR_PARM =          0x36,
  ERR_FMT_MISMATCH =  0x37,
  ERR_EOF =           0x3f,
  ERR_NO_START =      0x40, // command.tdd calls this IO ERROR (64) (empty name error)
  ERR_ID_CRC =        0x41,
  ERR_SEC_LEN =       0x42,
  ERR_FMT_VERIFY =    0x44,
  ERR_FMT_INTRPT =    0x46,
  ERR_ERASE_OFFSET =  0x47,
  ERR_DATA_CRC =      0x49,
  ERR_SEC_NUM =       0x4a,
  ERR_READ_TIMEOUT =  0x4b,
  ERR_SEC_NUM2 =      0x4d,
  ERR_WRITE_PROTECT = 0x50, // writing to a locked file
  ERR_DISK_NOINIT =   0x5e,
  ERR_DIR_FULL =      0x60, // command.tdd calls this disk full, as does the SW manual
  ERR_DISK_FULL =     0x61,
  ERR_FILE_LEN =      0x6e,
  ERR_NO_DISK =       0x70,
  ERR_DISK_CHG =      0x71
} error_t;

typedef enum openmode_e {
  OPEN_NONE =         0,
  OPEN_WRITE =        1,
  OPEN_APPEND =       2,
  OPEN_READ =         3,
#ifdef ENABLE_TPDD_EXTENSIONS
  OPEN_READ_WRITE =   4, // VirtualT defines.
  OPEN_CREATE =       5, // LaddieAlpha defines.
#endif
} openmode_t;

typedef enum enumtype_e {
  ENUM_PICK =         0,
  ENUM_FIRST =        1,
  ENUM_NEXT =         2,
  ENUM_PREV =         3,
  ENUM_DONE =         4
} enumtype_t;

typedef enum seektype_e {
  SEEKTYPE_SET =      1,
  SEEKTYPE_CUR =      2,
  SEEKTYPE_END =      3
} seektype_t;

#define SEEKTYPE_MAX  (SEEKTYPE_END + 1)

#define OFFSET_SEEK_TYPE      0x00 // TODO check this

typedef enum sysstate_e {
  SYS_IDLE,
  SYS_ENUM,
  SYS_REF,
  SYS_WRITE,
  SYS_READ_WRITE,
  SYS_READ,
//SYS_PUSH_FILE,  // LaddieAlpha defines, but no functionality
//SYS_PULL_FILE,
//SYS_PAUSED
} sysstate_t;

// 0 = waiting for command, 1 = waiting for full command, 2 = have full command
typedef enum cmdstate_s {
  IDLE,
  CMD_STARTED,
  CMD_COMPLETE,
  FOUND_Z,
  FOUND_OPCODE,
  FOUND_CMD,
  FOUND_LEN,
  FOUND_DATA,
  FOUND_CHK,
  FOUND_MODE,
  FOUND_MODE_DATA,
  FOUND_R
} cmdstate_t;


  void tpdd_scan(void);

#endif /* TPDD_H */

