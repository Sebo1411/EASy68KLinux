#include "listingwidget.h"
#include "simulatormanager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QFontMetrics>
#include <algorithm>

bool ListingScrollArea::isInstructionLine(const QString &line)
{
    if (line.length() < 12) return false;
    for (int i = 0; i < 8; ++i) {
        QChar c = line[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    if (line.length() > 8 && line[8] == '=') return false;
    if (line.length() > 10 && line[10] == '=') return false;
    if (line.length() > 10 && line[10] != ' ' && line[10] != '\t') return true;
    if (line.length() > 11 && line[11] != ' ' && line[11] != '\t') return true;
    return false;
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

    int firstRow = verticalScrollBar()->value();
    int visibleRows = (viewport()->height() / rowHeight) + 1;
    int lastRow = std::min(static_cast<int>(listingLines.size()) - 1, firstRow + visibleRows);

    int hScroll = horizontalScrollBar()->value();

    if (listingLines.isEmpty()) {
        painter.setPen(Qt::darkGray);
        painter.drawText(viewport()->rect(), Qt::AlignCenter,
                         tr("No source listing (.L68) loaded.\nAssemble program or load S-Record to view code."));
        return;
    }

    // Paint gutter background
    painter.fillRect(0, 0, gutterWidth, viewport()->height(), QColor(245, 245, 245));
    painter.setPen(QColor(210, 210, 210));
    painter.drawLine(gutterWidth - 1, 0, gutterWidth - 1, viewport()->height());

    for (int r = firstRow; r <= lastRow; ++r) {
        int y = (r - firstRow) * rowHeight;
        const QString &line = listingLines[r];
        bool isInstr = isInstructionLine(line);
        uint32_t addr = isInstr ? getLineAddress(line) : 0;
        bool hasBp = (isInstr && simManager && simManager->hasBreakpoint(addr));

        // Draw gutter dot
        if (isInstr) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            int dotRadius = 4;
            int dotCenterX = gutterWidth / 2 - 1;
            int dotCenterY = y + rowHeight / 2;

            if (hasBp) {
                // Red dot for breakpoint
                painter.setBrush(QColor(220, 38, 38));
                painter.setPen(QColor(153, 27, 27));
            } else {
                // Green dot for executable instruction
                painter.setBrush(QColor(22, 163, 74));
                painter.setPen(QColor(21, 128, 61));
            }
            painter.drawEllipse(QPoint(dotCenterX, dotCenterY), dotRadius, dotRadius);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }

        // Draw line background
        QRect textRect(gutterWidth, y, viewport()->width() - gutterWidth, rowHeight);
        if (r == highlightedLine) {
            // Sim68K classic royal navy blue highlight
            painter.fillRect(textRect, QColor(0, 0, 128));
            painter.setPen(Qt::white);
        } else if (r == selectedLine) {
            // Selected cursor row for Run To Cursor / F2
            painter.fillRect(textRect, QColor(219, 234, 254));
            painter.setPen(QColor(29, 78, 216));
            painter.drawRect(textRect.adjusted(0, 0, -1, -1));
            painter.setPen(Qt::black);
        } else {
            painter.setPen(Qt::black);
        }

        // Draw text
        int textX = gutterWidth + 4 - hScroll;
        int textY = y + fm.ascent() + (rowHeight - fm.height()) / 2;
        painter.drawText(textX, textY, line);
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

    headerLabel = new QLabel(tr(" Address  --------Code---------   Line -----------Source----------->>"), this);
    QFont monoFont("Monospace", 9, QFont::Bold);
    monoFont.setStyleHint(QFont::Monospace);
    headerLabel->setFont(monoFont);
    headerLabel->setStyleSheet("background-color: #E2E8F0; color: #1E293B; padding: 3px 6px; border-bottom: 1px solid #CBD5E1;");
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
