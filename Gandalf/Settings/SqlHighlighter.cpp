#include "SqlHighlighter.h"

SqlHighlighter::SqlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // 1. КЛЮЧОВІ СЛОВА (SELECT, FROM, WHERE...) -> Жирний Синій
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    QStringList keywordPatterns;
    keywordPatterns << "\\bSELECT\\b" << "\\bFROM\\b" << "\\bWHERE\\b" << "\\bINSERT\\b"
                    << "\\bINTO\\b" << "\\bVALUES\\b" << "\\bUPDATE\\b" << "\\bSET\\b"
                    << "\\bDELETE\\b" << "\\bJOIN\\b" << "\\bLEFT\\b" << "\\bRIGHT\\b"
                    << "\\bINNER\\b" << "\\bOUTER\\b" << "\\bON\\b" << "\\bAS\\b"
                    << "\\bGROUP BY\\b" << "\\bORDER BY\\b" << "\\bHAVING\\b" << "\\bLIMIT\\b"
                    << "\\bAND\\b" << "\\bOR\\b" << "\\bNOT\\b" << "\\bIN\\b"
                    << "\\bIS\\b" << "\\bNULL\\b" << "\\bBETWEEN\\b" << "\\bLIKE\\b"
                    << "\\bCASE\\b" << "\\bWHEN\\b" << "\\bTHEN\\b" << "\\bELSE\\b" << "\\bEND\\b"
                    << "\\bDISTINCT\\b" << "\\bUNION\\b" << "\\bALL\\b" << "\\bEXISTS\\b"
                    << "\\bRETURNING\\b" << "\\bMATCHING\\b";

    for (const QString &pattern : keywordPatterns) {
        // CaseInsensitiveOption дозволяє підсвічувати і select, і SELECT
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // 2. ТИПИ ДАНИХ (INTEGER, VARCHAR...) -> Темно-фіолетовий
    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    QStringList typesPatterns;
    typesPatterns << "\\bINTEGER\\b" << "\\bINT\\b" << "\\bVARCHAR\\b" << "\\bCHAR\\b"
                  << "\\bDATE\\b" << "\\bTIMESTAMP\\b" << "\\bNUMERIC\\b" << "\\bFLOAT\\b"
                  << "\\bBLOB\\b" << "\\bSMALLINT\\b";

    for (const QString &pattern : typesPatterns) {
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = classFormat;
        highlightingRules.append(rule);
    }

    // 3. ФУНКЦІЇ (COUNT, SUM...) -> Коричневий
    functionFormat.setFontItalic(true);
    functionFormat.setForeground(QColor(150, 80, 0)); // Коричневий
    QStringList funcPatterns;
    funcPatterns << "\\bCOUNT\\b" << "\\bSUM\\b" << "\\bAVG\\b" << "\\bMAX\\b" << "\\bMIN\\b"
                 << "\\bCOALESCE\\b" << "\\bCAST\\b" << "\\bIIF\\b";

    for (const QString &pattern : funcPatterns) {
        rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        rule.format = functionFormat;
        highlightingRules.append(rule);
    }

    // 4. РЯДКИ ('text') -> Червоний
    quotationFormat.setForeground(Qt::darkRed);
    rule.pattern = QRegularExpression("\'.*?\'");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // 5. КОМЕНТАРІ (-- comment) -> Зелений
    singleLineCommentFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("--[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);
}

void SqlHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
