"""
renode_controller.py

Renode simulasyonunu Python'dan otomatik olarak baslatip kontrol etmek icin
yardimci modul (Gorev 8). Renode'u headless (ekransiz) modda, monitor'unu
bir TCP portu uzerinden telnet ile erisilebilir sekilde baslatir; UART
ciktisini da ayri bir soket ("server socket terminal") uzerinden okur.

Bagimlilik: sadece Python standart kutuphanesi (socket, subprocess, time, os).
Bu modulun kendisi Renode'a veya donanima BAGIMLIDIR (Gorev 7'deki unit
testlerin aksine) -- cunku amaci zaten gercek Renode simulasyonunu surmek.
"""
import os
import socket
import subprocess
import time

RENODE_BIN = os.environ.get("RENODE_BIN", "renode")
MONITOR_PORT = int(os.environ.get("RENODE_MONITOR_PORT", "1234"))
UART_PORT = int(os.environ.get("RENODE_UART_PORT", "3456"))

# Repo koku (bu dosyanin bulundugu klasore gore, tests/ altindan calistirilsa da
# proje kokunden calistirilsa da dogru yollari bulabilsin diye)
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.dirname(_THIS_DIR)

PLATFORM_REPL = os.environ.get(
    "PLATFORM_REPL", os.path.join(_REPO_ROOT, "renode", "platform.repl")
)
PLATFORM_REPL_NO_SENSOR = os.environ.get(
    "PLATFORM_REPL_NO_SENSOR",
    os.path.join(_REPO_ROOT, "renode", "platform_no_sensor.repl"),
)
SENSOR_MODEL = os.environ.get(
    "SENSOR_MODEL", os.path.join(_REPO_ROOT, "renode-model", "ADS1x1x.cs")
)
DEFAULT_ELF = os.environ.get(
    "STM32_ELF",
    "/mnt/c/Users/Administrator/Desktop/STM32F407VG_ADS1115/Debug/STM32F407VG_ADS1115.elf",
)


class RenodeSession:
    """Tek bir Renode surecini ve monitor/UART soketlerini yonetir."""

    def __init__(self, elf_path=None, platform_repl=None, monitor_port=MONITOR_PORT,
                 uart_port=UART_PORT):
        self.elf_path = elf_path or DEFAULT_ELF
        self.platform_repl = platform_repl or PLATFORM_REPL
        self.monitor_port = monitor_port
        self.uart_port = uart_port
        self.process = None
        self.monitor_sock = None
        self.uart_sock = None
        self.uart_buffer = ""

    # -------------------------------------------------------------- #
    # Yasam dongusu
    # -------------------------------------------------------------- #
    def start(self):
        """Renode'u headless modda baslatir ve monitor soketine baglanir."""
        self.process = subprocess.Popen(
            [RENODE_BIN, "--disable-xwt", "--port", str(self.monitor_port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self._wait_for_port(self.monitor_port, timeout=20)
        self.monitor_sock = socket.create_connection(
            ("127.0.0.1", self.monitor_port), timeout=10
        )
        self._drain_monitor(0.5)  # baslangic banner'ini oku ve at

    def setup_platform(self):
        """Platformu, sensoru ve firmware'i yukler; UART soketini baglar."""
        self.send_monitor('mach create "test"')
        self.send_monitor(f"include @{SENSOR_MODEL}")
        out = self.send_monitor(f"machine LoadPlatformDescription @{self.platform_repl}")
        if "error" in out.lower():
            raise RuntimeError(f"Platform yuklenemedi:\n{out}")

        self.send_monitor(f'emulation CreateServerSocketTerminal {self.uart_port} "extUart"')
        self.send_monitor("connector Connect sysbus.usart1 extUart")

        # UART soketine, veri kaybetmemek icin emulasyon baslamadan ONCE baglaniyoruz
        self._wait_for_port(self.uart_port, timeout=10)
        self.uart_sock = socket.create_connection(("127.0.0.1", self.uart_port), timeout=5)

        out = self.send_monitor(f"sysbus LoadELF @{self.elf_path}")
        if "error" in out.lower():
            raise RuntimeError(f".elf yuklenemedi:\n{out}")

    def stop(self):
        try:
            if self.monitor_sock:
                self.send_monitor("quit", wait_seconds=0.3)
        except OSError:
            pass
        finally:
            if self.uart_sock:
                self.uart_sock.close()
            if self.monitor_sock:
                self.monitor_sock.close()
            if self.process:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()

    # -------------------------------------------------------------- #
    # Test senaryosu icin yuksek seviye yardimcilar
    # -------------------------------------------------------------- #
    def set_simulated_voltage(self, millivolts):
        """Sensor modelindeki SimulatedVoltageMilliVolts property'sini ayarlar."""
        self.send_monitor(f"sysbus.i2c1.ads1x1x SimulatedVoltageMilliVolts {millivolts}")

    def run_for(self, seconds):
        """Emulasyonu belirtilen sure kadar (simule edilen zaman) calistirip durdurur."""
        self.send_monitor(f'emulation RunFor "{seconds}"', wait_seconds=max(2.0, seconds + 1))

    def read_uart(self, wait_seconds=1.0):
        """UART soketinden o ana kadar biriken tum metni okuyup ic buffer'a ekler."""
        self.uart_sock.settimeout(wait_seconds)
        try:
            while True:
                data = self.uart_sock.recv(4096)
                if not data:
                    break
                self.uart_buffer += data.decode(errors="replace")
        except socket.timeout:
            pass
        return self.uart_buffer

    # -------------------------------------------------------------- #
    # Dusuk seviye monitor iletisimi
    # -------------------------------------------------------------- #
    def send_monitor(self, command, wait_seconds=0.5):
        self.monitor_sock.sendall((command + "\n").encode())
        return self._drain_monitor(wait_seconds)

    def _drain_monitor(self, wait_seconds=0.3):
        self.monitor_sock.settimeout(wait_seconds)
        chunks = []
        try:
            while True:
                data = self.monitor_sock.recv(4096)
                if not data:
                    break
                chunks.append(data.decode(errors="replace"))
        except socket.timeout:
            pass
        return "".join(chunks)

    def _wait_for_port(self, port, timeout=15):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=1):
                    return
            except OSError:
                time.sleep(0.3)
        raise TimeoutError(f"Port {port} zamaninda acilmadi (Renode baslamamis olabilir)")
