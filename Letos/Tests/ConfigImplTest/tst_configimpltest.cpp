#include "services/config.h"
#include "db/dbsqlite3.h"
#include "letos.h"
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

class ConfigImplTest : public QObject
{
    Q_OBJECT

    private:
        QTemporaryDir tempDir;

    private slots:
        void initTestCase();
        void cleanupTestCase();
        void testSqlHistoryUpdateHitsOwnEntry();
};

void ConfigImplTest::initTestCase()
{
    // Keep the test away from the real user configuration.
    QVERIFY(tempDir.isValid());
    QStandardPaths::setTestModeEnabled(true);
    qputenv("HOME", tempDir.path().toLocal8Bit());
    QVERIFY(QDir(tempDir.path()).mkdir("letos-cfg"));
    QVERIFY(QDir::setCurrent(tempDir.path()));

    LETOS->init({}, false);
    QVERIFY(!CFG->isInMemory());
}

void ConfigImplTest::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
    LETOS->cleanUp();
}

void ConfigImplTest::testSqlHistoryUpdateHitsOwnEntry()
{
    qint64 id1 = CFG->addSqlHistory("SELECT 1", "db", 0, 0);
    CFG->updateSqlHistory(id1, "SELECT 1", "db", 0, 1);
    qint64 id2 = CFG->addSqlHistory("SELECT 100", "db", 0, 0);
    CFG->updateSqlHistory(id2, "SELECT 100", "db", 0, 100);
    QThreadPool::globalInstance()->waitForDone();

    DbSqlite3 db("check", CFG->getConfigFilePath(), {});
    QVERIFY(db.open());
    SqlQueryPtr results = db.exec("SELECT id, sql, rows FROM sqleditor_history ORDER BY id");
    QVERIFY(!results->isError());

    QList<SqlResultsRowPtr> rows = results->getAll();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows[0]->value("id").toLongLong(), id1);
    QCOMPARE(rows[0]->value("sql").toString(), QString("SELECT 1"));
    QCOMPARE(rows[0]->value("rows").toInt(), 1);
    QCOMPARE(rows[1]->value("id").toLongLong(), id2);
    QCOMPARE(rows[1]->value("sql").toString(), QString("SELECT 100"));
    QCOMPARE(rows[1]->value("rows").toInt(), 100);
    db.close();
}

QTEST_GUILESS_MAIN(ConfigImplTest)

#include "tst_configimpltest.moc"
