#include "hffEZMUX_SINGCHAN.h"
#include <stdlib.h>
#define _aAddress 0x29
// SD CARD CONFIG
#define SD_FAT_TYPE 3
char filename[16];
/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

SdFs sd;
FsFile file;

Adafruit_BME680 bme;
bool bmeInit = false;


/*
					ROWS GO THIS WAY>>>
					C 0  | 1  | 2  | 3  |
 TCA9548A_address1  O----+----+----+----+
	  (0x70)        L 4  | 5  | 6  | 7  |
--------------------S----+----+----+----+
					↓ 8  | 9  | 10 | 11 |
 TCA9548A_address2  ↓----+----+----+----+
	  (0x71)        ↓ 12 | 13 | 14 | 15 |
					↓----+----+----+----+
*/

Nanolab::Nanolab() {
	_initialized = false;
	_integration = TSL2591_INTEGRATIONTIME_100MS;
	_gain = TSL2591_GAIN_MED;
	_sensorID = -1;
}

//Enable Function
void Nanolab::tcaselect1(uint8_t i) {
	if (i > 7) return;
	Disable_tcaselect2();
	Wire.beginTransmission(0x70);
	Wire.write(1 << i);
	Wire.endTransmission();
}
void Nanolab::tcaselect2(uint8_t i) {
	if (i > 7) return;
	Disable_tcaselect1();
	Wire.beginTransmission(0x71);
	Wire.write(1 << i);
	Wire.endTransmission();
}

// Disable functions
void Nanolab::Disable_tcaselect1() {
	Wire.beginTransmission(0x70);
	Wire.write(0);
	Wire.endTransmission();
}
void Nanolab::Disable_tcaselect2() {
	Wire.beginTransmission(0x71);
	Wire.write(0);
	Wire.endTransmission();
}


// Building block functions

boolean Nanolab::begin() {
	Wire.begin();
	uint8_t id = read8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_DEVICE_ID);
	if (id != 0x50) {
		return false;
	}
	_initialized = true;
	// Set default integration time and gain
	setTiming(_integration);
	setGain(_gain);
	// Note: by default, the device is in power down mode on bootup
	disable();

	return true;
}
void Nanolab::enable(void) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	// Enable the device by setting the control bit to 0x01
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
		TSL2591_ENABLE_POWERON | TSL2591_ENABLE_AEN | TSL2591_ENABLE_AIEN |
		TSL2591_ENABLE_NPIEN);
}
void Nanolab::disable(void) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
		TSL2591_ENABLE_POWEROFF);
}
void Nanolab::setGain(tsl2591Gain_t gain) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	enable();
	_gain = gain;
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, _integration | _gain);
	disable();
}
tsl2591Gain_t Nanolab::getGain() { return _gain; }

void Nanolab::setTiming(tsl2591IntegrationTime_t integration) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	enable();
	_integration = integration;
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CONTROL, _integration | _gain);
	disable();
}

tsl2591IntegrationTime_t Nanolab::getTiming() { return _integration; }

