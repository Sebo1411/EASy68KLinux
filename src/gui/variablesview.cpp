#include "variablesview.h"
#include "simulatormanager.h"
#include "memoryview.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QFontDatabase>

VariablesView::VariablesView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void VariablesView::setSimulatorManager(SimulatorManager *sm)
{
    m_simManager = sm;
}

void VariablesView::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // Toolbar on top
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(6);

    auto *watchLabel = new QLabel(tr("Watch:"), this);
    toolbarLayout->addWidget(watchLabel);

    m_watchInput = new QLineEdit(this);
    m_watchInput->setPlaceholderText(tr("Label, address ($4000), or (A0)"));
    m_watchInput->setToolTip(tr("Enter a label name, memory address (e.g. $4000, 4000), or register indirect (e.g. (A0), 4(A1))"));
    toolbarLayout->addWidget(m_watchInput, 2);

    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->addItem(tr("WORD (.W)"), 2);
    m_sizeCombo->addItem(tr("BYTE (.B)"), 1);
    m_sizeCombo->addItem(tr("LONG (.L)"), 4);
    toolbarLayout->addWidget(m_sizeCombo);

    m_addBtn = new QPushButton(tr("Add Watch"), this);
    m_addBtn->setToolTip(tr("Add custom watch expression to the list"));
    toolbarLayout->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setToolTip(tr("Remove selected watch from the list"));
    toolbarLayout->addWidget(m_removeBtn);

    toolbarLayout->addSpacing(12);

    m_autoRestoreCheck = new QCheckBox(tr("Auto-restore Initials"), this);
    m_autoRestoreCheck->setChecked(true);
    m_autoRestoreCheck->setToolTip(tr("Automatically re-apply persistent initial values to memory on Simulator Reset, Reload, and Rewind"));
    toolbarLayout->addWidget(m_autoRestoreCheck);

    m_applyInitialsBtn = new QPushButton(tr("Apply Initials"), this);
    m_applyInitialsBtn->setToolTip(tr("Write all persistent initial values into memory now"));
    toolbarLayout->addWidget(m_applyInitialsBtn);

    toolbarLayout->addStretch(1);
    mainLayout->addLayout(toolbarLayout);

    // Table widget
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    QStringList headers = {
        tr("Name / Expr"),
        tr("Address"),
        tr("Type"),
        tr("Initial Value (Persistent)"),
        tr("Live Hex"),
        tr("Signed Dec"),
        tr("Unsigned / Binary")
    };
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(24);

    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_table->setFont(monoFont);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    m_table->setColumnWidth(0, 120);
    m_table->setColumnWidth(1, 95);
    m_table->setColumnWidth(2, 95);
    m_table->setColumnWidth(3, 160);
    m_table->setColumnWidth(4, 95);
    m_table->setColumnWidth(5, 95);

    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { background-color: #1E1E1E; color: #D4D4D4; gridline-color: #333337; border: 1px solid #333337; }"
        "QTableWidget::item:selected { background-color: #094771; color: #FFFFFF; }"
        "QHeaderView::section { background-color: #252526; color: #CCCCCC; padding: 4px; border: 1px solid #333337; font-weight: bold; }"
    ));

    mainLayout->addWidget(m_table);

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &VariablesView::onAddWatchClicked);
    connect(m_watchInput, &QLineEdit::returnPressed, this, &VariablesView::onAddWatchClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &VariablesView::onRemoveWatchClicked);
    connect(m_applyInitialsBtn, &QPushButton::clicked, this, &VariablesView::onApplyInitialsClicked);
    connect(m_table, &QTableWidget::cellChanged, this, &VariablesView::onTableCellChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &VariablesView::onTableItemDoubleClicked);
}

void VariablesView::setDiscoveredVariables(const QList<VariableEntry> &vars)
{
    // Preserve custom watches that user added manually
    QList<VariableEntry> customWatches;
    for (const auto &v : m_variables) {
        if (v.isCustomWatch) {
            customWatches.append(v);
        }
    }

    m_variables = vars;
    m_variables.append(customWatches);

    rebuildTable();

    if (m_autoRestoreCheck && m_autoRestoreCheck->isChecked()) {
        applyPersistentInitialValues();
    } else {
        updateVariables();
    }
}

