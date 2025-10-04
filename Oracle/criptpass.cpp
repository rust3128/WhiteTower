#include "CriptPass.h"
#include "qaesencryption.h"
// Вам потрібно буде створити файл AppSettings.h або замінити на реальні значення
#include "AppParams.h" // Умовно, що цей файл є в Oracle
#include <QCryptographicHash>

// Реалізація синглтона
CriptPass& CriptPass::instance()
{
    static CriptPass self;
    return self;
}

// Конструктор тепер викликається лише один раз
CriptPass::CriptPass()
{
    const QString key = AppParams::instance().crypto.KEY;
    const QString iv = AppParams::instance().crypto.IV;

    hashKey = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    hashIV = QCryptographicHash::hash(iv.toUtf8(), QCryptographicHash::Md5);
}

QString CriptPass::criptPass(const QString& password)
{
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::CBC);
    QByteArray encodeText = encryption.encode(password.toUtf8(), hashKey, hashIV);
    return QString(encodeText.toBase64());
}

QString CriptPass::decriptPass(const QString& password)
{
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::CBC);
    QByteArray encodeText = QByteArray::fromBase64(password.toUtf8());
    QByteArray decodeText = encryption.decode(encodeText, hashKey, hashIV);
    return QString::fromUtf8(encryption.removePadding(decodeText));
}

// ... решта методів залишається без змін ...
QString CriptPass::cryptVNCPass(const QString& termID, const QString& pass)
{
    int desiredLength = 5;
    QString termID_copy = termID;
    while (termID_copy.length() < desiredLength) {
        termID_copy.prepend('0');
    }
    QString password = termID_copy.right(3) + pass + termID_copy.left(2);
    password = criptPass(password);
    return password;
}

QString CriptPass::decryptVNCPass(const QString& pass)
{
    QString password = decriptPass(pass);
    password = password.mid(3, password.length() - 5);
    return password;
}
