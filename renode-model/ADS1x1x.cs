//
// ADS1x1x.cs
//
// Renode C# peripheral model for the ADS1014 / ADS1015 / ADS1115 family
// (Texas Instruments I2C analog-to-digital converters).
//
// This model uses the same register map and bit fields as the C driver
// in lib/ads1x1x/{inc,src}, so STM32 firmware can talk to it over I2C
// exactly as it would talk to real hardware.
//
// Supported behavior:
//   - Config register (OS/MUX/PGA/MODE/DR/COMP_*) read/write
//   - Lo/Hi threshold register read/write
//   - Conversion register: based on the "SimulatedVoltageMilliVolts"
//     property (or a random value within a configured range -- see the
//     constructor), a REAL ADC code is computed according to the
//     selected PGA range and the variant's resolution (12-bit/16-bit) --
//     not a fixed/constant value.
//   - Writing the OS bit "starts a conversion"; on read it always
//     appears ready (1), since I2C transactions in Renode complete
//     instantly (a reasonable simplification for this environment).
//
using System;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.I2C;

namespace Antmicro.Renode.Peripherals.Sensors
{
    /// <summary>
    /// Renode I2C peripheral model for the ADS1014 / ADS1015 / ADS1115
    /// family of analog-to-digital converters.
    /// </summary>
    public class ADS1x1x : II2CPeripheral, IProvidesRegisterCollection<WordRegisterCollection>
    {
        /// <summary>
        /// Creates a new ADS1x1x model.
        /// </summary>
        /// <param name="variant">
        /// Which chip in the family to emulate: "ADS1014", "ADS1015", or
        /// "ADS1115" (case-insensitive). Comes from the platform (.repl)
        /// file as a plain string, so an invalid value throws a
        /// <see cref="Antmicro.Renode.Exceptions.ConstructionException"/>
        /// with a clear message rather than failing silently.
        /// </param>
        /// <param name="minVoltageMilliVolts">
        /// Optional. If set together with <paramref name="maxVoltageMilliVolts"/>
        /// to a range where max &gt; min, the model picks a NEW random
        /// voltage (in millivolts) within [min, max] every time the
        /// Conversion register is read, instead of using a fixed value.
        /// Leave both at their default (0) to keep the previous behavior:
        /// a fixed value set via the <see cref="SimulatedVoltageMilliVolts"/>
        /// property (e.g. from the monitor or from a test script).
        /// </param>
        /// <param name="maxVoltageMilliVolts">See <paramref name="minVoltageMilliVolts"/>.</param>
        public ADS1x1x(string variant = "ADS1115", double minVoltageMilliVolts = 0.0,
                        double maxVoltageMilliVolts = 0.0)
        {
            if (!Enum.TryParse(variant, true, out this.variant))
            {
                throw new Antmicro.Renode.Exceptions.ConstructionException(
                    $"Unknown ADS1x1x variant: '{variant}'. Valid values: ADS1014, ADS1015, ADS1115.");
            }

            this.minSimulatedVoltageMilliVolts = (decimal)minVoltageMilliVolts;
            this.maxSimulatedVoltageMilliVolts = (decimal)maxVoltageMilliVolts;
            this.randomizeVoltage = this.maxSimulatedVoltageMilliVolts > this.minSimulatedVoltageMilliVolts;

            RegistersCollection = new WordRegisterCollection(this);
            DefineRegisters();
            Reset();
        }

        // --------------------------------------------------------------
        // II2CPeripheral
        // --------------------------------------------------------------

        /// <summary>
        /// Handles an I2C write. Supports both calling conventions seen in
        /// practice:
        ///   - a single call carrying the pointer byte AND the 2 data bytes
        ///     together (data.Length == 3), and
        ///   - two separate calls: one with just the pointer byte
        ///     (data.Length == 1), followed later by one with just the 2
        ///     data bytes (data.Length == 2) for the register that pointer
        ///     selected. Some I2C master/HAL implementations split a
        ///     register write into a pointer burst and a data burst rather
        ///     than bundling them into one call.
        /// </summary>
        public void Write(byte[] data)
        {
            if (data.Length == 0)
            {
                this.Log(LogLevel.Warning, "Received an I2C write with no data; ignoring");
                return;
            }

            if (data.Length == 1)
            {
                // Only the Address Pointer Register byte; the data bytes
                // (if this is a register write) will arrive in a
                // subsequent Write() call.
                pointerRegister = data[0];
                return;
            }

            if (data.Length == 2)
            {
                // The 2 data bytes for the register selected by a
                // PREVIOUS Write() call (see the data.Length == 1 case).
                ushort value = (ushort)((data[0] << 8) | data[1]);
                RegistersCollection.Write(pointerRegister, value);
                return;
            }

            if (data.Length == 3)
            {
                // Pointer byte and the 2 data bytes bundled into one call.
                pointerRegister = data[0];
                ushort value = (ushort)((data[1] << 8) | data[2]);
                RegistersCollection.Write(pointerRegister, value);
                return;
            }

            this.Log(LogLevel.Warning,
                "Unexpected data length ({0} bytes): ADS1x1x registers are 16-bit; " +
                "expected a 1-byte pointer write, a 2-byte data write, or a combined " +
                "3-byte (pointer + data) write", data.Length);
        }

