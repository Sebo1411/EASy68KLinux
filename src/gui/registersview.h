#ifndef REGISTERSVIEW_H
#define REGISTERSVIEW_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <cstdint>

class RegistersView : public QWidget
{
    Q_OBJECT

public:
    explicit RegistersView(QWidget *parent = nullptr);

    void updateRegisters(const long *D, const long *A, long pc, short sr, uint64_t cycles);

signals:
    void registerChanged(const QString &regName, uint32_t val);

private:
    QLineEdit *dRegs[8];
    QLineEdit *aRegs[8];
    QLineEdit *pcEdit;
    QLineEdit *srEdit;
    QLineEdit *uspEdit;
    QLineEdit *sspEdit;
    QLabel *cyclesLabel;

    // CCR bit flags
    QCheckBox *flagT;
    QCheckBox *flagS;
    QCheckBox *flagX;
    QCheckBox *flagN;
    QCheckBox *flagZ;
    QCheckBox *flagV;
    QCheckBox *flagC;

    void setupUi();
};

#endif // REGISTERSVIEW_H
