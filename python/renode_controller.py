"""
renode_controller.py

Helper module for driving Renode headlessly and automatically from Python
(Görev 8). Starts Renode without a GUI (--disable-xwt), with its monitor
reachable over a TCP port for telnet-style commands, and reads UART
output from a separate server socket terminal.

Dependency: Python standard library only (socket, subprocess, time, os).
This module IS intentionally dependent on Renode/hardware (unlike the
Görev 7 unit tests) -- its whole purpose is to drive a real simulation.
"""
import os
import socket
import subprocess
import time

RENODE_BIN = os.environ.get("RENODE_BIN", "renode")
MONITOR_PORT = int(os.environ.get("RENODE_MONITOR_PORT", "1234"))
UART_PORT = int(os.environ.get("RENODE_UART_PORT", "3456"))

# Repo root (relative to this file's location, so it works whether this
# script is run from python/ or from the repo root)
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

# Default .elf location: the CMake build output at the repo root. No
# personal/absolute machine path is hardcoded here -- override via the
# STM32_ELF environment variable if your .elf lives elsewhere.
DEFAULT_ELF = os.environ.get(
    "STM32_ELF",
    os.path.join(_REPO_ROOT, "build", "STM32F407VG_ADS1x1x.elf"),
)


class RenodeSession:
    """Manages a single Renode process and its monitor/UART sockets."""

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
    # Lifecycle
    # -------------------------------------------------------------- #
    def start(self):
        """Starts Renode headlessly and connects to its monitor socket."""
        self.process = subprocess.Popen(
            [RENODE_BIN, "--disable-xwt", "--port", str(self.monitor_port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self._wait_for_port(self.monitor_port, timeout=20)
        self.monitor_sock = socket.create_connection(
            ("127.0.0.1", self.monitor_port), timeout=10
        )
        self._drain_monitor(0.5)  # read and discard the startup banner

    def setup_platform(self):
        """Loads the platform, the sensor, and the firmware; connects the UART socket."""
        self.send_monitor('mach create "test"')
        self.send_monitor(f"include @{SENSOR_MODEL}")
        out = self.send_monitor(f"machine LoadPlatformDescription @{self.platform_repl}")
        if "error" in out.lower():
            raise RuntimeError(f"Failed to load platform:\n{out}")

        self.send_monitor(f'emulation CreateServerSocketTerminal {self.uart_port} "extUart"')
        self.send_monitor("connector Connect sysbus.usart1 extUart")

        # Connect the UART socket BEFORE starting the emulation, so no
        # early bytes are lost.
        self._wait_for_port(self.uart_port, timeout=10)
        self.uart_sock = socket.create_connection(("127.0.0.1", self.uart_port), timeout=5)

        out = self.send_monitor(f"sysbus LoadELF @{self.elf_path}")
        if "error" in out.lower():
            raise RuntimeError(f"Failed to load .elf:\n{out}")

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
    # High-level helpers for test scenarios
    # -------------------------------------------------------------- #
    def set_simulated_voltage(self, millivolts):
        """Sets the sensor model's SimulatedVoltageMilliVolts property."""
        self.send_monitor(f"sysbus.i2c1.ads1x1x SimulatedVoltageMilliVolts {millivolts}")

    def run_for(self, seconds):
        """Runs the emulation for the given amount of simulated time, then pauses."""
        self.send_monitor(f'emulation RunFor "{seconds}"', wait_seconds=max(2.0, seconds + 1))

    def read_uart(self, wait_seconds=1.0):
        """Reads everything accumulated so far on the UART socket into the buffer."""
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
    # Low-level monitor communication
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
        raise TimeoutError(f"Port {port} did not open in time (Renode may not have started)")
