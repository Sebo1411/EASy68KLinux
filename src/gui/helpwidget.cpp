#include "helpwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>

HelpWidget::HelpWidget(QWidget *parent)
    : QWidget(parent)
{
    helpRootDir = findHelpRootDir();
    setupUi();
    onHomeClicked();
}

QString HelpWidget::findHelpRootDir() const
{
    QStringList candidates = {
        QDir::currentPath() + "/docs/help",
        QCoreApplication::applicationDirPath() + "/docs/help",
        QCoreApplication::applicationDirPath() + "/../docs/help",
        QCoreApplication::applicationDirPath() + "/../../docs/help",
        QCoreApplication::applicationDirPath() + "/../share/doc/easy68k/help",
        "/usr/share/doc/easy68k/help",
        "/usr/local/share/doc/easy68k/help"
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path + "/main.htm")) {
            return QDir(path).canonicalPath();
        }
    }

    return candidates.first();
}

void HelpWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // Top Navigation Toolbar
    auto *navBar = new QHBoxLayout();
    navBar->setSpacing(4);

    auto *backBtn = new QPushButton(tr("<- Back"), this);
    auto *fwdBtn = new QPushButton(tr("Forward ->"), this);
    auto *homeBtn = new QPushButton(tr("Home"), this);
    auto *instBtn = new QPushButton(tr("68000 Instructions"), this);
    auto *dirBtn = new QPushButton(tr("Directives"), this);
    auto *trapBtn = new QPushButton(tr("TRAP #15 I/O"), this);
    auto *hwBtn = new QPushButton(tr("Hardware I/O"), this);
    auto *pdfBtn = new QPushButton(tr("QuickRef PDF"), this);

    connect(backBtn, &QPushButton::clicked, this, [this]() { textBrowser->backward(); });
    connect(fwdBtn, &QPushButton::clicked, this, [this]() { textBrowser->forward(); });
    connect(homeBtn, &QPushButton::clicked, this, &HelpWidget::onHomeClicked);
    connect(instBtn, &QPushButton::clicked, this, &HelpWidget::onInstructionsClicked);
    connect(dirBtn, &QPushButton::clicked, this, &HelpWidget::onDirectivesClicked);
    connect(trapBtn, &QPushButton::clicked, this, &HelpWidget::onTrapIoClicked);
    connect(hwBtn, &QPushButton::clicked, this, &HelpWidget::onHardwareClicked);
    connect(pdfBtn, &QPushButton::clicked, this, &HelpWidget::onPdfClicked);

    navBar->addWidget(backBtn);
    navBar->addWidget(fwdBtn);
    navBar->addWidget(homeBtn);
    navBar->addWidget(instBtn);
    navBar->addWidget(dirBtn);
    navBar->addWidget(trapBtn);
    navBar->addWidget(hwBtn);
    navBar->addWidget(pdfBtn);
    navBar->addStretch();

    // Search Box
    navBar->addWidget(new QLabel(tr("Find:"), this));
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("Search in page..."));
    searchEdit->setMaximumWidth(160);
    connect(searchEdit, &QLineEdit::textChanged, this, &HelpWidget::onSearchTextChanged);
    connect(searchEdit, &QLineEdit::returnPressed, this, &HelpWidget::onFindNext);
    navBar->addWidget(searchEdit);

    mainLayout->addLayout(navBar);

    // HTML Browser
    textBrowser = new QTextBrowser(this);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setSearchPaths(QStringList{helpRootDir, helpRootDir + "/Images", helpRootDir + "/SIMHELP"});
    mainLayout->addWidget(textBrowser);
}

void HelpWidget::loadPage(const QString &relativeOrAbsolutePath)
{
    QString fullPath;
    if (QFileInfo(relativeOrAbsolutePath).isAbsolute()) {
        fullPath = relativeOrAbsolutePath;
    } else {
        fullPath = helpRootDir + "/" + relativeOrAbsolutePath;
    }

    QFileInfo fi(fullPath);
    if (fi.exists()) {
        textBrowser->setSource(QUrl::fromLocalFile(fi.canonicalFilePath()));
    } else {
        textBrowser->setHtml(tr("<h3>Help file not found</h3><p>Could not locate: <code>%1</code></p>"
                                "<p>Checked root directory: <code>%2</code></p>")
                             .arg(fullPath, helpRootDir));
    }
}

void HelpWidget::onHomeClicked()
{
    loadPage("main.htm");
}

void HelpWidget::onInstructionsClicked()
{
    loadPage("Reference/68ks1.htm");
}

void HelpWidget::onDirectivesClicked()
{
    loadPage("Directives/directives.htm");
}

void HelpWidget::onTrapIoClicked()
{
    loadPage("simio.htm");
}

void HelpWidget::onHardwareClicked()
{
    loadPage("SIMHELP/Hardware.htm");
}

void HelpWidget::onPdfClicked()
{
    openQuickRefPdf();
}

void HelpWidget::openQuickRefPdf()
{
    QString pdfPath = helpRootDir + "/EASy68KQuickRef.pdf";
    if (QFileInfo::exists(pdfPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(pdfPath));
    } else {
        QMessageBox::information(this, tr("PDF Not Found"),
                                 tr("Could not find EASy68K Quick Reference PDF at:\n%1").arg(pdfPath));
    }
}

void HelpWidget::onSearchTextChanged(const QString &text)
{
    if (!text.isEmpty()) {
        textBrowser->find(text);
    }
}

void HelpWidget::onFindNext()
{
    if (!searchEdit->text().isEmpty()) {
        if (!textBrowser->find(searchEdit->text())) {
            // Wrap around to beginning
            textBrowser->moveCursor(QTextCursor::Start);
            textBrowser->find(searchEdit->text());
        }
    }
}
