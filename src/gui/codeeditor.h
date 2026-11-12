#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QSet>
#include <QWidget>

class LineNumberArea;
class QCompleter;
class QStringListModel;
class QTimer;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void highlightLine(int lineNumber);

    void setDiagnosticLines(const QSet<int> &errors, const QSet<int> &warnings);
    void clearDiagnostics();
    const QSet<int> &errorLines() const { return m_errorLines; }
    const QSet<int> &warningLines() const { return m_warningLines; }

    void setEditorFontSize(int pt);
    int editorFontSize() const { return m_fontSize; }

    void refreshCompletionModel();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void insertCompletion(const QString &completion);

private:
    QString textUnderCursor() const;
    bool isInsideComment() const;

    QWidget *lineNumberArea;
    QSet<int> m_errorLines;
    QSet<int> m_warningLines;
    int m_fontSize = 10;

    QCompleter *m_completer = nullptr;
    QStringListModel *m_completerModel = nullptr;
    QTimer *m_labelScanTimer = nullptr;
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

#endif // CODEEDITOR_H
