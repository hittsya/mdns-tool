from zeroconf import Zeroconf, ServiceInfo
import socket

info = ServiceInfo(
    "_http._tcp.local.",
    "PythonServices._http._tcp.local.",
    addresses=[socket.inet_aton("192.168.1.50")],
    port=8080,
)

zeroconf = Zeroconf()
zeroconf.register_service(info)


while True:
    pass