float Nanolab::calculateLux(uint16_t ch0, uint16_t ch1) {
	float atime, again;
	float cpl, lux1, lux2, lux;
	uint32_t chan0, chan1;

	// Check for overflow conditions first
	if ((ch0 == 0xFFFF) | (ch1 == 0xFFFF)) {
		// Signal an overflow
		return -1;
	}

	// Note: This algorithm is based on preliminary coefficients
	// provided by AMS and may need to be updated in the future

	switch (_integration) {
	case TSL2591_INTEGRATIONTIME_100MS:
		atime = 100.0F;
		break;
	case TSL2591_INTEGRATIONTIME_200MS:
		atime = 200.0F;
		break;
	case TSL2591_INTEGRATIONTIME_300MS:
		atime = 300.0F;
		break;
	case TSL2591_INTEGRATIONTIME_400MS:
		atime = 400.0F;
		break;
	case TSL2591_INTEGRATIONTIME_500MS:
		atime = 500.0F;
		break;
	case TSL2591_INTEGRATIONTIME_600MS:
		atime = 600.0F;
		break;
	default: // 100ms
		atime = 100.0F;
		break;
	}

	switch (_gain) {
	case TSL2591_GAIN_LOW:
		again = 1.0F;
		break;
	case TSL2591_GAIN_MED:
		again = 25.0F;
		break;
	case TSL2591_GAIN_HIGH:
		again = 428.0F;
		break;
	case TSL2591_GAIN_MAX:
		again = 9876.0F;
		break;
	default:
		again = 1.0F;
		break;
	}

	// cpl = (ATIME * AGAIN) / DF
	cpl = (atime * again) / TSL2591_LUX_DF;

	// Original lux calculation (for reference sake)
	// lux1 = ( (float)ch0 - (TSL2591_LUX_COEFB * (float)ch1) ) / cpl;
	// lux2 = ( ( TSL2591_LUX_COEFC * (float)ch0 ) - ( TSL2591_LUX_COEFD *
	// (float)ch1 ) ) / cpl; lux = lux1 > lux2 ? lux1 : lux2;

	// Alternate lux calculation 1
	// See: https://github.com/adafruit/Adafruit_TSL2591_Library/issues/14
	lux = (((float)ch0 - (float)ch1)) * (1.0F - ((float)ch1 / (float)ch0)) / cpl;

	// Alternate lux calculation 2
	// lux = ( (float)ch0 - ( 1.7F * (float)ch1 ) ) / cpl;

	// Signal I2C had no errors
	return lux;

}

uint32_t Nanolab::getFullLuminosity(void) {

	enable();

	// Wait x ms for ADC to complete
	for (uint8_t d = 0; d <= _integration; d++) {
		delay(120);
	}

	// CHAN0 must be read before CHAN1
	// See: https://forums.adafruit.com/viewtopic.php?f=19&t=124176
	uint32_t x;
	uint16_t y;
	y = read16(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN0_LOW);
	x = read16(TSL2591_COMMAND_BIT | TSL2591_REGISTER_CHAN1_LOW);
	x <<= 16;
	x |= y;

	disable();

	return x;
}

uint16_t Nanolab::getLuminosity(uint8_t channel) {
	uint32_t x = getFullLuminosity();

	if (channel == TSL2591_FULLSPECTRUM) {
		// Reads two byte value from channel 0 (visible + infrared)
		return (x & 0xFFFF);
	}
	else if (channel == TSL2591_INFRARED) {
		// Reads two byte value from channel 1 (infrared)
		return (x >> 16);
	}
	else if (channel == TSL2591_VISIBLE) {
		// Reads all and subtracts out just the visible!
		return ((x & 0xFFFF) - (x >> 16));
	}

	// unknown channel!
	return 0;
}

void Nanolab::registerInterrupt(
	uint16_t lowerThreshold, uint16_t upperThreshold,
	tsl2591Persist_t persist = TSL2591_PERSIST_ANY) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	enable();
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_PERSIST_FILTER, persist);
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_THRESHOLD_AILTL,
		lowerThreshold);
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_THRESHOLD_AILTH,
		lowerThreshold >> 8);
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_THRESHOLD_AIHTL,
		upperThreshold);
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_THRESHOLD_AIHTH,
		upperThreshold >> 8);
	disable();
}
void Nanolab::clearInterrupt() {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	enable();
	write8(TSL2591_CLEAR_INT);
	disable();
}

uint8_t Nanolab::getStatus(void) {
	if (!_initialized) {
		if (!begin()) {
			return 0;
		}
	}

	// Enable the device
	enable();
	uint8_t x;
	x = read8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_DEVICE_STATUS);
	disable();
	return x;
}

