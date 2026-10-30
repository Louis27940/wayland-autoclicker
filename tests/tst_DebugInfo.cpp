#include <QtTest>
#include "DebugDialog.hpp"

class tst_DebugInfo : public QObject {
    Q_OBJECT

private slots:
    void testReadProcessMemory();
    void testFormatKb();
};

void tst_DebugInfo::testReadProcessMemory() {
    auto mem = DebugDialog::readProcessMemory();
    // On Linux procfs, RSS and VmSize must be positive non-zero values for any running process
    QVERIFY2(mem.vmRssKb > 0, "VmRSS should be greater than 0");
    QVERIFY2(mem.vmSizeKb > 0, "VmSize should be greater than 0");
    QVERIFY2(mem.threads >= 1, "Thread count should be at least 1");
}

void tst_DebugInfo::testFormatKb() {
    QCOMPARE(DebugDialog::formatKb(0), QStringLiteral("N/A"));
    QCOMPARE(DebugDialog::formatKb(-5), QStringLiteral("N/A"));
    QVERIFY(DebugDialog::formatKb(512).contains(QStringLiteral("512 kB")));
    QVERIFY(DebugDialog::formatKb(2048).contains(QStringLiteral("2.00 MB")));
}

QTEST_MAIN(tst_DebugInfo)
#include "tst_DebugInfo.moc"
