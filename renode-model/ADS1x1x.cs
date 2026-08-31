//
// ADS1x1x.cs
// Renode C# peripheral model: ADS1014 / ADS1015 / ADS1115 (I2C ADC ailesi)
//
// Bu model, sensor/ads1x1x.h/.c'deki C driver ile ayni register haritasini
// ve bit alanlarini kullanir; boylece STM32 firmware'i, gercek donanim
// olmadan bu C# model ile I2C uzerinden konusabilir.
//
// Desteklenen davranis:
//   - Config register (OS/MUX/PGA/MODE/DR/COMP_*) okuma/yazma
//   - Lo/Hi threshold register'lari okuma/yazma
//   - Conversion register: "SimulatedVoltageMilliVolts" property'sine gore,
//     secili PGA araligina ve variant'in cozunurlugune (12-bit/16-bit) gore
//     GERCEK KOD hesaplanip donuluyor (sabit deger degil).
//   - OS biti yazildiginda "donusum baslar", okunurken her zaman hazir (1) gorunur
//     (Renode'da I2C islemleri aninda gerceklestigi icin gercekci bir yaklasim).
//
using System;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.I2C;

namespace Antmicro.Renode.Peripherals.Sensors
{
    public class ADS1x1x : II2CPeripheral, IProvidesRegisterCollection<WordRegisterCollection>
    {
        // "variant" repl dosyasindan string olarak geliyor (orn. "ADS1115"),
        // enum parse hatasi olursa acikca hata verip kullaniciyi uyariyoruz.
        public ADS1x1x(string variant = "ADS1115")
        {
            if(!Enum.TryParse(variant, true, out this.variant))
            {
                throw new Antmicro.Renode.Exceptions.ConstructionException(
                    $"Bilinmeyen ADS1x1x variant: '{variant}'. Gecerli degerler: ADS1014, ADS1015, ADS1115.");
            }

            RegistersCollection = new WordRegisterCollection(this);
            DefineRegisters();
            Reset();
        }

        // --------------------------------------------------------------
        // II2CPeripheral
        // --------------------------------------------------------------

        public void Write(byte[] data)
        {
            if(data.Length == 0)
            {
                this.Log(LogLevel.Warning, "Veri icermeyen bir I2C yazma islemi alindi, yok sayildi");
                return;
            }

            // Ilk byte her zaman "Address Pointer Register" -- hangi register'a
            // erisilecegini belirtir (Conversion/Config/Lo_thresh/Hi_thresh).
            pointerRegister = data[0];

            if(data.Length == 1)
            {
                // Sadece pointer yazildi (sonraki islem bir okuma olacak) -- normal.
                return;
            }

            if(data.Length != 3)
            {
                this.Log(LogLevel.Warning,
                    "Beklenmeyen veri uzunlugu ({0} byte): ADS1x1x register'lari 16-bit'tir, " +
                    "pointer + 2 byte (MSB, LSB) bekleniyordu", data.Length);
                return;
            }

            ushort value = (ushort)((data[1] << 8) | data[2]);
            RegistersCollection.Write(pointerRegister, value);
        }

        public byte[] Read(int count)
        {
            var value = RegistersCollection.Read(pointerRegister);
            var result = new byte[count];
            if(count >= 1)
            {
                result[0] = (byte)(value >> 8);   // MSB once (datasheet'e gore)
            }
            if(count >= 2)
            {
                result[1] = (byte)(value & 0xFF); // LSB sonra
            }
            return result;
        }

        public void FinishTransmission()
        {
            // NOT: Gercek ADS1x1x donanaminda "address pointer register" bir
            // sonraki yazmaya kadar degerini korur (SI7210'un aksine burada
            // pointer'i sifirlamiyoruz) -- bu yuzden burada bilerek bos birakildi.
        }

        public void Reset()
        {
            RegistersCollection.Reset();
            pointerRegister = (byte)Registers.Conversion;
            simulatedVoltageMilliVolts = 0m;
        }

        public WordRegisterCollection RegistersCollection { get; }

        // --------------------------------------------------------------
        // Test/entegrasyon arayuzu: Python integration testleri bu property'yi
        // monitor uzerinden degistirip firmware'in dogru veriyi okudugunu
        // dogrulayabilir, ornegin:
        //   sysbus.i2c1.ads1x1x SimulatedVoltageMilliVolts 1250
        // --------------------------------------------------------------
        public decimal SimulatedVoltageMilliVolts
        {
            get => simulatedVoltageMilliVolts;
            set => simulatedVoltageMilliVolts = value;
        }

