#include "codeeditor.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStringListModel>
#include <QTextBlock>
#include <QTimer>

namespace {

const QStringList &baseAssemblyWords()
{
    static const QStringList words = {
        // Data movement & effective address
        "MOVE", "MOVEA", "MOVEM", "MOVEP", "MOVEQ", "LEA", "PEA", "EXG", "SWAP",
        "LINK", "UNLK",
        // Integer arithmetic
        "ADD", "ADDA", "ADDI", "ADDQ", "ADDX", "SUB", "SUBA", "SUBI", "SUBQ", "SUBX",
        "MULS", "MULU", "DIVS", "DIVU", "NEG", "NEGX", "CLR", "EXT", "CMP", "CMPA",
        "CMPI", "CMPM", "TST",
        // BCD arithmetic
        "ABCD", "SBCD", "NBCD",
        // Logic & shifts
        "AND", "ANDI", "OR", "ORI", "EOR", "EORI", "NOT",
        "ASL", "ASR", "LSL", "LSR", "ROL", "ROR", "ROXL", "ROXR",
        // Bit manipulation & TAS
        "BTST", "BSET", "BCLR", "BCHG", "TAS",
        // Branches & control
        "BRA", "BSR", "JMP", "JSR", "RTS", "RTR", "RTE", "NOP", "RESET", "STOP",
        "TRAP", "TRAPV", "CHK", "ILLEGAL",
        "BEQ", "BNE", "BLT", "BLE", "BGT", "BGE", "BCC", "BCS", "BVC", "BVS",
        "BPL", "BMI", "BHI", "BLS", "BHS", "BLO",
        "DBRA", "DBF", "DBT", "DBEQ", "DBNE", "DBLT", "DBLE", "DBGT", "DBGE",
        "DBCC", "DBCS", "DBVC", "DBVS", "DBPL", "DBMI", "DBHI", "DBLS", "DBHS", "DBLO",
        "SEQ", "SNE", "SLT", "SLE", "SGT", "SGE", "SCC", "SCS", "SVC", "SVS",
        "SPL", "SMI", "SHI", "SLS", "SF", "ST",
        // Common size-suffixed instructions
        "MOVE.B", "MOVE.W", "MOVE.L", "MOVEA.W", "MOVEA.L", "MOVEM.W", "MOVEM.L",
        "ADD.B", "ADD.W", "ADD.L", "ADDA.W", "ADDA.L", "ADDI.B", "ADDI.W", "ADDI.L",
        "SUB.B", "SUB.W", "SUB.L", "SUBA.W", "SUBA.L", "SUBI.B", "SUBI.W", "SUBI.L",
        "CMP.B", "CMP.W", "CMP.L", "CMPA.W", "CMPA.L", "CMPI.B", "CMPI.W", "CMPI.L",
        "CLR.B", "CLR.W", "CLR.L", "TST.B", "TST.W", "TST.L",
        "AND.B", "AND.W", "AND.L", "OR.B", "OR.W", "OR.L", "EOR.B", "EOR.W", "EOR.L",
        "LSL.B", "LSL.W", "LSL.L", "LSR.B", "LSR.W", "LSR.L",
        "ASL.B", "ASL.W", "ASL.L", "ASR.B", "ASR.W", "ASR.L",
        "BRA.S", "BSR.S", "BEQ.S", "BNE.S", "DC.B", "DC.W", "DC.L",
        "DS.B", "DS.W", "DS.L", "DCB.B", "DCB.W", "DCB.L",
        // Directives & structured assembly
        "ORG", "EQU", "SET", "REG", "DC", "DS", "DCB", "END", "OPT",
        "INCLUDE", "INCBIN", "MACRO", "ENDM", "MEXIT",
        "IF", "IFEQ", "IFNE", "IFLT", "IFLE", "IFGT", "IFGE", "IFARG", "ELSE", "ENDI",
        "FOR", "ENDF", "TO", "DOWNTO", "BY", "DO",
        "WHILE", "ENDW", "REPEAT", "UNTIL", "DBLOOP", "UNLESS",
        "SECTION", "OFFSET", "FAIL", "PAGE", "SPC", "TTL", "LLEN", "PLEN",
        "LIST", "NOLIST", "SIMHALT", "MEMORY", "COMLINE",
        // Registers
        "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
        "SP", "USP", "SSP", "PC", "SR", "CCR"
    };
    return words;
}

} // namespace

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    setEditorFontSize(10);
    document()->setDocumentMargin(6);
    setStyleSheet("QPlainTextEdit { background-color: #1E1E1E; color: #D4D4D4; selection-background-color: #264F78; selection-color: #FFFFFF; border: 1px solid #333337; }");

    // Setup assembler autocomplete (mnemonics, directives, registers + parsed labels)
    m_completerModel = new QStringListModel(baseAssemblyWords(), this);
    m_completer = new QCompleter(m_completerModel, this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &CodeEditor::insertCompletion);

    m_labelScanTimer = new QTimer(this);
    m_labelScanTimer->setSingleShot(true);
    m_labelScanTimer->setInterval(300);
    connect(m_labelScanTimer, &QTimer::timeout, this, &CodeEditor::refreshCompletionModel);
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        m_labelScanTimer->start();
    });

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

