#include <QtTest>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QVariantMap>

class tst_DBusSignatures : public QObject {
    Q_OBJECT

private:
    static QString computeArgumentsSignature(const QDBusMessage& msg) {
        QString sig;
        for (const auto& arg : msg.arguments()) {
            const char* typeSig = QDBusMetaType::typeToSignature(arg.metaType());
            if (typeSig) {
                sig += QString::fromLatin1(typeSig);
            }
        }
        return sig;
    }

private slots:
    void testNotifyPointerButtonSignature();
    void testNotifyPointerMotionRelativeSignature();
};

void tst_DBusSignatures::testNotifyPointerButtonSignature() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("NotifyPointerButton")
    );

    QDBusObjectPath sessionPath("/org/freedesktop/portal/desktop/session/1_2345/session_token");
    QVariantMap options;
    int32_t button = 272; // BTN_LEFT (signed 32-bit int: 'i')
    uint32_t state = 1;   // Pressed (unsigned 32-bit int: 'u')

    msg << QVariant::fromValue(sessionPath)
        << QVariant::fromValue(options)
        << button
        << state;

    // Strict D-Bus signature: o = object path, a{sv} = dict of string to variant, i = int32, u = uint32
    QCOMPARE(computeArgumentsSignature(msg), QStringLiteral("oa{sv}iu"));
}

void tst_DBusSignatures::testNotifyPointerMotionRelativeSignature() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.RemoteDesktop"),
        QStringLiteral("NotifyPointerMotionRelative")
    );

    QDBusObjectPath sessionPath("/org/freedesktop/portal/desktop/session/1_2345/session_token");
    QVariantMap options;
    double dx = 1.0;
    double dy = 0.0;

    msg << QVariant::fromValue(sessionPath)
        << QVariant::fromValue(options)
        << dx
        << dy;

    // Strict D-Bus signature: o = object path, a{sv} = dict of string to variant, d = double, d = double
    QCOMPARE(computeArgumentsSignature(msg), QStringLiteral("oa{sv}dd"));
}

QTEST_MAIN(tst_DBusSignatures)
#include "tst_DBusSignatures.moc"
