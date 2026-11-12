#include "breakpointdialog.h"
#include "simulatormanager.h"
#include "../simulator/def.h"
#include "../simulator/extern.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <stack>

BreakpointDialog::BreakpointDialog(SimulatorManager *simManager, QWidget *parent)
    : QDialog(parent)
    , m_simManager(simManager)
{
    setWindowTitle(tr("Advanced Breakpoints & Memory Watchpoints"));
    resize(780, 520);
    setupUi();
    reloadFromSimulator();
}

void BreakpointDialog::setupUi()
{
    setStyleSheet(
        "QDialog { background-color: #1E1E1E; color: #D4D4D4; }"
        "QGroupBox { border: 1px solid #3C3C3C; border-radius: 4px; margin-top: 8px; padding-top: 8px; color: #CCCCCC; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QTableWidget { background-color: #252526; color: #D4D4D4; gridline-color: #3C3C3C; border: 1px solid #3C3C3C; }"
        "QHeaderView::section { background-color: #2D2D30; color: #CCCCCC; border: 1px solid #3C3C3C; padding: 4px; }"
        "QLineEdit, QComboBox, QSpinBox { background-color: #252526; color: #D4D4D4; border: 1px solid #3C3C3C; padding: 3px 6px; }"
        "QPushButton { background-color: #333337; color: #E0E0E0; border: 1px solid #454545; padding: 5px 12px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #3E3E42; }"
    );

    auto *mainLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    // =========================================================================
    // Tab 1: PC / Register Conditions (breakPoints[0..49])
    // =========================================================================
    auto *regTab = new QWidget(this);
    auto *regLayout = new QVBoxLayout(regTab);

    m_regTable = new QTableWidget(0, 6, regTab);
    m_regTable->setHorizontalHeaderLabels({tr("ID"), tr("Target"), tr("Op"), tr("Value (Hex)"), tr("Size"), tr("Enabled")});
    m_regTable->horizontalHeader()->setStretchLastSection(true);
    m_regTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_regTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_regTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    regLayout->addWidget(m_regTable);

    auto *regFormBox = new QGroupBox(tr("Add PC / Register Condition"), regTab);
    auto *regForm = new QHBoxLayout(regFormBox);

    m_regTargetCombo = new QComboBox(regFormBox);
    m_regTargetCombo->addItem(QStringLiteral("PC"), PC_TYPE_ID);
    for (int i = 0; i < 8; ++i) {
        m_regTargetCombo->addItem(QString("D%1").arg(i), D0_TYPE_ID + i);
    }
    for (int i = 0; i < 8; ++i) {
        m_regTargetCombo->addItem(QString("A%1").arg(i), A0_TYPE_ID + i);
    }

    m_regOpCombo = new QComboBox(regFormBox);
    m_regOpCombo->addItem(QStringLiteral("=="), EQUAL_OP);
    m_regOpCombo->addItem(QStringLiteral("!="), NOT_EQUAL_OP);
    m_regOpCombo->addItem(QStringLiteral(">"), GT_OP);
    m_regOpCombo->addItem(QStringLiteral(">="), GT_EQUAL_OP);
    m_regOpCombo->addItem(QStringLiteral("<"), LT_OP);
    m_regOpCombo->addItem(QStringLiteral("<="), LT_EQUAL_OP);
    m_regOpCombo->addItem(QStringLiteral("Any (NA)"), NA_OP);

    m_regValEdit = new QLineEdit(regFormBox);
    m_regValEdit->setPlaceholderText(QStringLiteral("1000"));
    m_regValEdit->setMaximumWidth(110);

    m_regSizeCombo = new QComboBox(regFormBox);
    m_regSizeCombo->addItem(tr("Long"), LONG_SIZE);
    m_regSizeCombo->addItem(tr("Word"), WORD_SIZE);
    m_regSizeCombo->addItem(tr("Byte"), BYTE_SIZE);

    m_regCountSpin = new QSpinBox(regFormBox);
    m_regCountSpin->setRange(1, 1000000);
    m_regCountSpin->setValue(1);
    m_regCountSpin->setPrefix(tr("Hits: "));

    m_regAutoExprCheck = new QCheckBox(tr("Auto-Activate"), regFormBox);
    m_regAutoExprCheck->setChecked(true);
    m_regAutoExprCheck->setToolTip(tr("Automatically create and enable an expression for this condition"));

    auto *addRegBtn = new QPushButton(tr("Add"), regFormBox);
    auto *delRegBtn = new QPushButton(tr("Remove Selected"), regFormBox);

    regForm->addWidget(m_regTargetCombo);
    regForm->addWidget(m_regOpCombo);
    regForm->addWidget(m_regValEdit);
    regForm->addWidget(m_regSizeCombo);
    regForm->addWidget(m_regCountSpin);
    regForm->addWidget(m_regAutoExprCheck);
    regForm->addWidget(addRegBtn);
    regForm->addWidget(delRegBtn);
    regLayout->addWidget(regFormBox);

    m_tabs->addTab(regTab, tr("PC / Register Conditions"));

    // =========================================================================
    // Tab 2: Memory Watchpoints (breakPoints[50..99])
    // =========================================================================
    auto *addrTab = new QWidget(this);
    auto *addrLayout = new QVBoxLayout(addrTab);

    m_addrTable = new QTableWidget(0, 7, addrTab);
    m_addrTable->setHorizontalHeaderLabels({tr("ID"), tr("Address (Hex)"), tr("Access"), tr("Op"), tr("Value (Hex)"), tr("Size"), tr("Enabled")});
    m_addrTable->horizontalHeader()->setStretchLastSection(true);
    m_addrTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_addrTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_addrTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    addrLayout->addWidget(m_addrTable);

    auto *addrFormBox = new QGroupBox(tr("Add Memory Address Watchpoint"), addrTab);
    auto *addrForm = new QHBoxLayout(addrFormBox);

    m_addrEdit = new QLineEdit(addrFormBox);
    m_addrEdit->setPlaceholderText(QStringLiteral("Addr (hex, e.g. 2000)"));
    m_addrEdit->setMaximumWidth(120);

    m_addrRwCombo = new QComboBox(addrFormBox);
    m_addrRwCombo->addItem(tr("Read/Write"), RW_TYPE);
    m_addrRwCombo->addItem(tr("Read"), READ_TYPE);
    m_addrRwCombo->addItem(tr("Write"), WRITE_TYPE);
    m_addrRwCombo->addItem(tr("Ignore R/W (Value only)"), NA_TYPE);

    m_addrOpCombo = new QComboBox(addrFormBox);
    m_addrOpCombo->addItem(QStringLiteral("Any Value (NA)"), NA_OP);
    m_addrOpCombo->addItem(QStringLiteral("=="), EQUAL_OP);
    m_addrOpCombo->addItem(QStringLiteral("!="), NOT_EQUAL_OP);
    m_addrOpCombo->addItem(QStringLiteral(">"), GT_OP);
    m_addrOpCombo->addItem(QStringLiteral(">="), GT_EQUAL_OP);
    m_addrOpCombo->addItem(QStringLiteral("<"), LT_OP);
    m_addrOpCombo->addItem(QStringLiteral("<="), LT_EQUAL_OP);

    m_addrValEdit = new QLineEdit(addrFormBox);
    m_addrValEdit->setPlaceholderText(QStringLiteral("0"));
    m_addrValEdit->setMaximumWidth(90);

    m_addrSizeCombo = new QComboBox(addrFormBox);
    m_addrSizeCombo->addItem(tr("Byte"), BYTE_SIZE);
    m_addrSizeCombo->addItem(tr("Word"), WORD_SIZE);
    m_addrSizeCombo->addItem(tr("Long"), LONG_SIZE);

    m_addrCountSpin = new QSpinBox(addrFormBox);
    m_addrCountSpin->setRange(1, 1000000);
    m_addrCountSpin->setValue(1);
    m_addrCountSpin->setPrefix(tr("Hits: "));

    m_addrAutoExprCheck = new QCheckBox(tr("Auto-Activate"), addrFormBox);
    m_addrAutoExprCheck->setChecked(true);

    auto *addAddrBtn = new QPushButton(tr("Add"), addrFormBox);
    auto *delAddrBtn = new QPushButton(tr("Remove Selected"), addrFormBox);

    addrForm->addWidget(m_addrEdit);
    addrForm->addWidget(m_addrRwCombo);
    addrForm->addWidget(m_addrOpCombo);
    addrForm->addWidget(m_addrValEdit);
    addrForm->addWidget(m_addrSizeCombo);
    addrForm->addWidget(m_addrCountSpin);
    addrForm->addWidget(m_addrAutoExprCheck);
    addrForm->addWidget(addAddrBtn);
    addrForm->addWidget(delAddrBtn);
    addrLayout->addWidget(addrFormBox);

    m_tabs->addTab(addrTab, tr("Memory Watchpoints"));

    // =========================================================================
    // Tab 3: Combined Expressions (bpExpressions[0..MAX_BP_EXPR-1])
    // =========================================================================
    auto *exprTab = new QWidget(this);
    auto *exprLayout = new QVBoxLayout(exprTab);

    m_exprTable = new QTableWidget(0, 4, exprTab);
    m_exprTable->setHorizontalHeaderLabels({tr("Slot"), tr("Expression"), tr("Hit Count"), tr("Enabled")});
    m_exprTable->horizontalHeader()->setStretchLastSection(true);
    m_exprTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_exprTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_exprTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exprLayout->addWidget(m_exprTable);

    m_exprHelpLabel = new QLabel(
        tr("Combine conditions with AND / OR (or && / ||) and parentheses, referencing R0..R49 (PC/Reg) and A0..A49 (Memory). Example: (R0 AND A0) OR R1"),
        exprTab);
    m_exprHelpLabel->setWordWrap(true);
    m_exprHelpLabel->setStyleSheet("color: #9CDCFE;");
    exprLayout->addWidget(m_exprHelpLabel);

    auto *exprFormBox = new QGroupBox(tr("Add / Update Expression"), exprTab);
    auto *exprForm = new QHBoxLayout(exprFormBox);

    m_exprEdit = new QLineEdit(exprFormBox);
    m_exprEdit->setPlaceholderText(tr("e.g. R0 AND A0"));
    m_exprCountSpin = new QSpinBox(exprFormBox);
    m_exprCountSpin->setRange(1, 1000000);
    m_exprCountSpin->setValue(1);
    m_exprCountSpin->setPrefix(tr("Hits: "));

    auto *addExprBtn = new QPushButton(tr("Add Expression"), exprFormBox);
    auto *delExprBtn = new QPushButton(tr("Remove Selected"), exprFormBox);

    exprForm->addWidget(m_exprEdit, 1);
    exprForm->addWidget(m_exprCountSpin);
    exprForm->addWidget(addExprBtn);
    exprForm->addWidget(delExprBtn);
    exprLayout->addWidget(exprFormBox);

    m_tabs->addTab(exprTab, tr("Expressions"));
    mainLayout->addWidget(m_tabs);

    // Bottom buttons
    auto *bottomRow = new QHBoxLayout();
    auto *clearAllBtn = new QPushButton(tr("Clear All Conditions & Expressions"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    bottomRow->addWidget(clearAllBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(closeBtn);
    mainLayout->addLayout(bottomRow);

    connect(addRegBtn, &QPushButton::clicked, this, &BreakpointDialog::onAddRegBreakpoint);
    connect(delRegBtn, &QPushButton::clicked, this, &BreakpointDialog::onRemoveRegBreakpoint);
    connect(addAddrBtn, &QPushButton::clicked, this, &BreakpointDialog::onAddAddrWatchpoint);
    connect(delAddrBtn, &QPushButton::clicked, this, &BreakpointDialog::onRemoveAddrWatchpoint);
    connect(addExprBtn, &QPushButton::clicked, this, &BreakpointDialog::onAddExpression);
    connect(delExprBtn, &QPushButton::clicked, this, &BreakpointDialog::onRemoveExpression);
    connect(clearAllBtn, &QPushButton::clicked, this, &BreakpointDialog::onClearAll);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_regTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (col != 5) return;
        QTableWidgetItem *idItem = m_regTable->item(row, 0);
        QTableWidgetItem *enItem = m_regTable->item(row, 5);
        if (!idItem || !enItem) return;
        const int idx = idItem->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < ADDR_ID_OFFSET) {
            breakPoints[idx].isEnabled(enItem->checkState() == Qt::Checked);
            notifyChanged();
        }
    });

    connect(m_addrTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (col != 6) return;
        QTableWidgetItem *idItem = m_addrTable->item(row, 0);
        QTableWidgetItem *enItem = m_addrTable->item(row, 6);
        if (!idItem || !enItem) return;
        const int idx = idItem->data(Qt::UserRole).toInt();
        if (idx >= ADDR_ID_OFFSET && idx < MAX_BPOINTS) {
            breakPoints[idx].isEnabled(enItem->checkState() == Qt::Checked);
            notifyChanged();
        }
    });

    connect(m_exprTable, &QTableWidget::cellChanged, this, [this](int row, int col) {
        if (col != 3) return;
        QTableWidgetItem *idItem = m_exprTable->item(row, 0);
        QTableWidgetItem *enItem = m_exprTable->item(row, 3);
        if (!idItem || !enItem) return;
        const int idx = idItem->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < exprCount && idx < MAX_BP_EXPR) {
            bpExpressions[idx].isEnabled(enItem->checkState() == Qt::Checked);
            notifyChanged();
        }
    });
}