void CodeEditor::setEditorFontSize(int pt)
{
    m_fontSize = qBound(7, pt, 32);
    QFont font("Monospace", m_fontSize);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 8);
    updateLineNumberAreaWidth(0);
    lineNumberArea->update();
}

void CodeEditor::setDiagnosticLines(const QSet<int> &errors, const QSet<int> &warnings)
{
    m_errorLines = errors;
    m_warningLines = warnings;
    highlightCurrentLine();
    lineNumberArea->update();
}

void CodeEditor::clearDiagnostics()
{
    if (m_errorLines.isEmpty() && m_warningLines.isEmpty()) return;
    m_errorLines.clear();
    m_warningLines.clear();
    highlightCurrentLine();
    lineNumberArea->update();
}

int CodeEditor::lineNumberAreaWidth()
{
    int computedDigits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++computedDigits;
    }
    int digits = qMax(3, computedDigits);

    int space = 26 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth() + 4, 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::focusInEvent(QFocusEvent *event)
{
    if (m_completer) {
        m_completer->setWidget(this);
    }
    QPlainTextEdit::focusInEvent(event);
}

void CodeEditor::insertCompletion(const QString &completion)
{
    if (!m_completer || m_completer->widget() != this) return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - m_completer->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    if (extra > 0) {
        tc.insertText(completion.right(extra));
    } else if (extra < 0) {
        // Case-insensitive match with different prefix length shouldn't happen cleanly,
        // fallback: replace word under cursor
        tc.select(QTextCursor::WordUnderCursor);
        tc.insertText(completion);
    }
    setTextCursor(tc);
}

QString CodeEditor::textUnderCursor() const
{
    QTextCursor tc = textCursor();
    const QString line = tc.block().text();
    const int pos = tc.positionInBlock();
    int start = pos;
    while (start > 0) {
        const QChar ch = line.at(start - 1);
        if (ch.isLetterOrNumber() || ch == '_' || ch == '.') {
            --start;
        } else {
            break;
        }
    }
    return line.mid(start, pos - start);
}

bool CodeEditor::isInsideComment() const
{
    const QTextCursor tc = textCursor();
    const QString before = tc.block().text().left(tc.positionInBlock());
    if (before.trimmed().startsWith('*') || before.startsWith('*')) return true;
    // Detect inline comment delimiter outside quotes
    bool inSingle = false;
    bool inDouble = false;
    for (int i = 0; i < before.size(); ++i) {
        const QChar c = before.at(i);
        if (c == '\'' && !inDouble) inSingle = !inSingle;
        else if (c == '"' && !inSingle) inDouble = !inDouble;
        else if ((c == ';' || c == '*') && !inSingle && !inDouble && i > 0 && before.at(i - 1).isSpace()) {
            return true;
        }
    }
    return false;
}