void VariablesView::setPersistentInitialValue(const QString &varName, const QString &valStr)
{
    m_persistentInitialValues[varName.trimmed().toUpper()] = valStr.trimmed();
}

QString VariablesView::getPersistentInitialValue(const QString &varName) const
{
    return m_persistentInitialValues.value(varName.trimmed().toUpper());
}

void VariablesView::applyPersistentInitialValues()
{
    for (auto &entry : m_variables) {
        QString key = entry.name.trimmed().toUpper();
        if (m_persistentInitialValues.contains(key)) {
            QString valStr = m_persistentInitialValues.value(key);
            uint32_t val = 0;
            if (parseValueString(valStr, entry.byteSize, val)) {
                writeMemoryValue(entry.address, entry.byteSize, val);
            }
        }
    }
    updateVariables();
}

void VariablesView::clear()
{
    m_variables.clear();
    m_lastValues.clear();
    m_updatingTable = true;
    m_table->setRowCount(0);
    m_updatingTable = false;
}

void VariablesView::addWatchExpression(const QString &expr, int byteSize)
{
    QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) return;

    VariableEntry entry;
    entry.name = trimmed;
    entry.customExpr = trimmed;
    entry.isCustomWatch = true;
    entry.byteSize = byteSize;
    entry.elementCount = 1;
    entry.address = 0;

    m_variables.append(entry);
    rebuildTable();
    updateVariables();
}

void VariablesView::removeWatch(int index)
{
    if (index >= 0 && index < m_variables.size()) {
        m_variables.removeAt(index);
        rebuildTable();
        updateVariables();
    }
}

void VariablesView::onAddWatchClicked()
{
    QString expr = m_watchInput->text().trimmed();
    if (expr.isEmpty()) return;

    int sizeBytes = m_sizeCombo->currentData().toInt();
    if (sizeBytes <= 0) sizeBytes = 2;

    addWatchExpression(expr, sizeBytes);
    m_watchInput->clear();
}

void VariablesView::onRemoveWatchClicked()
{
    int row = m_table->currentRow();
    if (row >= 0 && row < m_variables.size()) {
        removeWatch(row);
    }
}

void VariablesView::onApplyInitialsClicked()
{
    applyPersistentInitialValues();
}

