"""Network detection and helper utilities."""
import socket


def get_laptop_ip(target_gateway: str = "172.20.10.1") -> str:
    """Lay dia chi IP cua Laptop trong mang Hotspot/LAN."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((target_gateway, 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        try:
            return socket.gethostbyname(socket.gethostname())
        except Exception:
            return "172.20.10.2"
