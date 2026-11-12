#ifndef SIMULATORMANAGER_H
#define SIMULATORMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <cstddef>

enum class SimExecState : uint8_t {
    Unloaded = 0,
    IdleHalted,
    Paused,
    SteppingInto,
    SteppingOver,
    RunningContinuous,
    RunningToCursor,
    AutoTracing,
    WaitingStopIRQ,
    WaitingTerminalInput,
    Count
};

enum class SimExecEvent : uint8_t {
    LoadSuccess = 0,
    LoadFail,
    CmdRun,
    CmdRunToCursor,
    CmdAutoTraceToggle,
    CmdTraceInto,
    CmdStepOver,
    CmdPause,
    CmdRewind,
    CmdReload,
    SubrEntered,
    TargetReached,
    StepCompleted,
    BreakpointHit,
    SimHaltHit,
    StopInstrHit,
    IrqAsserted,
    InputRequested,
    InputCompleted,
    ExceptionHalted,
    Count
};

inline constexpr size_t kSimExecStateCount = static_cast<size_t>(SimExecState::Count);
inline constexpr size_t kSimExecEventCount = static_cast<size_t>(SimExecEvent::Count);

constexpr SimExecState computeNextSimExecState(SimExecState s, SimExecEvent e) noexcept {
    switch (s) {
    case SimExecState::Unloaded:
        if (e == SimExecEvent::LoadSuccess) return SimExecState::IdleHalted;
        return SimExecState::Unloaded;

    case SimExecState::IdleHalted:
        switch (e) {
        case SimExecEvent::LoadSuccess: return SimExecState::IdleHalted;
        case SimExecEvent::LoadFail: return SimExecState::Unloaded;
        case SimExecEvent::CmdRun: return SimExecState::RunningContinuous;
        case SimExecEvent::CmdRunToCursor: return SimExecState::RunningToCursor;
        case SimExecEvent::CmdAutoTraceToggle: return SimExecState::AutoTracing;
        case SimExecEvent::CmdTraceInto: return SimExecState::SteppingInto;
        case SimExecEvent::CmdStepOver: return SimExecState::SteppingOver;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::IrqAsserted: return SimExecState::IdleHalted;
        default: return SimExecState::IdleHalted;
        }

    case SimExecState::Paused:
        switch (e) {
        case SimExecEvent::LoadSuccess: return SimExecState::IdleHalted;
        case SimExecEvent::LoadFail: return SimExecState::Unloaded;
        case SimExecEvent::CmdRun: return SimExecState::RunningContinuous;
        case SimExecEvent::CmdRunToCursor: return SimExecState::RunningToCursor;
        case SimExecEvent::CmdAutoTraceToggle: return SimExecState::AutoTracing;
        case SimExecEvent::CmdTraceInto: return SimExecState::SteppingInto;
        case SimExecEvent::CmdStepOver: return SimExecState::SteppingOver;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        default: return SimExecState::Paused;
        }

    case SimExecState::SteppingInto:
        switch (e) {
        case SimExecEvent::StepCompleted: return SimExecState::Paused;
        case SimExecEvent::BreakpointHit: return SimExecState::Paused;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        case SimExecEvent::StopInstrHit: return SimExecState::WaitingStopIRQ;
        case SimExecEvent::InputRequested: return SimExecState::WaitingTerminalInput;
        default: return SimExecState::Paused;
        }

    case SimExecState::SteppingOver:
        switch (e) {
        case SimExecEvent::SubrEntered: return SimExecState::RunningToCursor;
        case SimExecEvent::TargetReached: return SimExecState::Paused;
        case SimExecEvent::StepCompleted: return SimExecState::Paused;
        case SimExecEvent::BreakpointHit: return SimExecState::Paused;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::CmdRun: return SimExecState::RunningContinuous;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        case SimExecEvent::StopInstrHit: return SimExecState::WaitingStopIRQ;
        case SimExecEvent::InputRequested: return SimExecState::WaitingTerminalInput;
        default: return SimExecState::Paused;
        }

    case SimExecState::RunningContinuous:
        switch (e) {
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::BreakpointHit: return SimExecState::Paused;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        case SimExecEvent::StopInstrHit: return SimExecState::WaitingStopIRQ;
        case SimExecEvent::InputRequested: return SimExecState::WaitingTerminalInput;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::LoadSuccess: return SimExecState::IdleHalted;
        case SimExecEvent::LoadFail: return SimExecState::Unloaded;
        default: return SimExecState::RunningContinuous;
        }

    case SimExecState::RunningToCursor:
        switch (e) {
        case SimExecEvent::TargetReached: return SimExecState::Paused;
        case SimExecEvent::BreakpointHit: return SimExecState::Paused;
        case SimExecEvent::StepCompleted: return SimExecState::Paused;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        case SimExecEvent::StopInstrHit: return SimExecState::WaitingStopIRQ;
        case SimExecEvent::InputRequested: return SimExecState::WaitingTerminalInput;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::LoadSuccess: return SimExecState::IdleHalted;
        case SimExecEvent::LoadFail: return SimExecState::Unloaded;
        default: return SimExecState::RunningToCursor;
        }

    case SimExecState::AutoTracing:
        switch (e) {
        case SimExecEvent::CmdAutoTraceToggle: return SimExecState::Paused;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::StepCompleted: return SimExecState::AutoTracing;
        case SimExecEvent::BreakpointHit: return SimExecState::Paused;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        case SimExecEvent::StopInstrHit: return SimExecState::WaitingStopIRQ;
        case SimExecEvent::InputRequested: return SimExecState::WaitingTerminalInput;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        default: return SimExecState::AutoTracing;
        }

    case SimExecState::WaitingStopIRQ:
        switch (e) {
        case SimExecEvent::IrqAsserted: return SimExecState::IdleHalted;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        default: return SimExecState::WaitingStopIRQ;
        }

    case SimExecState::WaitingTerminalInput:
        switch (e) {
        case SimExecEvent::InputCompleted: return SimExecState::Paused;
        case SimExecEvent::CmdPause: return SimExecState::Paused;
        case SimExecEvent::CmdRewind: return SimExecState::IdleHalted;
        case SimExecEvent::CmdReload: return SimExecState::IdleHalted;
        case SimExecEvent::SimHaltHit: return SimExecState::IdleHalted;
        case SimExecEvent::ExceptionHalted: return SimExecState::IdleHalted;
        default: return SimExecState::WaitingTerminalInput;
        }

    default:
        return SimExecState::Unloaded;
    }
}

