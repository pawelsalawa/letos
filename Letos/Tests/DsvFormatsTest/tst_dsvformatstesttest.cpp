#include <QString>
#include <QList>
#include <QStringList>
#include <QtTest>
#include <QBuffer>
#include <QStringEncoder>
#include "tsvserializer.h"
#include "csvserializer.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtSystemDetection>
#else
#include <qsystemdetection.h>
#endif

// TODO Add tests for CsvSerializer

class DsvFormatsTestTest : public QObject
{
        Q_OBJECT

    public:
        DsvFormatsTestTest();

private:
        QString toString(const QList<QStringList>& input);

        QList<QStringList> sampleData;
        QList<QStringList> sampleDeserializedData;
        QString sampleTsv;

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void testTsv1();
        void testTsv2();
        void testCsv1();
        void testCsv2Unix();
        void testCsv2Win();
        void testCsv2Mac();
        void testCsv3Unix();
        void testCsv3Win();
        void testCsv3Mac();
        void testCsvUnicodeEntries_data();
        void testCsvUnicodeEntries();
        void testCsvLongSeparatorEntries();
        void testCsvQuotedUnicodeEntries();
        void testCsvPerformance();
};

DsvFormatsTestTest::DsvFormatsTestTest()
{
}

QString DsvFormatsTestTest::toString(const QList<QStringList>& input)
{
    QStringList outputLines;
    for (const QStringList& list : input)
        outputLines << "QStringList("+list.join(", ")+")";

    return "QList(\n    "+outputLines.join(",\n    ")+"\n)";
}

void DsvFormatsTestTest::initTestCase()
{
    sampleData << QStringList{"a", "b c", "\"d\""};
    sampleData << QStringList{"a\"a\"", "\"b\"c\"", "d\"\"e"};
    sampleData << QStringList{"a\na", "b\tc", "d\t\"e"};
    sampleData << QStringList{"a", "", "b", ""};

#ifdef Q_OS_MACX
    QString lineSep = "\r";
#else
    QString lineSep = "\n";
#endif

    sampleTsv = "";
    sampleTsv += "a\tb c\t\"d\"";
    sampleTsv += lineSep;
    sampleTsv += "a\"a\"\t\"b\"c\"\td\"\"e";
    sampleTsv += lineSep;
    sampleTsv += "\"a\na\"\t\"b\tc\"\t\"d\t\"\"e\"";
    sampleTsv += lineSep;
    sampleTsv += "a\t\tb\t";

    sampleDeserializedData << QStringList{"a", "b c", "\"d\""};
    sampleDeserializedData << QStringList{"a\"a\"", "\"b\"c\"", "d\"\"e"};
    sampleDeserializedData << QStringList{"a\na", "\"b", "c\"", "\"d", "\"\"e\""};
    sampleDeserializedData << QStringList{"a", "", "b", ""};
}

void DsvFormatsTestTest::cleanupTestCase()
{
}

void DsvFormatsTestTest::testTsv1()
{
    QString result = TsvSerializer::serialize(sampleData);

    QString common = "";
    int i = 0;
    if (result != sampleTsv)
    {
        int lgt = qMax(result.length(), sampleTsv.length());
        for (i = 0; i < lgt && result[i] == sampleTsv[i]; i++)
            common.append(result[i]);
    }

    QVERIFY2(result == sampleTsv, QString("Mismatch after %1: %2\nSample: %3\nGot   : %4").arg(i).arg(common, sampleTsv, result).toLocal8Bit().data());
}

void DsvFormatsTestTest::testTsv2()
{
    QList<QStringList> result = TsvSerializer::deserialize(sampleTsv);

    QVERIFY2(result == sampleDeserializedData, QString("Sample: %1\nGot: %2").arg(toString(sampleDeserializedData), toString(result)).toLocal8Bit().data());
}

void DsvFormatsTestTest::testCsv1()
{
    QList<QStringList> result = CsvSerializer::deserialize(QString("a,\"\""), CsvFormat::DEFAULT);

    QVERIFY(result.size() == 1);
    QVERIFY(result.first().size() == 2);
}

void DsvFormatsTestTest::testCsv2Unix()
{
    QString data = "v1,v2\nv3,v4\n";
    QList<QStringList> result = CsvSerializer::deserialize(data, CsvFormat::DEFAULT);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsv2Win()
{
    QString data = "v1,v2\r\nv3,v4\r\n";
    QList<QStringList> result = CsvSerializer::deserialize(data, CsvFormat::DEFAULT);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsv2Mac()
{
    QString data = "v1,v2\rv3,v4\r";
    QList<QStringList> result = CsvSerializer::deserialize(data, CsvFormat::DEFAULT);
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsv3Unix()
{
    QString data = "v1,v2\nv3,v4\n";
    QTextStream stream(&data);
    QList<QStringList> result;
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);

    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsv3Win()
{
    QString data = "v1,v2\r\nv3,v4\r\n";
    QTextStream stream(&data);
    QList<QStringList> result;
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);

    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsv3Mac()
{
    QString data = "v1,v2\rv3,v4\r";
    QTextStream stream(&data);
    QList<QStringList> result;
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);
    result << CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT);

    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].size(), 2);
    QCOMPARE(result[0][0], "v1");
    QCOMPARE(result[0][1], "v2");
    QCOMPARE(result[1].size(), 2);
    QCOMPARE(result[1][0], "v3");
    QCOMPARE(result[1][1], "v4");
}

