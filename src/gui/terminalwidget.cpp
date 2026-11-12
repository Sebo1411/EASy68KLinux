#include "terminalwidget.h"
#include "hardwarewidget.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>
#include <QApplication>
#include <QGuiApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QThread>
#include <QUrl>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "../simulator/extern.h"
#include "../simulator/proto.h"

#ifdef EASY68K_HAS_MULTIMEDIA
#include <QSoundEffect>
#endif

namespace {

void triggerKeyIrq(byte level)
{
    if (level >= 1 && level <= 7) {
        irq |= (1 << (level - 1));
        if (stopInstruction) {
            irqHandler();
        }
    }
}

QString normalizeFilterForQt(const QString &easyFilter)
{
    if (easyFilter.trimmed().isEmpty()) {
        return QStringLiteral("All Files (*)");
    }
    QString spaced = easyFilter;
    spaced.replace(QLatin1Char(';'), QLatin1Char(' '));
    return QStringLiteral("Files (%1);;All Files (*)").arg(spaced);
}

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    QFont monoFont("Monospace", 10);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    activeTextFormat.setForeground(QColor(0x00, 0xFF, 0x00));
    activeTextFormat.setFont(monoFont);
    setCurrentCharFormat(activeTextFormat);

    // Dark terminal styling
    setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #00ff00; font-family: monospace; }");

    connect(this, &TerminalWidget::appendTextSignal, this, &TerminalWidget::onAppendText, Qt::QueuedConnection);
    connect(this, &TerminalWidget::clearSignal, this, &TerminalWidget::onClear, Qt::QueuedConnection);
}

TerminalWidget::~TerminalWidget()
{
    ResetSounds();
}

void TerminalWidget::setHardwareWidget(HardwareWidget *hw)
{
    if (hardwareWidget) {
        disconnect(hardwareWidget, &HardwareWidget::canvasKeyEvent, this, &TerminalWidget::handleHostKeyEvent);
    }
    hardwareWidget = hw;
    if (hardwareWidget) {
        connect(hardwareWidget, &HardwareWidget::canvasKeyEvent, this, &TerminalWidget::handleHostKeyEvent);
    }
}

