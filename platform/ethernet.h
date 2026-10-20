#pragma once

#include <stdint.h>
#include "external/ch390/include/ch390_health.h"

enum class EthernetController : uint8_t {
	None = 0,
	ENC28J60,
	W5500,
	CH390,
};

#if defined(ARDUINO)

#include <Arduino.h>
#include <IPAddress.h>
#include <SPI.h>

class WiredEthernet {
public:
	void select(EthernetController controller);
	EthernetController controller() const { return _controller; }
	const char* controller_name() const;
	bool is_w5500() const { return _controller == EthernetController::W5500; }
	bool is_ch390() const { return _controller == EthernetController::CH390; }
	bool ch390_diagnostics(CH390Diagnostics& diagnostics) const;
	bool service_ch390();
	void end();

	bool config(const IPAddress& local_ip, const IPAddress& gateway,
		const IPAddress& subnet, const IPAddress& dns);

#if defined(ESP8266)
	bool raw_begin(const uint8_t* mac_address);
	bool begin(const uint8_t* mac_address);
	bool fault_pending() const;
	bool health_check();
	bool raw_reinit();
#elif defined(ESP32)
#if defined(OS_ETH_DIAGNOSTICS)
	void diagnostic_poll();
#endif
	void set_spi_drive(EthernetController controller);
	bool begin(SPIClass& spi, int8_t cs, int8_t irq, int8_t reset,
		uint32_t spi_clock_hz);
#endif

	IPAddress localIP() const;
	IPAddress subnetMask() const;
	IPAddress gatewayIP() const;
	IPAddress dnsIP() const;
	bool setDefault();
	bool connected() const;
	bool linkUp() const;
	bool macAddress(uint8_t* address) const;
	uint32_t spi_clock() const { return _spi_clock_hz; }

private:
	EthernetController _controller = EthernetController::None;
	uint32_t _spi_clock_hz = 0;
};

extern WiredEthernet eth;

#endif // ARDUINO