void VariablesView::rebuildTable()
{
    m_updatingTable = true;
    m_table->setRowCount(m_variables.size());

    for (int r = 0; r < m_variables.size(); ++r) {
        const auto &entry = m_variables[r];

        // Col 0: Name / Expr
        auto *nameItem = new QTableWidgetItem(entry.name);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (entry.isCustomWatch) {
            nameItem->setForeground(QColor("#4EC9B0")); // Cyan for watch
        } else {
            nameItem->setForeground(QColor("#9CDCFE")); // Blue for variables
        }
        m_table->setItem(r, 0, nameItem);

        // Col 1: Address
        QString addrStr = QString("$%1").arg(entry.address, 8, 16, QLatin1Char('0')).toUpper();
        auto *addrItem = new QTableWidgetItem(addrStr);
        addrItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        addrItem->setForeground(QColor("#DCDCAA"));
        addrItem->setToolTip(tr("Double-click to navigate in Memory View"));
        m_table->setItem(r, 1, addrItem);

        // Col 2: Type
        auto *typeCombo = new QComboBox(m_table);
        typeCombo->addItem(tr("BYTE (.B)"), 1);
        typeCombo->addItem(tr("WORD (.W)"), 2);
        typeCombo->addItem(tr("LONG (.L)"), 4);
        int idx = (entry.byteSize == 1) ? 0 : ((entry.byteSize == 4) ? 2 : 1);
        typeCombo->setCurrentIndex(idx);
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, r](int index) {
            if (r < m_variables.size()) {
                int newSize = (index == 0) ? 1 : ((index == 2) ? 4 : 2);
                m_variables[r].byteSize = newSize;
                updateVariables();
            }
        });
        m_table->setCellWidget(r, 2, typeCombo);

        // Col 3: Initial Value (Persistent)
        QString initStr = m_persistentInitialValues.value(entry.name.trimmed().toUpper());
        auto *initItem = new QTableWidgetItem(initStr);
        initItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        initItem->setForeground(QColor("#CE9178"));
        initItem->setToolTip(tr("Persistent value: re-applied on Reset/Reload/Rewind. Supports Hex ($1234), Dec (#100, -5), Binary (%1010)"));
        m_table->setItem(r, 3, initItem);

        // Col 4: Live Hex
        auto *hexItem = new QTableWidgetItem();
        hexItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        hexItem->setForeground(QColor("#B5CEA8"));
        hexItem->setToolTip(tr("Live Hex value (editable)"));
        m_table->setItem(r, 4, hexItem);

        // Col 5: Signed Dec
        auto *decItem = new QTableWidgetItem();
        decItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        decItem->setForeground(QColor("#B5CEA8"));
        decItem->setToolTip(tr("Signed decimal value (editable)"));
        m_table->setItem(r, 5, decItem);

        // Col 6: Unsigned / Binary
        auto *binItem = new QTableWidgetItem();
        binItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        binItem->setForeground(QColor("#808080"));
        m_table->setItem(r, 6, binItem);
    }

    m_updatingTable = false;
}

