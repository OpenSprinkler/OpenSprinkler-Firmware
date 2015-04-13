// -------------------------------------------------------------------------------------------
// Port for CubieBoard a library LiquidCrystal version 1.2.1 to drive the PCF8574* I2C
//
// @version API 1.0.0
//
// @author Kabanov Andrew - andrew3007@kabit.ru
// -------------------------------------------------------------------------------------------

#include "I2CDeviceLCD.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <linux/i2c-dev.h>
#include <linux/limits.h>

// ---------------------------------------------------------------------------------------------------------------

I2CDeviceLCD::I2CDeviceLCD()
   : fileDesc(-1),
	 _initialised(false), _shadow(0x0), _dirMask(0xFF)
{
}

I2CDeviceLCD::~I2CDeviceLCD()
{
	close();
}

bool I2CDeviceLCD::open(uint8_t i2cNumBus, uint8_t i2cDevAddr)
{
	if( _initialised )
	{
		fprintf(stderr, "File i2c device is already open.");
		return false;
	}

	char nameFile[PATH_MAX];
	snprintf(nameFile, PATH_MAX, "/dev/i2c/%d", i2cNumBus);
	nameFile[PATH_MAX - 1] = '\0';

	fileDesc = ::open(nameFile, O_WRONLY);
	if( (fileDesc < 0) && (errno == ENOENT || errno == ENOTDIR) )
	{
		snprintf(nameFile, PATH_MAX, "/dev/i2c-%d", i2cNumBus);
		nameFile[PATH_MAX - 1] = '\0';
		fileDesc = ::open(nameFile, O_WRONLY);
	}

	if( fileDesc < 0 )
	{
		if( errno == ENOENT )
		{
			fprintf(stderr, "Error: Could not open file `/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
							i2cNumBus, i2cNumBus, strerror(ENOENT));
		}
		else
		{
			fprintf(stderr, "Error: Could not open file `%s': %s\n", nameFile, strerror(errno));
			if( errno == EACCES )
			{
				fprintf(stderr, "Run as root?\n");
			}
		}

		return false;
	}

	if( ioctl(fileDesc, I2C_SLAVE, i2cDevAddr) < 0 )
	{
		fputs("Failed to acquire bus access and/or talk to slave.\n", stderr);
		return false;
	}

	_initialised = true;
	return true;
}

void I2CDeviceLCD::close()
{
	if( _initialised )
	{
		::close(fileDesc);
		fileDesc = -1;
		_initialised = false;
	}
}

// ----------------------------------------------------------- PinMode ---------------------------------------------------------------

//
// pinMode
void I2CDeviceLCD::pinMode(uint8_t pin, uint8_t dir)
{
	if( _initialised )
	{
		if( OUTPUT == dir )
		{
			_dirMask &= ~( 1 << pin );
		}
		else
		{
			_dirMask |= ( 1 << pin );
		}
	}
}

// ------------------------------------------------- Write to I2C-device ---------------------------------------------------

//
// digitalWrite
ssize_t I2CDeviceLCD::digitalWrite(uint8_t pin, uint8_t level)
{
	uint8_t writeVal;
	ssize_t ret = -1;

	// Check if initialised and that the pin is within range of the device
	// -------------------------------------------------------------------
	if( _initialised && (pin <= 7) )
	{
		// Only write to HIGH the port if the port has been configured as
		// an OUTPUT pin. Add the new state of the pin to the shadow
		writeVal = (1 << pin) & ~_dirMask;
		if( level == HIGH )
		{
			_shadow |= writeVal;
		}
		else
		{
			_shadow &= ~writeVal;
		}
		ret = writeByte(_shadow);
	}

	return ret;
}

//
// writeByte
ssize_t I2CDeviceLCD::writeByte(uint8_t value)
{
	ssize_t ret = -1;

	if( _initialised )
	{
		// Only write HIGH the values of the ports that have been initialised as
		// outputs updating the output shadow of the device
		_shadow = ( value & ~(_dirMask) );

		const char* byte = reinterpret_cast<const char*>(&_shadow);
		ret = write(fileDesc, byte, 1);
	}

	return ret;
}
