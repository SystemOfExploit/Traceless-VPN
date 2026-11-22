#include "awgInstaller.h"

#include <QRandomGenerator>
#include <QSet>
#include <QStringList>

#include "core/configurators/wireguardConfigurator.h"
#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/selfhosted/sshSession.h"
#include "core/utils/utilities.h"
#include "core/models/protocols/awgProtocolConfig.h"

using namespace amnezia;
using namespace ProtocolUtils;

AwgInstaller::AwgInstaller(QObject *parent)
    : InstallerBase(parent)
{
}

ContainerConfig AwgInstaller::generateConfig(DockerContainer container, int port, TransportProto transportProto)
{
    ContainerConfig config = createBaseConfig(container, port, transportProto);
    if (auto* awgConfig = config.getAwgProtocolConfig()) {
        generateAwgParameters(awgConfig->serverConfig);
        awgConfig->serverConfig.protocolVersion = protocols::awg::awgV3;
        if (container == DockerContainer::Awg2) {
            auto &server = awgConfig->serverConfig;
            const auto makeHeaderRange = [](int slot) {
                constexpr int slotSize = 500000000;
                constexpr int rangeWidth = 1024;
                const int first = slot * slotSize + QRandomGenerator::system()->bounded(5, slotSize - rangeWidth);
                return QString::number(first) + QLatin1Char('-') + QString::number(first + rangeWidth - 1);
            };
            server.initPacketMagicHeader = makeHeaderRange(0);
            server.responsePacketMagicHeader = makeHeaderRange(1);
            server.underloadPacketMagicHeader = makeHeaderRange(2);
            server.transportPacketMagicHeader = makeHeaderRange(3);
            server.junkPacketCount = protocols::awg::tp1JunkPacketCount;
            server.junkPacketMinSize = QString::number(16);
            server.contentPaddingAddition = protocols::awg::tp1ContentPadding;
            server.rekeyTimeout = protocols::awg::tp1RekeyTimeout;
            server.maxHandshakeAttempts = protocols::awg::tp2MaxHandshakeAttempts;
            server.tracelessProfile = protocols::awg::tp2Profile;
            server.specialJunk1 = protocols::awg::tp1HandshakeCamouflage;
            server.randomTrailers.clear();
            server.disableCookies.clear();
        }
    }
    return config;
}

void AwgInstaller::generateAwgParameters(AwgServerConfig &serverConfig)
{
    QString junkPacketCount = QString::number(QRandomGenerator::global()->bounded(4, 7));
    QString junkPacketMinSize = QString::number(10);
    QString junkPacketMaxSize = QString::number(50);

    serverConfig.junkPacketCount = junkPacketCount;
    serverConfig.junkPacketMinSize = junkPacketMinSize;
    serverConfig.junkPacketMaxSize = junkPacketMaxSize;
    serverConfig.initPacketJunkSize = protocols::awg::defaultPadding;
    serverConfig.responsePacketJunkSize = protocols::awg::defaultPadding;
    serverConfig.cookieReplyPacketJunkSize = protocols::awg::defaultPadding;
    serverConfig.transportPacketJunkSize = protocols::awg::defaultPadding;

    serverConfig.initPacketMagicHeader = protocols::awg::defaultInitPacketMagicHeader;
    serverConfig.responsePacketMagicHeader = protocols::awg::defaultResponsePacketMagicHeader;
    serverConfig.underloadPacketMagicHeader = protocols::awg::defaultUnderloadPacketMagicHeader;
    serverConfig.transportPacketMagicHeader = protocols::awg::defaultTransportPacketMagicHeader;

    serverConfig.headerProtectionKey = WireguardConfigurator::generatePresharedKey();
    serverConfig.rekeyAfterTime = protocols::awg::defaultRekeyAfterTime;
    serverConfig.rekeyTimeout = protocols::awg::defaultRekeyTimeout;
    serverConfig.rejectAfterTime = protocols::awg::defaultRejectAfterTime;
    serverConfig.keepaliveTimeout = protocols::awg::defaultKeepaliveTimeout;
    serverConfig.maxHandshakeAttempts = protocols::awg::defaultMaxHandshakeAttempts;
    serverConfig.randomTrailers = protocols::awg::defaultRandomTrailers;
    serverConfig.disableCookies = protocols::awg::defaultDisableCookies;
}

