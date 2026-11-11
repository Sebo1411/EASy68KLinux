#include "terminalwidget.h"
#include "hardwarewidget.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QFont>
#include <QApplication>
#include <cstdlib>
#include "../simulator/extern.h"

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    QFont monoFont("Monospace", 10);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    // Dark terminal styling
    setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #00ff00; font-family: monospace; }");

    connect(this, &TerminalWidget::appendTextSignal, this, &TerminalWidget::onAppendText, Qt::QueuedConnection);
    connect(this, &TerminalWidget::clearSignal, this, &TerminalWidget::onClear, Qt::QueuedConnection);
}

void TerminalWidget::textOut(const std::string& str)
{
    emit appendTextSignal(QString::fromStdString(str));
}

void TerminalWidget::textOutCR(const std::string& str)
{
    emit appendTextSignal(QString::fromStdString(str) + "\n");
}

void TerminalWidget::charOut(char ch)
{
    emit appendTextSignal(QString(QChar(ch)));
}

void TerminalWidget::charIn(char *ch)
{
    if (!ch) return;

    while (keyBuffer.empty()) {
        QApplication::processEvents(QEventLoop::WaitForMoreEvents, 50);
        if (!runMode || halt) {
            *ch = 0;
            return;
        }
    }

    QMutexLocker locker(&inputMutex);
    if (!keyBuffer.empty()) {
        *ch = keyBuffer.front();
        keyBuffer.pop_front();
    } else {
        *ch = 0;
    }
    pendingKey = 0;
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
            break;
        }
        if (c == '\b' || c == 127) {
            if (count > 0) {
                count--;
            }
        } else if (c != 0) {
            buf[count++] = c;
        }
    }
    buf[count] = '\0';
    *len = count;
    if (num) {
        *num = std::strtol(buf, nullptr, 0);
    }
    pendingKey = 0;
}

void TerminalWidget::doCRLF()
{
    emit appendTextSignal("\n");
}

void TerminalWidget::clear()
{
    emit clearSignal();
}

void TerminalWidget::keyPressEvent(QKeyEvent *e)
{
    QString txt = e->text();
    if (!txt.isEmpty()) {
        char ch = txt.at(0).toLatin1();
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            ch = '\n';
        }

        {
            QMutexLocker locker(&inputMutex);
            keyBuffer.push_back(ch);
            pendingKey = ch;
            inputCond.wakeOne();
        }

        // Echo key to terminal
        onAppendText(QString(QChar(ch)));
    }

    // Don't call base implementation to prevent editing previous terminal outputs directly
    e->accept();
}

void TerminalWidget::onAppendText(const QString &text)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void TerminalWidget::onClear()
{
    QPlainTextEdit::clear();
}

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

void TerminalWidget::unfilledRectangle(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->unfilledRectangle(x1, y1, x2, y2);
}

void TerminalWidget::unfilledEllipse(int x1, int y1, int x2, int y2)
{
    if (hardwareWidget) hardwareWidget->unfilledEllipse(x1, y1, x2, y2);
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