void VariablesView::updateVariables()
{
    if (!m_simManager) return;
    m_updatingTable = true;

    for (int r = 0; r < m_variables.size(); ++r) {
        auto &entry = m_variables[r];

        // Evaluate address if custom watch
        if (entry.isCustomWatch) {
            uint32_t resolvedAddr = entry.address;
            static const QRegularExpression regIndirectRe(QStringLiteral("^(?:([+-]?\\d+)\\s*)?\\(\\s*([AaDd])([0-7])\\s*\\)$"));
            auto match = regIndirectRe.match(entry.customExpr);
            if (match.hasMatch()) {
                int offset = match.captured(1).isEmpty() ? 0 : match.captured(1).toInt();
                QChar regType = match.captured(2).toUpper().at(0);
                int regNum = match.captured(3).toInt();
                uint32_t base = 0;
                if (regType == 'A') {
                    base = m_simManager->getA()[regNum];
                } else {
                    base = m_simManager->getD()[regNum];
                }
                resolvedAddr = static_cast<uint32_t>(base + offset);
            } else if (m_memoryView) {
                uint32_t outA = 0;
                if (m_memoryView->resolveAddressExpression(entry.customExpr, outA)) {
                    resolvedAddr = outA;
                }
            } else {
                bool ok = false;
                QString s = entry.customExpr.trimmed();
                if (s.startsWith('$')) s = s.mid(1);
                uint32_t outA = s.toUInt(&ok, 16);
                if (ok) resolvedAddr = outA;
            }
            entry.address = resolvedAddr;

            auto *addrItem = m_table->item(r, 1);
            if (addrItem) {
                addrItem->setText(QString("$%1").arg(entry.address, 8, 16, QLatin1Char('0')).toUpper());
            }
        }

        uint32_t val = readMemoryValue(entry.address, entry.byteSize);
        bool hasLast = m_lastValues.contains(entry.address);
        bool changed = hasLast && (m_lastValues.value(entry.address) != val);
        m_lastValues[entry.address] = val;

        // Format Live Hex
        QString hexText;
        if (entry.byteSize == 1) {
            hexText = QString("$%1").arg(val & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
        } else if (entry.byteSize == 2) {
            hexText = QString("$%1").arg(val & 0xFFFF, 4, 16, QLatin1Char('0')).toUpper();
        } else {
            hexText = QString("$%1").arg(val, 8, 16, QLatin1Char('0')).toUpper();
        }

        // Format Signed Dec
        QString decText;
        if (entry.byteSize == 1) {
            decText = QString::number(static_cast<int8_t>(val & 0xFF));
        } else if (entry.byteSize == 2) {
            decText = QString::number(static_cast<int16_t>(val & 0xFFFF));
        } else {
            decText = QString::number(static_cast<int32_t>(val));
        }

        // Format Unsigned / Binary
        int bits = entry.byteSize * 8;
        QString binDigits = QString("%1").arg(val, bits, 2, QLatin1Char('0'));
        QString binText = QString("%1 (%%%2)").arg(val).arg(binDigits);

        auto *hexItem = m_table->item(r, 4);
        auto *decItem = m_table->item(r, 5);
        auto *binItem = m_table->item(r, 6);

        if (hexItem) hexItem->setText(hexText);
        if (decItem) decItem->setText(decText);
        if (binItem) binItem->setText(binText);

        QColor fgColor = changed ? QColor("#FFCC00") : QColor("#B5CEA8");
        QColor bgColor = changed ? QColor("#3D2E00") : QColor(Qt::transparent);

        QFont itemFont = m_table->font();
        itemFont.setBold(changed);

        if (hexItem) {
            hexItem->setForeground(fgColor);
            hexItem->setBackground(bgColor);
            hexItem->setFont(itemFont);
        }
        if (decItem) {
            decItem->setForeground(fgColor);
            decItem->setBackground(bgColor);
            decItem->setFont(itemFont);
        }
        if (binItem) {
            binItem->setForeground(changed ? fgColor : QColor("#808080"));
            binItem->setBackground(bgColor);
            binItem->setFont(itemFont);
        }
    }

    m_updatingTable = false;
}

void VariablesView::onTableCellChanged(int row, int col)
{
    if (m_updatingTable || row < 0 || row >= m_variables.size()) return;

    auto &entry = m_variables[row];
    auto *item = m_table->item(row, col);
    if (!item) return;

    QString text = item->text().trimmed();

    if (col == 3) {
        // Initial Value (Persistent)
        m_persistentInitialValues[entry.name.trimmed().toUpper()] = text;
        uint32_t val = 0;
        if (parseValueString(text, entry.byteSize, val)) {
            writeMemoryValue(entry.address, entry.byteSize, val);
            updateVariables();
        }
    } else if (col == 4) {
        // Live Hex edited
        bool ok = false;
        QString s = text;
        if (s.startsWith('$')) s = s.mid(1);
        if (s.startsWith("0x", Qt::CaseInsensitive)) s = s.mid(2);
        uint32_t val = s.toUInt(&ok, 16);
        if (ok) {
            writeMemoryValue(entry.address, entry.byteSize, val);
            updateVariables();
        }
    } else if (col == 5) {
        // Signed Dec edited
        bool ok = false;
        qint64 signedVal = text.toLongLong(&ok, 10);
        if (ok) {
            uint32_t val = static_cast<uint32_t>(signedVal);
            writeMemoryValue(entry.address, entry.byteSize, val);
            updateVariables();
        }
    }
}

void VariablesView::onTableItemDoubleClicked(QTableWidgetItem *item)
{
    if (!item) return;
    int row = item->row();
    if (row >= 0 && row < m_variables.size()) {
        uint32_t targetAddr = m_variables[row].address;
        if (m_memoryView) {
            m_memoryView->setBaseAddress(targetAddr);
        }
    }
}

uint32_t VariablesView::readMemoryValue(uint32_t addr, int sizeBytes) const
{
    if (!m_simManager) return 0;
    const auto *mem = reinterpret_cast<const unsigned char*>(m_simManager->getMemory());
    uint32_t memSize = m_simManager->getMemorySize();
    if (!mem || addr >= memSize) return 0;

    if (sizeBytes == 1) {
        return mem[addr];
    } else if (sizeBytes == 2) {
        if (addr + 1 >= memSize) return mem[addr];
        return (static_cast<uint32_t>(mem[addr]) << 8) | mem[addr + 1];
    } else if (sizeBytes == 4) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            if (addr + i < memSize) {
                v = (v << 8) | mem[addr + i];
            }
        }
        return v;
    }
    return 0;
}

