#include "hffEZMUX_SINGCHAN.h"
#include <stdlib.h>
#define _aAddress 0x29
#define SD_FAT_TYPE 3
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS
SdFs sd;
FsFile dataFile;
char fileName[16];

/*
                    ROWS GO THIS WAY>>>
                    C 1  | 2  | 3  | 4  |
 TCA9548A_address1  O----+----+----+----+
      (0x70)        L 5  | 6  | 7  | 8  |
--------------------S----+----+----+----+
                    ↓ 9  | 10 | 11 | 12 |
 TCA9548A_address2  ↓----+----+----+----+
      (0x77)        ↓ 13 | 14 | 15 | 16 |
                    ↓----+----+----+----+
*/

TSL2591::TSL2591() {
	_initialized = false;
	_integration = TSL2591_INTEGRATIONTIME_100MS;
	_gain = TSL2591_GAIN_MED;
	_sensorID = -1;
}

//Enable Function
void TSL2591::tcaselect1(uint8_t i) {
	if (i > 7) return;
	Disable_tcaselect2();
	Wire.beginTransmission(0x70);
	Wire.write(1 << i);
	Wire.endTransmission();
}
void TSL2591::tcaselect2(uint8_t i) {
	if (i > 7) return;
	Disable_tcaselect1();
	Wire.beginTransmission(0x77);
	Wire.write(1 << i);
	Wire.endTransmission();
}

// Disable functions
void TSL2591::Disable_tcaselect1() {
	Wire.beginTransmission(0x70);
	Wire.write(0);
	Wire.endTransmission();
}
void TSL2591::Disable_tcaselect2() {
	Wire.beginTransmission(0x77);
	Wire.write(0);
	Wire.endTransmission();
}


// Building block functions

boolean TSL2591::begin() {
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
void TSL2591::enable(void) {
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
void TSL2591::disable(void) {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}
	write8(TSL2591_COMMAND_BIT | TSL2591_REGISTER_ENABLE,
		TSL2591_ENABLE_POWEROFF);
}
void TSL2591::setGain(tsl2591Gain_t gain) {
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
tsl2591Gain_t TSL2591::getGain() { return _gain; }

void TSL2591::setTiming(tsl2591IntegrationTime_t integration) {
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

tsl2591IntegrationTime_t TSL2591::getTiming() { return _integration; }

float TSL2591::calculateLux(uint16_t ch0, uint16_t ch1) {
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

uint32_t TSL2591::getFullLuminosity(void) {

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

uint16_t TSL2591::getLuminosity(uint8_t channel) {
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

void TSL2591::registerInterrupt(
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
void TSL2591::clearInterrupt() {
	if (!_initialized) {
		if (!begin()) {
			return;
		}
	}

	enable();
	write8(TSL2591_CLEAR_INT);
	disable();
}

uint8_t TSL2591::getStatus(void) {
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

void TSL2591::fileConfig(void)
{
  snprintf(fileName, sizeof(fileName), "data%d:%d:%d_%d/%d/%d.csv", hour(),minute(),second(),day(),month(), year()); // includes a three-digit sequence number in the file name
  dataFile.open(fileName, O_RDWR | O_CREAT | O_AT_END);
  dataFile.print("milliseconds");
  dataFile.print("\tIR Luminosity");
  dataFile.print("\tFull Luminosity");
  dataFile.print("\tVisible Luminosity");
  dataFile.print("\tLux");
  dataFile.println(""); // new line for subsequent data output
  dataFile.close();
}

//Reffered functions

/*
THIS IS THE FUNCTIONS STUDENTS MUST INTERACT WITH
*/

void TSL2591::simpleRead(int row, int column)
{
  // Simple data read example. Just read the infrared, fullspecrtrum diode 
  // or 'visible' (difference between the two) channels.
  // This can take 100-600 milliseconds! Uncomment whichever of the following you want to read
 	Wire.begin();
   	if(row == 1 || row == 2){
		if (row ==1){tcaselect1(column);}
		if (row == 2){tcaselect1(column+4);}
	}
	if(row == 3 || row == 4){
		if (row ==3){tcaselect2(column);}
		if (row == 4){tcaselect2(column+4);}
	}
 	uint16_t x = getLuminosity(TSL2591_VISIBLE);
	//uint16_t x = getLuminosity(TSL2591_FULLSPECTRUM);
	//uint16_t x = getLuminosity(TSL2591_INFRARED);

	sro.ms = millis();
	sro.lum = x;
}


void TSL2591::advancedRead(int row, int column)
{
	Wire.begin();
	if (row == 1 || row == 2) {
		if (row == 1) { tcaselect1(column); }
		if (row == 2) { tcaselect1(column + 4); }
	}
	if (row == 3 || row == 4) {
		if (row == 3) { tcaselect2(column); }
		if (row == 4) { tcaselect2(column + 4); }
	}
	// More advanced data read example. Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
	// That way you can do whatever math and comparisons you want!
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

void TSL2591::saveSD(){
	sd.begin();
	fileConfig();
	dataFile.open(fileName, O_RDWR | O_CREAT | O_AT_END);
	for(int i = 0; i<4; i++){
		for(int j = 0; j<4; j++){
			advancedRead(i,j);
			dataFile.print(aro.ms, 3);
			dataFile.print("\t");
			dataFile.print(aro.ir, 3);
			dataFile.print("\t");
			dataFile.print(aro.full, 3);
			dataFile.print("\t");
			dataFile.print(aro.vis, 3);
			dataFile.print("\t");
			dataFile.print(aro.lux, 3);
			dataFile.println("");
		}
	}
	dataFile.close();
}


void simleReadMatrix(){
	/*for(int i = 0; i<4; i++){
		for(int j = 0; j<4; j++){
			simpleRead(i,j);
			sroArray[i][j] = sro.lum;
		}
	}*/
} 



// Read/write code
uint8_t TSL2591::read8(uint8_t reg) {
	uint8_t value = 0;
	Wire.beginTransmission(_aAddress);
	Wire.send(reg);
	Wire.endTransmission();
	Wire.requestFrom(_aAddress, (byte)1);
	value = Wire.readByte();
	return value;
}
uint16_t TSL2591::read16(uint8_t reg) {
	uint8_t buffer[2];
	uint16_t value = 0;
	Wire.beginTransmission(_aAddress);\
	Wire.send(reg);
	Wire.endTransmission();
	Wire.requestFrom(_aAddress, (byte)2);
	buffer[0] = Wire.readByte();
	buffer[1] = Wire.readByte();
	return uint16_t(buffer[1]) << 8 | uint16_t(buffer[0]);
}
void TSL2591::write8(uint8_t reg, uint8_t value) {
	Wire.beginTransmission(_aAddress); //// MODIFIED RB
	Wire.send(reg); /// TO BE MODIFIED? NO
	Wire.send(value); /// TO BE MODIFIED? NO
	Wire.endTransmission(); 
}

void TSL2591::write8(uint8_t reg) {
	Wire.beginTransmission(_aAddress); 
	Wire.send(reg);
	Wire.endTransmission();
}