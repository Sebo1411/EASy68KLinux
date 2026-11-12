#include "listingwidget.h"
#include "simulatormanager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QFontMetrics>
#include <algorithm>

bool ListingScrollArea::isInstructionLine(const QString &line)
{
    if (line.length() < 14) return false;

    auto isHexDigit = [](QChar c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    };

    for (int i = 0; i < 8; ++i) {
        if (!isHexDigit(line[i])) {
            return false;
        }
    }
    if (line[8] == '=') return false;   // EQU / SET value line

    // Real instruction lines always carry at least one four-digit object-code
    // word in columns 10..13.  Requiring it rejects the "00001000 Starting
    // Address" header (and similar text) which otherwise looks like code at
    // the program's entry address.
    for (int i = 10; i < 14; ++i) {
        if (!isHexDigit(line[i])) {
            return false;
        }
    }
    return true;
}

uint32_t ListingScrollArea::getLineAddress(const QString &line)
{
    if (line.length() < 8) return 0;
    bool ok = false;
    uint32_t addr = line.left(8).toUInt(&ok, 16);
    return ok ? addr : 0;
}

ListingScrollArea::ListingScrollArea(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    setFont(monoFont);

    QFontMetrics fm(monoFont);
    rowHeight = std::max(16, fm.lineSpacing());

    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setAutoFillBackground(true);
}

void ListingScrollArea::setListingLines(const QStringList &lines)
{
    listingLines = lines;
    highlightedLine = -1;
    selectedLine = -1;

    QFontMetrics fm(font());
    maxLineWidthPx = 400;
    for (const QString &l : listingLines) {
        int w = fm.horizontalAdvance(l);
        if (w > maxLineWidthPx) {
            maxLineWidthPx = w;
        }
    }
    maxLineWidthPx += 40;

    updateScrollBars();
    viewport()->update();
}

void ListingScrollArea::clear()
{
    listingLines.clear();
    highlightedLine = -1;
    selectedLine = -1;
    updateScrollBars();
    viewport()->update();
}

void ListingScrollArea::setSelectedLine(int line)
{
    if (line >= -1 && line < listingLines.size()) {
        selectedLine = line;
        viewport()->update();
    }
}

int ListingScrollArea::getFirstVisibleLine() const
{
    return verticalScrollBar()->value();
}

int ListingScrollArea::getLastVisibleLine() const
{
    int visibleCount = viewport()->height() / rowHeight;
    return verticalScrollBar()->value() + std::max(0, visibleCount - 1);
}

void ListingScrollArea::updateScrollBars()
{
    int visibleLines = viewport()->height() / rowHeight;
    int totalLines = listingLines.size();

    verticalScrollBar()->setRange(0, std::max(0, totalLines - visibleLines));
    verticalScrollBar()->setPageStep(std::max(1, visibleLines));

    int viewWidth = viewport()->width() - gutterWidth;
    horizontalScrollBar()->setRange(0, std::max(0, maxLineWidthPx - viewWidth));
    horizontalScrollBar()->setPageStep(std::max(10, viewWidth));
}

void ListingScrollArea::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void ListingScrollArea::wheelEvent(QWheelEvent *event)
{
    QAbstractScrollArea::wheelEvent(event);
    viewport()->update();
}

void ListingScrollArea::highlightPC(uint32_t pc, bool isHalted)
{
    QString pcHex = QString("%1").arg(pc, 8, 16, QLatin1Char('0')).toUpper();
    int target = -1;

    for (int i = 0; i < listingLines.size(); ++i) {
        const QString &l = listingLines[i];
        if (l.length() >= 8 && l.left(8).toUpper() == pcHex) {
            if (isInstructionLine(l) || isHalted) {
                target = i; // Keep scanning to select the last matching line
            }
        }
    }

    highlightedLine = target;

    if (highlightedLine >= 0) {
        int firstVisible = getFirstVisibleLine();
        int lastVisible = getLastVisibleLine();

        // Exact original Sim68K behavior: DO NOT SNAP/SCROLL if already visible on screen!
        if (highlightedLine < firstVisible || highlightedLine > lastVisible) {
            verticalScrollBar()->setValue(std::max(0, highlightedLine - 2));
        }
    }

    viewport()->update();
}

