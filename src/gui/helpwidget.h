#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>

class HelpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HelpWidget(QWidget *parent = nullptr);
    ~HelpWidget() override = default;

    void loadPage(const QString &relativeOrAbsolutePath);
    void openQuickRefPdf();
    QString findHelpRootDir() const;

private slots:
    void onHomeClicked();
    void onInstructionsClicked();
    void onDirectivesClicked();
    void onTrapIoClicked();
    void onHardwareClicked();
    void onPdfClicked();
    void onSearchTextChanged(const QString &text);
    void onFindNext();

private:
    void setupUi();

    QTextBrowser *textBrowser;
    QLineEdit *searchEdit;
    QString helpRootDir;
};

#endif // HELPWIDGET_H
