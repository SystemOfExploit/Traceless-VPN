#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "utils/testCoreController.h"
#include "core/configurators/wireguardConfigurator.h"
#include "core/controllers/connectionController.h"
#include "core/installers/awgInstaller.h"
#include "core/installers/xrayInstaller.h"
#include "core/models/api/apiV2ServerConfig.h"
#include "core/models/protocols/awgProtocolConfig.h"
#include "core/models/selfhosted/nativeServerConfig.h"
#include "core/models/selfhosted/selfHostedAdminServerConfig.h"
#include "core/models/selfhosted/selfHostedUserServerConfig.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/serverConfigUtils.h"
#include "core/utils/selfhosted/scriptsRegistry.h"
#include "ui/models/containersModel.h"
#include "secureQSettings.h"
#include "vpnConnection.h"

using namespace amnezia;

namespace
{
    QJsonObject adminConfigJson(const QString &hostName, int formatVersion = -1)
    {
        QJsonObject json;
        json[configKey::hostName] = hostName;
        json[configKey::userName] = QStringLiteral("root");
        json[configKey::password] = QStringLiteral("secret");
        json[configKey::port] = 22;
        json[configKey::description] = QStringLiteral("Server ") + hostName;
        if (formatVersion >= 0) {
            json[configKey::formatVersion] = formatVersion;
        }
        return json;
    }

    QByteArray serversListBytes(const QList<QJsonObject> &configs)
    {
        QJsonArray array;
        for (const QJsonObject &config : configs) {
            array.append(config);
        }
        return QJsonDocument(array).toJson();
    }
} // namespace