void CodeEditor::refreshCompletionModel()
{
    if (!m_completerModel) return;

    static const QRegularExpression labelRe(
        QStringLiteral("^([A-Za-z_.@][A-Za-z0-9_.$]*):?\\b"));

    QSet<QString> seen;
    QStringList merged = baseAssemblyWords();
    for (const QString &w : merged) {
        seen.insert(w.toUpper());
    }

    const QString src = toPlainText();
    const QStringList lines = src.split(QLatin1Char('\n'));
    QStringList labels;
    for (const QString &line : lines) {
        if (line.isEmpty() || line.startsWith('*') || line.startsWith(';') || line.at(0).isSpace())
            continue;
        const QRegularExpressionMatch m = labelRe.match(line);
        if (m.hasMatch()) {
            const QString lbl = m.captured(1);
            const QString up = lbl.toUpper();
            if (!seen.contains(up)) {
                seen.insert(up);
                labels.append(lbl);
            }
        }
    }
    labels.sort(Qt::CaseInsensitive);
    merged.append(labels);
    m_completerModel->setStringList(merged);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    const bool isShortcut = (event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_Space);

    // Auto-indent on Enter/Return (preserve leading whitespace from current line)
    if (!isReadOnly() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier) {
        QTextCursor tc = textCursor();
        const QString lineText = tc.block().text();
        QString indent;
        for (const QChar c : lineText) {
            if (c == ' ' || c == '\t') indent.append(c);
            else break;
        }
        tc.beginEditBlock();
        QPlainTextEdit::keyPressEvent(event);
        if (!indent.isEmpty()) {
            textCursor().insertText(indent);
        }
        tc.endEditBlock();
        return;
    }

    if (!isShortcut) {
        QPlainTextEdit::keyPressEvent(event);
    }

    if (!m_completer || isReadOnly()) return;

    const bool ctrlOrShift = event->modifiers().testFlag(Qt::ControlModifier)
                          || event->modifiers().testFlag(Qt::ShiftModifier);
    if (ctrlOrShift && event->text().isEmpty() && !isShortcut) {
        return;
    }

    static const QString eow(QStringLiteral("~!@#$%^&*()+{}|:\"<>?,/;'[]\\-="));
    const bool hasModifier = (event->modifiers() != Qt::NoModifier) && !ctrlOrShift;
    const QString completionPrefix = textUnderCursor();

    if (!isShortcut && (hasModifier || event->text().isEmpty()
                        || completionPrefix.length() < 2
                        || eow.contains(event->text().right(1))
                        || isInsideComment())) {
        m_completer->popup()->hide();
        return;
    }

    if (completionPrefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(completionPrefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 16);
    m_completer->complete(cr);
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Diagnostic line highlights (drawn under the current line highlight)
    for (int line : m_warningLines) {
        QTextBlock block = document()->findBlockByLineNumber(line - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(0x4D, 0x3E, 0x16, 140));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = QTextCursor(block);
            sel.cursor.clearSelection();
            extraSelections.append(sel);
        }
    }
    for (int line : m_errorLines) {
        QTextBlock block = document()->findBlockByLineNumber(line - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(0x5C, 0x1E, 0x1E, 170));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = QTextCursor(block);
            sel.cursor.clearSelection();
            extraSelections.append(sel);
        }
    }

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor(0x28, 0x28, 0x28);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::highlightLine(int lineNumber)
{
    if (lineNumber <= 0 || lineNumber > blockCount()) return;

    QTextBlock block = document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) return;

    QTextCursor cursor(block);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(0x1E, 0x1E, 0x1E));
    painter.setPen(QColor(0x33, 0x33, 0x37));
    painter.drawLine(lineNumberArea->width() - 1, event->rect().top(),
                     lineNumberArea->width() - 1, event->rect().bottom());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int curBlock = textCursor().blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int lineH = fontMetrics().height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int oneBased = blockNumber + 1;
            const bool isError = m_errorLines.contains(oneBased);
            const bool isWarn = !isError && m_warningLines.contains(oneBased);

            if (isError || isWarn) {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(Qt::NoPen);
                painter.setBrush(isError ? QColor(0xE0, 0x53, 0x53) : QColor(0xD7, 0xBA, 0x7D));
                const int d = qMax(6, qMin(8, lineH - 4));
                const int y = top + (lineH - d) / 2;
                painter.drawEllipse(4, y, d, d);
                painter.restore();
            }

            QString number = QString::number(oneBased);
            if (isError) {
                painter.setPen(QColor(0xF4, 0x87, 0x71));
            } else if (isWarn) {
                painter.setPen(QColor(0xD7, 0xBA, 0x7D));
            } else {
                painter.setPen(blockNumber == curBlock ? QColor(0xC6, 0xC6, 0xC6) : QColor(0x85, 0x85, 0x85));
            }
            QRect numRect(12, top, lineNumberArea->width() - 20, lineH);
            painter.drawText(numRect, Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