template<size_t S, size_t E>
struct SimExecTransitionTableHolder {
    SimExecState table[S][E]{};
    constexpr SimExecTransitionTableHolder() {
        for (size_t s = 0; s < S; ++s) {
            for (size_t e = 0; e < E; ++e) {
                table[s][e] = computeNextSimExecState(static_cast<SimExecState>(s), static_cast<SimExecEvent>(e));
            }
        }
    }
};

inline constexpr SimExecTransitionTableHolder<kSimExecStateCount, kSimExecEventCount> kSimExecTransitionTableHolder{};

class SimulatorManager : public QObject
{
    Q_OBJECT

public:
    static constexpr SimExecState nextSimExecState(SimExecState current, SimExecEvent event) noexcept {
        auto sIdx = static_cast<size_t>(current);
        auto eIdx = static_cast<size_t>(event);
        if (sIdx >= kSimExecStateCount || eIdx >= kSimExecEventCount) {
            return current;
        }
        return kSimExecTransitionTableHolder.table[sIdx][eIdx];
    }

    explicit SimulatorManager(QObject *parent = nullptr);
    ~SimulatorManager() override;

    bool loadS68(const QString &filePath);
    void loadListingDirectives(const QStringList &lines);

    // State machine inspection & transition
    SimExecState execState() const noexcept { return m_execState; }
    void applyExecEvent(SimExecEvent event);

    // Execution modes matching Sim68K
    void traceInto();                          // F7: Trace into 1 instruction
    void stepInto() { traceInto(); }
    void stepOver();                           // F8: Step over subroutine (BSR / JSR)
    void step() { traceInto(); }               // Backward compatibility alias
    void run();                                // F9: Continuous execution
    void runProgram() { run(); }
    void runToCursor(uint32_t targetAddr);     // Ctrl+F9: Run until PC == targetAddr
    void pause();                              // F6: Pause execution
    void pauseProgram() { pause(); }
    void reset() { reloadProgram(); }          // Backward compatibility alias
    void rewindProgram();                      // Ctrl+F2: Rewind to initial start PC
    void reloadProgram();                      // Ctrl+F3: Reload S68 file from disk

    void toggleAutoTrace(int intervalMs = 200);// F10: Auto-trace timer
    void autoTrace(int intervalMs = 200) { toggleAutoTrace(intervalMs); }
    bool isAutoTracing() const;
    void setAutoTraceInterval(int ms);
    int getAutoTraceInterval() const { return autoTraceInterval; }

    void provideInput(const QString &input = QString());
    void setInputWaiting(bool waiting);
    void triggerIRQ(int level);
    void executeBatch();
    void autoTraceStep();

    void clearCycles();
    void setExceptionsEnabled(bool enabled);
    bool isExceptionsEnabled() const;
    void notifyBreakpointsChanged();

    bool isRunning() const { return running; }
    bool isLoaded() const { return programLoaded; }

    // Breakpoints
    void toggleBreakpoint(uint32_t addr);
    bool hasBreakpoint(uint32_t addr) const;
    void clearBreakpoints();

    // Memory & Registers
    char* getMemory();
    const char* getMemory() const;
    uint32_t getMemorySize() const;

    long getPC() const;
    uint32_t getStartPC() const { return startPC; }
    short getSR() const;
    uint64_t getCycles() const;
    const long* getD() const;
    const long* getA() const;

    // Register Mutators
    void setD(int idx, uint32_t val);
    void setA(int idx, uint32_t val);
    void setUS(uint32_t val);
    void setSS(uint32_t val);
    void setPC(uint32_t val);
    void setSR(uint16_t val);

signals:
    void stateChanged();
    void programLoadedChanged(uint32_t startAddr);
    void simulationHalted(const QString &reason);
    void messageLogged(const QString &msg);
    void simulationStarted();
    void simulationPaused();
    void registersChanged();
    void memoryChanged(uint32_t addr);
    void autoTraceStateChanged(bool active);

private slots:
    void executionBatch();
    void autoTraceTick();

private:
    SimExecState m_execState = SimExecState::Unloaded;
    bool programLoaded = false;
    bool running = false;
    uint32_t startPC = 0;
    QString currentS68File;

    QTimer *runTimer = nullptr;
    QTimer *autoTraceTimer = nullptr;
    int autoTraceInterval = 200;
    bool m_inExecutionBatch = false;
    bool m_pausedForInput = false;
    const int BATCH_SIZE = 200000;
};

#endif // SIMULATORMANAGER_H
