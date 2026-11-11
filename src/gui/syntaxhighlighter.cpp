#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Instruction Format (Bold blue)
    instructionFormat.setForeground(QColor(0, 102, 204));
    instructionFormat.setFontWeight(QFont::Bold);

    const QString instructionPatterns[] = {
        QStringLiteral("\\b(ABCD|ADD|ADDA|ADDI|ADDQ|ADDX|AND|ANDI|ASL|ASR)\\b"),
        QStringLiteral("\\b(BCC|BCS|BEQ|BGE|BGT|BHI|BLE|BLS|BLT|BMI|BNE|BPL|BVC|BVS|BRA|BSR)\\b"),
        QStringLiteral("\\b(BCHG|BCLR|BSET|BTST|CHK|CLR|CMP|CMPA|CMPI|CMPM)\\b"),
        QStringLiteral("\\b(DBCC|DBCS|DBEQ|DBF|DBGE|DBGT|DBHI|DBLE|DBLS|DBLT|DBMI|DBNE|DBPL|DBRA|DBT|DBVC|DBVS)\\b"),
        QStringLiteral("\\b(DIVS|DIVU|EOR|EORI|EXG|EXT|ILLEGAL|JMP|JSR|LEA|LINK|LSL|LSR)\\b"),
        QStringLiteral("\\b(MOVE|MOVEA|MOVEM|MOVEP|MOVEQ|MULS|MULU|NBCD|NEG|NEGX|NOP|NOT|OR|ORI)\\b"),
        QStringLiteral("\\b(PEA|RESET|ROL|ROR|ROXL|ROXR|RTE|RTR|RTS|SBCD|SCC|SCS|SEQ|SF)\\b"),
        QStringLiteral("\\b(SGE|SGT|SHI|SLE|SLS|SLT|SMI|SNE|SPL|ST|SVC|SVS|STOP|SWAP|TAS|TRAP|TRAPV|TST|UNLK|SIMHALT)\\b")
    };

    for (const QString &pattern : instructionPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = instructionFormat;
        highlightingRules.push_back(rule);
    }

    // Directives (Purple)
    directiveFormat.setForeground(QColor(128, 0, 128));
    directiveFormat.setFontWeight(QFont::Bold);

    const QString directivePatterns[] = {
        QStringLiteral("\\b(ORG|END|EQU|DC|DC\\.B|DC\\.W|DC\\.L|DS|DS\\.B|DS\\.W|DS\\.L|INCLUDE|INCBIN|OFFSET|SECTION|REG|SET|MACRO|ENDM)\\b")
    };

    for (const QString &pattern : directivePatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = directiveFormat;
        highlightingRules.push_back(rule);
    }

    // Registers (Teal)
    registerFormat.setForeground(QColor(0, 130, 130));
    registerFormat.setFontWeight(QFont::DemiBold);
    HighlightingRule regRule;
    regRule.pattern = QRegularExpression(QStringLiteral("\\b([DA][0-7]|SP|USP|SSP|PC|SR|CCR)\\b"), QRegularExpression::CaseInsensitiveOption);
    regRule.format = registerFormat;
    highlightingRules.push_back(regRule);

    // Numbers & Hex Constants (Dark Green)
    numberFormat.setForeground(QColor(0, 128, 0));
    HighlightingRule numRule;
    numRule.pattern = QRegularExpression(QStringLiteral("(#[\\$0-9A-Fa-f%]+|\\$[0-9A-Fa-f]+|%[01]+|\\b[0-9]+\\b)"));
    numRule.format = numberFormat;
    highlightingRules.push_back(numRule);

    // Strings (Dark Orange / Brown)
    stringFormat.setForeground(QColor(180, 80, 0));
    HighlightingRule strRuleSingle;
    strRuleSingle.pattern = QRegularExpression(QStringLiteral("'[^']*'"));
    strRuleSingle.format = stringFormat;
    highlightingRules.push_back(strRuleSingle);

    HighlightingRule strRuleDouble;
    strRuleDouble.pattern = QRegularExpression(QStringLiteral("\"[^\"]*\""));
    strRuleDouble.format = stringFormat;
    highlightingRules.push_back(strRuleDouble);

    // Comments (Gray italic) - Checked last so comments override keywords
    commentFormat.setForeground(QColor(120, 120, 120));
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
