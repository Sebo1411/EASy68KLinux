#include "simulatormanager.h"
#include <cstring>
#include <QFileInfo>
#include "../simulator/def.h"
#include "../simulator/extern.h"
#include "../simulator/proto.h"
#include "../simulator/simio.h"
#include "../simulator/simmessage.h"

extern int pbLoc;
extern bool inputMode;
extern int runToAddr;
extern int stepToAddr;

SimulatorManager::SimulatorManager(QObject *parent) : QObject(parent)
{
    if (!memory) {
        memory = new char[MEMSIZE]();
    }

    setSimMessageCallback([this](const std::string& msg) {
        emit messageLogged(QString::fromStdString(msg));
    });

    runTimer = new QTimer(this);
    runTimer->setInterval(10);
    connect(runTimer, &QTimer::timeout, this, &SimulatorManager::executionBatch);

    autoTraceTimer = new QTimer(this);
    autoTraceTimer->setInterval(autoTraceInterval);
    connect(autoTraceTimer, &QTimer::timeout, this, &SimulatorManager::autoTraceTick);
}

SimulatorManager::~SimulatorManager()
{
    pause();
    if (autoTraceTimer) {
        autoTraceTimer->stop();
    }
    if (memory) {
        delete[] memory;
        memory = nullptr;
    }
}

bool SimulatorManager::loadS68(const QString &filePath)
{
    pause();
    if (autoTraceTimer) autoTraceTimer->stop();
    currentS68File = filePath;

    if (memory) {
        std::memset(memory, 0, MEMSIZE);
        memory[pbLoc & ADDRMASK] = static_cast<char>(0xFF);
    }
    initSim();

    std::string pathStr = filePath.toStdString();
    if (loadSrec(const_cast<char*>(pathStr.c_str())) != SUCCESS) {
        programLoaded = false;
        emit messageLogged(tr("Failed to load S-Record file: %1").arg(filePath));
        return false;
    }

    programLoaded = true;
    startPC = static_cast<uint32_t>(PC);

    emit messageLogged(tr("Loaded S-Record: %1 (Initial PC = 0x%2)")
                       .arg(QFileInfo(filePath).fileName())
                       .arg(startPC, 8, 16, QLatin1Char('0')));
    emit stateChanged();
    return true;
}

void SimulatorManager::loadListingDirectives(const QStringList &lines)
{
    for (int i = 0; i < lines.size(); ++i) {
        QString lower = lines[i].toLower();
        if (lower.contains("*[sim68k]break") && i + 1 < lines.size()) {
            const QString &nextLine = lines[i + 1];
            if (nextLine.length() >= 8) {
                bool ok = false;
                uint32_t addr = nextLine.left(8).toUInt(&ok, 16);
                if (ok && !hasBreakpoint(addr)) {
                    toggleBreakpoint(addr);
                }
            }
        }
        if (lower.contains("*[sim68k]bitfield")) {
            bitfield = true;
        }
        if (lower.contains("*[sim68k]simhalt_off")) {
            simhalt_on = false;
        }
    }
}

void SimulatorManager::traceInto()
{
    if (!programLoaded) return;
    pause();

    trace = true;
    sstep = false;
    runMode = false;
    runprog();

    emit stateChanged();

    if (halt) {
        if (autoTraceTimer && autoTraceTimer->isActive()) autoTraceTimer->stop();
        emit simulationHalted(tr("Program halted."));
    }
}

void SimulatorManager::stepOver()
{
    if (!programLoaded) return;
    pause();

    trace = true;
    sstep = true;
    stepToAddr = 0;
    runMode = true;

    runprog();

    if (sstep && stepToAddr != 0 && !trace && runMode && !halt) {
        running = true;
        runTimer->start();
    } else {
        sstep = false;
        stepToAddr = 0;
        running = false;
    }

    emit stateChanged();

    if (halt) {
        if (autoTraceTimer && autoTraceTimer->isActive()) autoTraceTimer->stop();
        emit simulationHalted(tr("Program halted."));
    }
}

void SimulatorManager::run()
{
    if (!programLoaded || running) return;

    if (autoTraceTimer && autoTraceTimer->isActive()) {
        autoTraceTimer->stop();
    }

    running = true;
    runMode = true;
    trace = false;
    sstep = false;
    runTimer->start();
    emit stateChanged();
}

void SimulatorManager::runToCursor(uint32_t targetAddr)
{
    if (!programLoaded) return;
    runToAddr = static_cast<int>(targetAddr);
    run();
}