void BreakpointDialog::reloadFromSimulator()
{
    refreshTables();
}

int BreakpointDialog::findFreeBPointSlot(int startIdx, int endIdx)
{
    const int lo = qMax(0, startIdx);
    const int hi = qMin(MAX_BPOINTS - 1, endIdx);
    for (int i = lo; i <= hi; ++i) {
        if (breakPoints[i].getId() < 0) {
            return i;
        }
    }
    return -1;
}

int BreakpointDialog::ensureSimpleExpressionForBPoint(int bpIndex, int hitCount)
{
    if (bpIndex < 0 || bpIndex >= MAX_BPOINTS) return -1;

    const QString exprName = (bpIndex < ADDR_ID_OFFSET)
        ? QString("R%1").arg(bpIndex)
        : QString("A%1").arg(bpIndex - ADDR_ID_OFFSET);

    // Reuse if an identical single-token expression already exists
    for (int i = 0; i < exprCount && i < MAX_BP_EXPR; ++i) {
        int pf[MAX_LB_NODES];
        int pCount = 0;
        bpExpressions[i].getPostfixExpr(pf, pCount);
        if (pCount == 1 && pf[0] == bpIndex) {
            bpExpressions[i].setCount(hitCount > 0 ? hitCount : 1);
            bpExpressions[i].isEnabled(true);
            bpCountCond[i] = 0;
            return i;
        }
    }

    if (exprCount >= MAX_BP_EXPR) return -1;

    const int slot = exprCount++;
    int tok = bpIndex;
    bpExpressions[slot].setId(slot);
    bpExpressions[slot].setInfixExpr(&tok, 1);
    bpExpressions[slot].setPostfixExpr(&tok, 1);
    bpExpressions[slot].setExprString(exprName.toStdString());
    bpExpressions[slot].setCount(hitCount > 0 ? hitCount : 1);
    bpExpressions[slot].isEnabled(true);
    bpCountCond[slot] = 0;
    return slot;
}

