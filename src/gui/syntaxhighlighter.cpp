#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // VS Code Dark+ Palette for 68000 Assembly
    // 1. Labels at start of line or identifier: (#DCDCAA warm yellow)
    QTextCharFormat labelFormat;
    labelFormat.setForeground(QColor(0xDC, 0xDC, 0xAA));
    HighlightingRule labelRule;
    labelRule.pattern = QRegularExpression(QStringLiteral("^[A-Za-z_\\.][A-Za-z0-9_\\.]*:?"));
    labelRule.format = labelFormat;
    highlightingRules.push_back(labelRule);

    // 2. Instructions (#569CD6 VS Code Keyword Blue, Bold)
    instructionFormat.setForeground(QColor(0x56, 0x9C, 0xD6));
    instructionFormat.setFontWeight(QFont::Bold);

    const QString instructionPatterns[] = {
        QStringLiteral("\\b(ABCD|ADD|ADDA|ADDI|ADDQ|ADDX|AND|ANDI|ASL|ASR)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(BCC|BCS|BEQ|BGE|BGT|BHI|BLE|BLS|BLT|BMI|BNE|BPL|BVC|BVS|BRA|BSR)(\\.[BSWLbswl])?\\b"),
        QStringLiteral("\\b(BCHG|BCLR|BSET|BTST|CHK|CLR|CMP|CMPA|CMPI|CMPM)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(DBCC|DBCS|DBEQ|DBF|DBGE|DBGT|DBHI|DBLE|DBLS|DBLT|DBMI|DBNE|DBPL|DBRA|DBT|DBVC|DBVS)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(DIVS|DIVU|EOR|EORI|EXG|EXT|ILLEGAL|JMP|JSR|LEA|LINK|LSL|LSR)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(MOVE|MOVEA|MOVEM|MOVEP|MOVEQ|MULS|MULU|NBCD|NEG|NEGX|NOP|NOT|OR|ORI)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(PEA|RESET|ROL|ROR|ROXL|ROXR|RTE|RTR|RTS|SBCD|SCC|SCS|SEQ|SF)(\\.[BWLbwl])?\\b"),
        QStringLiteral("\\b(SGE|SGT|SHI|SLE|SLS|SLT|SMI|SNE|SPL|ST|SVC|SVS|STOP|SWAP|TAS|TRAP|TRAPV|TST|UNLK|SIMHALT)(\\.[BWLbwl])?\\b")
    };

    for (const QString &pattern : instructionPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = instructionFormat;
        highlightingRules.push_back(rule);
    }

    // 3. Directives (#C586C0 VS Code Control-Flow Purple, Bold)
    directiveFormat.setForeground(QColor(0xC5, 0x86, 0xC0));
    directiveFormat.setFontWeight(QFont::Bold);

    const QString directivePatterns[] = {
        QStringLiteral("\\b(ORG|END|EQU|DC|DC\\.B|DC\\.W|DC\\.L|DCB|DCB\\.B|DCB\\.W|DCB\\.L|DS|DS\\.B|DS\\.W|DS\\.L|INCLUDE|INCBIN|OFFSET|SECTION|OPT|REG|SET|MACRO|ENDM|IF|ELSE|ENDI|FOR|ENDF|REPEAT|UNTIL|WHILE|ENDW|DBLOOP)\\b")
    };

    for (const QString &pattern : directivePatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = directiveFormat;
        highlightingRules.push_back(rule);
    }

    // 4. Registers (#4EC9B0 VS Code Teal/Cyan, DemiBold)
    registerFormat.setForeground(QColor(0x4E, 0xC9, 0xB0));
    registerFormat.setFontWeight(QFont::DemiBold);
    HighlightingRule regRule;
    regRule.pattern = QRegularExpression(QStringLiteral("\\b([DA][0-7]|SP|USP|SSP|PC|SR|CCR)\\b"), QRegularExpression::CaseInsensitiveOption);
    regRule.format = registerFormat;
    highlightingRules.push_back(regRule);

    // 5. Numbers & Hex/Binary/Immediate Constants (#B5CEA8 VS Code Light Sage Green)
    numberFormat.setForeground(QColor(0xB5, 0xCE, 0xA8));
    HighlightingRule numRule;
    numRule.pattern = QRegularExpression(QStringLiteral("(#[\\$0-9A-Fa-f%]+|\\$[0-9A-Fa-f]+|%[01]+|\\b[0-9]+\\b)"));
    numRule.format = numberFormat;
    highlightingRules.push_back(numRule);

    // 6. Strings (#CE9178 VS Code Warm Terracotta)
    stringFormat.setForeground(QColor(0xCE, 0x91, 0x78));
    HighlightingRule strRuleSingle;
    strRuleSingle.pattern = QRegularExpression(QStringLiteral("'[^']*'"));
    strRuleSingle.format = stringFormat;
    highlightingRules.push_back(strRuleSingle);

    HighlightingRule strRuleDouble;
    strRuleDouble.pattern = QRegularExpression(QStringLiteral("\"[^\"]*\""));
    strRuleDouble.format = stringFormat;
    highlightingRules.push_back(strRuleDouble);

    // 7. Comments (#6A9955 VS Code Comment Green, Italic)
    commentFormat.setForeground(QColor(0x6A, 0x99, 0x55));
    commentFormat.setFontItalic(true);
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply syntax rules
    for (const HighlightingRule &rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Comments: line starting with '*' or anything after ';'
    if (text.startsWith('*')) {
        setFormat(0, text.length(), commentFormat);
    } else {
        int commentIndex = text.indexOf(';');
        if (commentIndex != -1) {
            setFormat(commentIndex, text.length() - commentIndex, commentFormat);
        }
    }
}