        /// <summary>
        /// Handles an I2C read from the register currently selected by the
        /// Address Pointer Register, returning up to <paramref name="count"/>
        /// bytes (MSB first, per the datasheet).
        /// </summary>
        public byte[] Read(int count)
        {
            var value = RegistersCollection.Read(pointerRegister);
            var result = new byte[count];
            if (count >= 1)
            {
                result[0] = (byte)(value >> 8);   // MSB first
            }
            if (count >= 2)
            {
                result[1] = (byte)(value & 0xFF); // LSB second
            }
            return result;
        }

        /// <summary>
        /// Called when an I2C transaction ends.
        /// </summary>
        public void FinishTransmission()
        {
            // NOTE: on real ADS1x1x hardware the Address Pointer Register
            // keeps its value until the next write (unlike, e.g., the
            // SI7210 model), so it is intentionally NOT reset here.
        }

        /// <summary>
        /// Resets all registers to their datasheet power-on-reset values.
        /// </summary>
        public void Reset()
        {
            RegistersCollection.Reset();
            pointerRegister = (byte)Registers.Conversion;
            simulatedVoltageMilliVolts = 0m;
        }

        public WordRegisterCollection RegistersCollection { get; }

        /// <summary>
        /// The voltage (in millivolts) the model currently reports on the
        /// analog input. Settable from Renode's monitor or from test
        /// automation, e.g.:
        /// <c>sysbus.i2c1.ads1x1x SimulatedVoltageMilliVolts 1250</c>
        /// If a random range was configured via the constructor
        /// (<see cref="ADS1x1x(string, double, double)"/>), this value is
        /// overwritten with a fresh random sample on every Conversion
        /// register read; the getter still returns whatever value was most
        /// recently used, which is useful for tests/logging.
        /// </summary>
        public decimal SimulatedVoltageMilliVolts
        {
            get => simulatedVoltageMilliVolts;
            set => simulatedVoltageMilliVolts = value;
        }

        // --------------------------------------------------------------
        // Register definitions (bit fields match lib/ads1x1x/inc/ads1x1x.h)
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
                    valueProviderCallback: _ => true, // conversions complete instantly in Renode -> always "ready"
                    writeCallback: (_, value) => { if (value) { StartConversion(); } },
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
            // ADS1014 has no physical MUX; for realism, warn if a MUX value
            // other than 0 (AIN0-AIN1) is set. This is not enforced here
            // (the C driver already validates it on the firmware side).
            if (variant == Variant.ADS1014 && muxField.Value != 0)
            {
                this.Log(LogLevel.Warning,
                    "MUX field has no physical effect on ADS1014");
            }

            this.Log(LogLevel.Debug, "Conversion started (variant={0}, PGA index={1}, simulatedVoltage={2}mV)",
                variant, pgaField.Value, simulatedVoltageMilliVolts);

            // The Conversion register's valueProviderCallback already
            // recomputes the code from the current PGA/variant on every
            // Read(), so nothing further needs to happen here; this is
            // kept only for logging/telemetry purposes.
        }

        /// <summary>
        /// If a random voltage range was configured, picks a new random
        /// value within [min, max] and stores it in
        /// <see cref="simulatedVoltageMilliVolts"/>. Called once per
        /// Conversion register read, so each read can return a different
        /// value.
        /// </summary>
        private void RandomizeVoltageIfConfigured()
        {
            if (!randomizeVoltage)
            {
                return;
            }

            var range = (double)(maxSimulatedVoltageMilliVolts - minSimulatedVoltageMilliVolts);
            var sample = random.NextDouble() * range;
            simulatedVoltageMilliVolts = minSimulatedVoltageMilliVolts + (decimal)sample;
        }

        /// <summary>
        /// Converts <see cref="simulatedVoltageMilliVolts"/> to an ADC code
        /// based on the selected PGA range and the variant's resolution
        /// (mirrors the "FSR" logic in the datasheet).
        /// </summary>
        private ushort ComputeConversionCode()
        {
            RandomizeVoltageIfConfigured();

            var fsrMilliVolts = PgaFullScaleMilliVolts[(int)Math.Min(pgaField.Value, 5)];
            var fullScaleCode = IsSixteenBit(variant) ? 32768m : 2048m;

            var rawCode = (simulatedVoltageMilliVolts / fsrMilliVolts) * fullScaleCode;
            var clamped = Math.Max(-fullScaleCode, Math.Min(fullScaleCode - 1, rawCode));
            var code = (short)Math.Round(clamped, MidpointRounding.AwayFromZero);

            if (IsSixteenBit(variant))
            {
                return unchecked((ushort)code);
            }

            // On the 12-bit family (ADS1014/1015), the result is
            // left-aligned within the 16-bit register; the low 4 bits are
            // always zero.
            var twelveBitMasked = (short)(code & 0x0FFF);
            return unchecked((ushort)(twelveBitMasked << 4));
        }

        private static bool IsSixteenBit(Variant variant) => variant == Variant.ADS1115;

        private byte pointerRegister;
        private decimal simulatedVoltageMilliVolts;
        private readonly decimal minSimulatedVoltageMilliVolts;
        private readonly decimal maxSimulatedVoltageMilliVolts;
        private readonly bool randomizeVoltage;
        private readonly Random random = new Random();
        private IValueRegisterField muxField;
        private IValueRegisterField pgaField;
        private readonly Variant variant;

        // PGA index -> full-scale range (mV); matches the table in ads1x1x.h.
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
