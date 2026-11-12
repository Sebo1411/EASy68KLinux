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

namespace {
// The simulator core keeps its 16 MB address space in the global `memory`
// pointer, so several SimulatorManager instances share one buffer. Track the
// instances and only release the buffer when the last one goes away.
int simulatorManagerInstances = 0;
}

SimulatorManager::SimulatorManager(QObject *parent) : QObject(parent)
{
    ++simulatorManagerInstances;
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
    if (--simulatorManagerInstances <= 0 && memory) {
        simulatorManagerInstances = 0;
        delete[] memory;
        memory = nullptr;
    }
}

void SimulatorManager::applyExecEvent(SimExecEvent event)
{
    SimExecState nextState = nextSimExecState(m_execState, event);
    m_execState = nextState;

    programLoaded = (m_execState != SimExecState::Unloaded);

    switch (m_execState) {
    case SimExecState::Unloaded:
        running = false;
        runMode = false;
        trace = false;
        sstep = false;
        halt = true;
        stopInstruction = false;
        inputMode = false;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    case SimExecState::IdleHalted:
        running = false;
        runMode = false;
        trace = false;
        sstep = false;
        halt = true;
        stopInstruction = false;
        inputMode = false;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    case SimExecState::Paused:
        running = false;
        runMode = false;
        trace = false;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        stepToAddr = 0;
        runToAddr = 0;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        emit simulationPaused();
        break;

    case SimExecState::SteppingInto:
        running = false;
        runMode = false;
        trace = true;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    case SimExecState::SteppingOver:
        running = (stepToAddr != 0);
        runMode = true;
        trace = (stepToAddr == 0);
        sstep = true;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        if (running) {
            if (runTimer && !runTimer->isActive()) runTimer->start();
        } else {
            if (runTimer && runTimer->isActive()) runTimer->stop();
        }
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    case SimExecState::RunningContinuous:
        running = true;
        runMode = true;
        trace = false;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        if (runTimer && !runTimer->isActive()) runTimer->start();
        if (autoTraceTimer) autoTraceTimer->stop();
        emit simulationStarted();
        break;

    case SimExecState::RunningToCursor:
        running = true;
        runMode = true;
        trace = false;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        if (runTimer && !runTimer->isActive()) runTimer->start();
        if (autoTraceTimer) autoTraceTimer->stop();
        emit simulationStarted();
        break;

    case SimExecState::AutoTracing:
        running = false;
        runMode = false;
        trace = true;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = false;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer && !autoTraceTimer->isActive()) autoTraceTimer->start();
        emit autoTraceStateChanged(true);
        break;

    case SimExecState::WaitingStopIRQ:
        running = false;
        runMode = false;
        trace = false;
        sstep = false;
        halt = true;
        stopInstruction = true;
        inputMode = false;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    case SimExecState::WaitingTerminalInput:
        running = false;
        runMode = false;
        trace = false;
        sstep = false;
        halt = false;
        stopInstruction = false;
        inputMode = true;
        if (runTimer) runTimer->stop();
        if (autoTraceTimer) autoTraceTimer->stop();
        break;

    default:
        break;
    }

    emit stateChanged();
    emit registersChanged();
    emit memoryChanged(static_cast<uint32_t>(PC));
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
    simhalt_on = true;  // Sim68K defaults SIMHALT on whenever a program is opened

    std::string pathStr = filePath.toStdString();
    if (loadSrec(const_cast<char*>(pathStr.c_str())) != SUCCESS) {
        applyExecEvent(SimExecEvent::LoadFail);
        emit messageLogged(tr("Failed to load S-Record file: %1").arg(filePath));
        return false;
    }

    applyExecEvent(SimExecEvent::LoadSuccess);
    startPC = static_cast<uint32_t>(PC);

    emit messageLogged(tr("Loaded S-Record: %1 (Initial PC = 0x%2)")
                       .arg(QFileInfo(filePath).fileName())
                       .arg(startPC, 8, 16, QLatin1Char('0')));
    emit programLoadedChanged(startPC);
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
    if (m_execState == SimExecState::Unloaded) return;
    pause();

    applyExecEvent(SimExecEvent::CmdTraceInto);
    runprog();

    if (halt) {
        applyExecEvent(SimExecEvent::SimHaltHit);
        emit simulationHalted(tr("Program halted."));
    } else if (inputMode) {
        applyExecEvent(SimExecEvent::InputRequested);
    } else if (stopInstruction) {
        applyExecEvent(SimExecEvent::StopInstrHit);
    } else {
        applyExecEvent(SimExecEvent::StepCompleted);
    }
}

