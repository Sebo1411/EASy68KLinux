#include "registersview.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QString>

RegistersView::RegistersView(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void RegistersView::setupUi()
{
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(8);

    // Group Box for Data and Address Registers
    auto *regsBox = new QGroupBox(tr("CPU Registers"), this);
    auto *grid = new QGridLayout(regsBox);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setSpacing(6);

    for (int i = 0; i < 8; ++i) {
        auto *dLabel = new QLabel(QString("D%1:").arg(i), regsBox);
        dRegs[i] = new QLineEdit(regsBox);
        dRegs[i]->setFont(monoFont);
        dRegs[i]->setReadOnly(true);
        dRegs[i]->setAlignment(Qt::AlignCenter);
        dRegs[i]->setText("00000000");

        grid->addWidget(dLabel, i, 0);
        grid->addWidget(dRegs[i], i, 1);

        auto *aLabel = new QLabel(QString("A%1:").arg(i), regsBox);
        aRegs[i] = new QLineEdit(regsBox);
        aRegs[i]->setFont(monoFont);
        aRegs[i]->setReadOnly(true);
        aRegs[i]->setAlignment(Qt::AlignCenter);
        aRegs[i]->setText("00000000");

        grid->addWidget(aLabel, i, 2);
        grid->addWidget(aRegs[i], i, 3);
    }
    mainLayout->addWidget(regsBox);

    // Control Registers
    auto *ctrlBox = new QGroupBox(tr("Control & Stack"), this);
    auto *ctrlGrid = new QGridLayout(ctrlBox);
    ctrlGrid->setContentsMargins(8, 8, 8, 8);
    ctrlGrid->setSpacing(6);

    ctrlGrid->addWidget(new QLabel("PC:", ctrlBox), 0, 0);
    pcEdit = new QLineEdit("00000000", ctrlBox);
    pcEdit->setFont(monoFont);
    pcEdit->setReadOnly(true);
    pcEdit->setAlignment(Qt::AlignCenter);
    ctrlGrid->addWidget(pcEdit, 0, 1);

    ctrlGrid->addWidget(new QLabel("SR:", ctrlBox), 0, 2);
    srEdit = new QLineEdit("2000", ctrlBox);
    srEdit->setFont(monoFont);
    srEdit->setReadOnly(true);
    srEdit->setAlignment(Qt::AlignCenter);
    ctrlGrid->addWidget(srEdit, 0, 3);

    ctrlGrid->addWidget(new QLabel("USP:", ctrlBox), 1, 0);
    uspEdit = new QLineEdit("00FF0000", ctrlBox);
    uspEdit->setFont(monoFont);
    uspEdit->setReadOnly(true);
    uspEdit->setAlignment(Qt::AlignCenter);
    ctrlGrid->addWidget(uspEdit, 1, 1);

    ctrlGrid->addWidget(new QLabel("SSP:", ctrlBox), 1, 2);
    sspEdit = new QLineEdit("01000000", ctrlBox);
    sspEdit->setFont(monoFont);
    sspEdit->setReadOnly(true);
    sspEdit->setAlignment(Qt::AlignCenter);
    ctrlGrid->addWidget(sspEdit, 1, 3);

    mainLayout->addWidget(ctrlBox);

    // Flags Box
    auto *flagsBox = new QGroupBox(tr("Condition Code Flags (CCR)"), this);
    auto *flagsLayout = new QHBoxLayout(flagsBox);

    flagT = new QCheckBox("T", flagsBox);
    flagS = new QCheckBox("S", flagsBox);
    flagX = new QCheckBox("X", flagsBox);
    flagN = new QCheckBox("N", flagsBox);
    flagZ = new QCheckBox("Z", flagsBox);
    flagV = new QCheckBox("V", flagsBox);
    flagC = new QCheckBox("C", flagsBox);

    flagT->setEnabled(false);
    flagS->setEnabled(false);
    flagX->setEnabled(false);
    flagN->setEnabled(false);
    flagZ->setEnabled(false);
    flagV->setEnabled(false);
    flagC->setEnabled(false);

    flagsLayout->addWidget(flagT);
    flagsLayout->addWidget(flagS);
    flagsLayout->addSpacing(10);
    flagsLayout->addWidget(flagX);
    flagsLayout->addWidget(flagN);
    flagsLayout->addWidget(flagZ);
    flagsLayout->addWidget(flagV);
    flagsLayout->addWidget(flagC);

    mainLayout->addWidget(flagsBox);

    // Cycles Box
    cyclesLabel = new QLabel(tr("Total Cycles: 0"), this);
    QFont boldFont = font();
    boldFont.setBold(true);
    cyclesLabel->setFont(boldFont);
    mainLayout->addWidget(cyclesLabel);

    mainLayout->addStretch();
}

void RegistersView::updateRegisters(const long *D, const long *A, long pc, short sr, uint64_t cycles)
{
    for (int i = 0; i < 8; ++i) {
        dRegs[i]->setText(QString("%1").arg(static_cast<uint32_t>(D[i]), 8, 16, QLatin1Char('0')).toUpper());
        aRegs[i]->setText(QString("%1").arg(static_cast<uint32_t>(A[i]), 8, 16, QLatin1Char('0')).toUpper());
    }

    pcEdit->setText(QString("%1").arg(static_cast<uint32_t>(pc), 8, 16, QLatin1Char('0')).toUpper());
    srEdit->setText(QString("%1").arg(static_cast<uint16_t>(sr), 4, 16, QLatin1Char('0')).toUpper());
    uspEdit->setText(QString("%1").arg(static_cast<uint32_t>(A[7]), 8, 16, QLatin1Char('0')).toUpper());
    sspEdit->setText(QString("%1").arg(static_cast<uint32_t>(A[8]), 8, 16, QLatin1Char('0')).toUpper());

    flagT->setChecked((sr & 0x8000) != 0);
    flagS->setChecked((sr & 0x2000) != 0);
    flagX->setChecked((sr & 0x0010) != 0);
    flagN->setChecked((sr & 0x0008) != 0);
    flagZ->setChecked((sr & 0x0004) != 0);
    flagV->setChecked((sr & 0x0002) != 0);
    flagC->setChecked((sr & 0x0001) != 0);

    cyclesLabel->setText(tr("Total Cycles: %1").arg(cycles));
}