class TestConfigFormatVersion : public QObject
{
    Q_OBJECT

private:
    TestCoreController *m_coreController = nullptr;
    SecureQSettings *m_settings = nullptr;

private slots:
    void initTestCase()
    {
        const QString testOrg = "AmneziaVPN-Test-" + QUuid::createUuid().toString();
        m_settings = new SecureQSettings(testOrg, "amnezia-client", nullptr, false);
        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);
        m_coreController = new TestCoreController(vpnConnection, m_settings, nullptr, this);
    }

    void cleanupTestCase()
    {
        m_settings->clearSettings();
        delete m_coreController;
        delete m_settings;
    }

    void init()
    {
        m_settings->clearSettings();
        m_coreController->m_serversRepository->invalidateCache();
    }

    void testAwgConfigParsingPreservesPaddingAndKeys()
    {
        const auto options = ProtocolUtils::parseWireGuardConfig(QStringLiteral(
                "[Interface]\r\n"
                "Address=10.0.0.2/32\r\n"
                "HeaderProtectionKey = placeholder=\r\n"
                "# I1 = template = value\r\n"
                "I2\t=\ttext = value\r\n"
                "[Peer]\r\n"
                "Endpoint = vpn.example:51820\r\n"));

        QCOMPARE(options.size(), 5);
        QCOMPARE(options.value(QStringLiteral("Address")), QStringLiteral("10.0.0.2/32"));
        QCOMPARE(options.value(configKey::headerProtectionKey), QStringLiteral("placeholder="));
        QCOMPARE(options.value(QStringLiteral("# I1")), QStringLiteral("template = value"));
        QCOMPARE(options.value(configKey::specialJunk2), QStringLiteral("text = value"));
        QCOMPARE(options.value(QStringLiteral("Endpoint")), QStringLiteral("vpn.example:51820"));
    }

    void testTp1UsesCompatibleAwgProfile()
    {
        AwgInstaller installer;
        const ContainerConfig container = installer.generateConfig(DockerContainer::Awg2, 51820, TransportProto::Udp);
        const auto *config = container.getAwgProtocolConfig();
        QVERIFY(config);
        const auto &server = config->serverConfig;
        QVERIFY(server.isTp1());
        QCOMPARE(config->serverProtocolVersion(), QStringLiteral("3.1"));

        const auto restored = AwgServerConfig::fromJson(server.toJson());
        QVERIFY(restored.isTp1());
        QVERIFY(server.hasEqualServerSettings(restored));

        const QStringList headers = { server.initPacketMagicHeader, server.responsePacketMagicHeader,
                                      server.underloadPacketMagicHeader, server.transportPacketMagicHeader };
        int previousEnd = 4;
        for (const QString &header : headers) {
            const auto range = header.split(QLatin1Char('-'));
            QCOMPARE(range.size(), 2);
            const int first = range.at(0).toInt();
            const int last = range.at(1).toInt();
            QVERIFY(first > previousEnd);
            QCOMPARE(last - first, 1023);
            previousEnd = last;
        }

        const ScriptVars vars = genAwgVars(container);
        QVERIFY(!server.headerProtectionKey.isEmpty());
        QVERIFY(vars.contains({ QStringLiteral("$JUNK_PACKET_COUNT"), QStringLiteral("4") }));
        QVERIFY(vars.contains({ QStringLiteral("$REKEY_TIMEOUT"), QStringLiteral("2-3") }));
        QVERIFY(vars.contains({ QStringLiteral("$MAX_HANDSHAKE_ATTEMPTS"), QStringLiteral("30-40") }));
        QVERIFY(vars.contains({ QStringLiteral("$PERSISTENT_KEEPALIVE"), QStringLiteral("18-25") }));
        QVERIFY(vars.contains({ QStringLiteral("$SPECIAL_JUNK_1"), QString(protocols::awg::tp1HandshakeCamouflage) }));
        QVERIFY(vars.contains({ QStringLiteral("$RANDOM_TRAILERS"), QString() }));
        QVERIFY(vars.contains({ QStringLiteral("$TRANSPORT_PACKET_JUNK_SIZE"), QStringLiteral("12") }));
        QVERIFY(vars.contains({ QStringLiteral("$CONTENT_PADDING_ADDITION"), QStringLiteral("1-8") }));
        QVERIFY(vars.contains({ QStringLiteral("$DISABLE_COOKIES"), QString() }));

        const auto legacy = installer.generateConfig(DockerContainer::Awg, 51821, TransportProto::Udp);
        QVERIFY(!legacy.getAwgProtocolConfig()->serverConfig.isTp1());
        ContainersModel model;
        const auto allContainers = ContainerUtils::allContainers();
        QCOMPARE(model.data(allContainers.indexOf(DockerContainer::Awg2), ContainersModel::NameRole).toString(),
                 QStringLiteral("TP1"));
        QCOMPARE(model.data(allContainers.indexOf(DockerContainer::Xray), ContainersModel::NameRole).toString(),
                 QStringLiteral("XRay"));

        AwgClientConfig client;
        client.contentPaddingAddition = server.contentPaddingAddition;
        client.transportPacketJunkSize = server.transportPacketJunkSize;
        client.headerProtectionKey = server.headerProtectionKey;
        ContainerConfig imported;
        imported.container = DockerContainer::Awg2;
        AwgProtocolConfig clientOnly;
        clientOnly.setClientConfig(client);
        imported.protocolConfig = clientOnly;
        model.updateModel({ { DockerContainer::Awg2, imported } });
        QCOMPARE(model.data(allContainers.indexOf(DockerContainer::Awg2), ContainersModel::NameRole).toString(),
                 QStringLiteral("TP1"));
    }

    void testTp1UsesIndependentKeys()
    {
        const auto first = QByteArray::fromBase64(WireguardConfigurator::generatePresharedKey().toLatin1());
        const auto second = QByteArray::fromBase64(WireguardConfigurator::generatePresharedKey().toLatin1());
        const auto peer = WireguardConfigurator::genClientKeys();
        QCOMPARE(first.size(), 32);
        QCOMPARE(second.size(), 32);
        QVERIFY(first != second);
        QCOMPARE(QByteArray::fromBase64(peer.clientPrivKey.toLatin1()).size(), 32);
        QCOMPARE(QByteArray::fromBase64(peer.clientPubKey.toLatin1()).size(), 32);
        QVERIFY(peer.clientPrivKey != peer.clientPubKey);
    }

    void testTp1MtuBudget()
    {
        QCOMPARE(AwgProtocolConfig::tp1TunnelMtu(1500, 0, false), 1420);
        QCOMPARE(AwgProtocolConfig::tp1TunnelMtu(1472, 1376, true), 1372);
        QCOMPARE(AwgProtocolConfig::tp1TunnelMtu(1500, 1330, false), 1330);
        QCOMPARE(AwgProtocolConfig::tp1TunnelMtu(0, 1376, false), 1280);
        QVERIFY(!AwgProtocolConfig::isToggleEnabled(QStringLiteral("false")));
        QVERIFY(!AwgProtocolConfig::isToggleEnabled(QStringLiteral("0")));
        QVERIFY(AwgProtocolConfig::isToggleEnabled(QStringLiteral("1")));
    }

    void testTp1SessionKeepsIssuedSecrets()
    {
        QJsonObject client;
        client[configKey::headerProtectionKey] = QStringLiteral("issued-header-key");
        client[configKey::initPacketMagicHeader] = QStringLiteral("100-1123");
        client[configKey::initPacketJunkSize] = QStringLiteral("12");
        client[configKey::pskKey] = QStringLiteral("issued-psk");
        client[configKey::clientPrivKey] = QStringLiteral("issued-private");
        client[configKey::junkPacketCount] = QStringLiteral("3");
        client[configKey::rekeyTimeout] = QStringLiteral("3-7");
        client[configKey::randomTrailers] = QStringLiteral("off");
        client[configKey::disableCookies] = QString();

        AwgProtocolConfig::tuneTp1Session(client);

        QCOMPARE(client.value(configKey::headerProtectionKey).toString(), QStringLiteral("issued-header-key"));
        QCOMPARE(client.value(configKey::initPacketMagicHeader).toString(), QStringLiteral("100-1123"));
        QCOMPARE(client.value(configKey::initPacketJunkSize).toString(), QStringLiteral("12"));
        QCOMPARE(client.value(configKey::pskKey).toString(), QStringLiteral("issued-psk"));
        QCOMPARE(client.value(configKey::clientPrivKey).toString(), QStringLiteral("issued-private"));
        QCOMPARE(client.value(configKey::junkPacketCount).toString(), QStringLiteral("4"));
        QCOMPARE(client.value(configKey::specialJunk1).toString(), QString(protocols::awg::tp1HandshakeCamouflage));
        QCOMPARE(client.value(configKey::rekeyTimeout).toString(), QStringLiteral("2-3"));
        QCOMPARE(client.value(configKey::maxHandshakeAttempts).toString(), QStringLiteral("30-40"));
        QCOMPARE(client.value(configKey::persistentKeepAlive).toString(), QStringLiteral("18-25"));
        QCOMPARE(client.value(configKey::randomTrailers).toString(), QStringLiteral("off"));

        client[configKey::junkPacketCount] = QStringLiteral("8");
        client[configKey::specialJunk1] = QStringLiteral("<r 12>");
        AwgProtocolConfig::tuneTp1Session(client);
        QCOMPARE(client.value(configKey::junkPacketCount).toString(), QStringLiteral("8"));
        QCOMPARE(client.value(configKey::specialJunk1).toString(), QStringLiteral("<r 12>"));
        QCOMPARE(client.value(configKey::headerProtectionKey).toString(), QStringLiteral("issued-header-key"));
    }

