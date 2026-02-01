from zeroconf import Zeroconf, ServiceInfo
import socket
import time

ip = "192.168.1.50"
addr = socket.inet_aton(ip)

services = [
    # Web servers
    ServiceInfo("_http._tcp.local.", "WebServer-1._http._tcp.local.", addresses=[addr], port=8080),
    ServiceInfo("_http._tcp.local.", "WebServer-2._http._tcp.local.", addresses=[addr], port=8081),

    # HTTPS
    ServiceInfo("_https._tcp.local.", "SecureWeb._https._tcp.local.", addresses=[addr], port=8443),

    # Printer
    ServiceInfo("_printer._tcp.local.", "OfficePrinter._printer._tcp.local.", addresses=[addr], port=9100),

    # SSH
    ServiceInfo("_ssh._tcp.local.", "DevMachine._ssh._tcp.local.", addresses=[addr], port=22),

    # FTP
    ServiceInfo("_ftp._tcp.local.", "FileServer._ftp._tcp.local.", addresses=[addr], port=21),

    # Custom app
    ServiceInfo("_myapp._tcp.local.", "TestNode-A._myapp._tcp.local.", addresses=[addr], port=5000),
    ServiceInfo("_myapp._tcp.local.", "TestNode-B._myapp._tcp.local.", addresses=[addr], port=5001),

    # Chromecast-style
    ServiceInfo("_googlecast._tcp.local.", "LivingRoomTV._googlecast._tcp.local.", addresses=[addr], port=8009),

    # AirPlay
    ServiceInfo("_airplay._tcp.local.", "MacDisplay._airplay._tcp.local.", addresses=[addr], port=7000),
]

zeroconf = Zeroconf()

print("Registering test services...")
for svc in services:
    zeroconf.register_service(svc)
    print("  +", svc.name)

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Shutting down...")
    
    for svc in services:
        zeroconf.unregister_service(svc)

    zeroconf.close()
