#include <stdio.h>
#include <iostream>
#include <functional>
#include "extern.h"
#include "proto.h"
#include "simio.h"

extern bool inputMode;
extern bool disableKeyCommands;

static std::function<void()> g_regsCallback = nullptr;

void setRegsUpdateCallback(std::function<void()> cb) {
    g_regsCallback = cb;
}

void initSim()                   // initialization for the simulator
{
  int	i;

  autoTraceInProgress = false;
  simIO->clear();               // clear output window
  simIO->setWindowSize(0,0);    // reset output window size
  simIO->setDrawingMode(4);     // set default pen mode
  simIO->setDrawingMode(16);    // turn off double buffering
  simIO->setPenWidth(1);        // set default pen width

  inputMode = false;
  pendingKey = 0;               // clear pendingKey

  runMode = false;
  lbuf[0] = '\0';		// initialize to prevent memory access violations
  wordptr[0] = lbuf;
  for (i = 0; i <= 7; i++)
    A[i] = D[i] = 0;
  cycles = 0;
  SR = 0x2000;
  A[7] = 0x00FF0000;            // user stack
  A[8] = 0x01000000;            // supervisor stack
  OLD_PC = -1;                  // set different from 'PC' and 'cycles'
  trace = sstep = false;
  stopInstruction = false;
  halt = false;
  stepToAddr = 0;               // clear step
  runToAddr = 0;                // clear runTo
  scrshow();
  keyboardEcho = true;          // true, EASy68K input is echoed (default)
  inputPrompt = true;           // true, display prompt during input (default)
  inputLFdisplay = true;

  for (i=0; i<MAXFILES; i++) {  // clear file structures
    files[i].fp = NULL;
  }

  irq = 0;                      // reset IRQ flags
  hardReset = false;

  FullScreenMonitor = 0;

  mouseX = 0;
  mouseY = 0;
  mouseLeft = false;
  mouseRight = false;
  mouseMiddle = false;
  mouseDouble = false;
  keyShift = false;
  keyAlt = false;
  keyCtrl =false;

  mouseXUp = 0;
  mouseYUp = 0;
  mouseLeftUp = false;
  mouseRightUp = false;
  mouseMiddleUp = false;
  mouseDoubleUp = false;
  keyShiftUp = false;
  keyAltUp = false;
  keyCtrlUp = false;

  mouseXDown = 0;
  mouseYDown = 0;
  mouseLeftDown = false;
  mouseRightDown = false;
  mouseMiddleDown = false;
  mouseDoubleDown = false;
  keyShiftDown = false;
  keyAltDown = false;
  keyCtrlDown = false;
  mouseDownIRQ = 0;
  mouseUpIRQ = 0;
  mouseMoveIRQ = 0;
  keyDownIRQ = 0;
  keyUpIRQ = 0;

  simIO->ResetSounds(); // stop all playing sounds and clear sound memory
}


void scrshow() 	        // update register display
{
  if (g_regsCallback) {
    g_regsCallback();
  }
}