bool BreakpointDialog::parseExpression(const QString &text,
                                       QVector<int> &infixOut,
                                       QVector<int> &postfixOut,
                                       QString *errorOut)
{
    infixOut.clear();
    postfixOut.clear();

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (errorOut) *errorOut = tr("Expression cannot be empty.");
        return false;
    }

    // Tokenize identifiers, logical operators, and parentheses
    static const QRegularExpression tokenRe(
        QStringLiteral("\\s*(?:(&&|\\|\\||[()&|])|([A-Za-z][A-Za-z0-9]*))\\s*|(\\S+)"));

    QVector<int> tokens;
    int pos = 0;
    while (pos < trimmed.length()) {
        const QRegularExpressionMatch m = tokenRe.match(trimmed, pos);
        if (!m.hasMatch() || m.capturedStart() != pos) {
            if (errorOut) *errorOut = tr("Invalid token near: %1").arg(trimmed.mid(pos, 16));
            return false;
        }
        pos = m.capturedEnd();

        if (!m.captured(3).isEmpty()) {
            if (errorOut) *errorOut = tr("Unexpected character: %1").arg(m.captured(3));
            return false;
        }

        const QString sym = m.captured(1);
        const QString word = m.captured(2).toUpper();

        if (sym == QLatin1String("(")) {
            tokens.append(LPAREN);
        } else if (sym == QLatin1String(")")) {
            tokens.append(RPAREN);
        } else if (sym == QLatin1String("&&") || sym == QLatin1String("&") || word == QLatin1String("AND")) {
            tokens.append(MAX_BPOINTS + AND_OP);
        } else if (sym == QLatin1String("||") || sym == QLatin1String("|") || word == QLatin1String("OR")) {
            tokens.append(MAX_BPOINTS + OR_OP);
        } else if (!word.isEmpty() && (word.startsWith('R') || word.startsWith('P') || word.startsWith('A'))) {
            bool ok = false;
            const int num = word.mid(1).toInt(&ok);
            const int maxPerGroup = ADDR_ID_OFFSET;
            if (!ok || num < 0 || num >= maxPerGroup) {
                if (errorOut) *errorOut = tr("Out-of-range breakpoint reference: %1 (use 0..%2)").arg(word).arg(maxPerGroup - 1);
                return false;
            }
            const int bpId = (word.startsWith('A')) ? (ADDR_ID_OFFSET + num) : num;
            tokens.append(bpId);
        } else {
            if (errorOut) *errorOut = tr("Unknown identifier: %1").arg(word.isEmpty() ? sym : word);
            return false;
        }
    }

    if (tokens.isEmpty() || tokens.size() > MAX_LB_NODES) {
        if (errorOut) *errorOut = tr("Expression must have 1 to %1 tokens.").arg(MAX_LB_NODES);
        return false;
    }

    // Validate adjacent token grammar and convert infix -> postfix via shunting-yard
    auto isOperand = [](int t) { return t >= 0 && t < MAX_BPOINTS; };
    auto isOp = [](int t) { return t == (MAX_BPOINTS + AND_OP) || t == (MAX_BPOINTS + OR_OP); };
    auto precedence = [](int op) { return (op == (MAX_BPOINTS + AND_OP)) ? 2 : 1; };

    bool expectOperand = true;
    std::stack<int> opStack;

    for (int tok : tokens) {
        if (expectOperand) {
            if (isOperand(tok)) {
                postfixOut.append(tok);
                expectOperand = false;
            } else if (tok == LPAREN) {
                opStack.push(tok);
            } else {
                if (errorOut) *errorOut = tr("Expected breakpoint operand (e.g. R0 or A0) or '('.");
                return false;
            }
        } else {
            if (isOp(tok)) {
                while (!opStack.empty() && opStack.top() != LPAREN && precedence(opStack.top()) >= precedence(tok)) {
                    postfixOut.append(opStack.top());
                    opStack.pop();
                }
                opStack.push(tok);
                expectOperand = true;
            } else if (tok == RPAREN) {
                bool foundLeft = false;
                while (!opStack.empty()) {
                    const int top = opStack.top();
                    opStack.pop();
                    if (top == LPAREN) {
                        foundLeft = true;
                        break;
                    }
                    postfixOut.append(top);
                }
                if (!foundLeft) {
                    if (errorOut) *errorOut = tr("Mismatched closing parenthesis ')'.");
                    return false;
                }
            } else {
                if (errorOut) *errorOut = tr("Expected operator (AND / OR) or ')'.");
                return false;
            }
        }
    }

    if (expectOperand) {
        if (errorOut) *errorOut = tr("Expression ends unexpectedly; missing operand.");
        return false;
    }

    while (!opStack.empty()) {
        const int top = opStack.top();
        opStack.pop();
        if (top == LPAREN) {
            if (errorOut) *errorOut = tr("Unclosed opening parenthesis '('.");
            return false;
        }
        postfixOut.append(top);
    }

    if (postfixOut.size() > MAX_LB_NODES) {
        if (errorOut) *errorOut = tr("Postfix expression exceeds %1 nodes.").arg(MAX_LB_NODES);
        return false;
    }

    infixOut = tokens;
    return true;
}

