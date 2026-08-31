"""
test_integration.py

Gorev 8 - Renode uzerinde CALISAN gercek firmware'i uctan uca test eden
Python integration testleri. Gorev 7'deki unit testlerin aksine, bu
testler kasitli olarak Renode'a bagimlidir -- amac zaten gercek
simulasyonu otomatik surup davranisi dogrulamak.

Calistirma:
    pytest -v test_integration.py

Test basarisiz olursa pytest zaten non-zero exit code ile cikar (ekstra
bir sys.exit gerekmez) -- bu da gorev metninin istedigi "test basarisiz
oldugunda non-zero exit code" kriterini otomatik karsilar.
"""
import re
import time

import pytest

from renode_controller import RenodeSession, PLATFORM_REPL, PLATFORM_REPL_NO_SENSOR


VOLTAGE_LINE_RE = re.compile(r"Voltage:\s*(-?\d+\.\d+)\s*mV")


@pytest.fixture
def renode_session():
    """Her test icin temiz, izole bir Renode oturumu acar; test bitince kapatir."""
    session = RenodeSession(platform_repl=PLATFORM_REPL)
    session.start()
    session.setup_platform()
    yield session
    session.stop()


@pytest.fixture
def renode_session_no_sensor():
    """Sensorun BAGLI OLMADIGI bir platformla oturum acar (iletisim hatasi senaryosu icin)."""
    session = RenodeSession(platform_repl=PLATFORM_REPL_NO_SENSOR)
    session.start()
    session.setup_platform()
    yield session
    session.stop()


def _wait_for_text(session, expected_substring, total_timeout=6.0, poll=1.0):
    """UART ciktisinda beklenen metin gorunene kadar, belirli araliklarla
    simulasyonu ilerletip UART'i okur. Bulunca True, bulunamazsa False doner."""
    elapsed = 0.0
    while elapsed < total_timeout:
        session.run_for(poll)
        text = session.read_uart(wait_seconds=1.0)
        if expected_substring in text:
            return True
        elapsed += poll
    return False


# --------------------------------------------------------------------- #
# 1) test_sensor_initialization
# --------------------------------------------------------------------- #
def test_sensor_initialization(renode_session):
    """Firmware baslarken sensorun basariyla init edildigini UART'tan dogrular."""
    found = _wait_for_text(renode_session, "Sensor initialized")
    assert found, (
        "UART ciktisinda 'Sensor initialized' mesaji bulunamadi -- "
        f"alinan cikti:\n{renode_session.uart_buffer}"
    )


# --------------------------------------------------------------------- #
# 2) test_sensor_configuration
# --------------------------------------------------------------------- #
def test_sensor_configuration(renode_session):
    """Sensor konfigurasyonunun (mux/pga/mode/data_rate) basariyla
    uygulandigini UART'tan dogrular."""
    found = _wait_for_text(renode_session, "Sensor configuration OK")
    assert found, (
        "UART ciktisinda 'Sensor configuration OK' mesaji bulunamadi -- "
        f"alinan cikti:\n{renode_session.uart_buffer}"
    )


# --------------------------------------------------------------------- #
# 3) test_sensor_register_read
# --------------------------------------------------------------------- #
def test_sensor_register_read(renode_session):
    """Firmware'in Conversion register'ini gercekten okudugunu, UART'ta
    en az bir 'Voltage: ...' satirinin belirmesinden anlariz."""
    renode_session.set_simulated_voltage(1000)
    found = _wait_for_text(renode_session, "Voltage:")
    assert found, "Conversion register okuma sonucu (Voltage: ...) UART'ta hic gorunmedi"


# --------------------------------------------------------------------- #
# 4) test_sensor_register_write
# --------------------------------------------------------------------- #
def test_sensor_register_write(renode_session):
    """Config register'ina yazmanin basarili oldugunu dolayli olarak dogrular:
    yazma basarisiz olsaydi ads1x1x_configure() ADS1X1X_ERROR_COMM donerdi
    ve 'Sensor configuration OK' hic UART'a yazdirilmazdi (main.c akisi)."""
    found = _wait_for_text(renode_session, "Sensor configuration OK")
    assert found, "Config register yazma basarisiz oldu (config OK mesaji gelmedi)"


# --------------------------------------------------------------------- #
# 5) test_sensor_measurement
# --------------------------------------------------------------------- #
def test_sensor_measurement(renode_session):
    """Belirli bir simule voltaj ayarlanip, UART'a yazdirilan olcum
    degerinin makul bir toleransla o voltaja yakin oldugunu dogrular."""
    target_mv = 500.0
    renode_session.set_simulated_voltage(target_mv)

    found = _wait_for_text(renode_session, "Voltage:")
    assert found, "Olcum sonucu UART'ta hic gorunmedi"

    matches = VOLTAGE_LINE_RE.findall(renode_session.uart_buffer)
    assert matches, "UART ciktisinda 'Voltage: X.XX mV' formatinda satir bulunamadi"

    last_value = float(matches[-1])
    # ADS1115, 16-bit + PGA=+-2.048V ile ~0.0625mV cozunurluge sahip;
    # 5 mV toleransi hem bu kuantalama hattini hem de zamanlama farklarini kapsar.
    assert abs(last_value - target_mv) < 5.0, (
        f"Beklenen ~{target_mv}mV, UART'ta okunan degistirilmis deger: {last_value}mV"
    )


# --------------------------------------------------------------------- #
# 6) test_sensor_invalid_configuration
# --------------------------------------------------------------------- #
def test_sensor_invalid_configuration(renode_session_no_sensor):
    """Sensor I2C hattinda hic yokken (adres bos), donanim NACK doner ve
    HAL_I2C_Mem_Write/Read basarisiz olur -> driver ADS1X1X_ERROR_COMM
    dondurur -> main.c Error_Handler()'a dusup CPU'yu durdurur. Bu durumda
    'Sensor configuration OK' mesaji ASLA UART'a yazdirilmamalidir."""
    time.sleep(0.5)
    found = _wait_for_text(renode_session_no_sensor, "Sensor configuration OK",
                            total_timeout=4.0, poll=1.0)
    assert not found, (
        "Sensor hic bagli degilken bile 'Sensor configuration OK' mesaji geldi -- "
        "driver iletisim hatasini dogru ele almiyor olabilir"
    )