void SimulatorManager::pause()
{
    if (!running && (!autoTraceTimer || !autoTraceTimer->isActive())) return;

    running = false;
    runMode = false;
    sstep = false;
    stepToAddr = 0;
    runToAddr = 0;

    if (runTimer) runTimer->stop();
    if (autoTraceTimer) autoTraceTimer->stop();

    emit stateChanged();
    emit messageLogged(tr("Simulation paused at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
}

void SimulatorManager::rewindProgram()
{
    pause();
    initSim();
    PC = startPC;
    emit stateChanged();
    emit messageLogged(tr("Program rewound to start PC=0x%1").arg(startPC, 8, 16, QLatin1Char('0')));
}

void SimulatorManager::reloadProgram()
{
    pause();
    if (!currentS68File.isEmpty()) {
        loadS68(currentS68File);
    } else {
        initSim();
        if (memory) {
            memory[pbLoc & ADDRMASK] = static_cast<char>(0xFF);
        }
        emit stateChanged();
    }
}

void SimulatorManager::toggleAutoTrace(int intervalMs)
{
    if (autoTraceTimer->isActive()) {
        autoTraceTimer->stop();
        emit messageLogged(tr("Auto-trace stopped."));
    } else {
        if (!programLoaded) return;
        pause();
        if (intervalMs > 0) {
            autoTraceInterval = intervalMs;
        }
        autoTraceTimer->setInterval(autoTraceInterval);
        autoTraceTimer->start();
        emit messageLogged(tr("Auto-trace started (%1 ms interval).").arg(autoTraceInterval));
    }
    emit stateChanged();
}

bool SimulatorManager::isAutoTracing() const
{
    return autoTraceTimer && autoTraceTimer->isActive();
}

void SimulatorManager::setAutoTraceInterval(int ms)
{
    if (ms > 0) {
        autoTraceInterval = ms;
        if (autoTraceTimer->isActive()) {
            autoTraceTimer->setInterval(ms);
        }
    }
}

void SimulatorManager::autoTraceTick()
{
    if (!programLoaded || inputMode || halt) {
        autoTraceTimer->stop();
        emit stateChanged();
        if (halt) {
            emit simulationHalted(tr("Program halted during auto-trace."));
        }
        return;
    }
    traceInto();
}

void SimulatorManager::clearCycles()
{
    cycles = 0;
    emit stateChanged();
}

void SimulatorManager::executionBatch()
{
    if (!running) return;

    for (int i = 0; i < BATCH_SIZE; ++i) {
        if (halt || !runMode) {
            running = false;
            runTimer->stop();
            sstep = false;
            stepToAddr = 0;
            runToAddr = 0;
            emit stateChanged();
            if (halt) {
                emit simulationHalted(tr("Execution halted at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
            }
            return;
        }
        runprog();
    }

    emit stateChanged();
}

void SimulatorManager::toggleBreakpoint(uint32_t addr)
{
    for (int i = 0; i < bpoints; ++i) {
        if (static_cast<uint32_t>(brkpt[i]) == addr) {
            for (int j = i; j < bpoints - 1; ++j) {
                brkpt[j] = brkpt[j + 1];
            }
            bpoints--;
            emit messageLogged(tr("Breakpoint removed at 0x%1").arg(addr, 8, 16, QLatin1Char('0')));
            emit stateChanged();
            return;
        }
    }
    if (bpoints < 100) {
        brkpt[static_cast<int>(bpoints++)] = static_cast<int>(addr);
        emit messageLogged(tr("Breakpoint set at 0x%1").arg(addr, 8, 16, QLatin1Char('0')));
        emit stateChanged();
    }
}

bool SimulatorManager::hasBreakpoint(uint32_t addr) const
{
    for (int i = 0; i < bpoints; ++i) {
        if (static_cast<uint32_t>(brkpt[i]) == addr) {
            return true;
        }
    }
    return false;
}

void SimulatorManager::clearBreakpoints()
{
    bpoints = 0;
    emit messageLogged(tr("All breakpoints cleared."));
    emit stateChanged();
}

char* SimulatorManager::getMemory()
{
    return memory;
}

const char* SimulatorManager::getMemory() const
{
    return memory;
}

uint32_t SimulatorManager::getMemorySize() const
{
    return MEMSIZE;
}

long SimulatorManager::getPC() const
{
    return PC;
}

short SimulatorManager::getSR() const
{
    return SR;
}

uint64_t SimulatorManager::getCycles() const
{
    return cycles;
}

const long* SimulatorManager::getD() const
{
    return D;
}

const long* SimulatorManager::getA() const
{
    return A;
}

void SimulatorManager::setD(int idx, uint32_t val)
{
    if (idx >= 0 && idx < 8) {
        D[idx] = static_cast<long>(val);
        emit stateChanged();
    }
}

void SimulatorManager::setA(int idx, uint32_t val)
{
    if (idx >= 0 && idx < 7) {
        A[idx] = static_cast<long>(val);
        emit stateChanged();
    } else if (idx == 7) {
        // Sync active stack pointer
        int active = a_reg(7);
        A[active] = static_cast<long>(val);
        emit stateChanged();
    }
}

void SimulatorManager::setUS(uint32_t val)
{
    A[7] = static_cast<long>(val);
    emit stateChanged();
}

void SimulatorManager::setSS(uint32_t val)
{
    A[8] = static_cast<long>(val);
    emit stateChanged();
}

void SimulatorManager::setPC(uint32_t val)
{
    PC = static_cast<long>(val);
    emit stateChanged();
}

void SimulatorManager::setSR(uint16_t val)
{
    SR = static_cast<short>(val);
    emit stateChanged();
}
