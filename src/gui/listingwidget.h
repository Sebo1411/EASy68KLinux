#ifndef LISTINGWIDGET_H
#define LISTINGWIDGET_H

#include <QWidget>
#include <QAbstractScrollArea>
#include <QStringList>
#include <cstdint>

class SimulatorManager;
class QLabel;

class ListingScrollArea : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit ListingScrollArea(QWidget *parent = nullptr);

    void setSimulatorManager(SimulatorManager *sm) { simManager = sm; }
    void setListingLines(const QStringList &lines);
    void highlightPC(uint32_t pc, bool isHalted);
    void clear();

    int getHighlightedLine() const { return highlightedLine; }
    int getSelectedLine() const { return selectedLine; }
    void setSelectedLine(int line);
    int getLineCount() const { return listingLines.size(); }
    int getFirstVisibleLine() const;
    int getLastVisibleLine() const;
    static bool isInstructionLine(const QString &line);
    static uint32_t getLineAddress(const QString &line);

signals:
    void lineDoubleClicked(uint32_t addr);
    void breakpointToggled(uint32_t addr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateScrollBars();

    SimulatorManager *simManager = nullptr;
    QStringList listingLines;
    int highlightedLine = -1;
    int selectedLine = -1;
    int gutterWidth = 26;
    int rowHeight = 18;
    int maxLineWidthPx = 600;
};

class ListingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ListingWidget(QWidget *parent = nullptr);

    void setSimulatorManager(SimulatorManager *sm);
    void loadListing(const QStringList &rawLines);
    void highlightPC(uint32_t pc, bool isHalted);
    void clear();

    ListingScrollArea* scrollArea() const { return area; }

signals:
    void lineDoubleClicked(uint32_t addr);
    void breakpointToggled(uint32_t addr);

private:
    QLabel *headerLabel;
    ListingScrollArea *area;

    void setupUi();
    static QString expandTabs(const QString &line, int tabStop = 8);
};

#endif // LISTINGWIDGET_H
