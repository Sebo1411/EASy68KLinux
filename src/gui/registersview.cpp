#include "registersview.h"
#include "simulatormanager.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QRegularExpressionValidator>

RegisterLineEdit::RegisterLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setAlignment(Qt::AlignCenter);
    setMaxLength(8);
}

void RegisterLineEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    QLineEdit::mouseDoubleClickEvent(event);
    emit doubleClicked();
}

RegistersView::RegistersView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void RegistersView::setSimulatorManager(SimulatorManager *sm)
{
    simManager = sm;
}

void RegistersView::setupUi()
{
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(0);

    auto *groupBox = new QGroupBox(tr("Registers"), this);
    auto *grid = new QGridLayout(groupBox);
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(3);

    auto *hexValidator = new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f]{1,8}"), this);
    auto *binValidator = new QRegularExpressionValidator(QRegularExpression("[01]{1,16}"), this);

    // D0..D3 (Col 0, 1) and D4..D7 (Col 2, 3)
    for (int i = 0; i < 4; ++i) {
        auto *lblLow = new QLabel(QString("D%1=").arg(i), groupBox);
        lblLow->setFont(monoFont);
        dRegs[i] = new RegisterLineEdit(groupBox);
        dRegs[i]->setFont(monoFont);
        dRegs[i]->setValidator(hexValidator);
        dRegs[i]->setText("00000000");

        grid->addWidget(lblLow, i, 0);
        grid->addWidget(dRegs[i], i, 1);

        int highIdx = i + 4;
        auto *lblHigh = new QLabel(QString("D%1=").arg(highIdx), groupBox);
        lblHigh->setFont(monoFont);
        dRegs[highIdx] = new RegisterLineEdit(groupBox);
        dRegs[highIdx]->setFont(monoFont);
        dRegs[highIdx]->setValidator(hexValidator);
        dRegs[highIdx]->setText("00000000");

        grid->addWidget(lblHigh, i, 2);
        grid->addWidget(dRegs[highIdx], i, 3);

        connect(dRegs[i], &QLineEdit::editingFinished, this, [this, i]() { onDRegisterEdited(i); });
        connect(dRegs[highIdx], &QLineEdit::editingFinished, this, [this, highIdx]() { onDRegisterEdited(highIdx); });

        connect(dRegs[i], &RegisterLineEdit::doubleClicked, this, [this, i]() {
            emit addressDoubleClicked(getD(i));
        });
        connect(dRegs[highIdx], &RegisterLineEdit::doubleClicked, this, [this, highIdx]() {
            emit addressDoubleClicked(getD(highIdx));
        });
    }

    // A0..A3 (Col 4, 5) and A4..A7 (Col 6, 7)
    for (int i = 0; i < 4; ++i) {
        auto *lblLow = new QLabel(QString("A%1=").arg(i), groupBox);
        lblLow->setFont(monoFont);
        aRegs[i] = new RegisterLineEdit(groupBox);
        aRegs[i]->setFont(monoFont);
        aRegs[i]->setValidator(hexValidator);
        aRegs[i]->setText("00000000");

        grid->addWidget(lblLow, i, 4);
        grid->addWidget(aRegs[i], i, 5);

        int highIdx = i + 4;
        auto *lblHigh = new QLabel(QString("A%1=").arg(highIdx), groupBox);
        lblHigh->setFont(monoFont);
        aRegs[highIdx] = new RegisterLineEdit(groupBox);
        aRegs[highIdx]->setFont(monoFont);
        aRegs[highIdx]->setValidator(hexValidator);
        aRegs[highIdx]->setText("00000000");

        grid->addWidget(lblHigh, i, 6);
        grid->addWidget(aRegs[highIdx], i, 7);

        connect(aRegs[i], &QLineEdit::editingFinished, this, [this, i]() { onARegisterEdited(i); });
        connect(aRegs[highIdx], &QLineEdit::editingFinished, this, [this, highIdx]() { onARegisterEdited(highIdx); });

        connect(aRegs[i], &RegisterLineEdit::doubleClicked, this, [this, i]() {
            emit addressDoubleClicked(getA(i));
        });
        connect(aRegs[highIdx], &RegisterLineEdit::doubleClicked, this, [this, highIdx]() {
            emit addressDoubleClicked(getA(highIdx));
        });
    }

    // Column 8, 9: SR header, SR, US, SS
    auto *srFlagsHeader = new QLabel(tr("T S  INT   XNZVC"), groupBox);
    srFlagsHeader->setFont(monoFont);
    srFlagsHeader->setAlignment(Qt::AlignCenter);
    grid->addWidget(srFlagsHeader, 0, 8, 1, 2);

    auto *srLabel = new QLabel("SR=", groupBox);
    srLabel->setFont(monoFont);
    srEdit = new RegisterLineEdit(groupBox);
    srEdit->setFont(monoFont);
    srEdit->setMaxLength(16);
    srEdit->setValidator(binValidator);
    srEdit->setText("0010000000000000");
    grid->addWidget(srLabel, 1, 8);
    grid->addWidget(srEdit, 1, 9);
    connect(srEdit, &QLineEdit::editingFinished, this, &RegistersView::onSRRegisterEdited);

    auto *usLabel = new QLabel("US=", groupBox);
    usLabel->setFont(monoFont);
    usEdit = new RegisterLineEdit(groupBox);
    usEdit->setFont(monoFont);
    usEdit->setValidator(hexValidator);
    usEdit->setText("00FF0000");
    grid->addWidget(usLabel, 2, 8);
    grid->addWidget(usEdit, 2, 9);
    connect(usEdit, &QLineEdit::editingFinished, this, &RegistersView::onUSRegisterEdited);
    connect(usEdit, &RegisterLineEdit::doubleClicked, this, [this]() {
        bool ok = false;
        uint32_t val = usEdit->text().toUInt(&ok, 16);
        if (ok) emit addressDoubleClicked(val);
    });

    auto *ssLabel = new QLabel("SS=", groupBox);
    ssLabel->setFont(monoFont);
    ssEdit = new RegisterLineEdit(groupBox);
    ssEdit->setFont(monoFont);
    ssEdit->setValidator(hexValidator);
    ssEdit->setText("01000000");
    grid->addWidget(ssLabel, 3, 8);
    grid->addWidget(ssEdit, 3, 9);
    connect(ssEdit, &QLineEdit::editingFinished, this, &RegistersView::onSSRegisterEdited);
    connect(ssEdit, &RegisterLineEdit::doubleClicked, this, [this]() {
        bool ok = false;
        uint32_t val = ssEdit->text().toUInt(&ok, 16);
        if (ok) emit addressDoubleClicked(val);
    });

    // Column 10, 11: Cycles, Clear Cycles, PC
    cyclesEdit = new QLineEdit("0", groupBox);
    cyclesEdit->setFont(monoFont);
    cyclesEdit->setReadOnly(true);
    cyclesEdit->setAlignment(Qt::AlignRight);
    grid->addWidget(new QLabel(tr("Cycles:"), groupBox), 1, 10);
    grid->addWidget(cyclesEdit, 1, 11);

    clearCyclesBtn = new QPushButton(tr("Clear Cycles"), groupBox);
    grid->addWidget(clearCyclesBtn, 2, 10, 1, 2);
    connect(clearCyclesBtn, &QPushButton::clicked, this, &RegistersView::onClearCyclesClicked);

    auto *pcLabel = new QLabel("PC=", groupBox);
    pcLabel->setFont(monoFont);
    pcEdit = new RegisterLineEdit(groupBox);
    pcEdit->setFont(monoFont);
    pcEdit->setValidator(hexValidator);
    pcEdit->setText("00000000");
    grid->addWidget(pcLabel, 3, 10);
    grid->addWidget(pcEdit, 3, 11);
    connect(pcEdit, &QLineEdit::editingFinished, this, &RegistersView::onPCRegisterEdited);
    connect(pcEdit, &RegisterLineEdit::doubleClicked, this, [this]() {
        emit pcDoubleClicked(getPC());
    });

    mainLayout->addWidget(groupBox);
}