void ListingScrollArea::mousePressEvent(QMouseEvent *event)
{
    int lineIndex = verticalScrollBar()->value() + (event->pos().y() / rowHeight);
    if (lineIndex >= 0 && lineIndex < listingLines.size()) {
        selectedLine = lineIndex;
        const QString &line = listingLines[lineIndex];
        if (isInstructionLine(line)) {
            uint32_t addr = getLineAddress(line);
            if (event->pos().x() <= gutterWidth) {
                // Click in gutter toggles breakpoint
                if (simManager) {
                    simManager->toggleBreakpoint(addr);
                }
                emit breakpointToggled(addr);
                viewport()->update();
                return;
            }
        }
        viewport()->update();
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void ListingScrollArea::mouseDoubleClickEvent(QMouseEvent *event)
{
    int lineIndex = verticalScrollBar()->value() + (event->pos().y() / rowHeight);
    if (lineIndex >= 0 && lineIndex < listingLines.size()) {
        const QString &line = listingLines[lineIndex];
        if (isInstructionLine(line)) {
            uint32_t addr = getLineAddress(line);
            if (simManager) {
                simManager->setPC(addr);
            }
            emit lineDoubleClicked(addr);
            viewport()->update();
            return;
        }
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void ListingScrollArea::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(viewport());
    painter.setFont(font());
    QFontMetrics fm = painter.fontMetrics();

    // VS Code Dark+ viewport background (#1E1E1E)
    painter.fillRect(viewport()->rect(), QColor(0x1E, 0x1E, 0x1E));

    int firstRow = verticalScrollBar()->value();
    int visibleRows = (viewport()->height() / rowHeight) + 1;
    int lastRow = std::min(static_cast<int>(listingLines.size()) - 1, firstRow + visibleRows);

    int hScroll = horizontalScrollBar()->value();

    if (listingLines.isEmpty()) {
        painter.setPen(QColor(0x85, 0x85, 0x85));
        painter.drawText(viewport()->rect(), Qt::AlignCenter,
                         tr("No source listing (.L68) loaded.\nAssemble program or load S-Record to view code."));
        return;
    }

    // Paint gutter background (#252526)
    painter.fillRect(0, 0, gutterWidth, viewport()->height(), QColor(0x25, 0x25, 0x26));
    painter.setPen(QColor(0x3C, 0x3C, 0x3C));
    painter.drawLine(gutterWidth - 1, 0, gutterWidth - 1, viewport()->height());

    for (int r = firstRow; r <= lastRow; ++r) {
        int y = (r - firstRow) * rowHeight;
        const QString &line = listingLines[r];
        bool isInstr = isInstructionLine(line);
        uint32_t addr = isInstr ? getLineAddress(line) : 0;
        bool hasBp = (isInstr && simManager && simManager->hasBreakpoint(addr));

        bool isModified = false;
        uint16_t liveWord = 0;
        if (isInstr && simManager && simManager->getMemory() && addr + 1 < simManager->getMemorySize() && line.length() >= 14) {
            bool hexOk = false;
            uint16_t assembledWord = line.mid(10, 4).toUShort(&hexOk, 16);
            if (hexOk) {
                const auto *mem = reinterpret_cast<const unsigned char*>(simManager->getMemory());
                liveWord = (static_cast<uint16_t>(mem[addr]) << 8) | static_cast<uint16_t>(mem[addr + 1]);
                if (liveWord != assembledWord) {
                    isModified = true;
                }
            }
        }

        // Draw line background first
        QRect textRect(gutterWidth, y, viewport()->width() - gutterWidth, rowHeight);
        if (r == highlightedLine) {
            // VS Code Dark+ active debug execution line (#094771 with #007ACC border)
            painter.fillRect(textRect, QColor(0x09, 0x47, 0x71));
            painter.setPen(QColor(0x00, 0x7A, 0xCC));
            painter.drawLine(textRect.left(), textRect.top(), textRect.right(), textRect.top());
            painter.drawLine(textRect.left(), textRect.bottom(), textRect.right(), textRect.bottom());
        } else if (r == selectedLine) {
            // Selected cursor row for Run To Cursor / F2
            painter.fillRect(textRect, QColor(0x2A, 0x2D, 0x2E));
            painter.setPen(QColor(0x37, 0x94, 0xFF));
            painter.drawRect(textRect.adjusted(0, 0, -1, -1));
        }

        // Draw gutter breakpoint dot & current PC execution arrow
        if (isInstr) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            int dotRadius = 4;
            int dotCenterX = 9;
            int dotCenterY = y + rowHeight / 2;

            if (hasBp) {
                // Bright VS Code red breakpoint circle (#F14C4C)
                painter.setBrush(QColor(0xF1, 0x4C, 0x4C));
                painter.setPen(QColor(0xFF, 0x80, 0x80));
            } else {
                // Subtle green executable line indicator (#4EC9B0)
                painter.setBrush(QColor(0x38, 0x8A, 0x34));
                painter.setPen(QColor(0x4E, 0xC9, 0xB0));
            }
            painter.drawEllipse(QPoint(dotCenterX, dotCenterY), dotRadius, dotRadius);

            if (r == highlightedLine) {
                // Draw yellow VS Code debug execution arrow ▶ in gutter
                QPainterPath arrow;
                qreal ay = y + rowHeight / 2.0;
                arrow.moveTo(15.0, ay - 4.5);
                arrow.lineTo(23.0, ay);
                arrow.lineTo(15.0, ay + 4.5);
                arrow.closeSubpath();
                painter.setBrush(QColor(0xFF, 0xCC, 0x00));
                painter.setPen(QColor(0xD9, 0x77, 0x06));
                painter.drawPath(arrow);
            }
            painter.setRenderHint(QPainter::Antialiasing, false);
        }

        int textX = gutterWidth + 6 - hScroll;
        int textY = y + fm.ascent() + (rowHeight - fm.height()) / 2;

        painter.save();
        painter.setClipRect(textRect);

        if (r == highlightedLine) {
            QFont boldFont = font();
            boldFont.setBold(true);
            painter.setFont(boldFont);
            if (line.length() >= 32) {
                QString addrPart = line.left(10);
                QString codePart = line.mid(10, 22);
                QString srcPart = line.mid(32);

                QString codeDisplay = codePart;
                if (isModified) {
                    QString memTag = QString("[mem: %1]").arg(liveWord, 4, 16, QChar('0')).toUpper();
                    QString trimmedCode = codePart.trimmed();
                    codeDisplay = QString("%1 %2").arg(trimmedCode, memTag);
                    if (codeDisplay.length() < 22) {
                        codeDisplay = codeDisplay.leftJustified(22, ' ');
                    }
                }

                painter.setPen(Qt::white);
                painter.drawText(textX, textY, addrPart);
                int xCode = textX + fm.horizontalAdvance(addrPart);

                if (isModified) {
                    QRect modBg(xCode - 2, y + 1, fm.horizontalAdvance(codeDisplay) + 4, rowHeight - 2);
                    painter.fillRect(modBg, QColor(0xFF, 0xCC, 0x00, 50));
                    painter.setPen(QColor(0xFF, 0xCC, 0x00));
                } else {
                    painter.setPen(Qt::white);
                }
                painter.drawText(xCode, textY, codeDisplay);
                int xSrc = xCode + fm.horizontalAdvance(codeDisplay);

                painter.setPen(Qt::white);
                painter.drawText(xSrc, textY, srcPart);
            } else {
                painter.setPen(Qt::white);
                painter.drawText(textX, textY, line);
            }
        } else {
            QString trimmed = line.trimmed();
            bool isMetaOrComment = trimmed.startsWith('*') ||
                                   trimmed.startsWith("Assembler used:") ||
                                   trimmed.startsWith("Created On:") ||
                                   trimmed.startsWith("No errors") ||
                                   trimmed.startsWith("No warnings") ||
                                   line.contains("Starting Address");
            if (isMetaOrComment) {
                painter.setPen(QColor(0x6A, 0x99, 0x55)); // VS Code Comment Green
                painter.drawText(textX, textY, line);
            } else if (line.length() >= 32) {
                QString addrPart = line.left(10);
                QString codePart = line.mid(10, 22);
                QString srcPart = line.mid(32);

                // 1. Address column (#9CDCFE light blue)
                painter.setPen(QColor(0x9C, 0xDC, 0xFE));
                painter.drawText(textX, textY, addrPart);
                int xCode = textX + fm.horizontalAdvance(addrPart);

                // 2. Machine code hex column (#4EC9B0 bright teal, or #FFCC00 gold for self-modifying code)
                QString codeDisplay = codePart;
                if (isModified) {
                    QString memTag = QString("[mem: %1]").arg(liveWord, 4, 16, QChar('0')).toUpper();
                    QString trimmedCode = codePart.trimmed();
                    codeDisplay = QString("%1 %2").arg(trimmedCode, memTag);
                    if (codeDisplay.length() < 22) {
                        codeDisplay = codeDisplay.leftJustified(22, ' ');
                    }
                    QRect modBg(xCode - 2, y + 1, fm.horizontalAdvance(codeDisplay) + 4, rowHeight - 2);
                    painter.fillRect(modBg, QColor(0xFF, 0xCC, 0x00, 40));
                    painter.setPen(QColor(0xFF, 0xCC, 0x00));
                } else {
                    painter.setPen(QColor(0x4E, 0xC9, 0xB0));
                }
                painter.drawText(xCode, textY, codeDisplay);
                int xSrc = xCode + fm.horizontalAdvance(codeDisplay);

                // 3. Source code column (#D4D4D4 crisp light grey, or green if inline comment)
                if (srcPart.trimmed().startsWith('*')) {
                    painter.setPen(QColor(0x6A, 0x99, 0x55));
                } else {
                    painter.setPen(QColor(0xD4, 0xD4, 0xD4));
                }
                painter.drawText(xSrc, textY, srcPart);
            } else {
                painter.setPen(QColor(0xD4, 0xD4, 0xD4));
                painter.drawText(textX, textY, line);
            }
        }
        painter.restore();
    }
}

// -----------------------------------------------------------------------------
// ListingWidget
// -----------------------------------------------------------------------------

ListingWidget::ListingWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ListingWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    headerLabel = new QLabel(tr("   Address  --------Code---------   Line -----------Source----------->>"), this);
    QFont monoFont("Monospace", 9, QFont::Bold);
    monoFont.setStyleHint(QFont::Monospace);
    headerLabel->setFont(monoFont);
    headerLabel->setStyleSheet("background-color: #252526; color: #CCCCCC; padding: 4px 6px; border-bottom: 1px solid #3C3C3C;");
    layout->addWidget(headerLabel);

    area = new ListingScrollArea(this);
    layout->addWidget(area);

    connect(area, &ListingScrollArea::lineDoubleClicked, this, &ListingWidget::lineDoubleClicked);
    connect(area, &ListingScrollArea::breakpointToggled, this, &ListingWidget::breakpointToggled);
}

QString ListingWidget::expandTabs(const QString &line, int tabStop)
{
    QString res;
    res.reserve(line.length() + 16);
    int col = 0;
    for (QChar c : line) {
        if (c == QLatin1Char('\t')) {
            int spaces = tabStop - (col % tabStop);
            res.append(QString(spaces, QLatin1Char(' ')));
            col += spaces;
        } else {
            res.append(c);
            col++;
        }
    }
    return res;
}

void ListingWidget::loadListing(const QStringList &rawLines)
{
    QStringList expanded;
    expanded.reserve(rawLines.size());
    for (const QString &line : rawLines) {
        expanded.append(expandTabs(line, 8));
    }
    area->setListingLines(expanded);
}

void ListingWidget::highlightPC(uint32_t pc, bool isHalted)
{
    area->highlightPC(pc, isHalted);
}

void ListingWidget::clear()
{
    area->clear();
}

void ListingWidget::setSimulatorManager(SimulatorManager *sm)
{
    area->setSimulatorManager(sm);
}
