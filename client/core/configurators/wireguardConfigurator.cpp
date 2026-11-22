#include "wireguardConfigurator.h"

#include <QDebug>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <array>
#include <memory>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/utils/selfhosted/sshSession.h"
#include "core/utils/selfhosted/scriptsRegistry.h"
#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"
#include "core/utils/utilities.h"
#include "core/models/containerConfig.h"
#include "core/models/protocols/wireGuardProtocolConfig.h"
#include "core/models/protocols/awgProtocolConfig.h"
#include <QJsonArray>

using namespace amnezia;

WireguardConfigurator::WireguardConfigurator(SshSession* sshSession, bool isAwg,
                                             QObject *parent)
    : ConfiguratorBase(sshSession, parent), m_isAwg(isAwg)
{
    m_serverConfigPath =
            m_isAwg ? amnezia::protocols::awg::serverConfigPath : amnezia::protocols::wireguard::serverConfigPath;
    m_serverPublicKeyPath =
            m_isAwg ? amnezia::protocols::awg::serverPublicKeyPath : amnezia::protocols::wireguard::serverPublicKeyPath;
    m_serverPskKeyPath =
            m_isAwg ? amnezia::protocols::awg::serverPskKeyPath : amnezia::protocols::wireguard::serverPskKeyPath;
    m_configTemplate = m_isAwg ? ProtocolScriptType::awg_template : ProtocolScriptType::wireguard_template;

    m_protocolName = m_isAwg ? configKey::awg : configKey::wireguard;
    m_defaultPort = m_isAwg ? protocols::awg::defaultPort : protocols::wireguard::defaultPort;
}

WireguardConfigurator::ConnectionData WireguardConfigurator::genClientKeys()
{
    constexpr int keyLength = 32;
    std::array<unsigned char, keyLength> privateKey{};
    if (RAND_priv_bytes(privateKey.data(), keyLength) != 1) {
        return {};
    }

    const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
            EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, privateKey.data(), privateKey.size()), EVP_PKEY_free);
    std::array<unsigned char, keyLength> publicKey{};
    size_t publicKeyLength = publicKey.size();

    ConnectionData data;
    if (key && EVP_PKEY_get_raw_public_key(key.get(), publicKey.data(), &publicKeyLength) == 1
        && publicKeyLength == publicKey.size()) {
        data.clientPrivKey = QString::fromLatin1(QByteArray::fromRawData(
                reinterpret_cast<const char *>(privateKey.data()), keyLength).toBase64());
        data.clientPubKey = QString::fromLatin1(QByteArray::fromRawData(
                reinterpret_cast<const char *>(publicKey.data()), keyLength).toBase64());
    }
    OPENSSL_cleanse(privateKey.data(), privateKey.size());
    return data;
}

QString WireguardConfigurator::generatePresharedKey()
{
    constexpr int keyLength = 32;
    std::array<unsigned char, keyLength> secret{};
    if (RAND_priv_bytes(secret.data(), keyLength) != 1) {
        return {};
    }
    const QString key = QString::fromLatin1(QByteArray::fromRawData(
            reinterpret_cast<const char *>(secret.data()), keyLength).toBase64());
    OPENSSL_cleanse(secret.data(), secret.size());
    return key;
}

QList<QHostAddress> WireguardConfigurator::getIpsFromConf(const QString &input)
{
    QRegularExpression regex("AllowedIPs = (\\d+\\.\\d+\\.\\d+\\.\\d+)");
    QRegularExpressionMatchIterator matchIterator = regex.globalMatch(input);

    QList<QHostAddress> ips;

    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        const QString address_string { match.captured(1) };
        const QHostAddress address { address_string };
        if (address.isNull()) {
            qWarning() << "Couldn't recognize the ip address: " << address_string;
        } else {
            ips << address;
        }
    }

    return ips;
}