void RegistersView::updateRegisters(const long *D, const long *A, long pc, short sr, uint64_t cycles)
{
    updatingUi = true;

    for (int i = 0; i < 8; ++i) {
        if (!dRegs[i]->hasFocus()) {
            dRegs[i]->setText(QString("%1").arg(static_cast<uint32_t>(D[i]), 8, 16, QLatin1Char('0')).toUpper());
        }
        if (!aRegs[i]->hasFocus()) {
            long aVal = (i == 7) ? A[(sr & 0x2000) ? 8 : 7] : A[i];
            aRegs[i]->setText(QString("%1").arg(static_cast<uint32_t>(aVal), 8, 16, QLatin1Char('0')).toUpper());
        }
    }

    if (!pcEdit->hasFocus()) {
        pcEdit->setText(QString("%1").arg(static_cast<uint32_t>(pc), 8, 16, QLatin1Char('0')).toUpper());
    }

    if (!srEdit->hasFocus()) {
        // 16-bit binary string
        QString binStr;
        binStr.reserve(16);
        for (int b = 15; b >= 0; --b) {
            binStr.append(((sr >> b) & 1) ? '1' : '0');
        }
        srEdit->setText(binStr);
    }

    if (!usEdit->hasFocus()) {
        usEdit->setText(QString("%1").arg(static_cast<uint32_t>(A[7]), 8, 16, QLatin1Char('0')).toUpper());
    }

    if (!ssEdit->hasFocus()) {
        ssEdit->setText(QString("%1").arg(static_cast<uint32_t>(A[8]), 8, 16, QLatin1Char('0')).toUpper());
    }

    cyclesEdit->setText(QString::number(cycles));

    updatingUi = false;
}

