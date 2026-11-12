#ifndef FINDREPLACEBAR_H
#define FINDREPLACEBAR_H

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class CodeEditor;
class ListingWidget;

class FindReplaceBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplaceBar(QWidget *parent = nullptr);

    void setEditor(CodeEditor *editor);
    void setListingWidget(ListingWidget *listing);

    void showFind(const QString &initialQuery = QString());
    void showReplace(const QString &initialQuery = QString());
    void setQuery(const QString &query);
    QString query() const;
    void setReplacement(const QString &replacement);

public slots:
    bool findNext();
    bool findPrev();
    bool replaceCurrent();
    int replaceAll();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool performFind(bool forward);
    void updateReplaceVisibility(bool showReplaceRow);

    CodeEditor *m_editor = nullptr;
    ListingWidget *m_listing = nullptr;
    bool m_replaceVisible = false;

    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_caseCheck = nullptr;
    QCheckBox *m_wholeWordCheck = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_replaceBtn = nullptr;
    QPushButton *m_replaceAllBtn = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QLabel *m_replaceLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // FINDREPLACEBAR_H
