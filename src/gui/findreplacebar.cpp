#include "findreplacebar.h"
#include "codeeditor.h"
#include "listingwidget.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(
        "QWidget { background-color: #252526; color: #CCCCCC; border-top: 1px solid #3C3C3C; }"
        "QLineEdit { background-color: #1E1E1E; color: #D4D4D4; border: 1px solid #3C3C3C; padding: 3px 6px; border-radius: 2px; }"
        "QPushButton { background-color: #333337; color: #E0E0E0; border: 1px solid #454545; padding: 3px 8px; border-radius: 2px; }"
        "QPushButton:hover { background-color: #3E3E42; }"
        "QToolButton { background: transparent; color: #CCCCCC; border: none; padding: 2px 6px; font-weight: bold; }"
        "QToolButton:hover { background-color: #3E3E42; }"
    );

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);

    auto *findLabel = new QLabel(tr("Find:"), this);
    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(tr("Search text..."));
    m_findEdit->setMinimumWidth(200);

    m_prevBtn = new QPushButton(tr("Prev"), this);
    m_nextBtn = new QPushButton(tr("Next"), this);
    m_caseCheck = new QCheckBox(tr("Match Case"), this);
    m_wholeWordCheck = new QCheckBox(tr("Whole Word"), this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #9CDCFE; padding-left: 6px;");

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip(tr("Close (Esc)"));

    grid->addWidget(findLabel, 0, 0);
    grid->addWidget(m_findEdit, 0, 1);
    grid->addWidget(m_prevBtn, 0, 2);
    grid->addWidget(m_nextBtn, 0, 3);
    grid->addWidget(m_caseCheck, 0, 4);
    grid->addWidget(m_wholeWordCheck, 0, 5);
    grid->addWidget(m_statusLabel, 0, 6);
    grid->setColumnStretch(6, 1);
    grid->addWidget(m_closeBtn, 0, 7);

    m_replaceLabel = new QLabel(tr("Replace:"), this);
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(tr("Replacement text..."));
    m_replaceBtn = new QPushButton(tr("Replace"), this);
    m_replaceAllBtn = new QPushButton(tr("Replace All"), this);

    grid->addWidget(m_replaceLabel, 1, 0);
    grid->addWidget(m_replaceEdit, 1, 1);
    grid->addWidget(m_replaceBtn, 1, 2);
    grid->addWidget(m_replaceAllBtn, 1, 3);

    connect(m_nextBtn, &QPushButton::clicked, this, &FindReplaceBar::findNext);
    connect(m_prevBtn, &QPushButton::clicked, this, &FindReplaceBar::findPrev);
    connect(m_replaceBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceCurrent);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() {
        hide();
        if (m_editor) m_editor->setFocus();
        else if (m_listing) m_listing->setFocus();
    });
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::findNext);
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::replaceCurrent);

    updateReplaceVisibility(false);
    hide();
}

void FindReplaceBar::setEditor(CodeEditor *editor)
{
    m_editor = editor;
    if (editor) {
        m_listing = nullptr;
    }
    updateReplaceVisibility(m_replaceVisible && m_editor != nullptr);
}

void FindReplaceBar::setListingWidget(ListingWidget *listing)
{
    m_listing = listing;
    if (listing) {
        m_editor = nullptr;
    }
    updateReplaceVisibility(false);
}

void FindReplaceBar::setQuery(const QString &query)
{
    m_findEdit->setText(query);
}

QString FindReplaceBar::query() const
{
    return m_findEdit->text();
}

void FindReplaceBar::setReplacement(const QString &replacement)
{
    m_replaceEdit->setText(replacement);
}

void FindReplaceBar::updateReplaceVisibility(bool showReplaceRow)
{
    const bool visible = showReplaceRow && (m_editor != nullptr) && !m_editor->isReadOnly();
    m_replaceLabel->setVisible(visible);
    m_replaceEdit->setVisible(visible);
    m_replaceBtn->setVisible(visible);
    m_replaceAllBtn->setVisible(visible);
}

