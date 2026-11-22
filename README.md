# Traceless VPN

Self-hosted VPN client for your own server.

### [English](README.md) | [Русский](README_RU.md)

![Home](metadata/img-readme/home-screen.png)

Traceless installs a VPN on your server and manages the connection through the TP2 profile.

This project is based on [Amnezia VPN](https://github.com/amnezia-vpn/amnezia-client). Amnezia remains the upstream project. Traceless is distributed under the same [GNU GPL v3.0](LICENSE).

## TP2 and AmneziaWG

TP2 is a Traceless profile based on AmneziaWG 3.1. It uses Curve25519 for key agreement and ChaCha20-Poly1305 for authenticated encryption. Traceless sets the profile parameters, issues keys, and manages the connection. [AmneziaWG 3.1 configuration](https://github.com/amnezia-vpn/amneziawg-go/blob/v3.1.20260814/README.md#configuration) · [WireGuard cryptography](https://www.wireguard.com/protocol/)

| Capability | AmneziaWG 3.1 | TP2 in Traceless |
| --- | --- | --- |
| Packet headers | Configurable H1–H4 ranges and optional header protection | Installation generates distinct ranges for each server. The profile requires header protection and checks ranges for overlap. |
| Client keys | Supports a separate PSK for each peer | Traceless generates a separate PSK when it issues each client profile. |
| Packet sizes | Supports configurable padding | Adds up to 8 bytes to data by default. The client calculates the tunnel MTU from the network interface MTU, whether the outer tunnel uses IPv4 or IPv6, and the TP2 overhead. |
| Handshake | Configurable retry interval and attempt count | The standard profile uses a retry interval of 2–3 seconds and a MaxHandshakeAttempts setting of 12–16. Cookies are enabled. |
| Fallback connection | The AmneziaWG tunnel uses UDP | Supported desktop builds of Traceless can fall back to a separate XRay REALITY connection over TCP. |

**TCP fallback** requires an XRay REALITY profile on the same server and both Kill Switch modes to be off. With fallback available, the client gives the initial TP2 handshake up to 8 seconds over UDP before it starts switching.

### Upgrading from TP1

1. Open the server protocol settings in a profile with SSH access and select **Update protocol to TP2**.
2. Compare the SSH fingerprint with your server's host key and confirm the update.

The upgrade applies to the running interface on a standard TP1 server. It preserves the address, port, header parameters, and issued keys, so existing client profiles remain usable. If applying the update fails, Traceless attempts to restore the previous setting.

## Build

Requirements: CMake, a C++ toolchain, Qt 6.10 or newer (Core, Gui, Network, Quick, Svg, and the Compat, RemoteObjects, HttpServer modules), Conan, and Qt Installer Framework for the Windows package.

```bat
deploy\build.bat -i ifw
```

```bash
deploy/build.sh
```

After checkout, load the submodules:

```bash
git submodule update --init --recursive
```

The Windows installer is produced in the build directory as `Traceless_<version>_windows_x64.exe`. The `deploy/build` directory is generated output and is not part of the source tree.

## Credits

- [Amnezia VPN](https://github.com/amnezia-vpn/amnezia-client) — the client this project is based on
- [AmneziaWG](https://docs.amnezia.org/documentation/amnezia-wg/), WireGuard, XRay, OpenVPN, OpenSSL, Qt

## License

[GNU General Public License v3.0](LICENSE).
