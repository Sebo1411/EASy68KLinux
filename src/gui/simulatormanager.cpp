#include "simulatormanager.h"
#include <cstring>
#include <QFileInfo>
#include "../simulator/def.h"
#include "../simulator/extern.h"
#include "../simulator/proto.h"
#include "../simulator/simio.h"
#include "../simulator/simmessage.h"

extern int pbLoc;

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
}

SimulatorManager::~SimulatorManager()
{
    pause();
    if (memory) {
        delete[] memory;
        memory = nullptr;
    }
}

bool SimulatorManager::loadS68(const QString &filePath)
{
    pause();
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
    emit messageLogged(tr("Loaded S-Record: %1 (Initial PC = 0x%2)").arg(QFileInfo(filePath).fileName()).arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
    emit stateChanged();
    return true;
}

void SimulatorManager::step()
{
    if (!programLoaded) return;
    pause();

    trace = true;
    runMode = false;
    runprog();

    emit stateChanged();

    if (halt) {
        emit simulationHalted(tr("Program halted."));
    }
}

void SimulatorManager::run()
{
    if (!programLoaded || running) return;

    running = true;
    runMode = true;
    trace = false;
    runTimer->start();
    emit stateChanged();
}

void SimulatorManager::pause()
{
    if (!running) return;

    running = false;
    runMode = false;
    runTimer->stop();
    emit stateChanged();
    emit messageLogged(tr("Simulation paused at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
}

void SimulatorManager::reset()
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

void SimulatorManager::executionBatch()
{
    if (!running) return;

    for (int i = 0; i < BATCH_SIZE; ++i) {
        if (halt || !runMode) {
            running = false;
            runTimer->stop();
            emit stateChanged();
            emit simulationHalted(tr("Execution halted at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
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
        brkpt[static_cast<int>(bpoints++)] = addr;
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
