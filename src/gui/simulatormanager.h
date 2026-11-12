#ifndef SIMULATORMANAGER_H
#define SIMULATORMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <cstdint>

class SimulatorManager : public QObject
{
    Q_OBJECT

public:
    explicit SimulatorManager(QObject *parent = nullptr);
    ~SimulatorManager() override;

    bool loadS68(const QString &filePath);
    void loadListingDirectives(const QStringList &lines);

    // Execution modes matching Sim68K
    void traceInto();                          // F7: Trace into 1 instruction
    void stepOver();                           // F8: Step over subroutine (BSR / JSR)
    void step() { traceInto(); }               // Backward compatibility alias
    void run();                                // F9: Continuous execution
    void runToCursor(uint32_t targetAddr);     // Ctrl+F9: Run until PC == targetAddr
    void pause();                              // F6: Pause execution
    void reset() { reloadProgram(); }          // Backward compatibility alias
    void rewindProgram();                      // Ctrl+F2: Rewind to initial start PC
    void reloadProgram();                      // Ctrl+F3: Reload S68 file from disk

    void toggleAutoTrace(int intervalMs = 200);// F10: Auto-trace timer
    bool isAutoTracing() const;
    void setAutoTraceInterval(int ms);
    int getAutoTraceInterval() const { return autoTraceInterval; }

    void clearCycles();

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

private slots:
    void executionBatch();
    void autoTraceTick();

private:
    bool programLoaded = false;
    bool running = false;
    uint32_t startPC = 0;
    QString currentS68File;

    QTimer *runTimer = nullptr;
    QTimer *autoTraceTimer = nullptr;
    int autoTraceInterval = 200;
    const int BATCH_SIZE = 5000;
};

#endif // SIMULATORMANAGER_H