void DsvFormatsTestTest::testCsvUnicodeEntries_data()
{
    QTest::addColumn<QString>("lineEnding");
    QTest::addColumn<int>("encoding");
    for (auto encoding : {QStringConverter::Utf8, QStringConverter::Utf16LE, QStringConverter::Utf16BE})
    {
        for (const QString& lineEnding : {QString("\n"), QString("\r"), QString("\r\n")})
        {
            QByteArray name = QByteArray::number(encoding) + "-" + lineEnding.toUtf8().toHex();
            QTest::newRow(name.constData()) << lineEnding << int(encoding);
        }
    }
}

void DsvFormatsTestTest::testCsvUnicodeEntries()
{
    QFETCH(QString, lineEnding);
    QFETCH(int, encoding);
    QList<QStringList> expected = {{"heading", "value"}, {QStringLiteral("\u00e9clair"), "one"}, {QStringLiteral("\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2"), "two"},
                                   {QStringLiteral("\u4e2d\u6587"), "three"}, {QStringLiteral("\U0001f680rocket"), "four"}};
    QStringList lines;
    for (const QStringList& row : expected)
        lines << row.join(',');

    QByteArray bytes = QStringEncoder(QStringConverter::Encoding(encoding))(lines.join(lineEnding) + lineEnding);
    QBuffer buffer(&bytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QTextStream stream(&buffer);
    stream.setEncoding(QStringConverter::Encoding(encoding));
    for (const QStringList& row : expected)
        QCOMPARE(CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT), row);

    QVERIFY(stream.atEnd());
    QVERIFY(stream.seek(0));
    QCOMPARE(CsvSerializer::deserialize(stream, CsvFormat::DEFAULT), expected);
}

void DsvFormatsTestTest::testCsvLongSeparatorEntries()
{
    QString text = QStringLiteral("heading|||value\n\u00e9clair|||one|\n\U0001f680rocket|||three\n\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2|||two\n");
    QByteArray bytes = text.toUtf8();
    QBuffer buffer(&bytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QTextStream stream(&buffer);
    stream.setEncoding(QStringConverter::Utf8);
    CsvFormat format(QStringList{"|||"}, QStringList{"\r\n", "\n", "\r"});
    format.strictColumnSeparator = true;
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, format), (QStringList{"heading", "value"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, format), (QStringList{QStringLiteral("\u00e9clair"), "one|"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, format), (QStringList{QStringLiteral("\U0001f680rocket"), "three"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, format), (QStringList{QStringLiteral("\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2"), "two"}));
    QTextStream stringStream(&text);
    QCOMPARE(CsvSerializer::deserializeOneEntry(stringStream, format), (QStringList{"heading", "value"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stringStream, format), (QStringList{QStringLiteral("\u00e9clair"), "one|"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stringStream, format), (QStringList{QStringLiteral("\U0001f680rocket"), "three"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stringStream, format), (QStringList{QStringLiteral("\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2"), "two"}));
}

void DsvFormatsTestTest::testCsvQuotedUnicodeEntries()
{
    QByteArray bytes = QStringLiteral("heading,value\n\"\u00e9clair,\"\"\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2\"\"\n\u4e2d\u6587\",one\n\U0001f680rocket,two\n").toUtf8();
    QBuffer buffer(&bytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QTextStream stream(&buffer);
    stream.setEncoding(QStringConverter::Utf8);
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT), (QStringList{"heading", "value"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT), (QStringList{QStringLiteral("\u00e9clair,\"\u03ba\u03cc\u03c3\u03bc\u03bf\u03c2\"\n\u4e2d\u6587"), "one"}));
    QCOMPARE(CsvSerializer::deserializeOneEntry(stream, CsvFormat::DEFAULT), (QStringList{QStringLiteral("\U0001f680rocket"), "two"}));
}

void DsvFormatsTestTest::testCsvPerformance()
{
    QString input;
    for (int i = 0; i < 10000; i++)
        input += "abc,d,g,\"jkl\nh\",mno\r\n";

    QTemporaryFile theFile;
    if (!theFile.open())
    {
        qCritical() << "Failed to open" << theFile.fileName();
        return;
    }
    theFile.write(input.toLatin1());
    theFile.seek(0);
    QTextStream stream(&theFile);

    QElapsedTimer timer;
    timer.start();
    QList<QStringList> result = CsvSerializer::deserialize(stream, CsvFormat::DEFAULT);
    int time = timer.elapsed();

    QVERIFY(result.size() == 10000);
    QVERIFY(result.first().size() == 5);
    QVERIFY(result.last().size() == 5);

    qDebug() << "Deserialization time:" << time;
}

QTEST_APPLESS_MAIN(DsvFormatsTestTest)

#include "tst_dsvformatstesttest.moc"
