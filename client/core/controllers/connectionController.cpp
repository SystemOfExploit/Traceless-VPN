#include "connectionController.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>

#include "core/configurators/configuratorBase.h"
#include "core/utils/protocolEnum.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/payloadSender.h"
#include "core/utils/utilities.h"
#include "core/utils/serverConfigUtils.h"
#include "version.h"
#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/models/containerConfig.h"
#include "core/models/protocolConfig.h"
#include "core/models/protocols/awgProtocolConfig.h"
#include "core/models/protocols/xrayProtocolConfig.h"
#include "core/utils/networkUtilities.h"

using namespace amnezia;
using namespace ProtocolUtils;

ConnectionController::ConnectionController(SecureServersRepository* serversRepository,
                                         SecureAppSettingsRepository* appSettingsRepository,
                                         VpnConnection* vpnConnection,
                                         QObject* parent)
    : QObject(parent),
      m_serversRepository(serversRepository),
      m_appSettingsRepository(appSettingsRepository),
      m_vpnConnection(vpnConnection)
{
    connect(m_vpnConnection, &VpnConnection::connectionStateChanged, this, &ConnectionController::connectionStateChanged);
    connect(this, &ConnectionController::openConnectionRequested, m_vpnConnection, &VpnConnection::connectToVpn, Qt::QueuedConnection);
    connect(this, &ConnectionController::closeConnectionRequested, m_vpnConnection, &VpnConnection::disconnectFromVpn, Qt::QueuedConnection);
    connect(this, &ConnectionController::killSwitchModeChangedRequested, m_vpnConnection, &VpnConnection::onKillSwitchModeChanged, Qt::QueuedConnection);
#ifdef Q_OS_ANDROID
    connect(this, &ConnectionController::restoreConnectionRequested, m_vpnConnection, &VpnConnection::restoreConnection, Qt::QueuedConnection);
#endif
}

bool ConnectionController::isConnected() const
{
    return m_vpnConnection && m_vpnConnection->connectionState() == Vpn::ConnectionState::Connected;
}

void ConnectionController::setConnectionState(Vpn::ConnectionState state)
{
    emit connectionStateChanged(state);
}

ErrorCode ConnectionController::defaultContainerForServer(const QString &serverId, DockerContainer &container) const
{
    const auto kind = m_serversRepository->serverKind(serverId);
    switch (kind) {
    case serverConfigUtils::ConfigType::SelfHostedAdmin: {
        const auto cfg = m_serversRepository->selfHostedAdminConfig(serverId);
        if (!cfg.has_value()) {
            return ErrorCode::InternalError;
        }
        container = cfg->defaultContainer;
        return ErrorCode::NoError;
    }
    case serverConfigUtils::ConfigType::SelfHostedUser: {
        const auto cfg = m_serversRepository->selfHostedUserConfig(serverId);
        if (!cfg.has_value()) {
            return ErrorCode::InternalError;
        }
        container = cfg->defaultContainer;
        return ErrorCode::NoError;
    }
    case serverConfigUtils::ConfigType::Native: {
        const auto cfg = m_serversRepository->nativeConfig(serverId);
        if (!cfg.has_value()) {
            return ErrorCode::InternalError;
        }
        container = cfg->defaultContainer;
        return ErrorCode::NoError;
    }
    case serverConfigUtils::ConfigType::AmneziaPremiumV2:
    case serverConfigUtils::ConfigType::AmneziaFreeV3:
    case serverConfigUtils::ConfigType::ExternalPremium: {
        const auto cfg = m_serversRepository->apiV2Config(serverId);
        if (!cfg.has_value()) {
            return ErrorCode::InternalError;
        }
        container = cfg->defaultContainer;
        return ErrorCode::NoError;
    }
    case serverConfigUtils::ConfigType::AmneziaPremiumV1:
    case serverConfigUtils::ConfigType::AmneziaFreeV2:
        return ErrorCode::LegacyApiV1NotSupportedError;
    case serverConfigUtils::ConfigType::Invalid:
    default:
        return ErrorCode::InternalError;
    }
}