uint32_t RegistersView::getD(int idx) const
{
    if (idx < 0 || idx >= 8) return 0;
    bool ok = false;
    uint32_t val = dRegs[idx]->text().toUInt(&ok, 16);
    return ok ? val : 0;
}

uint32_t RegistersView::getA(int idx) const
{
    if (idx < 0 || idx >= 8) return 0;
    bool ok = false;
    uint32_t val = aRegs[idx]->text().toUInt(&ok, 16);
    return ok ? val : 0;
}

uint32_t RegistersView::getPC() const
{
    bool ok = false;
    uint32_t val = pcEdit->text().toUInt(&ok, 16);
    return ok ? val : 0;
}

uint16_t RegistersView::getSR() const
{
    bool ok = false;
    uint16_t val = srEdit->text().toUShort(&ok, 2);
    return ok ? val : 0;
}

uint64_t RegistersView::getCycles() const
{
    return cyclesEdit->text().toULongLong();
}

void RegistersView::onDRegisterEdited(int idx)
{
    if (updatingUi) return;
    uint32_t val = getD(idx);
    if (simManager) {
        simManager->setD(idx, val);
    }
    emit registerChanged(QString("D%1").arg(idx), val);
}

void RegistersView::onARegisterEdited(int idx)
{
    if (updatingUi) return;
    uint32_t val = getA(idx);
    if (simManager) {
        simManager->setA(idx, val);
    }
    emit registerChanged(QString("A%1").arg(idx), val);
}

void RegistersView::onUSRegisterEdited()
{
    if (updatingUi) return;
    bool ok = false;
    uint32_t val = usEdit->text().toUInt(&ok, 16);
    if (ok) {
        if (simManager) {
            simManager->setUS(val);
        }
        emit registerChanged("US", val);
    }
}

void RegistersView::onSSRegisterEdited()
{
    if (updatingUi) return;
    bool ok = false;
    uint32_t val = ssEdit->text().toUInt(&ok, 16);
    if (ok) {
        if (simManager) {
            simManager->setSS(val);
        }
        emit registerChanged("SS", val);
    }
}

void RegistersView::onPCRegisterEdited()
{
    if (updatingUi) return;
    uint32_t val = getPC();
    if (simManager) {
        simManager->setPC(val);
    }
    emit registerChanged("PC", val);
}

void RegistersView::onSRRegisterEdited()
{
    if (updatingUi) return;
    uint16_t val = getSR();
    if (simManager) {
        simManager->setSR(val);
    }
    emit registerChanged("SR", val);
}

void RegistersView::onClearCyclesClicked()
{
    if (simManager) {
        simManager->clearCycles();
    }
}