bool BreakpointDialog::parseHexValue(const QString &text, long &outVal)
{
    QString clean = text.trimmed();
    if (clean.startsWith('$')) clean = clean.mid(1);
    else if (clean.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) clean = clean.mid(2);
    if (clean.isEmpty()) {
        outVal = 0;
        return true;
    }
    bool ok = false;
    const qlonglong v = clean.toLongLong(&ok, 16);
    if (!ok) return false;
    outVal = static_cast<long>(static_cast<uint32_t>(v));
    return true;
}

QString BreakpointDialog::regTypeIdName(long typeId)
{
    if (typeId == PC_TYPE_ID) return QStringLiteral("PC");
    if (typeId >= D0_TYPE_ID && typeId <= D7_TYPE_ID) return QString("D%1").arg(typeId - D0_TYPE_ID);
    if (typeId >= A0_TYPE_ID && typeId <= A7_TYPE_ID) return QString("A%1").arg(typeId - A0_TYPE_ID);
    return QString("?%1").arg(typeId);
}

QString BreakpointDialog::operatorName(int op)
{
    switch (op) {
    case EQUAL_OP: return QStringLiteral("==");
    case NOT_EQUAL_OP: return QStringLiteral("!=");
    case GT_OP: return QStringLiteral(">");
    case GT_EQUAL_OP: return QStringLiteral(">=");
    case LT_OP: return QStringLiteral("<");
    case LT_EQUAL_OP: return QStringLiteral("<=");
    case NA_OP: return QStringLiteral("NA");
    default: return QStringLiteral("?");
    }
}