ErrorCode ConnectionController::isConnectionSupported(const QString &serverId) const
{
    if (serverId.isEmpty()) {
        return ErrorCode::InternalError;
    }

    if (!isServiceReady()) {
        return ErrorCode::AmneziaServiceNotRunning;
    }

    const serverConfigUtils::ConfigType kind = m_serversRepository->serverKind(serverId);
    if (serverConfigUtils::isLegacyApiSubscription(kind)) {
        return ErrorCode::LegacyApiV1NotSupportedError;
    }

    DockerContainer container = DockerContainer::None;
    const ErrorCode errorCode = defaultContainerForServer(serverId, container);
    if (errorCode != ErrorCode::NoError) {
        return errorCode;
    }

    if (container == DockerContainer::None) {
        if (serverConfigUtils::isApiV2Subscription(kind)) {
            return ErrorCode::NoError;
        }
        return ErrorCode::NoInstalledContainersError;
    }

    if (ContainerUtils::isUnsupportedContainer(container)) {
        return ErrorCode::LegacyContainerNotSupportedError;
    }

    if (!isContainerSupported(container)) {
        return ErrorCode::NotSupportedOnThisPlatform;
    }

    return ErrorCode::NoError;
}

ErrorCode ConnectionController::prepareConnection(const QString &serverId,
                                                 QJsonObject& vpnConfiguration,
                                                 DockerContainer& container)
{
    ContainerConfig containerConfigModel;
    QPair<QString, QString> dns;
    QString hostName;
    QString description;
    int configVersion = 0;
    bool isApiConfig = false;

    const auto kind = m_serversRepository->serverKind(serverId);
    const QString primaryDns = m_appSettingsRepository->primaryDns();
    const QString secondaryDns = m_appSettingsRepository->secondaryDns();
    switch (kind) {
    case serverConfigUtils::ConfigType::SelfHostedAdmin: {
        const auto cfg = m_serversRepository->selfHostedAdminConfig(serverId);
        if (!cfg.has_value()) return ErrorCode::InternalError;
        container = cfg->defaultContainer;
        containerConfigModel = cfg->containerConfig(container);
        dns = cfg->getDnsPair(m_appSettingsRepository->useAmneziaDns(), primaryDns, secondaryDns);
        hostName = cfg->hostName;
        description = cfg->description;
        break;
    }
    case serverConfigUtils::ConfigType::SelfHostedUser: {
        const auto cfg = m_serversRepository->selfHostedUserConfig(serverId);
        if (!cfg.has_value()) return ErrorCode::InternalError;
        container = cfg->defaultContainer;
        containerConfigModel = cfg->containerConfig(container);
        dns = cfg->getDnsPair(primaryDns, secondaryDns);
        hostName = cfg->hostName;
        description = cfg->description;
        break;
    }
    case serverConfigUtils::ConfigType::Native: {
        const auto cfg = m_serversRepository->nativeConfig(serverId);
        if (!cfg.has_value()) return ErrorCode::InternalError;
        container = cfg->defaultContainer;
        containerConfigModel = cfg->containerConfig(container);
        dns = cfg->getDnsPair(primaryDns, secondaryDns);
        hostName = cfg->hostName;
        description = cfg->description;
        break;
    }
    case serverConfigUtils::ConfigType::AmneziaPremiumV2:
    case serverConfigUtils::ConfigType::AmneziaFreeV3:
    case serverConfigUtils::ConfigType::ExternalPremium: {
        const auto cfg = m_serversRepository->apiV2Config(serverId);
        if (!cfg.has_value()) return ErrorCode::InternalError;
        container = cfg->defaultContainer;
        containerConfigModel = cfg->containerConfig(container);
        dns = cfg->getDnsPair(primaryDns, secondaryDns);
        hostName = cfg->hostName;
        description = cfg->description;
        configVersion = serverConfigUtils::ConfigSource::AmneziaGateway;
        isApiConfig = true;
        break;
    }
    case serverConfigUtils::ConfigType::AmneziaPremiumV1:
    case serverConfigUtils::ConfigType::AmneziaFreeV2:
        return ErrorCode::InternalError;
    case serverConfigUtils::ConfigType::Invalid:
    default:
        return ErrorCode::InternalError;
    }

    const auto *preparedAwg = containerConfigModel.getAwgProtocolConfig();
    if (preparedAwg && !preparedAwg->serverConfig.tracelessProfile.isEmpty()
        && (!preparedAwg->serverConfig.isTp2() || !preparedAwg->hasValidTp2ClientSettings())) {
        return ErrorCode::Tp2ConfigInvalid;
    }

    vpnConfiguration = createConnectionConfiguration(dns, isApiConfig, hostName, description, configVersion,
                                                     containerConfigModel, container);

#ifdef AMNEZIA_DESKTOP
    const auto *awg = containerConfigModel.getAwgProtocolConfig();
    if (container == DockerContainer::Awg2 && awg && awg->isTraceless()
        && !m_appSettingsRepository->isKillSwitchEnabled() && !m_appSettingsRepository->isStrictKillSwitchEnabled()
        && isContainerSupported(DockerContainer::Xray)) {
        ContainerConfig fallback;
        if (const auto admin = m_serversRepository->selfHostedAdminConfig(serverId); admin) {
            fallback = admin->containerConfig(DockerContainer::Xray);
        } else if (const auto user = m_serversRepository->selfHostedUserConfig(serverId); user) {
            fallback = user->containerConfig(DockerContainer::Xray);
        }
        const auto *xray = fallback.getXrayProtocolConfig();
        if (xray && xray->hasClientConfig()) {
            namespace px = protocols::xray;
            const auto settings = QJsonDocument::fromJson(xray->clientConfig->nativeConfig.toUtf8()).object();
            const auto outbounds = settings.value(px::outbounds).toArray();
            const auto outbound = outbounds.isEmpty() ? QJsonObject() : outbounds.first().toObject();
            const auto stream = outbound.value(px::streamSettings).toObject();
            const auto network = stream.value(px::network).toString();
            if (outbound.value(px::protocol).toString() == QLatin1String(px::protocolVless)
                && (network == QLatin1String(px::networkTcp) || network == QLatin1String(px::transportRaw))
                && stream.value(px::security).toString() == QLatin1String(px::securityReality)) {
                vpnConfiguration.insert(QStringLiteral("tp1TcpFallback"),
                        createConnectionConfiguration(dns, false, hostName, description, configVersion, fallback, DockerContainer::Xray));
            }
        }
    }
#endif

    if (preparedAwg && preparedAwg->serverConfig.isTp2()
        && vpnConfiguration.value(ProtocolUtils::key_proto_config_data(Proto::Awg)).toObject()
                   .value(configKey::mtu).toString() == QLatin1String("0")) {
        return ErrorCode::Tp2ConfigInvalid;
    }

    return ErrorCode::NoError;
}