void FindReplaceBar::showFind(const QString &initialQuery)
{
    m_replaceVisible = false;
    updateReplaceVisibility(false);
    if (!initialQuery.isEmpty()) {
        m_findEdit->setText(initialQuery);
    }
    m_statusLabel->clear();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceBar::showReplace(const QString &initialQuery)
{
    m_replaceVisible = true;
    updateReplaceVisibility(true);
    if (!initialQuery.isEmpty()) {
        m_findEdit->setText(initialQuery);
    }
    m_statusLabel->clear();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void FindReplaceBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        if (m_editor) m_editor->setFocus();
        else if (m_listing) m_listing->setFocus();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers().testFlag(Qt::ShiftModifier)
        && m_findEdit->hasFocus()) {
        findPrev();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool FindReplaceBar::findNext()
{
    return performFind(true);
}

bool FindReplaceBar::findPrev()
{
    return performFind(false);
}

bool FindReplaceBar::performFind(bool forward)
{
    const QString term = m_findEdit->text();
    if (term.isEmpty()) {
        m_statusLabel->clear();
        return false;
    }

    const bool cs = m_caseCheck->isChecked();
    const bool ww = m_wholeWordCheck->isChecked();

    if (m_listing) {
        const bool ok = m_listing->findText(term, forward, cs, ww);
        m_statusLabel->setText(ok ? tr("Match found") : tr("Not found"));
        return ok;
    }

    if (!m_editor) return false;

    QTextDocument::FindFlags flags;
    if (!forward) flags |= QTextDocument::FindBackward;
    if (cs) flags |= QTextDocument::FindCaseSensitively;
    if (ww) flags |= QTextDocument::FindWholeWords;

    bool found = m_editor->find(term, flags);
    if (!found) {
        // Wrap around once
        QTextCursor tc = m_editor->textCursor();
        tc.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        m_editor->setTextCursor(tc);
        found = m_editor->find(term, flags);
    }

    m_statusLabel->setText(found ? tr("Match found") : tr("Not found"));
    return found;
}

bool FindReplaceBar::replaceCurrent()
{
    if (!m_editor || m_editor->isReadOnly()) return false;
    const QString term = m_findEdit->text();
    if (term.isEmpty()) return false;

    QTextCursor tc = m_editor->textCursor();
    const Qt::CaseSensitivity cs = m_caseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (tc.hasSelection() && QString::compare(tc.selectedText(), term, cs) == 0) {
        tc.insertText(m_replaceEdit->text());
        m_editor->setTextCursor(tc);
        findNext();
        return true;
    }

    if (findNext()) {
        QTextCursor nextTc = m_editor->textCursor();
        if (nextTc.hasSelection()) {
            nextTc.insertText(m_replaceEdit->text());
            m_editor->setTextCursor(nextTc);
            m_statusLabel->setText(tr("Replaced"));
            return true;
        }
    }
    return false;
}

int FindReplaceBar::replaceAll()
{
    if (!m_editor || m_editor->isReadOnly()) return 0;
    const QString term = m_findEdit->text();
    if (term.isEmpty()) return 0;

    QTextDocument::FindFlags flags;
    if (m_caseCheck->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (m_wholeWordCheck->isChecked()) flags |= QTextDocument::FindWholeWords;

    const QString replacement = m_replaceEdit->text();
    int count = 0;

    QTextCursor editCursor = m_editor->textCursor();
    editCursor.beginEditBlock();

    QTextCursor searchCursor(m_editor->document());
    searchCursor.movePosition(QTextCursor::Start);
    m_editor->setTextCursor(searchCursor);

    while (m_editor->find(term, flags)) {
        QTextCursor match = m_editor->textCursor();
        match.insertText(replacement);
        ++count;
    }

    editCursor.endEditBlock();
    m_statusLabel->setText(tr("Replaced %1").arg(count));
    return count;
}