QString BreakpointDialog::sizeName(long sz)
{
    switch (sz) {
    case BYTE_SIZE: return QStringLiteral("Byte");
    case WORD_SIZE: return QStringLiteral("Word");
    case LONG_SIZE: return QStringLiteral("Long");
    default: return QStringLiteral("?");
    }
}

QString BreakpointDialog::rwName(int rw)
{
    switch (rw) {
    case RW_TYPE: return QStringLiteral("R/W");
    case READ_TYPE: return QStringLiteral("Read");
    case WRITE_TYPE: return QStringLiteral("Write");
    case NA_TYPE: return QStringLiteral("Value Only");
    default: return QStringLiteral("?");
    }
}

void BreakpointDialog::refreshTables()
{
    // Populate PC/Reg table
    {
        QSignalBlocker blocker(m_regTable);
        m_regTable->setRowCount(0);
        for (int i = 0; i < ADDR_ID_OFFSET; ++i) {
            if (breakPoints[i].getId() < 0) continue;
            const int row = m_regTable->rowCount();
            m_regTable->insertRow(row);

            auto *idItem = new QTableWidgetItem(QString("R%1").arg(i));
            idItem->setData(Qt::UserRole, i);
            m_regTable->setItem(row, 0, idItem);
            m_regTable->setItem(row, 1, new QTableWidgetItem(regTypeIdName(breakPoints[i].getTypeId())));
            m_regTable->setItem(row, 2, new QTableWidgetItem(operatorName(breakPoints[i].getOperator())));
            m_regTable->setItem(row, 3, new QTableWidgetItem(
                QString("%1").arg(static_cast<uint32_t>(breakPoints[i].getValue()), 8, 16, QLatin1Char('0')).toUpper()));
            m_regTable->setItem(row, 4, new QTableWidgetItem(sizeName(breakPoints[i].getSize())));

            auto *enItem = new QTableWidgetItem();
            enItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            enItem->setCheckState(breakPoints[i].isEnabled() ? Qt::Checked : Qt::Unchecked);
            m_regTable->setItem(row, 5, enItem);
        }
    }

    // Populate Memory Watchpoints table
    {
        QSignalBlocker blocker(m_addrTable);
        m_addrTable->setRowCount(0);
        for (int i = ADDR_ID_OFFSET; i < MAX_BPOINTS; ++i) {
            if (breakPoints[i].getId() < 0) continue;
            const int row = m_addrTable->rowCount();
            m_addrTable->insertRow(row);

            auto *idItem = new QTableWidgetItem(QString("A%1").arg(i - ADDR_ID_OFFSET));
            idItem->setData(Qt::UserRole, i);
            m_addrTable->setItem(row, 0, idItem);
            m_addrTable->setItem(row, 1, new QTableWidgetItem(
                QString("%1").arg(static_cast<uint32_t>(breakPoints[i].getTypeId()), 8, 16, QLatin1Char('0')).toUpper()));
            m_addrTable->setItem(row, 2, new QTableWidgetItem(rwName(breakPoints[i].getReadWrite())));
            m_addrTable->setItem(row, 3, new QTableWidgetItem(operatorName(breakPoints[i].getOperator())));
            m_addrTable->setItem(row, 4, new QTableWidgetItem(
                QString("%1").arg(static_cast<uint32_t>(breakPoints[i].getValue()), 8, 16, QLatin1Char('0')).toUpper()));
            m_addrTable->setItem(row, 5, new QTableWidgetItem(sizeName(breakPoints[i].getSize())));

            auto *enItem = new QTableWidgetItem();
            enItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            enItem->setCheckState(breakPoints[i].isEnabled() ? Qt::Checked : Qt::Unchecked);
            m_addrTable->setItem(row, 6, enItem);
        }
    }

    // Populate Expressions table
    {
        QSignalBlocker blocker(m_exprTable);
        m_exprTable->setRowCount(0);
        for (int i = 0; i < exprCount && i < MAX_BP_EXPR; ++i) {
            const int row = m_exprTable->rowCount();
            m_exprTable->insertRow(row);

            auto *idItem = new QTableWidgetItem(QString("E%1").arg(i));
            idItem->setData(Qt::UserRole, i);
            m_exprTable->setItem(row, 0, idItem);
            m_exprTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(bpExpressions[i].getExprString())));
            m_exprTable->setItem(row, 2, new QTableWidgetItem(QString::number(bpExpressions[i].getCount())));

            auto *enItem = new QTableWidgetItem();
            enItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            enItem->setCheckState(bpExpressions[i].isEnabled() ? Qt::Checked : Qt::Unchecked);
            m_exprTable->setItem(row, 3, enItem);
        }
    }
}