#ifdef AMNEZIA_DESKTOP
    void testTp1FallbackRequiresApprovedTransportAndDisabledKillSwitch()
    {
        AwgInstaller awgInstaller;
        XrayInstaller xrayInstaller;
        ContainerConfig awg = awgInstaller.generateConfig(DockerContainer::Awg2, 51820, TransportProto::Udp);
        ContainerConfig xray = xrayInstaller.generateConfig(DockerContainer::Xray, 443, TransportProto::Tcp);
        auto *xrayConfig = xray.getXrayProtocolConfig();
        QVERIFY(xrayConfig);
        XrayClientConfig client;
        client.nativeConfig = QStringLiteral(R"({"outbounds":[{"protocol":"vless","streamSettings":{"network":"raw","security":"reality"}}]})");
        xrayConfig->setClientConfig(client);

        SelfHostedAdminServerConfig server;
        server.hostName = QStringLiteral("192.0.2.7");
        server.userName = QStringLiteral("admin");
        server.password = QStringLiteral("test-fixture");
        server.port = 22;
        server.defaultContainer = DockerContainer::Awg2;
        server.updateContainerConfig(awg.container, awg);
        server.updateContainerConfig(xray.container, xray);

        auto *servers = m_coreController->m_serversRepository;
        auto *settings = m_coreController->m_appSettingsRepository;
        const auto id = servers->addServer(QString(), server.toJson(), serverConfigUtils::ConfigType::SelfHostedAdmin);
        QVERIFY(!id.isEmpty());
        QCOMPARE(servers->serverKind(id), serverConfigUtils::ConfigType::SelfHostedAdmin);
        VpnConnection connection(servers, settings);
        ConnectionController controller(servers, settings, &connection);
        QJsonObject configuration;
        DockerContainer container = DockerContainer::None;
        const auto fallback = QStringLiteral("tp1TcpFallback");

        settings->setKillSwitchEnabled(false);
        settings->setStrictKillSwitchEnabled(false);
        QCOMPARE(controller.prepareConnection(id, configuration, container), ErrorCode::NoError);
        QCOMPARE(container, DockerContainer::Awg2);
        QCOMPARE(configuration.value(fallback).toObject().value(configKey::vpnProto).toString(), QStringLiteral("xray"));

        settings->setKillSwitchEnabled(true);
        QCOMPARE(controller.prepareConnection(id, configuration, container), ErrorCode::NoError);
        QVERIFY(!configuration.contains(fallback));

        settings->setKillSwitchEnabled(false);
        client.nativeConfig = QStringLiteral(R"({"outbounds":[{"protocol":"vless","streamSettings":{"network":"tcp","security":"none"}}]})");
        xrayConfig->setClientConfig(client);
        server.updateContainerConfig(xray.container, xray);
        servers->editServer(id, server.toJson(), serverConfigUtils::ConfigType::SelfHostedAdmin);
        QCOMPARE(controller.prepareConnection(id, configuration, container), ErrorCode::NoError);
        QVERIFY(!configuration.contains(fallback));
    }