uint8_t TerminalWidget::qtKeyToEasy68kCode(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<uint8_t>('A' + (key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<uint8_t>('0' + (key - Qt::Key_0));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return static_cast<uint8_t>(0x70 + (key - Qt::Key_F1));
    }
    switch (key) {
    case Qt::Key_Backspace: return 0x08;
    case Qt::Key_Tab:       return 0x09;
    case Qt::Key_Return:
    case Qt::Key_Enter:     return 0x0D;
    case Qt::Key_Shift:     return 0x10;
    case Qt::Key_Control:   return 0x11;
    case Qt::Key_Alt:       return 0x12;
    case Qt::Key_Escape:    return 0x1B;
    case Qt::Key_Space:     return 0x20;
    case Qt::Key_PageUp:    return 0x21;
    case Qt::Key_PageDown:  return 0x22;
    case Qt::Key_End:       return 0x23;
    case Qt::Key_Home:      return 0x24;
    case Qt::Key_Left:      return 0x25;
    case Qt::Key_Up:        return 0x26;
    case Qt::Key_Right:     return 0x27;
    case Qt::Key_Down:      return 0x28;
    case Qt::Key_Delete:    return 0x2E;
    default:
        if (key >= 0 && key < 128) {
            return static_cast<uint8_t>(key);
        }
        return 0;
    }
}

void TerminalWidget::insertTextAtCursor(const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.setCharFormat(activeTextFormat);
    cursor.insertText(text);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void TerminalWidget::removeLastCharOnScreen()
{
    QTextCursor cursor = textCursor();
    if (!cursor.atBlockStart()) {
        cursor.deletePreviousChar();
        setTextCursor(cursor);
    }
}

void TerminalWidget::textOut(const std::string& str)
{
    QString qstr = QString::fromStdString(str);
    if (thread() == QThread::currentThread()) {
        insertTextAtCursor(qstr);
    } else {
        emit appendTextSignal(qstr);
    }
}

void TerminalWidget::textOutCR(const std::string& str)
{
    QString qstr = QString::fromStdString(str) + QLatin1Char('\n');
    if (thread() == QThread::currentThread()) {
        insertTextAtCursor(qstr);
    } else {
        emit appendTextSignal(qstr);
    }
}

void TerminalWidget::charOut(char ch)
{
    if (ch == '\b') {
        if (thread() == QThread::currentThread()) {
            removeLastCharOnScreen();
        }
        return;
    }
    QString qstr(QChar(static_cast<unsigned char>(ch)));
    if (thread() == QThread::currentThread()) {
        insertTextAtCursor(qstr);
    } else {
        emit appendTextSignal(qstr);
    }
}

void TerminalWidget::charIn(char *ch)
{
    if (!ch) return;

    emit inputWaitChanged(true);
    while (keyBuffer.empty()) {
        QApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
        if (!runMode || halt) {
            emit inputWaitChanged(false);
            *ch = 0;
            return;
        }
    }

    {
        QMutexLocker locker(&inputMutex);
        if (!keyBuffer.empty()) {
            *ch = keyBuffer.front();
            keyBuffer.pop_front();
        } else {
            *ch = 0;
        }
        pendingKey = keyBuffer.empty() ? 0 : keyBuffer.front();
    }
    emit inputWaitChanged(false);
}

void TerminalWidget::textIn(char *buf, long *len, long *num)
{
    if (!buf || !len) return;

    long maxLen = *len;
    long count = 0;

    if (maxLen <= 0) {
        // Zero capacity: do not index buf[-1] or write past the buffer.
        *len = 0;
        if (num) *num = 0;
        return;
    }

    emit inputWaitChanged(true);
    while (count < maxLen - 1) {
        while (keyBuffer.empty()) {
            QApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
            if (!runMode || halt) break;
        }
        if (!runMode || halt) break;

        char c = 0;
        {
            QMutexLocker locker(&inputMutex);
            if (!keyBuffer.empty()) {
                c = keyBuffer.front();
                keyBuffer.pop_front();
            }
        }

        if (c == '\r' || c == '\n') {
            if (keyboardEcho && inputLFdisplay) {
                insertTextAtCursor(QStringLiteral("\n"));
            }
            break;
        }
        if (c == '\b' || c == 127) {
            if (count > 0) {
                count--;
                if (keyboardEcho) {
                    removeLastCharOnScreen();
                }
            }
        } else if (c != 0) {
            buf[count++] = c;
            if (keyboardEcho) {
                insertTextAtCursor(QString(QChar(static_cast<unsigned char>(c))));
            }
        }
    }
    buf[count] = '\0';
    *len = count;
    if (num) {
        *num = std::strtol(buf, nullptr, 0);
    }
    {
        QMutexLocker locker(&inputMutex);
        pendingKey = keyBuffer.empty() ? 0 : keyBuffer.front();
    }
    emit inputWaitChanged(false);
}

void TerminalWidget::doCRLF()
{
    textOut("\n");
}

void TerminalWidget::clear()
{
    if (thread() == QThread::currentThread()) {
        onClear();
    } else {
        emit clearSignal();
    }
}

void TerminalWidget::gotorc(int r, int c)
{
    int row = static_cast<unsigned char>(r);
    int col = static_cast<unsigned char>(c);
    if (row > 128 || col > 255) return;

    QTextDocument *doc = document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    while (doc->blockCount() <= row) {
        cursor.insertText(QStringLiteral("\n"));
    }

    QTextBlock blk = doc->findBlockByNumber(row);
    if (!blk.isValid()) return;
    int lineLen = blk.text().length();
    if (lineLen < col) {
        cursor = QTextCursor(blk);
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.insertText(QString(col - lineLen, QLatin1Char(' ')));
        blk = doc->findBlockByNumber(row);
    }
    cursor = QTextCursor(blk);
    cursor.setPosition(blk.position() + col);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void TerminalWidget::getrc(short *d1)
{
    if (!d1) return;
    QTextCursor cursor = textCursor();
    int row = std::clamp(cursor.blockNumber(), 0, 128);
    int col = std::clamp(cursor.positionInBlock(), 0, 255);
    *d1 = static_cast<short>(((col & 0xFF) << 8) | (row & 0xFF));
}

void TerminalWidget::getCharAt(unsigned short r, unsigned short c, char *d1)
{
    if (!d1) return;
    QTextBlock blk = document()->findBlockByNumber(static_cast<int>(r));
    if (blk.isValid() && static_cast<int>(c) < blk.text().length()) {
        *d1 = blk.text().at(static_cast<int>(c)).toLatin1();
    } else {
        *d1 = ' ';
    }
}

void TerminalWidget::setFontProperties(int c, int s)
{
    QColor col(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF);
    activeTextFormat.setForeground(col);

    int highWord = (s >> 16) & 0xFFFF;
    if (highWord != 0) {
        int ptSize = highWord & 0xFF;
        int fontCode = (highWord >> 8) & 0xFF;
        QString family = QStringLiteral("Monospace");
        switch (fontCode) {
        case 1: family = QStringLiteral("Fixedsys"); break;
        case 2: family = QStringLiteral("Courier"); break;
        case 3: family = QStringLiteral("Courier New"); break;
        case 4: family = QStringLiteral("Lucida Console"); break;
        case 5: family = QStringLiteral("Lucida Sans Typewriter"); break;
        case 6: family = QStringLiteral("Consolas"); break;
        case 7: family = QStringLiteral("Terminal"); break;
        default: break;
        }
        QFont f = activeTextFormat.font();
        f.setFamily(family);
        f.setStyleHint(QFont::Monospace);
        if (ptSize > 0) {
            f.setPointSize(ptSize);
        }
        activeTextFormat.setFont(f);
    }

    int styleBits = s & 0xFFFF;
    activeTextFormat.setFontWeight((styleBits & 0x01) ? QFont::Bold : QFont::Normal);
    activeTextFormat.setFontItalic((styleBits & 0x02) != 0);
    activeTextFormat.setFontUnderline((styleBits & 0x04) != 0);
    activeTextFormat.setFontStrikeOut((styleBits & 0x08) != 0);

    setCurrentCharFormat(activeTextFormat);
}

void TerminalWidget::scrollRect(unsigned short r, unsigned short c, unsigned short h, unsigned short w, unsigned short dir)
{
    if (h == 0 || w == 0 || r + h > 129 || c + w > 256) return;

    QStringList lines = toPlainText().split(QLatin1Char('\n'));
    while (lines.size() < static_cast<int>(r + h)) {
        lines.append(QString());
    }
    for (int row = r; row < r + h; ++row) {
        if (lines[row].length() < static_cast<int>(c + w)) {
            lines[row].append(QString(static_cast<int>(c + w) - lines[row].length(), QLatin1Char(' ')));
        }
    }

    if (dir == 0) {
        // Scroll up
        for (int row = r; row < r + h - 1; ++row) {
            QString slice = lines[row + 1].mid(c, w);
            lines[row].replace(c, w, slice);
        }
        lines[r + h - 1].replace(c, w, QString(w, QLatin1Char(' ')));
    } else if (dir == 1) {
        // Scroll down
        for (int row = r + h - 1; row > r; --row) {
            QString slice = lines[row - 1].mid(c, w);
            lines[row].replace(c, w, slice);
        }
        lines[r].replace(c, w, QString(w, QLatin1Char(' ')));
    } else if (dir == 2) {
        // Scroll left
        for (int row = r; row < r + h; ++row) {
            QString slice = (w > 1) ? lines[row].mid(c + 1, w - 1) : QString();
            slice.append(QLatin1Char(' '));
            lines[row].replace(c, w, slice);
        }
    } else if (dir == 3) {
        // Scroll right
        for (int row = r; row < r + h; ++row) {
            QString slice = QString(QLatin1Char(' ')) + ((w > 1) ? lines[row].mid(c, w - 1) : QString());
            lines[row].replace(c, w, slice);
        }
    } else {
        return;
    }

    QTextCursor saved = textCursor();
    int savedPos = saved.position();
    setPlainText(lines.join(QLatin1Char('\n')));
    saved = textCursor();
    saved.setPosition(std::min(savedPos, document()->characterCount() - 1));
    setTextCursor(saved);
}

void TerminalWidget::getKeyState(long *d1)
{
    if (!d1) return;
    uint32_t req = static_cast<uint32_t>(*d1);
    if (req == 0) {
        *d1 = (static_cast<long>(lastKeyUpCode) << 16) | static_cast<long>(lastKeyDownCode);
        return;
    }

    uint32_t result = 0;
    for (int shift = 24; shift >= 0; shift -= 8) {
        uint8_t code = static_cast<uint8_t>((req >> shift) & 0xFFU);
        if (code != 0 && keyStates[code]) {
            result |= (0xFFU << shift);
        }
    }
    *d1 = static_cast<long>(result);
}

void TerminalWidget::displayFileDialog(long *mode, int A1, int A2, int A3, short *result)
{
    if (!mode) {
        if (result) *result = -1;
        return;
    }
    if (!memory) {
        *mode = 0;
        if (result) *result = -1;
        return;
    }

    // Never block headless test runs with a native modal dialog
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        *mode = 0;
        if (result) *result = 0;
        return;
    }

    auto readMemStr = [](int guestAddr) -> QString {
        if (guestAddr == 0) return QString();
        uint32_t addr = static_cast<uint32_t>(guestAddr) & ADDRMASK;
        char buf[256];
        size_t i = 0;
        while (i < sizeof(buf) - 1 && addr + i < MEMSIZE) {
            char ch = memory[addr + i];
            if (ch == '\0') break;
            buf[i++] = ch;
        }
        buf[i] = '\0';
        return QString::fromUtf8(buf);
    };

    bool isSave = (*mode == 1);
    QString title = readMemStr(A1);
    if (title.isEmpty()) {
        title = isSave ? tr("Save File") : tr("Open File");
    }
    QString filter = normalizeFilterForQt(readMemStr(A2));
    QString initialPath = readMemStr(A3);

    QString chosen = isSave
        ? QFileDialog::getSaveFileName(this, title, initialPath, filter)
        : QFileDialog::getOpenFileName(this, title, initialPath, filter);

    if (chosen.isEmpty()) {
        *mode = 0;
        if (result) *result = 0;
        return;
    }

    uint32_t outAddr = static_cast<uint32_t>(A3) & ADDRMASK;
    QByteArray utf8 = chosen.toUtf8();
    size_t maxWrite = std::min<size_t>(255, static_cast<size_t>(MEMSIZE) - outAddr - 1);
    size_t copyLen = std::min<size_t>(static_cast<size_t>(utf8.size()), maxWrite);
    if (copyLen > 0) {
        std::memcpy(&memory[outAddr], utf8.constData(), copyLen);
    }
    memory[outAddr + copyLen] = '\0';

    *mode = 1;
    if (result) *result = 0;
}

void TerminalWidget::handleHostKeyEvent(QKeyEvent *e, bool pressed)
{
    if (!e) return;

    uint8_t keyCode = qtKeyToEasy68kCode(e->key());
    if (keyCode != 0) {
        keyStates[keyCode] = pressed;
        if (pressed) {
            lastKeyDownCode = keyCode;
            triggerKeyIrq(keyDownIRQ);
        } else {
            lastKeyUpCode = keyCode;
            triggerKeyIrq(keyUpIRQ);
        }
    }

    if (!pressed) {
        return;
    }

    QString txt = e->text();
    char ch = 0;
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        ch = '\n';
    } else if (e->key() == Qt::Key_Backspace) {
        ch = '\b';
    } else if (!txt.isEmpty()) {
        ch = txt.at(0).toLatin1();
    }

    if (ch != 0) {
        {
            QMutexLocker locker(&inputMutex);
            keyBuffer.push_back(ch);
            pendingKey = ch;
            inputCond.wakeOne();
        }
        // When waiting for line input, textIn() handles echo and Backspace editing.
        // Outside textIn(), echo only if keyboardEcho is enabled and input isn't pending a charIn().
        if (!inputMode && keyboardEcho && ch != '\b') {
            insertTextAtCursor(QString(QChar(static_cast<unsigned char>(ch))));
        }
    }
}

void TerminalWidget::keyPressEvent(QKeyEvent *e)
{
    handleHostKeyEvent(e, true);
    // Don't call base implementation to prevent editing previous terminal outputs directly
    e->accept();
}

void TerminalWidget::keyReleaseEvent(QKeyEvent *e)
{
    handleHostKeyEvent(e, false);
    e->accept();
}

void TerminalWidget::onAppendText(const QString &text)
{
    insertTextAtCursor(text);
}

void TerminalWidget::onClear()
{
    QPlainTextEdit::clear();
}

// -----------------------------------------------------------------------------
// Sound (TRAP #15 Tasks 70-77)
// -----------------------------------------------------------------------------
void TerminalWidget::playSound(char *fileName, short *result)
{
    if (!fileName || !QFileInfo::exists(QString::fromUtf8(fileName))) {
        if (result) *result = 0;
        return;
    }
#ifdef EASY68K_HAS_MULTIMEDIA
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        if (result) *result = 1;
        return;
    }
    if (!stdSoundPlayer) {
        stdSoundPlayer = new QSoundEffect(this);
    }
    if (stdSoundPlayer->isPlaying()) {
        if (result) *result = 0;
        return;
    }
    stdSoundPlayer->setLoopCount(1);
    stdSoundPlayer->setSource(QUrl::fromLocalFile(QFileInfo(QString::fromUtf8(fileName)).absoluteFilePath()));
    stdSoundPlayer->play();
    if (result) *result = 1;
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::loadSound(char *fileName, int waveIndex)
{
    if (!fileName || waveIndex < 0 || waveIndex >= 256) return;
    QFileInfo fi(QString::fromUtf8(fileName));
    if (!fi.exists()) return;
    stdSoundPaths[static_cast<size_t>(waveIndex)] = fi.absoluteFilePath();
}

void TerminalWidget::playSoundMem(int waveIndex, short *result)
{
    if (waveIndex < 0 || waveIndex >= 256 || stdSoundPaths[static_cast<size_t>(waveIndex)].isEmpty()) {
        if (result) *result = 0;
        return;
    }
#ifdef EASY68K_HAS_MULTIMEDIA
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        if (result) *result = 1;
        return;
    }
    if (!stdSoundPlayer) {
        stdSoundPlayer = new QSoundEffect(this);
    }
    if (stdSoundPlayer->isPlaying()) {
        if (result) *result = 0;
        return;
    }
    stdSoundPlayer->setLoopCount(1);
    stdSoundPlayer->setSource(QUrl::fromLocalFile(stdSoundPaths[static_cast<size_t>(waveIndex)]));
    stdSoundPlayer->play();
    if (result) *result = 1;
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::controlSound(int control, int waveIndex, short *result)
{
    if (control == 3) {
#ifdef EASY68K_HAS_MULTIMEDIA
        if (stdSoundPlayer) stdSoundPlayer->stop();
#endif
        if (result) *result = 1;
        return;
    }
    if (waveIndex < 0 || waveIndex >= 256 || stdSoundPaths[static_cast<size_t>(waveIndex)].isEmpty()) {
        if (result) *result = 0;
        return;
    }
    if (control == 0) {
        playSoundMem(waveIndex, result);
        return;
    }
#ifdef EASY68K_HAS_MULTIMEDIA
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        if (result) *result = 1;
        return;
    }
    if (!stdSoundPlayer) {
        stdSoundPlayer = new QSoundEffect(this);
    }
    if (control == 1) {
        if (stdSoundPlayer->isPlaying()) {
            if (result) *result = 0;
            return;
        }
        stdSoundPlayer->setLoopCount(QSoundEffect::Infinite);
        stdSoundPlayer->setSource(QUrl::fromLocalFile(stdSoundPaths[static_cast<size_t>(waveIndex)]));
        stdSoundPlayer->play();
        if (result) *result = 1;
    } else if (control == 2) {
        stdSoundPlayer->stop();
        if (result) *result = 1;
    } else {
        if (result) *result = 0;
    }
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::playSoundDX(char *fileName, short *result)
{
    if (!fileName || !QFileInfo::exists(QString::fromUtf8(fileName))) {
        if (result) *result = 0;
        return;
    }
#ifdef EASY68K_HAS_MULTIMEDIA
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        if (result) *result = 1;
        return;
    }
    auto *fx = new QSoundEffect(this);
    fx->setLoopCount(1);
    fx->setSource(QUrl::fromLocalFile(QFileInfo(QString::fromUtf8(fileName)).absoluteFilePath()));
    connect(fx, &QSoundEffect::playingChanged, this, [this, fx]() {
        if (!fx->isPlaying()) {
            oneshotDxPlayers.erase(
                std::remove(oneshotDxPlayers.begin(), oneshotDxPlayers.end(), fx),
                oneshotDxPlayers.end());
            fx->deleteLater();
        }
    });
    oneshotDxPlayers.push_back(fx);
    fx->play();
    if (result) *result = 1;
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::loadSoundDX(char *fileName, int waveIndex, short *result)
{
    if (!fileName || waveIndex < 0 || waveIndex >= 256) {
        if (result) *result = 0;
        return;
    }
    QFileInfo fi(QString::fromUtf8(fileName));
    if (!fi.exists()) {
        if (result) *result = 0;
        return;
    }
    dxSoundPaths[static_cast<size_t>(waveIndex)] = fi.absoluteFilePath();
#ifdef EASY68K_HAS_MULTIMEDIA
    if (result) *result = 1;
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::playSoundMemDX(int waveIndex, short *result)
{
    if (waveIndex < 0 || waveIndex >= 256 || dxSoundPaths[static_cast<size_t>(waveIndex)].isEmpty()) {
        if (result) *result = 0;
        return;
    }
    QByteArray path = dxSoundPaths[static_cast<size_t>(waveIndex)].toUtf8();
    playSoundDX(path.data(), result);
}

void TerminalWidget::controlSoundDX(int control, int waveIndex, short *result)
{
    if (control == 3) {
        ResetSounds();
        if (result) *result = 1;
        return;
    }
    if (waveIndex < 0 || waveIndex >= 256 || dxSoundPaths[static_cast<size_t>(waveIndex)].isEmpty()) {
        if (result) *result = 0;
        return;
    }
    if (control == 0) {
        playSoundMemDX(waveIndex, result);
        return;
    }
#ifdef EASY68K_HAS_MULTIMEDIA
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        if (result) *result = 1;
        return;
    }
    if (control == 1) {
        auto *&slot = dxSoundPlayers[static_cast<size_t>(waveIndex)];
        if (!slot) {
            slot = new QSoundEffect(this);
        }
        slot->setLoopCount(QSoundEffect::Infinite);
        slot->setSource(QUrl::fromLocalFile(dxSoundPaths[static_cast<size_t>(waveIndex)]));
        slot->play();
        if (result) *result = 1;
    } else if (control == 2) {
        auto *slot = dxSoundPlayers[static_cast<size_t>(waveIndex)];
        if (slot) slot->stop();
        if (result) *result = 1;
    } else {
        if (result) *result = 0;
    }
#else
    if (result) *result = 0;
#endif
}

void TerminalWidget::ResetSounds()
{
    stdSoundPaths.fill(QString());
    dxSoundPaths.fill(QString());
#ifdef EASY68K_HAS_MULTIMEDIA
    if (stdSoundPlayer) {
        stdSoundPlayer->stop();
    }
    for (auto *&slot : dxSoundPlayers) {
        if (slot) {
            slot->stop();
            slot->deleteLater();
            slot = nullptr;
        }
    }
    for (auto *fx : oneshotDxPlayers) {
        if (fx) {
            fx->stop();
            fx->deleteLater();
        }
    }
    oneshotDxPlayers.clear();
#endif
}

// -----------------------------------------------------------------------------
// Graphics forwarding to HardwareWidget
// -----------------------------------------------------------------------------
void TerminalWidget::setLineColor(int c)
{
    if (hardwareWidget) hardwareWidget->setLineColor(c);
}

void TerminalWidget::setFillColor(int c)
{
    if (hardwareWidget) hardwareWidget->setFillColor(c);
}

void TerminalWidget::setPenWidth(int w)
{
    if (hardwareWidget) hardwareWidget->setPenWidth(w);
}

void TerminalWidget::drawPixel(int x, int y)
{
    if (hardwareWidget) hardwareWidget->drawPixel(x, y);
}

int TerminalWidget::getPixel(int x, int y)
{
    if (hardwareWidget) return hardwareWidget->getPixel(x, y);
    return 0;
}

void TerminalWidget::line(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->line(x1, y1, x2, y2);
}

void TerminalWidget::lineTo(int x, int y)
{
    if (hardwareWidget) hardwareWidget->lineTo(x, y);
}

void TerminalWidget::moveTo(int x, int y)
{
    if (hardwareWidget) hardwareWidget->moveTo(x, y);
}

void TerminalWidget::getXY(short *x, short *y)
{
    if (hardwareWidget) hardwareWidget->getXY(x, y);
}

void TerminalWidget::rectangle(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->rectangle(x1, y1, x2, y2);
}

void TerminalWidget::ellipse(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->ellipse(x1, y1, x2, y2);
}

void TerminalWidget::floodFill(int x1, int y1)
{
    if (hardwareWidget) hardwareWidget->floodFill(x1, y1);
}

void TerminalWidget::unfilledRectangle(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->unfilledRectangle(x1, y1, x2, y2);
}

void TerminalWidget::unfilledEllipse(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->unfilledEllipse(x1, y1, x2, y2);
}

void TerminalWidget::setDrawingMode(int m)
{
    if (hardwareWidget) hardwareWidget->setDrawingMode(m);
}

void TerminalWidget::FormPaint(void *)
{
    if (hardwareWidget) hardwareWidget->repaintCanvas();
}

void TerminalWidget::drawText(const std::string& str, int x, int y)
{
    if (hardwareWidget) hardwareWidget->drawText(str, x, y);
}

void TerminalWidget::getWindowSize(unsigned short &w, unsigned short &h)
{
    if (hardwareWidget) hardwareWidget->getWindowSize(w, h);
    else { w = 640; h = 480; }
}

void TerminalWidget::setWindowSize(unsigned short w, unsigned short h)
{
    if (hardwareWidget) hardwareWidget->setWindowSize(w, h);
}