ErrorCode ConnectionController::openConnection(const QString &serverId)
{
    QJsonObject vpnConfiguration;
    DockerContainer container;

    ErrorCode errorCode = prepareConnection(serverId, vpnConfiguration, container);
    if (errorCode != ErrorCode::NoError) {
        return errorCode;
    }

    const auto apiV2 = m_serversRepository->apiV2Config(serverId);
    if (apiV2.has_value() && !apiV2->sendPayload.isEmpty()) {
        PayloadSender::sendAll(apiV2->sendPayload);
    }

    emit openConnectionRequested(serverId, container, vpnConfiguration);
    return ErrorCode::NoError;
}

void ConnectionController::closeConnection()
{
    if (m_vpnConnection) {
        emit closeConnectionRequested();
    }
}

#ifdef Q_OS_ANDROID
void ConnectionController::restoreConnection()
{
    if (m_vpnConnection) {
        emit restoreConnectionRequested();
    }
}
#endif

void ConnectionController::onKillSwitchModeChanged(bool enabled)
{
    if (m_vpnConnection) {
        emit killSwitchModeChangedRequested(enabled);
    }
}

ErrorCode ConnectionController::lastConnectionError() const
{
    return m_vpnConnection->lastError();
}

QJsonObject ConnectionController::createConnectionConfiguration(const QPair<QString, QString> &dns,
                                                              bool isApiConfig,
                                                              const QString &hostName,
                                                              const QString &description,
                                                              int configVersion,
                                                              const ContainerConfig &containerConfig,
                                                              DockerContainer container)
{
    QJsonObject vpnConfiguration {};

    if (ContainerUtils::containerService(container) == ServiceType::Other) {
        return vpnConfiguration;
    }

    Proto proto = ContainerUtils::defaultProtocol(container);

    ConnectionSettings connectionSettings = {
        { dns.first, dns.second },
        isApiConfig,
        {
            m_appSettingsRepository->isSitesSplitTunnelingEnabled(),
            m_appSettingsRepository->routeMode()
        }
    };

    auto configurator = ConfiguratorBase::create(proto, nullptr);
    ProtocolConfig processedConfig = configurator->processConfigWithLocalSettings(connectionSettings,
                                                                                  containerConfig.protocolConfig);

    QJsonObject vpnConfigData = processedConfig.getClientConfigJson();
    if (ContainerUtils::isAwgContainer(container) || container == DockerContainer::WireGuard) {
        if (vpnConfigData[configKey::mtu].toString().isEmpty()) {
            vpnConfigData[configKey::mtu] =
                    ContainerUtils::isAwgContainer(container) ? protocols::awg::defaultMtu :
                    protocols::wireguard::defaultMtu;
        }

        const QString cascadeEndpoint = vpnConfigData.value(configKey::cascadeEndpoint).toString();
        if (!cascadeEndpoint.isEmpty()) {
            vpnConfigData[configKey::hostName] = cascadeEndpoint;
        }
        const QString cascadePort = vpnConfigData.value(configKey::cascadePort).toString();
        if (!cascadePort.isEmpty()) {
            vpnConfigData[configKey::port] = cascadePort.toInt();
        }

        const auto *awg = containerConfig.getAwgProtocolConfig();
        if (container == DockerContainer::Awg2 && awg && awg->isTraceless()) {
            const auto configuredHost = vpnConfigData.value(configKey::hostName).toString();
            const QHostAddress endpoint(awg->serverConfig.isTp2()
                                                ? NetworkUtilities::getIPAddress(configuredHost)
                                                : configuredHost);
            int interfaceMtu = 0;
#ifdef Q_OS_WIN
            if (endpoint.protocol() == QAbstractSocket::IPv4Protocol) {
                const int index = NetworkUtilities::AdapterIndexTo(endpoint);
                if (index > 0) {
                    interfaceMtu = QNetworkInterface::interfaceFromIndex(index).maximumTransmissionUnit();
                }
            }
#endif
            if (interfaceMtu <= 0) {
                interfaceMtu = NetworkUtilities::getGatewayAndIface().second.maximumTransmissionUnit();
            }
            const auto family = endpoint.protocol();
            const int requestedMtu = vpnConfigData.value(configKey::mtu).toString().toInt();
            const bool ipv6Endpoint = family != QAbstractSocket::IPv4Protocol;
            int mtu;
            if (awg->serverConfig.isTp2()) {
                mtu = AwgProtocolConfig::tp2TunnelMtu(interfaceMtu, requestedMtu, ipv6Endpoint,
                                                      vpnConfigData.value(configKey::transportPacketJunkSize).toString(),
                                                      vpnConfigData.value(configKey::contentPaddingAddition).toString());
                AwgProtocolConfig::tuneTp2Session(vpnConfigData);
            } else {
                mtu = AwgProtocolConfig::tp1TunnelMtu(interfaceMtu, requestedMtu, ipv6Endpoint);
                AwgProtocolConfig::tuneTp1Session(vpnConfigData);
            }
            vpnConfigData[configKey::mtu] = QString::number(mtu);
        }
    }

    vpnConfiguration.insert(ProtocolUtils::key_proto_config_data(proto), vpnConfigData);
    vpnConfiguration[configKey::vpnProto] = ProtocolUtils::protoToString(proto);

    vpnConfiguration[configKey::dns1] = dns.first;
    vpnConfiguration[configKey::dns2] = dns.second;

    vpnConfiguration[configKey::hostName] = hostName;
    vpnConfiguration[configKey::description] = description;
    vpnConfiguration[configKey::configVersion] = configVersion;

    return vpnConfiguration;
}

bool ConnectionController::isServiceReady() const
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
    return Utils::processIsRunning(Utils::executable(SERVICE_NAME, false), true);
#else
    return true;
#endif
}

bool ConnectionController::isContainerSupported(DockerContainer container) const
{
    return ContainerUtils::isSupportedByCurrentPlatform(container);
}
