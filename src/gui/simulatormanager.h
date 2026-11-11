#ifndef SIMULATORMANAGER_H
#define SIMULATORMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <cstdint>

class SimulatorManager : public QObject
{
    Q_OBJECT

public:
    explicit SimulatorManager(QObject *parent = nullptr);
    ~SimulatorManager() override;

    bool loadS68(const QString &filePath);
    void step();
    void run();
    void pause();
    void reset();

    bool isRunning() const { return running; }
    bool isLoaded() const { return programLoaded; }

    void toggleBreakpoint(uint32_t addr);
    bool hasBreakpoint(uint32_t addr) const;
    void clearBreakpoints();

    const char* getMemory() const;
    uint32_t getMemorySize() const;

    long getPC() const;
    short getSR() const;
    uint64_t getCycles() const;
    const long* getD() const;
    const long* getA() const;

signals:
    void stateChanged();
    void simulationHalted(const QString &reason);
    void messageLogged(const QString &msg);

private slots:
    void executionBatch();

private:
    bool programLoaded = false;
    bool running = false;
    QString currentS68File;
    QTimer *runTimer = nullptr;
    const int BATCH_SIZE = 5000;
};

#endif // SIMULATORMANAGER_H