void Nanolab::fileConfig(void)
{
	//snprintf(filename, sizeof(filename), "data%d:%d:%d_%d/%d/%d.csv", hour(),minute(),second(),day(),month(), year()); // includes a three-digit sequence number in the file name
	//dataFile.open(filename, O_RDWR | O_CREAT | O_AT_END);
	//dataFile.print("milliseconds");
	//dataFile.print("\tIR Luminosity");
	//dataFile.print("\tFull Luminosity");
	//dataFile.print("\tVisible Luminosity");
	//dataFile.print("\tLux");
	//dataFile.println(""); // new line for subsequent data output
	//dataFile.close();
}

//Reffered functions

/*
THIS IS THE FUNCTIONS STUDENTS MUST INTERACT WITH
*/

void Nanolab::simpleRead(int x, int y)
{
	// Simple data read example. Just read the infrared, fullspecrtrum diode 
	// or 'visible' (difference between the two) channels.
	// This can take 100-600 milliseconds! Uncomment whichever of the following you want to read
	LED(x, y);
	Wire.begin();
	if (y == 0 || y == 1) {
		if (y == 0) { tcaselect1(x); }
		if (y == 1) { tcaselect1(x + 4); }
	}
	if (y == 2 || y == 3) {
		if (y == 2) { tcaselect2(x); }
		if (y == 3) { tcaselect2(x + 4); }
	}
	begin();
	uint16_t lx = getLuminosity(TSL2591_VISIBLE);
	//uint16_t x = getLuminosity(TSL2591_FULLSPECTRUM);
	//uint16_t x = getLuminosity(TSL2591_INFRARED);

	sro.ms = millis();
	sro.lum = lx;
}


void Nanolab::advancedRead(int x, int y)
{
	LED(x, y);
	Wire.begin();
	if (y == 0 || y == 1) {
		if (y == 0) { tcaselect1(x); }
		if (y == 1) { tcaselect1(x + 4); }
	}
	if (y == 2 || y == 3) {
		if (y == 2) { tcaselect2(x); }
		if (y == 3) { tcaselect2(x + 4); }
	}
	// More advanced data read example. Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
	// That way you can do whatever math and comparisons you want!
	begin();
	uint32_t lum = getFullLuminosity();
	uint16_t ir, full;
	ir = lum >> 16;
	full = lum & 0xFFFF;
	aro.ms = millis();
	aro.ir = ir;
	aro.full = full;
	aro.vis = (full - ir);
	aro.lux = calculateLux(full, ir);
}

