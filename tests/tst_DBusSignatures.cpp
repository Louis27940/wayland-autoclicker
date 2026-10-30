#include <QtTest>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QVariantMap>
#include "PortalManager.hpp"

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
    void testBindShortcutsSignature();
    void testFormatTriggerForXdg();
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

void tst_DBusSignatures::testBindShortcutsSignature() {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QStringLiteral("BindShortcuts")
    );

    QDBusObjectPath sessionPath("/org/freedesktop/portal/desktop/session/1_2345/session_token");
    QList<PortalShortcut> shortcuts;
    shortcuts.append(PortalShortcut{"toggle", QVariantMap{{"description", "Toggle"}, {"preferred_trigger", "F6"}}});
    shortcuts.append(PortalShortcut{"stop", QVariantMap{{"description", "Stop"}, {"preferred_trigger", "F7"}}});
    QString parentWindow = "";
    QVariantMap options;

    msg << QVariant::fromValue(sessionPath)
        << QVariant::fromValue(shortcuts)
        << parentWindow
        << QVariant::fromValue(options);

    // Strict D-Bus signature: o = object path, a(sa{sv}) = array of shortcut structs, s = parent window, a{sv} = options
    QCOMPARE(computeArgumentsSignature(msg), QStringLiteral("oa(sa{sv})sa{sv}"));
}

void tst_DBusSignatures::testFormatTriggerForXdg() {
    QCOMPARE(PortalManager::formatTriggerForXdg(QStringLiteral("F6")), QStringLiteral("F6"));
    QCOMPARE(PortalManager::formatTriggerForXdg(QStringLiteral("F7")), QStringLiteral("F7"));
    QCOMPARE(PortalManager::formatTriggerForXdg(QStringLiteral("Ctrl+F6")), QStringLiteral("CTRL+F6"));
    QCOMPARE(PortalManager::formatTriggerForXdg(QStringLiteral("Meta+F7")), QStringLiteral("LOGO+F7"));
    QCOMPARE(PortalManager::formatTriggerForXdg(QStringLiteral("Ctrl+Alt+S")), QStringLiteral("CTRL+ALT+s"));
}

QTEST_MAIN(tst_DBusSignatures)
#include "tst_DBusSignatures.moc"