void SimulatorManager::stepOver()
{
    if (m_execState == SimExecState::Unloaded) return;
    pause();

    stepToAddr = 0;
    applyExecEvent(SimExecEvent::CmdStepOver);

    runprog();

    if (sstep && stepToAddr != 0 && !trace && runMode && !halt) {
        applyExecEvent(SimExecEvent::SubrEntered);
    } else {
        if (halt) {
            applyExecEvent(SimExecEvent::SimHaltHit);
            emit simulationHalted(tr("Program halted."));
        } else if (inputMode) {
            applyExecEvent(SimExecEvent::InputRequested);
        } else {
            applyExecEvent(SimExecEvent::StepCompleted);
        }
    }
}

void SimulatorManager::run()
{
    if (m_execState == SimExecState::Unloaded || running) return;

    if (autoTraceTimer && autoTraceTimer->isActive()) {
        autoTraceTimer->stop();
    }

    applyExecEvent(SimExecEvent::CmdRun);
}

void SimulatorManager::runToCursor(uint32_t targetAddr)
{
    if (m_execState == SimExecState::Unloaded) return;
    runToAddr = static_cast<int>(targetAddr);
    applyExecEvent(SimExecEvent::CmdRunToCursor);
}

void SimulatorManager::pause()
{
    if (!running && (!autoTraceTimer || !autoTraceTimer->isActive()) &&
        m_execState != SimExecState::RunningContinuous &&
        m_execState != SimExecState::RunningToCursor &&
        m_execState != SimExecState::AutoTracing) {
        return;
    }

    applyExecEvent(SimExecEvent::CmdPause);
    emit messageLogged(tr("Simulation paused at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
}

void SimulatorManager::rewindProgram()
{
    pause();
    initSim();
    PC = startPC;
    applyExecEvent(SimExecEvent::CmdRewind);
    emit messageLogged(tr("Program rewound to start PC=0x%1").arg(startPC, 8, 16, QLatin1Char('0')));
}

void SimulatorManager::reloadProgram()
{
    pause();
    applyExecEvent(SimExecEvent::CmdReload);
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
    if (m_execState == SimExecState::AutoTracing || (autoTraceTimer && autoTraceTimer->isActive())) {
        applyExecEvent(SimExecEvent::CmdAutoTraceToggle);
        emit messageLogged(tr("Auto-trace stopped."));
    } else {
        if (m_execState == SimExecState::Unloaded) return;
        pause();
        if (intervalMs > 0) {
            autoTraceInterval = intervalMs;
        }
        autoTraceTimer->setInterval(autoTraceInterval);
        applyExecEvent(SimExecEvent::CmdAutoTraceToggle);
        emit messageLogged(tr("Auto-trace started (%1 ms interval).").arg(autoTraceInterval));
    }
}

bool SimulatorManager::isAutoTracing() const
{
    return m_execState == SimExecState::AutoTracing;
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
    if (m_execState == SimExecState::Unloaded || inputMode || halt) {
        applyExecEvent(SimExecEvent::CmdPause);
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
    emit registersChanged();
}

void SimulatorManager::provideInput(const QString &input)
{
    (void)input;
    if (m_execState == SimExecState::WaitingTerminalInput) {
        applyExecEvent(SimExecEvent::InputCompleted);
    }
}

void SimulatorManager::triggerIRQ(int level)
{
    if (level >= 1 && level <= 7) {
        irq |= (1 << (level - 1));
        applyExecEvent(SimExecEvent::IrqAsserted);
    }
}

void SimulatorManager::executeBatch()
{
    executionBatch();
}

void SimulatorManager::autoTraceStep()
{
    autoTraceTick();
}

void SimulatorManager::executionBatch()
{
    if (!running) return;

    for (int i = 0; i < BATCH_SIZE; ++i) {
        if (halt || !runMode) {
            applyExecEvent(SimExecEvent::SimHaltHit);
            if (halt) {
                emit simulationHalted(tr("Execution halted at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
            }
            return;
        }
        if (inputMode) {
            applyExecEvent(SimExecEvent::InputRequested);
            return;
        }
        if (stopInstruction) {
            applyExecEvent(SimExecEvent::StopInstrHit);
            return;
        }
        if (hasBreakpoint(static_cast<uint32_t>(PC))) {
            applyExecEvent(SimExecEvent::BreakpointHit);
            return;
        }
        if (runToAddr != 0 && static_cast<uint32_t>(PC) == static_cast<uint32_t>(runToAddr)) {
            applyExecEvent(SimExecEvent::TargetReached);
            return;
        }
        if (stepToAddr != 0 && static_cast<uint32_t>(PC) == static_cast<uint32_t>(stepToAddr)) {
            applyExecEvent(SimExecEvent::TargetReached);
            return;
        }

        runprog();

        if (halt) {
            applyExecEvent(SimExecEvent::SimHaltHit);
            emit simulationHalted(tr("Execution halted at PC=0x%1").arg(static_cast<uint32_t>(PC), 8, 16, QLatin1Char('0')));
            return;
        }
        if (inputMode) {
            applyExecEvent(SimExecEvent::InputRequested);
            return;
        }
        if (stopInstruction) {
            applyExecEvent(SimExecEvent::StopInstrHit);
            return;
        }
        if (hasBreakpoint(static_cast<uint32_t>(PC))) {
            applyExecEvent(SimExecEvent::BreakpointHit);
            return;
        }
        if (runToAddr != 0 && static_cast<uint32_t>(PC) == static_cast<uint32_t>(runToAddr)) {
            applyExecEvent(SimExecEvent::TargetReached);
            return;
        }
        if (stepToAddr != 0 && static_cast<uint32_t>(PC) == static_cast<uint32_t>(stepToAddr)) {
            applyExecEvent(SimExecEvent::TargetReached);
            return;
        }
    }

    emit stateChanged();
    emit registersChanged();
    emit memoryChanged(static_cast<uint32_t>(PC));
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
        emit registersChanged();
    }
}

void SimulatorManager::setA(int idx, uint32_t val)
{
    if (idx >= 0 && idx < 7) {
        A[idx] = static_cast<long>(val);
        emit stateChanged();
        emit registersChanged();
    } else if (idx == 7) {
        // Sync active stack pointer
        int active = a_reg(7);
        A[active] = static_cast<long>(val);
        emit stateChanged();
        emit registersChanged();
    }
}

void SimulatorManager::setUS(uint32_t val)
{
    A[7] = static_cast<long>(val);
    emit stateChanged();
    emit registersChanged();
}

void SimulatorManager::setSS(uint32_t val)
{
    A[8] = static_cast<long>(val);
    emit stateChanged();
    emit registersChanged();
}

void SimulatorManager::setPC(uint32_t val)
{
    PC = static_cast<long>(val);
    emit stateChanged();
    emit registersChanged();
}

void SimulatorManager::setSR(uint16_t val)
{
    SR = static_cast<short>(val);
    emit stateChanged();
    emit registersChanged();
}