void BreakpointDialog::notifyChanged()
{
    if (m_simManager) {
        m_simManager->notifyBreakpointsChanged();
    }
    emit breakpointsUpdated();
}

void BreakpointDialog::onAddRegBreakpoint()
{
    const int slot = findFreeBPointSlot(0, ADDR_ID_OFFSET - 1);
    if (slot < 0) {
        QMessageBox::warning(this, tr("Breakpoints Full"), tr("Maximum number of PC/Register conditions (%1) reached.").arg(ADDR_ID_OFFSET));
        return;
    }

    long val = 0;
    if (!parseHexValue(m_regValEdit->text(), val)) {
        QMessageBox::warning(this, tr("Invalid Value"), tr("Enter a valid hexadecimal value (e.g. 1000 or $FF)."));
        return;
    }

    breakPoints[slot].setId(slot);
    breakPoints[slot].setType(PC_REG_TYPE);
    breakPoints[slot].setTypeId(m_regTargetCombo->currentData().toLongLong());
    breakPoints[slot].setOperator(m_regOpCombo->currentData().toInt());
    breakPoints[slot].setValue(val);
    breakPoints[slot].setSize(m_regSizeCombo->currentData().toLongLong());
    breakPoints[slot].setReadWrite(NA_TYPE);
    breakPoints[slot].isEnabled(true);

    if (m_regAutoExprCheck->isChecked()) {
        ensureSimpleExpressionForBPoint(slot, m_regCountSpin->value());
    }

    refreshTables();
    notifyChanged();
}

