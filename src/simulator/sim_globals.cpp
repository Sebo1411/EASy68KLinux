#include "var.h"
#include "simio.h"

// Hardware I/O Locations
int seg7loc = 0xE00000;
int LEDloc = 0xE00010;
int switchLoc = 0xE00012;
int pbLoc = 0xE00014;

// Memory Map Settings
bool ROMMap = false;
int ROMStart = 0;
int ROMEnd = 0;

bool ReadMap = false;
int ReadStart = 0;
int ReadEnd = 0;

bool ProtectedMap = false;
int ProtectedStart = 0;
int ProtectedEnd = 0;

bool InvalidMap = false;
int InvalidStart = 0;
int InvalidEnd = 0;

void windowLine()
{
    if (simIO) {
        simIO->doCRLF();
    }
}

void scrollWindow()
{
    if (simIO) {
        simIO->doCRLF();
    }
}
