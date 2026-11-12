#ifndef asmH
#define asmH

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <algorithm>

using std::string;
typedef std::string AnsiString;
typedef std::string String;

inline std::string IntToHex(unsigned int val, int digits = 8) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%0*X", digits, val);
    return std::string(buf);
}

#define stricmp   strcasecmp
#define strcmpi   strcasecmp
#define strnicmp  strncasecmp
#define strnicmpi strncasecmp

void REMOVECR(char *line);

#define isTerm(c)   (c == ',' || c == '/' || c == '-' || isspace((unsigned char)c) || !c || c == '{')
#define isRegNum(c) ((c >= '0') && (c <= '7'))

#ifndef EASY68K_VERSION_STRING
#define EASY68K_VERSION_STRING "5.17.0"
#endif

#define VERSION EASY68K_VERSION_STRING
#define TITLE "EASy68K Assembler v" EASY68K_VERSION_STRING

/* Status values */
#define OK                    0x00
#define SEVERE                0x400
#define SYNTAX                0x401
#define INV_OPCODE            0x402
#define INV_ADDR_MODE         0x403
#define LABEL_REQUIRED        0x404
#define NO_ENDM               0x405
#define TOO_MANY_ARGS         0x406
#define INVALID_ARG           0x407
#define COMMA_EXPECTED        0x408
#define PHASE_ERROR           0x409
#define FILE_ERROR            0x40A
#define MACRO_NEST            0x40B
#define NO_IF                 0x40C
#define NO_WHILE              0x40D
#define NO_REPEAT             0x40E
#define NO_FOR                0x40F
#define ENDI_EXPECTED         0x410
#define ENDW_EXPECTED         0x411
#define ENDF_EXPECTED         0x412
#define REPEAT_EXPECTED       0x413
#define LABEL_ERROR           0x414
#define NO_DBLOOP             0x415
#define DBLOOP_EXPECTED       0x416
#define BAD_BITFIELD          0x417
#define ILLEGAL_SYMBOL        0x418
#define EXCEPTION             0x999
#define ERRORN                0x300
#define UNDEFINED             0x301
#define DIV_BY_ZERO           0x302
#define MULTIPLE_DEFS         0x303
#define REG_MULT_DEFS         0x304
#define REG_LIST_UNDEF        0x305
#define INV_FORWARD_REF       0x306
#define INV_LENGTH            0x307
#define MINOR                 0x200
#define INV_SIZE_CODE         0x201
#define INV_QUICK_CONST       0x202
#define INV_MOVE_QUICK_CONST  0x203
#define INV_VECTOR_NUM        0x204
#define INV_BRANCH_DISP       0x205
#define INV_DISP              0x206
#define INV_ABS_ADDRESS       0x207
#define INV_8_BIT_DATA        0x208
#define INV_16_BIT_DATA       0x209
#define NOT_REG_LIST          0x20A
#define REG_LIST_SPEC         0x20B
#define INV_SHIFT_COUNT       0x20C
#define INV_OPERATOR          0x20D
#define FAIL_ERROR            0x20E
#define WARNING               0x100
#define ASCII_TOO_BIG         0x101
#define NUMBER_TOO_BIG        0x102
#define INCOMPLETE            0x103
#define FORCING_SHORT         0x104
#define ODD_ADDRESS           0x105
#define END_MISSING           0x106
#define ADDRESS_MISSING       0x107
#define THEN_EXPECTED         0x108
#define DO_EXPECTED           0x109
#define FORWARD_REF           0x10A
#define LABEL_TOO_LONG        0x10B
#define SEVERITY              0xF00
#define BACKREF               0x01
#define REDEFINABLE           0x02
#define REG_LIST_SYM          0x04
#define MACRO_SYM             0x08
#define DS_SYM                0x10
#define IMMEDIATE             0x00800
#define BYTE_SIZE             1
#define WORD_SIZE             2
#define LONG_SIZE             4
#define SHORT_SIZE            8
#define MEM_SIZE              0x00FFFFFF
#define NORMAL                0
#define MILD_ERROR            1
#define CRITICAL              2
#define TAB1                  12
#define TAB2                  20
#define TAB3                  44
#define MACRO_NEST_LIMIT      256

#define NEWERROR(var, code)   var = ((code & SEVERITY) > var) ? code : var

#define SIGCHARS 33
#define MAX_ARGS 36
#define ARG_SIZE 256

struct opDescriptor {
  int  mode;
  int  data;
  int  field;
  char reg;
  char index;
  char size;
  bool backRef;
};

typedef struct symbolEntry {
  int value;
  struct symbolEntry *next;
  char flags;
  char name[SIGCHARS+1];
} symbolDef;

typedef struct {
  int source, dest;
  char sizes;
  int (*exec)(int, int, opDescriptor *, opDescriptor *, int *);
  unsigned short int bytemask, wordmask, longmask;
} flavor;

typedef struct {
  const char *mnemonic;
  flavor *flavorPtr;
  char flavorCount;
  bool parseFlag;
  int (*exec)(int, char *, char *, int *);
} instruction;

#define DnDirect               0x00001
#define AnDirect               0x00002
#define AnInd                  0x00004
#define AnIndPost              0x00008
#define AnIndPre               0x00010
#define AnIndDisp              0x00020
#define AnIndIndex             0x00040
#define AbsShort               0x0080
#define AbsLong                0x0100
#define PCDisp                 0x0200
#define PCIndex                0x0400
#define SRDirect               0x01000
#define CCRDirect              0x02000
#define USPDirect              0x04000
#define SFCDirect              0x08000
#define DFCDirect              0x10000
#define VBRDirect              0x20000

#define ControlAlt  (AnInd | AnIndDisp | AnIndIndex | AbsShort | AbsLong)
#define NEW_PAGE_MARKER "<------------------------------ PAGE ------------------------------>"

enum tabTypes { Assembly, Fixed };

typedef uint32_t TColor;
typedef struct {
  TColor color;
  bool   bold;
  bool   italic;
  bool   underline;
} FontStyle;

extern char currentFile[256];
extern char *currentCapLine;

extern bool listFlag;
extern bool objFlag;
extern bool CEXflag;
extern bool BITflag;
extern bool CREflag;
extern bool MEXflag;
extern bool SEXflag;
extern bool WARflag;
extern bool noFileName;

struct AsmMessage {
  int line;
  std::string text;
  std::string file;
  bool isWarning;
};
extern std::vector<AsmMessage> g_asmMessages;

#include "proto.h"

#endif