WireguardConfigurator::ConnectionData WireguardConfigurator::prepareWireguardConfig(const ServerCredentials &credentials,
                                                                                    DockerContainer container,
                                                                                    const WireGuardServerConfig* serverConfig,
                                                                                    const AwgServerConfig* awgServerConfig,
                                                                                    const DnsSettings &dnsSettings,
                                                                                    ErrorCode &errorCode)
{
    WireguardConfigurator::ConnectionData connData = WireguardConfigurator::genClientKeys();
    connData.host = credentials.hostName;
    
    QString portStr = m_defaultPort;
    if (serverConfig && !serverConfig->port.isEmpty()) {
        portStr = serverConfig->port;
    } else if (awgServerConfig && !awgServerConfig->port.isEmpty()) {
        portStr = awgServerConfig->port;
    }
    connData.port = portStr;

    if (connData.clientPrivKey.isEmpty() || connData.clientPubKey.isEmpty()) {
        errorCode = ErrorCode::InternalError;
        return connData;
    }

    QString configPath = m_serverConfigPath;
    if (container == DockerContainer::Awg) {
        configPath = amnezia::protocols::awg::serverLegacyConfigPath;
    }
    QString getIpsScript = QString("cat %1 | grep AllowedIPs").arg(configPath);
    QString stdOut;
    auto cbReadStdOut = [&](const QString &data, libssh::Client &) {
        stdOut += data + "\n";
        return ErrorCode::NoError;
    };

    errorCode = m_sshSession->runContainerScript(credentials, container, getIpsScript, cbReadStdOut);
    if (errorCode != ErrorCode::NoError) {
        return connData;
    }
    auto ips = getIpsFromConf(stdOut);

    QHostAddress nextIp = [&] {
        QHostAddress result;
        QHostAddress lastIp;
        QString subnetAddress = protocols::wireguard::defaultSubnetAddress;
        if (serverConfig && !serverConfig->subnetAddress.isEmpty()) {
            subnetAddress = serverConfig->subnetAddress;
        } else if (awgServerConfig && !awgServerConfig->subnetAddress.isEmpty()) {
            subnetAddress = awgServerConfig->subnetAddress;
        }
        if (ips.empty()) {
            lastIp.setAddress(subnetAddress);
        } else {
            lastIp = ips.last();
        }
        quint8 lastOctet = static_cast<quint8>(lastIp.toIPv4Address());
        switch (lastOctet) {
        case 254: result.setAddress(lastIp.toIPv4Address() + 3); break;
        case 255: result.setAddress(lastIp.toIPv4Address() + 2); break;
        default: result.setAddress(lastIp.toIPv4Address() + 1); break;
        }

        return result;
    }();

    connData.clientIP = nextIp.toString();

    // Get keys
    connData.serverPubKey =
            m_sshSession->getTextFileFromContainer(container, credentials, m_serverPublicKeyPath, errorCode);
    connData.serverPubKey.replace("\n", "");
    if (errorCode != ErrorCode::NoError) {
        return connData;
    }

    if (container == DockerContainer::Awg2 && awgServerConfig
        && !awgServerConfig->tracelessProfile.isEmpty()
        && (!awgServerConfig->isTp2() || !awgServerConfig->hasValidTp2Settings())) {
        errorCode = ErrorCode::Tp2ConfigInvalid;
        return connData;
    }

    if (container == DockerContainer::Awg2 && awgServerConfig && awgServerConfig->isTraceless()) {
        connData.pskKey = generatePresharedKey();
        if (connData.pskKey.isEmpty()) {
            errorCode = ErrorCode::InternalError;
            return connData;
        }
    } else {
        connData.pskKey = m_sshSession->getTextFileFromContainer(container, credentials, m_serverPskKeyPath, errorCode).trimmed();
        if (errorCode != ErrorCode::NoError) {
            return connData;
        }
    }

    // Add client to config
    QString configPart = QString("[Peer]\n"
                                 "PublicKey = %1\n"
                                 "PresharedKey = %2\n"
                                 "AllowedIPs = %3/32\n\n")
                                 .arg(connData.clientPubKey, connData.pskKey, connData.clientIP);

    errorCode = m_sshSession->uploadTextFileToContainer(container, credentials, configPart, configPath,
                                                              libssh::ScpOverwriteMode::ScpAppendToExisting);

    if (errorCode != ErrorCode::NoError) {
        return connData;
    }

    bool isAwg = (container == DockerContainer::Awg2);
    QString bin = isAwg ? QStringLiteral("awg") : QStringLiteral("wg");
    QString iface = isAwg ? QStringLiteral("awg0") : QStringLiteral("wg0");
    QString script = QString(
        "sudo docker exec -i $CONTAINER_NAME bash -c '%1 syncconf %2 <(%1-quick strip %3)'").arg(bin, iface, configPath);

    errorCode = m_sshSession->runScript(
            credentials,
            m_sshSession->replaceVars(script, amnezia::genBaseVars(credentials, container, dnsSettings.primaryDns, dnsSettings.secondaryDns)));

    return connData;
}

