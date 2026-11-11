#include "simio.h"

static CliSimIO defaultCliSimIO;
SimIO *simIO = &defaultCliSimIO;

void setSimIO(SimIO *io) {
    if (io) {
        simIO = io;
    } else {
        simIO = &defaultCliSimIO;
    }
}