void Nanolab::housekeeping()
{
	if (bmeInit == false) {
		// Set up oversampling and filter initialization
		bme.begin();
		bme.setTemperatureOversampling(BME680_OS_8X);
		bme.setHumidityOversampling(BME680_OS_2X);
		bme.setPressureOversampling(BME680_OS_4X);
		bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
		bme.setGasHeater(320, 150); // 320*C for 150 ms
		bmeInit = true;
	}

	bme.performReading();
	hkro.temperature = bme.temperature;
	hkro.pressure = bme.pressure / 100.0;
	hkro.humidity = bme.humidity;
	hkro.gas_resistance = bme.gas_resistance / 1000.0;
	hkro.alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

time_t getTeensy3Time()
{
	return Teensy3Clock.get();
}

void TSL2591::saveSD(){
	setSyncProvider(getTeensy3Time);
	snprintf(filename, sizeof(filename), "%d_%d--%d-%d.csv", hour(),minute(),day(),month());
	
	sd.begin(SD_CONFIG);
	file.open(filename, FILE_WRITE);
	file.print("Column");
	file.print(";");
	file.print("Row");
	file.print(";");
	file.print("milliseconds");
	file.print(";");
	file.print("IR Luminosity");
	file.print(";");
	file.print("Full Luminosity");
	file.print(";");
	file.print("Visible Luminosity");
	file.print(";");
	file.print("Lux");
	file.println("");

	for(int i = 0; i<4; i++){
		for(int j = 0; j<4; j++){
			for(int p = 0; p<11;p++){
				advancedRead(j,i);
				file.print(j, 3);
				file.print(";");
				file.print(i, 3);
				file.print(";");
				file.print(aro.ms, 3);
				file.print(";");
				file.print(aro.ir, 3);
				file.print(";");
				file.print(aro.full, 3);
				file.print(";");
				file.print(aro.vis, 3);
				file.print(";");
				file.print(aro.lux, 3);
				file.println("");
			}
		}
	}
	file.close();
}

void Nanolab::LEDReset() {
	pinMode(24, OUTPUT);
	pinMode(25, OUTPUT);
	pinMode(26, OUTPUT);
	pinMode(27, OUTPUT);
	pinMode(28, OUTPUT);
	pinMode(29, OUTPUT);
	pinMode(30, OUTPUT);
	pinMode(31, OUTPUT);

}
void Nanolab::LED(int column, int row) {
	LEDReset();
	switch (column) {
	case 0:
		digitalWrite(24, HIGH);
		digitalWrite(25, LOW);
		digitalWrite(26, LOW);
		digitalWrite(27, LOW);
		break;

	case 1:
		digitalWrite(24, LOW);
		digitalWrite(25, HIGH);
		digitalWrite(26, LOW);
		digitalWrite(27, LOW);
		break;

	case 2:
		digitalWrite(24, LOW);
		digitalWrite(25, LOW);
		digitalWrite(26, HIGH);
		digitalWrite(27, LOW);
		break;

	case 3:
		digitalWrite(24, LOW);
		digitalWrite(25, LOW);
		digitalWrite(26, LOW);
		digitalWrite(27, HIGH);
		break;

	default:
		break;
	}
	switch (row) {
	case 0:
		digitalWrite(28, LOW);
		digitalWrite(29, HIGH);
		digitalWrite(30, HIGH);
		digitalWrite(31, HIGH);
		break;

	case 1:
		digitalWrite(28, HIGH);
		digitalWrite(29, LOW);
		digitalWrite(30, HIGH);
		digitalWrite(31, HIGH);
		break;

	case 2:
		digitalWrite(28, HIGH);
		digitalWrite(29, HIGH);
		digitalWrite(30, LOW);
		digitalWrite(31, HIGH);
		break;

	case 3:
		digitalWrite(28, HIGH);
		digitalWrite(29, HIGH);
		digitalWrite(30, HIGH);
		digitalWrite(31, LOW);
		break;

	default:
		break;
	}


}


void simleReadMatrix() {
	//for(i = 0; i<4; i++){
	//	for(j = 0; j<4; j++){
	//		sroArray[i][j] = simpleRead(i,j).lum;
	//	}
	//}
}



// Read/write code
uint8_t Nanolab::read8(uint8_t reg) {
	uint8_t value = 0;
	Wire.beginTransmission(_aAddress);
	Wire.send(reg);
	Wire.endTransmission();
	Wire.requestFrom(_aAddress, (byte)1);
	value = Wire.readByte();
	return value;
}
uint16_t Nanolab::read16(uint8_t reg) {
	uint8_t buffer[2];
	uint16_t value = 0;
	Wire.beginTransmission(_aAddress); \
		Wire.send(reg);
	Wire.endTransmission();
	Wire.requestFrom(_aAddress, (byte)2);
	buffer[0] = Wire.readByte();
	buffer[1] = Wire.readByte();
	return uint16_t(buffer[1]) << 8 | uint16_t(buffer[0]);
}
void Nanolab::write8(uint8_t reg, uint8_t value) {
	Wire.beginTransmission(_aAddress); //// MODIFIED RB
	Wire.send(reg); /// TO BE MODIFIED? NO
	Wire.send(value); /// TO BE MODIFIED? NO
	Wire.endTransmission();
}

void Nanolab::write8(uint8_t reg) {
	Wire.beginTransmission(_aAddress);
	Wire.send(reg);
	Wire.endTransmission();
}