        // --------------------------------------------------------------
        // Register tanimlari (ads1x1x.h ile birebir ayni bit alanlari)
        // --------------------------------------------------------------
        private void DefineRegisters()
        {
            Registers.Conversion.Define(this, resetValue: 0)
                .WithValueField(0, 16, FieldMode.Read,
                    valueProviderCallback: _ => ComputeConversionCode(),
                    name: "D")
            ;

            Registers.Config.Define(this, resetValue: 0x8583)
                .WithFlag(15,
                    valueProviderCallback: _ => true, // Renode'da donusum aninda biter -> her zaman "hazir"
                    writeCallback: (_, value) => { if(value) { StartConversion(); } },
                    name: "OS")
                .WithValueField(12, 3, out muxField, name: "MUX")
                .WithValueField(9, 3, out pgaField, name: "PGA")
                .WithFlag(8, name: "MODE")
                .WithValueField(5, 3, name: "DR")
                .WithFlag(4, name: "COMP_MODE")
                .WithFlag(3, name: "COMP_POL")
                .WithFlag(2, name: "COMP_LAT")
                .WithValueField(0, 2, name: "COMP_QUE")
            ;

            Registers.LoThresh.Define(this, resetValue: 0x8000)
                .WithValueField(0, 16, name: "LoThresh")
            ;

            Registers.HiThresh.Define(this, resetValue: 0x7FFF)
                .WithValueField(0, 16, name: "HiThresh")
            ;
        }

        private void StartConversion()
        {
            // ADS1014'te MUX'in fiziksel bir karsiligi yok -- gercekci davranis
            // icin, MUX != 0 (AIN0-AIN1) ayarlanmaya calisilirsa uyar (engellemiyoruz,
            // bu kontrol zaten C driver tarafinda yapiliyor).
            if(variant == Variant.ADS1014 && muxField.Value != 0)
            {
                this.Log(LogLevel.Warning,
                    "ADS1014'te MUX alani fiziksel olarak yok, bu chip'te etkisizdir");
            }

            this.Log(LogLevel.Debug, "Donusum baslatildi (variant={0}, PGA index={1}, simuleVoltaj={2}mV)",
                variant, pgaField.Value, simulatedVoltageMilliVolts);

            // Conversion register'i, Read() sirasinda valueProviderCallback zaten
            // guncel PGA/variant'a gore yeniden hesapliyor -- burada ekstra bir
            // islem yapmaya gerek yok, sadece log/telemetri amacli birakildi.
        }

        // simulatedVoltageMilliVolts'u, secili PGA araligina ve variant'in
        // cozunurlugune gore ADC koduna cevirir (datasheet Table "FSR" mantigi).
        private ushort ComputeConversionCode()
        {
            var fsrMilliVolts = PgaFullScaleMilliVolts[(int)Math.Min(pgaField.Value, 5)];
            var fullScaleCode = IsSixteenBit(variant) ? 32768m : 2048m;

            var rawCode = (simulatedVoltageMilliVolts / fsrMilliVolts) * fullScaleCode;
            var clamped = Math.Max(-fullScaleCode, Math.Min(fullScaleCode - 1, rawCode));
            var code = (short)Math.Round(clamped, MidpointRounding.AwayFromZero);

            if(IsSixteenBit(variant))
            {
                return unchecked((ushort)code);
            }

            // 12-bit ailede (ADS1014/1015) sonuc, 16-bit register'in ustunde
            // sol-hizali durur; alt 4 bit her zaman 0'dir.
            var twelveBitMasked = (short)(code & 0x0FFF);
            return unchecked((ushort)(twelveBitMasked << 4));
        }

        private static bool IsSixteenBit(Variant variant) => variant == Variant.ADS1115;

        private byte pointerRegister;
        private decimal simulatedVoltageMilliVolts;
        private IValueRegisterField muxField;
        private IValueRegisterField pgaField;
        private readonly Variant variant;

        // PGA index -> tam skala araligi (mV) -- ads1x1x.h'deki tabloyla ayni
        private static readonly decimal[] PgaFullScaleMilliVolts =
        {
            6144m, 4096m, 2048m, 1024m, 512m, 256m
        };

        private enum Registers : byte
        {
            Conversion = 0x00,
            Config     = 0x01,
            LoThresh   = 0x02,
            HiThresh   = 0x03,
        }

        private enum Variant
        {
            ADS1014,
            ADS1015,
            ADS1115,
        }
    }
}