void BreakpointDialog::onRemoveRegBreakpoint()
{
    const int row = m_regTable->currentRow();
    if (row < 0) return;
    QTableWidgetItem *idItem = m_regTable->item(row, 0);
    if (!idItem) return;
    const int slot = idItem->data(Qt::UserRole).toInt();
    if (slot >= 0 && slot < ADDR_ID_OFFSET) {
        breakPoints[slot] = BPoint();
        refreshTables();
        notifyChanged();
    }
}

void BreakpointDialog::onAddAddrWatchpoint()
{
    const int slot = findFreeBPointSlot(ADDR_ID_OFFSET, MAX_BPOINTS - 1);
    if (slot < 0) {
        QMessageBox::warning(this, tr("Watchpoints Full"), tr("Maximum number of memory watchpoints (%1) reached.").arg(MAX_BPOINTS - ADDR_ID_OFFSET));
        return;
    }

    long addr = 0;
    if (m_addrEdit->text().trimmed().isEmpty() || !parseHexValue(m_addrEdit->text(), addr)) {
        QMessageBox::warning(this, tr("Invalid Address"), tr("Enter a valid hexadecimal memory address (e.g. 2000)."));
        return;
    }

    long val = 0;
    if (!parseHexValue(m_addrValEdit->text(), val)) {
        QMessageBox::warning(this, tr("Invalid Value"), tr("Enter a valid hexadecimal comparison value."));
        return;
    }

    breakPoints[slot].setId(slot);
    breakPoints[slot].setType(ADDR_TYPE);
    breakPoints[slot].setTypeId(static_cast<long>(static_cast<uint32_t>(addr) & ADDRMASK));
    breakPoints[slot].setReadWrite(m_addrRwCombo->currentData().toInt());
    breakPoints[slot].setOperator(m_addrOpCombo->currentData().toInt());
    breakPoints[slot].setValue(val);
    breakPoints[slot].setSize(m_addrSizeCombo->currentData().toLongLong());
    breakPoints[slot].isEnabled(true);

    if (m_addrAutoExprCheck->isChecked()) {
        ensureSimpleExpressionForBPoint(slot, m_addrCountSpin->value());
    }

    refreshTables();
    notifyChanged();
}

