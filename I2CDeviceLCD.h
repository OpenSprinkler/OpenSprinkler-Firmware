// -------------------------------------------------------------------------------------------
// Port for CubieBoard a library LiquidCrystal version 1.2.1 to drive the PCF8574* I2C
//
// @version API 1.0.0
//
// @author Kabanov Andrew - andrew3007@kabit.ru
// -------------------------------------------------------------------------------------------

#ifndef I2C_DEVICE_LCD_H
#define I2C_DEVICE_LCD_H

#include <unistd.h>
#include <stdint.h>

// Modes data exchange
#define LCD_4BIT  1
#define LCD_8BIT  0

// Direct data exchange
#define INPUT 0
#define OUTPUT 1

//Level
#define HIGH 1
#define LOW  0

//Backlight
#define LCD_NOBACKLIGHT 0xFF

class I2CDeviceLCD
{
	public:
		I2CDeviceLCD();
		~I2CDeviceLCD();

	protected:
		bool open(uint8_t i2cNumBus, uint8_t i2cDevAddr);
		void close();

		bool init(uint8_t i2cNumBus, uint8_t i2cDevAddr,
				  uint8_t fourbitMode, uint8_t rs, uint8_t rw, uint8_t enable,
				  uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3,
				  uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7);

		void pinMode(uint8_t pin, uint8_t dir);

		ssize_t digitalWrite(uint8_t pin, uint8_t level);

	private:
		int fileDesc;       // File descriptor
		bool _initialised;  // Initialised object
		uint8_t _shadow;    // Shadow output
		uint8_t _dirMask;   // Direction mask

		ssize_t writeByte(uint8_t value);
};

#endif
