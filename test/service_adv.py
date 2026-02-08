from zeroconf import Zeroconf, ServiceInfo
import socket
import time
import random

ip = "192.168.2.6"
addr = socket.inet_aton(ip)

services = [
    ServiceInfo("_http._tcp.local.", "WebServer-1._http._tcp.local.", addresses=[addr], port=8080),
    ServiceInfo("_http._tcp.local.", "WebServer-2._http._tcp.local.", addresses=[addr], port=8081),
    ServiceInfo("_https._tcp.local.", "SecureWeb._https._tcp.local.", addresses=[addr], port=8443),
    ServiceInfo("_printer._tcp.local.", "OfficePrinter._printer._tcp.local.", addresses=[addr], port=9100),
    ServiceInfo("_ssh._tcp.local.", "DevMachine._ssh._tcp.local.", addresses=[addr], port=22),
    ServiceInfo("_ftp._tcp.local.", "FileServer._ftp._tcp.local.", addresses=[addr], port=21),
    ServiceInfo("_myapp._tcp.local.", "TestNode-A._myapp._tcp.local.", addresses=[addr], port=5000),
    ServiceInfo("_myapp._tcp.local.", "TestNode-B._myapp._tcp.local.", addresses=[addr], port=5001),
    ServiceInfo("_googlecast._tcp.local.", "LivingRoomTV._googlecast._tcp.local.", addresses=[addr], port=8009),
    ServiceInfo("_airplay._tcp.local.", "MacDisplay._airplay._tcp.local.", addresses=[addr], port=7000),
]

zeroconf = Zeroconf()
online = {svc.name: True for svc in services}

print("Registering initial services...")
for svc in services:
    zeroconf.register_service(svc)
    print("  +", svc.name)

try:
    while True:
        time.sleep(random.uniform(2, 5))

        svc = random.choice(services)

        if online[svc.name]:
            print("  -", svc.name, "(offline)")
            zeroconf.unregister_service(svc)
            online[svc.name] = False
        else:
            print("  +", svc.name, "(online)")
            zeroconf.register_service(svc)
            online[svc.name] = True

except KeyboardInterrupt:
    print("Shutting down...")

    for svc in services:
        if online[svc.name]:
            zeroconf.unregister_service(svc)

    zeroconf.close()