void VariablesView::writeMemoryValue(uint32_t addr, int sizeBytes, uint32_t val)
{
    if (!m_simManager) return;
    auto *mem = reinterpret_cast<unsigned char*>(m_simManager->getMemory());
    uint32_t memSize = m_simManager->getMemorySize();
    if (!mem || addr >= memSize) return;

    if (sizeBytes == 1) {
        mem[addr] = static_cast<unsigned char>(val & 0xFF);
    } else if (sizeBytes == 2) {
        if (addr + 1 < memSize) {
            mem[addr] = static_cast<unsigned char>((val >> 8) & 0xFF);
            mem[addr + 1] = static_cast<unsigned char>(val & 0xFF);
        }
    } else if (sizeBytes == 4) {
        for (int i = 0; i < 4; ++i) {
            if (addr + i < memSize) {
                mem[addr + i] = static_cast<unsigned char>((val >> ((3 - i) * 8)) & 0xFF);
            }
        }
    }

    emit memoryModified(addr);
}

bool VariablesView::parseValueString(const QString &str, int sizeBytes, uint32_t &outVal) const
{
    QString s = str.trimmed();
    if (s.isEmpty()) return false;

    // Binary format %101010
    if (s.startsWith('%')) {
        bool ok = false;
        outVal = s.mid(1).trimmed().toUInt(&ok, 2);
        return ok;
    }

    // Decimal with # prefix, e.g. #100 or #-5
    if (s.startsWith('#')) {
        bool ok = false;
        qint64 dec = s.mid(1).trimmed().toLongLong(&ok, 10);
        if (!ok) return false;
        if (sizeBytes == 1) outVal = static_cast<uint32_t>(dec & 0xFF);
        else if (sizeBytes == 2) outVal = static_cast<uint32_t>(dec & 0xFFFF);
        else outVal = static_cast<uint32_t>(dec & 0xFFFFFFFF);
        return true;
    }

    // Hex with $ or 0x prefix
    if (s.startsWith('$')) {
        bool ok = false;
        outVal = s.mid(1).trimmed().toUInt(&ok, 16);
        return ok;
    }
    if (s.startsWith("0x", Qt::CaseInsensitive)) {
        bool ok = false;
        outVal = s.mid(2).trimmed().toUInt(&ok, 16);
        return ok;
    }

    // Signed decimal if starts with '-' or '+'
    if (s.startsWith('-') || s.startsWith('+')) {
        bool ok = false;
        qint64 dec = s.toLongLong(&ok, 10);
        if (ok) {
            if (sizeBytes == 1) outVal = static_cast<uint32_t>(dec & 0xFF);
            else if (sizeBytes == 2) outVal = static_cast<uint32_t>(dec & 0xFFFF);
            else outVal = static_cast<uint32_t>(dec & 0xFFFFFFFF);
            return true;
        }
    }

    // Try hex first if string contains a..f
    bool hasHexAlpha = false;
    for (const QChar &ch : s) {
        if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
            hasHexAlpha = true;
            break;
        }
    }

    if (hasHexAlpha) {
        bool ok = false;
        outVal = s.toUInt(&ok, 16);
        return ok;
    }

    // Otherwise try decimal, fallback to hex
    bool okDec = false;
    qint64 dec = s.toLongLong(&okDec, 10);
    if (okDec) {
        if (sizeBytes == 1) outVal = static_cast<uint32_t>(dec & 0xFF);
        else if (sizeBytes == 2) outVal = static_cast<uint32_t>(dec & 0xFFFF);
        else outVal = static_cast<uint32_t>(dec & 0xFFFFFFFF);
        return true;
    }

    bool okHex = false;
    outVal = s.toUInt(&okHex, 16);
    return okHex;
}
