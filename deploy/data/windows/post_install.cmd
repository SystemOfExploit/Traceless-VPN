sc stop AmneziaWGTunnel$Traceless
sc delete AmneziaWGTunnel$Traceless
taskkill /IM "Traceless-service.exe" /F
taskkill /IM "Traceless.exe" /F
exit /b 0
