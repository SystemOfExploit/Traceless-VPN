#include "awgProtocolConfig.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QSet>

#include <algorithm>

#include "../../../core/utils/protocolEnum.h"
#include "../../../core/protocols/protocolUtils.h"
#include "../../../core/utils/constants/configKeys.h"
#include "../../../core/utils/constants/protocolConstants.h"

using namespace amnezia;
using namespace ProtocolUtils;
namespace amnezia
{

namespace
{
    template <typename T>
    bool hasAwg3Markers(const T &config)
    {
        auto hasValue = [](const QString &value) { return !value.trimmed().isEmpty(); };

        const QStringList awg3Params = { config.headerProtectionKey, config.contentPaddingAddition,
                                         config.rekeyAfterTime,      config.rekeyTimeout,
                                         config.rejectAfterTime,     config.keepaliveTimeout,
                                         config.maxHandshakeAttempts };
        if (std::any_of(awg3Params.begin(), awg3Params.end(), hasValue)) {
            return true;
        }

        return AwgProtocolConfig::isToggleEnabled(config.randomTrailers)
                || AwgProtocolConfig::isToggleEnabled(config.disableCookies);
    }

    template <typename T>
    QString awgVersionOf(const T &config)
    {
        auto hasValue = [](const QString &value) { return !value.trimmed().isEmpty(); };

        if (hasAwg3Markers(config)) {
            return protocols::awg::awgV3;
        }

        const QStringList junkSizes = { config.cookieReplyPacketJunkSize, config.transportPacketJunkSize };
        const QStringList magicHeaders = { config.initPacketMagicHeader, config.responsePacketMagicHeader,
                                           config.underloadPacketMagicHeader, config.transportPacketMagicHeader };
        bool hasJunkSizes = std::any_of(junkSizes.begin(), junkSizes.end(), hasValue);
        bool hasHeaderRanges = std::any_of(magicHeaders.begin(), magicHeaders.end(),
                                           [](const QString &header) { return header.contains('-'); });
        if (hasJunkSizes || hasHeaderRanges) {
            return protocols::awg::awgV2;
        }

        const QStringList specialJunk = { config.specialJunk1, config.specialJunk2, config.specialJunk3,
                                          config.specialJunk4, config.specialJunk5 };
        if (std::any_of(specialJunk.begin(), specialJunk.end(), hasValue)) {
            return protocols::awg::awgV1_5;
        }

        return QString();
    }
} // namespace

QJsonObject AwgServerConfig::toJson() const
{
    QJsonObject obj;
    
    if (!port.isEmpty()) {
        obj[configKey::port] = this->port;
    }
    if (!cascadeEndpoint.isEmpty()) {
        obj[configKey::cascadeEndpoint] = cascadeEndpoint;
    }
    if (!cascadePort.isEmpty()) {
        obj[configKey::cascadePort] = cascadePort;
    }
    if (!transportProto.isEmpty()) {
        obj[configKey::transportProto] = transportProto;
    }
    if (!protocolVersion.isEmpty()) {
        obj[configKey::protocolVersion] = protocolVersion;
    }
    if (!subnetAddress.isEmpty()) {
        obj[configKey::subnetAddress] = subnetAddress;
    }
    if (!subnetCidr.isEmpty()) {
        obj[configKey::subnetCidr] = subnetCidr;
    }
    
    if (!junkPacketCount.isEmpty()) {
        obj[configKey::junkPacketCount] = junkPacketCount;
    }
    if (!junkPacketMinSize.isEmpty()) {
        obj[configKey::junkPacketMinSize] = junkPacketMinSize;
    }
    if (!junkPacketMaxSize.isEmpty()) {
        obj[configKey::junkPacketMaxSize] = junkPacketMaxSize;
    }
    if (!initPacketJunkSize.isEmpty()) {
        obj[configKey::initPacketJunkSize] = initPacketJunkSize;
    }
    if (!responsePacketJunkSize.isEmpty()) {
        obj[configKey::responsePacketJunkSize] = responsePacketJunkSize;
    }
    if (!cookieReplyPacketJunkSize.isEmpty()) {
        obj[configKey::cookieReplyPacketJunkSize] = cookieReplyPacketJunkSize;
    }
    if (!transportPacketJunkSize.isEmpty()) {
        obj[configKey::transportPacketJunkSize] = transportPacketJunkSize;
    }
    
    if (!initPacketMagicHeader.isEmpty()) {
        obj[configKey::initPacketMagicHeader] = initPacketMagicHeader;
    }
    if (!responsePacketMagicHeader.isEmpty()) {
        obj[configKey::responsePacketMagicHeader] = responsePacketMagicHeader;
    }
    if (!underloadPacketMagicHeader.isEmpty()) {
        obj[configKey::underloadPacketMagicHeader] = underloadPacketMagicHeader;
    }
    if (!transportPacketMagicHeader.isEmpty()) {
        obj[configKey::transportPacketMagicHeader] = transportPacketMagicHeader;
    }
    
    obj[configKey::specialJunk1] = specialJunk1;
    obj[configKey::specialJunk2] = specialJunk2;
    obj[configKey::specialJunk3] = specialJunk3;
    obj[configKey::specialJunk4] = specialJunk4;
    obj[configKey::specialJunk5] = specialJunk5;
    
    if (!headerProtectionKey.isEmpty()) {
        obj[configKey::headerProtectionKey] = headerProtectionKey;
    }
    if (!contentPaddingAddition.isEmpty()) {
        obj[configKey::contentPaddingAddition] = contentPaddingAddition;
    }
    if (!rekeyAfterTime.isEmpty()) {
        obj[configKey::rekeyAfterTime] = rekeyAfterTime;
    }
    if (!rekeyTimeout.isEmpty()) {
        obj[configKey::rekeyTimeout] = rekeyTimeout;
    }
    if (!rejectAfterTime.isEmpty()) {
        obj[configKey::rejectAfterTime] = rejectAfterTime;
    }
    if (!keepaliveTimeout.isEmpty()) {
        obj[configKey::keepaliveTimeout] = keepaliveTimeout;
    }
    if (!maxHandshakeAttempts.isEmpty()) {
        obj[configKey::maxHandshakeAttempts] = maxHandshakeAttempts;
    }
    if (!randomTrailers.isEmpty()) {
        obj[configKey::randomTrailers] = randomTrailers;
    }
    if (!disableCookies.isEmpty()) {
        obj[configKey::disableCookies] = disableCookies;
    }

    if (isThirdPartyConfig) {
        obj[configKey::isThirdPartyConfig] = isThirdPartyConfig;
    }
    
    return obj;
}

AwgServerConfig AwgServerConfig::fromJson(const QJsonObject& json)
{
    AwgServerConfig config;
    
    config.port = json.value(configKey::port).toString();
    config.cascadeEndpoint = json.value(configKey::cascadeEndpoint).toString();
    config.cascadePort = json.value(configKey::cascadePort).toString();
    config.transportProto = json.value(configKey::transportProto).toString();
    config.protocolVersion = json.value(configKey::protocolVersion).toString();
    config.subnetAddress = json.value(configKey::subnetAddress).toString();
    config.subnetCidr = json.value(configKey::subnetCidr).toString();
    
    config.junkPacketCount = json.value(configKey::junkPacketCount).toString();
    config.junkPacketMinSize = json.value(configKey::junkPacketMinSize).toString();
    config.junkPacketMaxSize = json.value(configKey::junkPacketMaxSize).toString();
    config.initPacketJunkSize = json.value(configKey::initPacketJunkSize).toString();
    config.responsePacketJunkSize = json.value(configKey::responsePacketJunkSize).toString();
    config.cookieReplyPacketJunkSize = json.value(configKey::cookieReplyPacketJunkSize).toString();
    config.transportPacketJunkSize = json.value(configKey::transportPacketJunkSize).toString();
    
    config.initPacketMagicHeader = json.value(configKey::initPacketMagicHeader).toString();
    config.responsePacketMagicHeader = json.value(configKey::responsePacketMagicHeader).toString();
    config.underloadPacketMagicHeader = json.value(configKey::underloadPacketMagicHeader).toString();
    config.transportPacketMagicHeader = json.value(configKey::transportPacketMagicHeader).toString();
    
    config.specialJunk1 = json.value(configKey::specialJunk1).toString();
    config.specialJunk2 = json.value(configKey::specialJunk2).toString();
    config.specialJunk3 = json.value(configKey::specialJunk3).toString();
    config.specialJunk4 = json.value(configKey::specialJunk4).toString();
    config.specialJunk5 = json.value(configKey::specialJunk5).toString();

    config.headerProtectionKey = json.value(configKey::headerProtectionKey).toString();
    config.contentPaddingAddition = json.value(configKey::contentPaddingAddition).toString();
    config.rekeyAfterTime = json.value(configKey::rekeyAfterTime).toString();
    config.rekeyTimeout = json.value(configKey::rekeyTimeout).toString();
    config.rejectAfterTime = json.value(configKey::rejectAfterTime).toString();
    config.keepaliveTimeout = json.value(configKey::keepaliveTimeout).toString();
    config.maxHandshakeAttempts = json.value(configKey::maxHandshakeAttempts).toString();
    config.randomTrailers = json.value(configKey::randomTrailers).toString();
    config.disableCookies = json.value(configKey::disableCookies).toString();

    config.isThirdPartyConfig = json.value(configKey::isThirdPartyConfig).toBool(false);
    
    return config;
}

QJsonObject AwgClientConfig::toJson() const
{
    QJsonObject obj;
    
    if (!nativeConfig.isEmpty()) {
        obj[configKey::config] = nativeConfig;
    }
    if (!hostName.isEmpty()) {
        obj[configKey::hostName] = hostName;
    }
    if (port > 0) {
        obj[configKey::port] = port;
    }
    if (!cascadeEndpoint.isEmpty()) {
        obj[configKey::cascadeEndpoint] = cascadeEndpoint;
    }
    if (!cascadePort.isEmpty()) {
        obj[configKey::cascadePort] = cascadePort;
    }
    if (!clientIp.isEmpty()) {
        obj[configKey::clientIp] = clientIp;
    }
    if (!clientPrivateKey.isEmpty()) {
        obj[configKey::clientPrivKey] = clientPrivateKey;
    }
    if (!clientPublicKey.isEmpty()) {
        obj[configKey::clientPubKey] = clientPublicKey;
    }
    if (!serverPublicKey.isEmpty()) {
        obj[configKey::serverPubKey] = serverPublicKey;
    }
    if (!presharedKey.isEmpty()) {
        obj[configKey::pskKey] = presharedKey;
    }
    if (!clientId.isEmpty()) {
        obj[configKey::clientId] = clientId;
    }
    
    if (!allowedIps.isEmpty()) {
        QJsonArray arr;
        for (const QString& ip : allowedIps) {
            arr.append(ip);
        }
        obj[configKey::allowedIps] = arr;
    }
    if (!persistentKeepAlive.isEmpty()) {
        obj[configKey::persistentKeepAlive] = persistentKeepAlive;
    }
    if (!mtu.isEmpty()) {
        obj[configKey::mtu] = mtu;
    }
    
    if (!junkPacketCount.isEmpty()) {
        obj[configKey::junkPacketCount] = junkPacketCount;
    }
    if (!junkPacketMinSize.isEmpty()) {
        obj[configKey::junkPacketMinSize] = junkPacketMinSize;
    }
    if (!junkPacketMaxSize.isEmpty()) {
        obj[configKey::junkPacketMaxSize] = junkPacketMaxSize;
    }
    if (!initPacketJunkSize.isEmpty()) {
        obj[configKey::initPacketJunkSize] = initPacketJunkSize;
    }
    if (!responsePacketJunkSize.isEmpty()) {
        obj[configKey::responsePacketJunkSize] = responsePacketJunkSize;
    }
    if (!cookieReplyPacketJunkSize.isEmpty()) {
        obj[configKey::cookieReplyPacketJunkSize] = cookieReplyPacketJunkSize;
    }
    if (!transportPacketJunkSize.isEmpty()) {
        obj[configKey::transportPacketJunkSize] = transportPacketJunkSize;
    }
    
    if (!initPacketMagicHeader.isEmpty()) {
        obj[configKey::initPacketMagicHeader] = initPacketMagicHeader;
    }
    if (!responsePacketMagicHeader.isEmpty()) {
        obj[configKey::responsePacketMagicHeader] = responsePacketMagicHeader;
    }
    if (!underloadPacketMagicHeader.isEmpty()) {
        obj[configKey::underloadPacketMagicHeader] = underloadPacketMagicHeader;
    }
    if (!transportPacketMagicHeader.isEmpty()) {
        obj[configKey::transportPacketMagicHeader] = transportPacketMagicHeader;
    }
    
    if (!specialJunk1.isEmpty()) {
        obj[configKey::specialJunk1] = specialJunk1;
    }
    if (!specialJunk2.isEmpty()) {
        obj[configKey::specialJunk2] = specialJunk2;
    }
    if (!specialJunk3.isEmpty()) {
        obj[configKey::specialJunk3] = specialJunk3;
    }
    if (!specialJunk4.isEmpty()) {
        obj[configKey::specialJunk4] = specialJunk4;
    }
    if (!specialJunk5.isEmpty()) {
        obj[configKey::specialJunk5] = specialJunk5;
    }

    if (!headerProtectionKey.isEmpty()) {
        obj[configKey::headerProtectionKey] = headerProtectionKey;
    }
    if (!contentPaddingAddition.isEmpty()) {
        obj[configKey::contentPaddingAddition] = contentPaddingAddition;
    }
    if (!rekeyAfterTime.isEmpty()) {
        obj[configKey::rekeyAfterTime] = rekeyAfterTime;
    }
    if (!rekeyTimeout.isEmpty()) {
        obj[configKey::rekeyTimeout] = rekeyTimeout;
    }
    if (!rejectAfterTime.isEmpty()) {
        obj[configKey::rejectAfterTime] = rejectAfterTime;
    }
    if (!keepaliveTimeout.isEmpty()) {
        obj[configKey::keepaliveTimeout] = keepaliveTimeout;
    }
    if (!maxHandshakeAttempts.isEmpty()) {
        obj[configKey::maxHandshakeAttempts] = maxHandshakeAttempts;
    }
    if (!randomTrailers.isEmpty()) {
        obj[configKey::randomTrailers] = randomTrailers;
    }
    if (!disableCookies.isEmpty()) {
        obj[configKey::disableCookies] = disableCookies;
    }

    return obj;
}

void AwgClientConfig::setCascade(const QString &newEndpoint, const QString &newPort)
{
    cascadeEndpoint = newEndpoint;
    cascadePort = newPort;
    nativeConfig = replaceWireGuardEndpoint(nativeConfig, newEndpoint.isEmpty() ? hostName : newEndpoint,
                                            newPort.isEmpty() ? port : newPort.toInt());
}

AwgClientConfig AwgClientConfig::fromJson(const QJsonObject& json)
{
    AwgClientConfig config;
    
    config.nativeConfig = json.value(configKey::config).toString();
    config.hostName = json.value(configKey::hostName).toString();
    config.port = json.value(configKey::port).toInt(0);
    config.cascadeEndpoint = json.value(configKey::cascadeEndpoint).toString();
    config.cascadePort = json.value(configKey::cascadePort).toString();
    config.clientIp = json.value(configKey::clientIp).toString();
    config.clientPrivateKey = json.value(configKey::clientPrivKey).toString();
    config.clientPublicKey = json.value(configKey::clientPubKey).toString();
    config.serverPublicKey = json.value(configKey::serverPubKey).toString();
    config.presharedKey = json.value(configKey::pskKey).toString();
    config.clientId = json.value(configKey::clientId).toString();
    
    QJsonArray allowedIpsArr = json.value(configKey::allowedIps).toArray();
    for (const QJsonValue& val : allowedIpsArr) {
        config.allowedIps.append(val.toString());
    }
    config.persistentKeepAlive = json.value(configKey::persistentKeepAlive).toString();
    config.mtu = json.value(configKey::mtu).toString();
    
    config.junkPacketCount = json.value(configKey::junkPacketCount).toString();
    config.junkPacketMinSize = json.value(configKey::junkPacketMinSize).toString();
    config.junkPacketMaxSize = json.value(configKey::junkPacketMaxSize).toString();
    config.initPacketJunkSize = json.value(configKey::initPacketJunkSize).toString();
    config.responsePacketJunkSize = json.value(configKey::responsePacketJunkSize).toString();
    config.cookieReplyPacketJunkSize = json.value(configKey::cookieReplyPacketJunkSize).toString();
    config.transportPacketJunkSize = json.value(configKey::transportPacketJunkSize).toString();
    
    config.initPacketMagicHeader = json.value(configKey::initPacketMagicHeader).toString();
    config.responsePacketMagicHeader = json.value(configKey::responsePacketMagicHeader).toString();
    config.underloadPacketMagicHeader = json.value(configKey::underloadPacketMagicHeader).toString();
    config.transportPacketMagicHeader = json.value(configKey::transportPacketMagicHeader).toString();
    
    config.specialJunk1 = json.value(configKey::specialJunk1).toString();
    config.specialJunk2 = json.value(configKey::specialJunk2).toString();
    config.specialJunk3 = json.value(configKey::specialJunk3).toString();
    config.specialJunk4 = json.value(configKey::specialJunk4).toString();
    config.specialJunk5 = json.value(configKey::specialJunk5).toString();

    config.headerProtectionKey = json.value(configKey::headerProtectionKey).toString();
    config.contentPaddingAddition = json.value(configKey::contentPaddingAddition).toString();
    config.rekeyAfterTime = json.value(configKey::rekeyAfterTime).toString();
    config.rekeyTimeout = json.value(configKey::rekeyTimeout).toString();
    config.rejectAfterTime = json.value(configKey::rejectAfterTime).toString();
    config.keepaliveTimeout = json.value(configKey::keepaliveTimeout).toString();
    config.maxHandshakeAttempts = json.value(configKey::maxHandshakeAttempts).toString();
    config.randomTrailers = json.value(configKey::randomTrailers).toString();
    config.disableCookies = json.value(configKey::disableCookies).toString();
    return config;
}

QJsonObject AwgProtocolConfig::toJson() const
{
    QJsonObject obj = serverConfig.toJson();

    if (clientConfig.has_value()) {
        QJsonObject clientJson = clientConfig->toJson();
        obj[configKey::lastConfig] = QString::fromUtf8(QJsonDocument(clientJson).toJson(QJsonDocument::Compact));
    }

    return obj;
}

AwgProtocolConfig AwgProtocolConfig::fromJson(const QJsonObject& json)
{
    AwgProtocolConfig config;
    
    config.serverConfig = AwgServerConfig::fromJson(json);
    
    QString lastConfigStr = json.value(configKey::lastConfig).toString();
    if (!lastConfigStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(lastConfigStr.toUtf8());
        if (doc.isObject()) {
            config.clientConfig = AwgClientConfig::fromJson(doc.object());
        }
    }
    
    return config;
}

QString AwgProtocolConfig::serverProtocolVersion() const
{
    return awgVersionOf(serverConfig);
}

QString AwgProtocolConfig::clientProtocolVersion() const
{
    return clientConfig.has_value() ? awgVersionOf(clientConfig.value()) : QString();
}

bool AwgProtocolConfig::isToggleEnabled(const QString &value)
{
    const QString toggle = value.trimmed().toLower();
    return toggle == QLatin1String("on") || toggle == QLatin1String("1")
           || toggle == QLatin1String("true") || toggle == QLatin1String("yes") || toggle == QLatin1String("t");
}

int AwgProtocolConfig::tp1TunnelMtu(int linkMtu, int requestedMtu, bool ipv6Endpoint)
{
    constexpr int minIpv6Mtu = 1280;
    constexpr int udpHeader = 8;
    constexpr int awgTransportOverhead = AwgConstant::messageTransportSize + 12 + 8;
    const int ipHeader = ipv6Endpoint ? 40 : 20;
    const int budget = std::max(minIpv6Mtu, linkMtu - ipHeader - udpHeader - awgTransportOverhead);
    return requestedMtu > 0 ? std::min(requestedMtu, budget) : budget;
}

void AwgProtocolConfig::tuneTp1Session(QJsonObject &client)
{
    const auto text = [&client](QLatin1String key) {
        return client.value(key).toString().trimmed();
    };
    const auto leading = [](const QString &value) {
        const int dash = value.indexOf(QLatin1Char('-'));
        bool ok = false;
        const int number = (dash > 0 ? value.left(dash) : value).trimmed().toInt(&ok);
        return ok ? number : -1;
    };

    if (leading(text(configKey::junkPacketCount)) < 4) {
        client[configKey::junkPacketCount] = QLatin1String(protocols::awg::tp1JunkPacketCount);
    }
    if (text(configKey::junkPacketMinSize).isEmpty()) {
        client[configKey::junkPacketMinSize] = QStringLiteral("16");
    }
    if (text(configKey::junkPacketMaxSize).isEmpty()) {
        client[configKey::junkPacketMaxSize] = QStringLiteral("50");
    }

    const bool noSignature = text(configKey::specialJunk1).isEmpty() && text(configKey::specialJunk2).isEmpty()
            && text(configKey::specialJunk3).isEmpty() && text(configKey::specialJunk4).isEmpty()
            && text(configKey::specialJunk5).isEmpty();
    if (noSignature) {
        client[configKey::specialJunk1] = QLatin1String(protocols::awg::tp1HandshakeCamouflage);
    }

    client[configKey::rekeyTimeout] = QLatin1String(protocols::awg::tp1RekeyTimeout);
    client[configKey::maxHandshakeAttempts] = QLatin1String(protocols::awg::tp1MaxHandshakeAttempts);
    client[configKey::persistentKeepAlive] = QLatin1String(protocols::awg::tp1PersistentKeepAlive);
    if (text(configKey::contentPaddingAddition).isEmpty()) {
        client[configKey::contentPaddingAddition] = QLatin1String(protocols::awg::tp1ContentPadding);
    }
}

bool AwgProtocolConfig::isTp1() const
{
    if (serverConfig.isThirdPartyConfig) {
        return false;
    }
    return serverConfig.isTp1() || (clientConfig
            && clientConfig->transportPacketJunkSize == QLatin1String(protocols::awg::defaultPadding)
            && clientConfig->contentPaddingAddition == QLatin1String(protocols::awg::tp1ContentPadding)
            && !clientConfig->headerProtectionKey.isEmpty() && !isToggleEnabled(clientConfig->disableCookies));
}

QString AwgProtocolConfig::protocolVersionString(const QString &version)
{
    if (version == protocols::awg::awgV3) return QObject::tr(" (version 3.1)");
    if (version == protocols::awg::awgV2) return QObject::tr(" (version 2)");
    if (version == protocols::awg::awgV1_5) return QObject::tr(" (version 1.5)");
    return "";
}

bool AwgProtocolConfig::hasClientConfig() const
{
    return clientConfig.has_value();
}

void AwgProtocolConfig::setClientConfig(const AwgClientConfig& config)
{
    clientConfig = config;
}

void AwgProtocolConfig::clearClientConfig()
{
    clientConfig.reset();
}

bool AwgServerConfig::hasEqualServerSettings(const AwgServerConfig& other) const
{
    if (subnetAddress != other.subnetAddress || port != other.port || 
        junkPacketCount != other.junkPacketCount ||
        junkPacketMinSize != other.junkPacketMinSize || junkPacketMaxSize != other.junkPacketMaxSize ||
        initPacketJunkSize != other.initPacketJunkSize || responsePacketJunkSize != other.responsePacketJunkSize ||
        initPacketMagicHeader != other.initPacketMagicHeader ||
        responsePacketMagicHeader != other.responsePacketMagicHeader ||
        underloadPacketMagicHeader != other.underloadPacketMagicHeader ||
        transportPacketMagicHeader != other.transportPacketMagicHeader ||
        specialJunk1 != other.specialJunk1 || specialJunk2 != other.specialJunk2 ||
        specialJunk3 != other.specialJunk3 || specialJunk4 != other.specialJunk4 ||
        specialJunk5 != other.specialJunk5 ||
        cookieReplyPacketJunkSize != other.cookieReplyPacketJunkSize ||
        transportPacketJunkSize != other.transportPacketJunkSize ||
        headerProtectionKey != other.headerProtectionKey ||
        contentPaddingAddition != other.contentPaddingAddition ||
        rekeyAfterTime != other.rekeyAfterTime || rekeyTimeout != other.rekeyTimeout ||
        rejectAfterTime != other.rejectAfterTime || keepaliveTimeout != other.keepaliveTimeout ||
        maxHandshakeAttempts != other.maxHandshakeAttempts ||
        randomTrailers != other.randomTrailers || disableCookies != other.disableCookies) {
        return false;
    }

    return true;
}

bool AwgServerConfig::hasAwg3Params() const
{
    return hasAwg3Markers(*this);
}

bool AwgServerConfig::isTp1() const
{
    namespace awg = protocols::awg;
    return !isThirdPartyConfig
           && transportPacketJunkSize == QLatin1String(awg::defaultPadding)
           && contentPaddingAddition == QLatin1String(awg::tp1ContentPadding)
           && !headerProtectionKey.isEmpty()
           && !AwgProtocolConfig::isToggleEnabled(disableCookies);
}

bool AwgProtocolConfig::isHeadersEqual(const QString &h1, const QString &h2, const QString &h3, const QString &h4)
{
    QSet<QString> uniqueHeaders;
    int filledHeaders = 0;
    for (const QString &header : { h1, h2, h3, h4 }) {
        if (!header.trimmed().isEmpty()) {
            ++filledHeaders;
            uniqueHeaders.insert(header);
        }
    }
    return uniqueHeaders.size() != filledHeaders;
}

bool AwgProtocolConfig::isPacketSizeEqual(int s1, int s2, int s3, int s4)
{
    int initSize = AwgConstant::messageInitiationSize + s1;
    int responseSize = AwgConstant::messageResponseSize + s2;
    int cookieSize = AwgConstant::messageCookieReplySize + s3;
    int transportSize = AwgConstant::messageTransportSize + s4;

    return (initSize == responseSize || initSize == cookieSize || initSize == transportSize || responseSize == cookieSize
            || responseSize == transportSize || cookieSize == transportSize);
}

} // namespace amnezia