#endif

    void testVersionCheckHelpers()
    {
        QCOMPARE(serverConfigUtils::configFormatVersion(QJsonObject()), 0);
        QVERIFY(serverConfigUtils::isConfigFormatVersionSupported(QJsonObject()));
        QVERIFY(serverConfigUtils::isConfigFormatVersionSupported(
                adminConfigJson("1.1.1.1", serverConfigUtils::currentConfigFormatVersion)));
        QVERIFY(!serverConfigUtils::isConfigFormatVersionSupported(
                adminConfigJson("1.1.1.1", serverConfigUtils::currentConfigFormatVersion + 1)));
    }

    void testToJsonWritesCurrentFormatVersion()
    {
        SelfHostedAdminServerConfig admin;
        admin.hostName = "1.1.1.1";
        QCOMPARE(admin.toJson().value(configKey::formatVersion).toInt(-1), serverConfigUtils::currentConfigFormatVersion);

        SelfHostedUserServerConfig user;
        user.hostName = "1.1.1.1";
        QCOMPARE(user.toJson().value(configKey::formatVersion).toInt(-1), serverConfigUtils::currentConfigFormatVersion);

        NativeServerConfig native;
        native.hostName = "1.1.1.1";
        QCOMPARE(native.toJson().value(configKey::formatVersion).toInt(-1), serverConfigUtils::currentConfigFormatVersion);

        ApiV2ServerConfig api;
        api.hostName = "1.1.1.1";
        QCOMPARE(api.toJson().value(configKey::formatVersion).toInt(-1), serverConfigUtils::currentConfigFormatVersion);
    }

    void testImportAcceptsMissingAndCurrentVersion()
    {
        auto *importController = m_coreController->m_importCoreController;
        QSignalSpy importFinishedSpy(importController, &ImportController::importFinished);

        const QString withoutVersion = QJsonDocument(adminConfigJson("1.1.1.1")).toJson();
        auto result = importController->extractConfigFromData(withoutVersion);
        QCOMPARE(result.errorCode, ErrorCode::NoError);
        importController->importConfig(result.config);

        const QString currentVersion =
                QJsonDocument(adminConfigJson("2.2.2.2", serverConfigUtils::currentConfigFormatVersion)).toJson();
        result = importController->extractConfigFromData(currentVersion);
        QCOMPARE(result.errorCode, ErrorCode::NoError);
        importController->importConfig(result.config);

        QCOMPARE(importFinishedSpy.count(), 2);
        QCOMPARE(m_coreController->m_serversRepository->serversCount(), 2);

        const auto stored = m_coreController->m_serversRepository->selfHostedAdminConfig(
                m_coreController->m_serversRepository->serverIdAt(0));
        QVERIFY(stored.has_value());
        QCOMPARE(stored->toJson().value(configKey::formatVersion).toInt(-1), serverConfigUtils::currentConfigFormatVersion);
    }

    void testImportRejectsNewerVersion()
    {
        auto *importController = m_coreController->m_importCoreController;
        QSignalSpy importFinishedSpy(importController, &ImportController::importFinished);
        QSignalSpy importErrorSpy(importController, &ImportController::importErrorOccurred);

        const QJsonObject newer = adminConfigJson("3.3.3.3", serverConfigUtils::currentConfigFormatVersion + 1);
        const QByteArray newerBytes = QJsonDocument(newer).toJson();

        auto result = importController->extractConfigFromData(QString::fromUtf8(newerBytes));
        QCOMPARE(result.errorCode, ErrorCode::ConfigFormatVersionNotSupportedError);
        QVERIFY(result.config.isEmpty());

        const QString vpnUrl = "vpn://"
                + QString::fromUtf8(qCompress(newerBytes, 8).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
        result = importController->extractConfigFromData(vpnUrl);
        QCOMPARE(result.errorCode, ErrorCode::ConfigFormatVersionNotSupportedError);
        QVERIFY(result.config.isEmpty());

        result = importController->extractConfigFromQr(newerBytes);
        QCOMPARE(result.errorCode, ErrorCode::ConfigFormatVersionNotSupportedError);
        QVERIFY(result.config.isEmpty());

        importController->importConfig(newer);
        QCOMPARE(importErrorSpy.count(), 1);
        QCOMPARE(importErrorSpy.first().at(0).value<ErrorCode>(), ErrorCode::ConfigFormatVersionNotSupportedError);
        QCOMPARE(importFinishedSpy.count(), 0);
        QCOMPARE(m_coreController->m_serversRepository->serversCount(), 0);
    }

    void testRepositorySkipsNewerVersionOnLoad()
    {
        m_settings->setValue("Servers/serversList",
                             serversListBytes({ adminConfigJson("1.1.1.1"),
                                                adminConfigJson("2.2.2.2", serverConfigUtils::currentConfigFormatVersion + 1),
                                                adminConfigJson("3.3.3.3", serverConfigUtils::currentConfigFormatVersion) }));
        m_coreController->m_serversRepository->invalidateCache();

        QCOMPARE(m_coreController->m_serversRepository->serversCount(), 2);
        QCOMPARE(m_coreController->m_serversRepository->unsupportedFormatConfigsCount(), 1);

        const auto rejected = m_coreController->m_serversRepository->addServer(
                QString(), adminConfigJson("4.4.4.4", serverConfigUtils::currentConfigFormatVersion + 1),
                serverConfigUtils::ConfigType::SelfHostedAdmin);
        QVERIFY(!rejected.isEmpty());
        QCOMPARE(m_coreController->m_serversRepository->serversCount(), 2);
    }

    void testRestoreBackupReportsSkippedConfigs()
    {
        QJsonObject backup;
        backup["Servers/serversList"] = QString::fromUtf8(serversListBytes(
                { adminConfigJson("1.1.1.1"),
                  adminConfigJson("2.2.2.2", serverConfigUtils::currentConfigFormatVersion + 1) }));

        QSignalSpy errorSpy(m_coreController->m_settingsUiController, &SettingsUiController::errorOccurred);
        QSignalSpy restoredSpy(m_coreController->m_settingsUiController, &SettingsUiController::restoreBackupFinished);

        m_coreController->m_settingsUiController->restoreAppConfigFromData(QJsonDocument(backup).toJson());

        QCOMPARE(restoredSpy.count(), 1);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.first().at(0).value<ErrorCode>(), ErrorCode::RestoreBackupUnsupportedConfigsSkipped);
        QCOMPARE(m_coreController->m_serversRepository->serversCount(), 1);

        errorSpy.clear();
        restoredSpy.clear();
        QJsonObject cleanBackup;
        cleanBackup["Servers/serversList"] = QString::fromUtf8(serversListBytes({ adminConfigJson("5.5.5.5") }));
        m_coreController->m_settingsUiController->restoreAppConfigFromData(QJsonDocument(cleanBackup).toJson());
        QCOMPARE(restoredSpy.count(), 1);
        QCOMPARE(errorSpy.count(), 0);
    }
};

QTEST_MAIN(TestConfigFormatVersion)
#include "testConfigFormatVersion.moc"