ErrorCode AwgInstaller::extractConfigFromContainer(DockerContainer container, const ServerCredentials &credentials,
                                                   SshSession* sshSession, ContainerConfig &config)
{
    ErrorCode errorCode = ErrorCode::NoError;
    
    // Use appropriate config path based on container type
    QString configPath = protocols::awg::serverConfigPath;
    if (container == DockerContainer::Awg) {
        configPath = protocols::awg::serverLegacyConfigPath;
    }
    
    QString serverConfig = sshSession->getTextFileFromContainer(container, credentials, configPath, errorCode);
    if (errorCode != ErrorCode::NoError) {
        return errorCode;
    }

    const auto serverConfigMap = parseWireGuardConfig(serverConfig);

    if (auto* awgConfig = config.getAwgProtocolConfig()) {
        QString addressValue = serverConfigMap.value("Address");
        QStringList addressParts = addressValue.split("/");
        awgConfig->serverConfig.subnetAddress = addressParts.value(0);
        if (addressParts.size() > 1) {
            awgConfig->serverConfig.subnetCidr = addressParts.value(1);
        }
        awgConfig->serverConfig.junkPacketCount = serverConfigMap.value(configKey::junkPacketCount);
        awgConfig->serverConfig.junkPacketMinSize = serverConfigMap.value(configKey::junkPacketMinSize);
        awgConfig->serverConfig.junkPacketMaxSize = serverConfigMap.value(configKey::junkPacketMaxSize);
        awgConfig->serverConfig.initPacketJunkSize = serverConfigMap.value(configKey::initPacketJunkSize);
        awgConfig->serverConfig.responsePacketJunkSize = serverConfigMap.value(configKey::responsePacketJunkSize);
        awgConfig->serverConfig.initPacketMagicHeader = serverConfigMap.value(configKey::initPacketMagicHeader);
        awgConfig->serverConfig.responsePacketMagicHeader = serverConfigMap.value(configKey::responsePacketMagicHeader);
        awgConfig->serverConfig.underloadPacketMagicHeader = serverConfigMap.value(configKey::underloadPacketMagicHeader);
        awgConfig->serverConfig.transportPacketMagicHeader = serverConfigMap.value(configKey::transportPacketMagicHeader);

        const auto specialJunk = [&serverConfigMap](const QString &key) {
            return serverConfigMap.value(key, serverConfigMap.value(QStringLiteral("# ") + key));
        };
        awgConfig->serverConfig.specialJunk1 = specialJunk(configKey::specialJunk1);
        awgConfig->serverConfig.specialJunk2 = specialJunk(configKey::specialJunk2);
        awgConfig->serverConfig.specialJunk3 = specialJunk(configKey::specialJunk3);
        awgConfig->serverConfig.specialJunk4 = specialJunk(configKey::specialJunk4);
        awgConfig->serverConfig.specialJunk5 = specialJunk(configKey::specialJunk5);

        awgConfig->serverConfig.cookieReplyPacketJunkSize = serverConfigMap.value(configKey::cookieReplyPacketJunkSize);
        awgConfig->serverConfig.transportPacketJunkSize = serverConfigMap.value(configKey::transportPacketJunkSize);

        awgConfig->serverConfig.headerProtectionKey = serverConfigMap.value(configKey::headerProtectionKey);
        awgConfig->serverConfig.contentPaddingAddition = serverConfigMap.value(configKey::contentPaddingAddition);
        awgConfig->serverConfig.rekeyAfterTime = serverConfigMap.value(configKey::rekeyAfterTime);
        awgConfig->serverConfig.rekeyTimeout = serverConfigMap.value(configKey::rekeyTimeout);
        awgConfig->serverConfig.rejectAfterTime = serverConfigMap.value(configKey::rejectAfterTime);
        awgConfig->serverConfig.keepaliveTimeout = serverConfigMap.value(configKey::keepaliveTimeout);
        awgConfig->serverConfig.maxHandshakeAttempts = serverConfigMap.value(configKey::maxHandshakeAttempts);
        awgConfig->serverConfig.randomTrailers = serverConfigMap.value(configKey::randomTrailers);
        awgConfig->serverConfig.disableCookies = serverConfigMap.value(configKey::disableCookies);

        awgConfig->serverConfig.protocolVersion = awgConfig->serverProtocolVersion();
        if (container == DockerContainer::Awg2) {
            ErrorCode profileError = ErrorCode::NoError;
            const auto profile = sshSession->getTextFileFromContainer(container, credentials,
                                                                     QStringLiteral("/opt/amnezia/awg/traceless-profile"), profileError);
            if (profileError == ErrorCode::NoError && profile.trimmed() == protocols::awg::tp2Profile) {
                if (!awgConfig->serverConfig.hasValidTp2Settings()) {
                    return ErrorCode::Tp2ConfigInvalid;
                }
                awgConfig->serverConfig.tracelessProfile = protocols::awg::tp2Profile;
            }
        }
    }

    return ErrorCode::NoError;
}