void BreakpointDialog::onRemoveAddrWatchpoint()
{
    const int row = m_addrTable->currentRow();
    if (row < 0) return;
    QTableWidgetItem *idItem = m_addrTable->item(row, 0);
    if (!idItem) return;
    const int slot = idItem->data(Qt::UserRole).toInt();
    if (slot >= ADDR_ID_OFFSET && slot < MAX_BPOINTS) {
        breakPoints[slot] = BPoint();
        refreshTables();
        notifyChanged();
    }
}

void BreakpointDialog::onAddExpression()
{
    if (exprCount >= MAX_BP_EXPR) {
        QMessageBox::warning(this, tr("Expressions Full"), tr("Maximum number of expressions (%1) reached.").arg(MAX_BP_EXPR));
        return;
    }

    QVector<int> infixTokens;
    QVector<int> postfixTokens;
    QString errMsg;
    if (!parseExpression(m_exprEdit->text(), infixTokens, postfixTokens, &errMsg)) {
        QMessageBox::warning(this, tr("Invalid Expression"), errMsg);
        return;
    }

    const int slot = exprCount++;
    bpExpressions[slot].setId(slot);
    bpExpressions[slot].setInfixExpr(infixTokens.data(), infixTokens.size());
    bpExpressions[slot].setPostfixExpr(postfixTokens.data(), postfixTokens.size());
    bpExpressions[slot].setExprString(m_exprEdit->text().trimmed().toStdString());
    bpExpressions[slot].setCount(m_exprCountSpin->value());
    bpExpressions[slot].isEnabled(true);
    bpCountCond[slot] = 0;

    m_exprEdit->clear();
    refreshTables();
    notifyChanged();
}

void BreakpointDialog::onRemoveExpression()
{
    const int row = m_exprTable->currentRow();
    if (row < 0) return;
    QTableWidgetItem *idItem = m_exprTable->item(row, 0);
    if (!idItem) return;
    const int slot = idItem->data(Qt::UserRole).toInt();
    if (slot >= 0 && slot < exprCount && slot < MAX_BP_EXPR) {
        for (int i = slot; i < exprCount - 1; ++i) {
            bpExpressions[i] = bpExpressions[i + 1];
            bpExpressions[i].setId(i);
            bpCountCond[i] = bpCountCond[i + 1];
        }
        bpExpressions[exprCount - 1] = BPointExpr();
        bpCountCond[exprCount - 1] = 0;
        --exprCount;
        refreshTables();
        notifyChanged();
    }
}

void BreakpointDialog::onClearAll()
{
    for (int i = 0; i < MAX_BPOINTS; ++i) {
        breakPoints[i] = BPoint();
        bpCountCond[i] = 0;
    }
    for (int i = 0; i < MAX_BP_EXPR; ++i) {
        bpExpressions[i] = BPointExpr();
    }
    exprCount = 0;
    refreshTables();
    notifyChanged();
}
