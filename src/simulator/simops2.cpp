
/***************************** 68000 SIMULATOR ****************************

File Name: SIMOPS2.C

This file contains various routines for simulator operation

   Modified: Chuck Kelly
             Monroe County Community College
             http://www.monroeccc.edu/ckelly

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <iostream>
#include "extern.h"
#include "proto.h"
#include "simmessage.h"

extern int ROMStart, ROMEnd, ReadStart, ReadEnd;
extern int ProtectedStart, ProtectedEnd, InvalidStart, InvalidEnd;
extern bool ROMMap, ReadMap, ProtectedMap, InvalidMap;

#define	MODIFY_DATA	0
#define	MODIFY_MEMORY	1

//-----------------------------------------------------------------------
// Close all files
void closeFiles(short *result)
{
  *result = F_SUCCESS;
  for (int i=0; i<MAXFILES; i++) {  // clear file structures
    if (files[i].fp != NULL) {
      if(fclose(files[i].fp) == EOF)
        *result = F_ERROR;
      files[i].fp = NULL;
    }
  }
}

//-----------------------------------------------------------------------
// Other file operations.
// mode == 0, does file exist?
//   all other values reserved.
// result:
//    F_ERROR - file does not exist
//    F_SUCCESS - file exists and may be written to
//    F_READONLY - file exists but is read only
void fileOp(long *mode, char *fname, short *result)
{
  (void)mode;
  FILE *fp;
  *result = F_ERROR;                    // default to error
  fp = fopen(fname, "r+b");             // try to open file for update
  if (fp != NULL) {                     // if success
    *result = F_SUCCESS;
  } else {
    fp = fopen(fname, "rb");            // try to open file for read only
    if (fp != NULL) {                   // if success
      *result = F_READONLY;
    }
  }
  if (fp != NULL)                       // both fopen() calls may have failed
    fclose(fp);
}

//-----------------------------------------------------------------------
// Open existing file
void openFile(long *fn, char *fname, short *result)
{
  *result = F_ERROR;                    // default to error
  *fn = -1;                             // "
  for (int i=0; i<MAXFILES; i++) {      // find empty file pointer
    if (files[i].fp == NULL) {          // if this file structure is empty
      files[i].fp = fopen(fname, "r+b"); // try to open file for update
      if (files[i].fp != NULL) {        // if success
        snprintf(files[i].name, sizeof(files[i].name), "%s", fname);    // save name of file to file structure
        *result = F_SUCCESS;
        *fn = i;                        // file number is array position
        break;
      }
      files[i].fp = fopen(fname, "rb"); // try to open file for read only
      if (files[i].fp != NULL) {        // if success
        snprintf(files[i].name, sizeof(files[i].name), "%s", fname);    // save name of file to file structure
        *result = F_READONLY;
        *fn = i;                        // file number is array position
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------
// Open new file
void newFile(long *fn, char *fname, short *result)
{
  *result = F_ERROR;                    // default to error
  *fn = -1;                             // "
  for (int i=0; i<MAXFILES; i++) {      // find empty file pointer
    if (files[i].fp == NULL) {          // if this file structure is empty
      files[i].fp = fopen(fname, "w+b"); // open file
      if (files[i].fp != NULL) {        // if success
        snprintf(files[i].name, sizeof(files[i].name), "%s", fname);    // save name of file to file structure
        *result = F_SUCCESS;
        *fn = i;                        // file number is array position
        break;
      }
    }
  }
}

//-----------------------------------------------------------------------
// Read from file
void readFile(long fn, char *buf, unsigned int *size, short *result)
{
  *result = F_ERROR;            // default to error
  if (fn < 0 || fn >= MAXFILES) // if invalid file pointer
    return;
  if (files[fn].fp == NULL)     // if invalid file pointer
    return;

  unsigned int r = fread(buf, 1, *size, files[fn].fp);  // read from file

  if (r < 1) {                  // if error or EOF
    if (feof(files[fn].fp))
      *result = F_EOF;
    else
      *result = F_ERROR;
  } else {
    *result = F_SUCCESS;
    *size = r;                  // number of bytes read
  }

}

//-----------------------------------------------------------------------
// Write to file
void writeFile(long fn, char *buf, unsigned int size, short *result)
{
  *result = F_ERROR;            // default to error
  if (fn < 0 || fn >= MAXFILES) // if invalid file pointer
    return;
  if (files[fn].fp == NULL)     // if invalid file pointer
    return;

  unsigned int r = fwrite(buf, size, 1, files[fn].fp);

  if (r >= 1)                   // if success
    *result = F_SUCCESS;
}

//-----------------------------------------------------------------------
// Position file pointer
void positionFile(long fn, int offset, short *result)
{
  *result = F_ERROR;            // default to error
  if (fn < 0 || fn >= MAXFILES) // if invalid file pointer
    return;
  if (files[fn].fp == NULL)     // if invalid file pointer
    return;

  int r = fseek(files[fn].fp, offset, SEEK_SET);

  if (r == 0)                   // if success
    *result = F_SUCCESS;
}

//-----------------------------------------------------------------------
// Close file
void closeFile(long fn, short *result)
{
  *result = F_ERROR;            // default to error
  if (fn < 0 || fn >= MAXFILES) // if invalid file pointer
    return;
  if (files[fn].fp == NULL)     // if invalid file pointer
    return;

  int r = fclose(files[fn].fp);
  if (r == 0)
    *result = F_SUCCESS;
  files[fn].fp = NULL;          // clear file pointer
}

//-----------------------------------------------------------------------
// Delete file
void deleteFile(char *fname, short *result)
{
  *result = F_ERROR;            // default to error
  if (remove(fname) == 0)
    *result = F_SUCCESS;
}

//-----------------------------------------------------------------------
// Load S Record file
//                               /-version number
//          module name         / /-revision number                     checksum
//S0  0000 6 8 K P R O G       2 0 C R E A T E D   B Y   E A S Y 6 8 K /
//S021000036384B50524F47202020323043524541544544204259204541535936384B6D
//S0  0000             R O M   0 0 0 0 0 0 , 0 0 0 0 0 0cc
//S0  0000           R E A D   0 0 0 0 0 0 , 0 0 0 0 0 0cc
//S0  0000 P R O T E C T E D   0 0 0 0 0 0 , 0 0 0 0 0 0cc
//S0  0000     I N V A L I D   0 0 0 0 0 0 , 0 0 0 0 0 0cc
//S0nn0000
int loadSrec(char *name)        // load memory with contents of s_record file
{
  FILE *fp;
  int line = 0;
  bool end__of__file = false;
  std::string str;
  const std::string EASy68K_HEADER = "S0 = 68KPROG   20CREATED BY EASY68K";
  bool EASy68Kv2_0_SRecord = false;
  bool sRecError = false;

  try {
    fp = fopen(name, "rt");
    if (fp == NULL) {             // if file cannot be opened, print message
      simMessage("error: cannot open file %s", name);
      return FAILURE;
    }

    SRecordLineFSM fsm;

    while (fgets(lbuf, SREC_MAX, fp) != NULL) { // read file until end
      line++;
      if (!fsm.parseLine(lbuf)) {
        sRecError = true;
        break;
      }

      if (fsm.state != SRecordLineState::Complete) {
        continue;
      }

      if (!fsm.checksumValid) {
        simMessage("Checksum error on line %d...", line);
      }

      switch (fsm.recordType) {
        case '0': {
          str = "S0 = ";
          for (uint8_t b : fsm.payload) {
            if (b >= ' ' && b <= '~') {
              str += static_cast<char>(b);
            } else {
              str += '.';
            }
          }
          simMessage("%s", str.c_str());

          if (EASy68Kv2_0_SRecord) {
            if (str.length() >= 29) {
              std::string tag = str.substr(5, 9);
              if (tag == "      ROM") {
                ROMMap = true;
                ROMStart = std::strtoul(str.substr(15, 6).c_str(), nullptr, 16);
                ROMEnd = std::strtoul(str.substr(22, 6).c_str(), nullptr, 16);
              } else if (tag == "     READ") {
                ReadMap = true;
                ReadStart = std::strtoul(str.substr(15, 6).c_str(), nullptr, 16);
                ReadEnd = std::strtoul(str.substr(22, 6).c_str(), nullptr, 16);
              } else if (tag == "PROTECTED") {
                ProtectedMap = true;
                ProtectedStart = std::strtoul(str.substr(15, 6).c_str(), nullptr, 16);
                ProtectedEnd = std::strtoul(str.substr(22, 6).c_str(), nullptr, 16);
              } else if (tag == "  INVALID") {
                InvalidMap = true;
                InvalidStart = std::strtoul(str.substr(15, 6).c_str(), nullptr, 16);
                InvalidEnd = std::strtoul(str.substr(22, 6).c_str(), nullptr, 16);
              }
            }
          } else if (str == EASy68K_HEADER) {
            EASy68Kv2_0_SRecord = true;
          }
          str = "";
          break;
        }

        case '1':
        case '2':
        case '3': {
          uint32_t loc = fsm.address;
          for (size_t i = 0; i < fsm.payload.size(); ++i) {
            if (loc >= MEMSIZE) {
              simMessage("Invalid Address on line %d...", line);
              sRecError = true;
              break;
            }
            memory[loc++ & ADDRMASK] = static_cast<char>(fsm.payload[i]);
          }
          break;
        }

        case '5':
          // Ignore length count record
          break;

        case '7':
        case '8':
        case '9': {
          PC = static_cast<long>(fsm.address);
          end__of__file = true;
          break;
        }

        default:
          sRecError = true;
          break;
      }

      if (end__of__file || sRecError) break;
    }

    if (sRecError) {
      simMessage("Invalid data on line %d of .S68 file...", line);
      simMessage("%d: %s", line, lbuf);
      simMessage("Remainder of load stopped...");
      fclose(fp);
      return FAILURE;
    } else {
      simMessage(".S68 file read successful");
    }
    fclose(fp);
    return SUCCESS;
  }
  catch( ... ) {
    simMessage("Unexpected Error reading .S68 file");
    return FAILURE;
  }
}