ProtocolConfig WireguardConfigurator::createConfig(const ServerCredentials &credentials, DockerContainer container,
                                                    const ContainerConfig &containerConfig,
                                                    const DnsSettings &dnsSettings,
                                                    ErrorCode &errorCode)
{
    const WireGuardServerConfig* wireguardServerConfig = nullptr;
    const WireGuardClientConfig* wireguardClientConfig = nullptr;
    const AwgServerConfig* awgServerConfig = nullptr;
    const AwgClientConfig* awgClientConfig = nullptr;
    
    if (auto* wireGuardProtocolConfig = containerConfig.getWireGuardProtocolConfig()) {
        wireguardServerConfig = &wireGuardProtocolConfig->serverConfig;
        if (wireGuardProtocolConfig->clientConfig.has_value()) {
            wireguardClientConfig = &wireGuardProtocolConfig->clientConfig.value();
        }
    } else if (auto* awgProtocolConfig = containerConfig.getAwgProtocolConfig()) {
        awgServerConfig = &awgProtocolConfig->serverConfig;
        if (awgProtocolConfig->clientConfig.has_value()) {
            awgClientConfig = &awgProtocolConfig->clientConfig.value();
        }
    }
    
    amnezia::ScriptVars vars = amnezia::genBaseVars(credentials, container, dnsSettings.primaryDns, dnsSettings.secondaryDns);
    vars.append(amnezia::genProtocolVarsForContainer(container, containerConfig));
    QString scriptData = amnezia::scriptData(m_configTemplate, container);
    QString config = m_sshSession->replaceVars(scriptData, vars);

    // The template lists every possible key, but each parameter is optional -
    // drop the lines whose value came out empty
    static const QRegularExpression emptyValueLine(R"(^\s*\S+\s*=\s*$)");
    auto configTemplateLines = config.split("\n");
    configTemplateLines.removeIf([](const QString &line) { return emptyValueLine.match(line).hasMatch(); });
    config = configTemplateLines.join("\n");

    ConnectionData connData = prepareWireguardConfig(credentials, container, wireguardServerConfig, awgServerConfig, dnsSettings, errorCode);
    if (errorCode != ErrorCode::NoError) {
        return WireGuardProtocolConfig{};
    }

    config.replace("$WIREGUARD_CLIENT_PRIVATE_KEY", connData.clientPrivKey);
    config.replace("$WIREGUARD_CLIENT_IP", connData.clientIP);
    config.replace("$WIREGUARD_SERVER_PUBLIC_KEY", connData.serverPubKey);
    config.replace("$WIREGUARD_PSK", connData.pskKey);

    QString mtu = protocols::wireguard::defaultMtu;
    if (wireguardClientConfig && !wireguardClientConfig->mtu.isEmpty()) {
        mtu = wireguardClientConfig->mtu;
    } else if (awgClientConfig && !awgClientConfig->mtu.isEmpty()) {
        mtu = awgClientConfig->mtu;
    }
    
    WireGuardProtocolConfig protocolConfig;
    if (wireguardServerConfig) {
        protocolConfig.serverConfig = *wireguardServerConfig;
    }
    
    WireGuardClientConfig clientConfig;
    clientConfig.nativeConfig = config;
    clientConfig.hostName = connData.host;
    clientConfig.port = connData.port.toInt();
    if (wireguardServerConfig) {
        clientConfig.setCascade(wireguardServerConfig->cascadeEndpoint, wireguardServerConfig->cascadePort);
    } else if (awgServerConfig) {
        clientConfig.setCascade(awgServerConfig->cascadeEndpoint, awgServerConfig->cascadePort);
    }
    clientConfig.clientIp = connData.clientIP;
    clientConfig.clientPrivateKey = connData.clientPrivKey;
    clientConfig.clientPublicKey = connData.clientPubKey;
    clientConfig.serverPublicKey = connData.serverPubKey;
    clientConfig.presharedKey = connData.pskKey;
    clientConfig.clientId = connData.clientPubKey;
    clientConfig.allowedIps = QStringList { "0.0.0.0/0", "::/0" };
    if (awgServerConfig && awgServerConfig->isTraceless()) {
        clientConfig.persistentKeepAlive = protocols::awg::tp1PersistentKeepAlive;
    } else if (awgServerConfig && awgServerConfig->hasAwg3Params()) {
        clientConfig.persistentKeepAlive = protocols::awg::defaultPersistentKeepAlive;
    } else {
        clientConfig.persistentKeepAlive = protocols::wireguard::defaultPersistentKeepAlive;
    }
    clientConfig.mtu = mtu;
    clientConfig.isObfuscationEnabled = false;
    
    protocolConfig.setClientConfig(clientConfig);
    
    return protocolConfig;
}

ProtocolConfig WireguardConfigurator::processConfigWithLocalSettings(const ConnectionSettings &settings,
                                                                     ProtocolConfig protocolConfig)
{
    return ConfiguratorBase::processConfigWithLocalSettings(settings, protocolConfig);
}

ProtocolConfig WireguardConfigurator::processConfigWithExportSettings(const ExportSettings &settings,
                                                                      ProtocolConfig protocolConfig)
{
    return ConfiguratorBase::processConfigWithExportSettings(settings, protocolConfig);
}
