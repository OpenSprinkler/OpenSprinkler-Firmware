#include "platform/ethernet.h"

#if defined(ARDUINO)

#include "defines.h"

#if defined(ESP8266)

#include <ENC28J60lwIP.h>
#include <W5500lwIP.h>
#include <lwip/dns.h>

#include "drivers/ch390_raw.h"

namespace {

ENC28J60lwIP enc28j60(PIN_ETHER_CS);
Wiznet5500lwIP w5500(PIN_ETHER_CS);
CH390lwIP ch390(PIN_ETHER_CS);

} // namespace

#elif defined(ESP32)

#include <ETH.h>
#include <Network.h>
#include <NetworkInterface.h>
#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_eth.h>
#include <esp_eth_mac.h>
#include <esp_eth_mac_ch390.h>
#include <esp_eth_phy.h>
#include <esp_eth_phy_ch390.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_netif_defaults.h>

namespace {

class CH390NetworkInterface : public NetworkInterface {
public:
	bool begin(SPIClass& spi, int8_t cs, int8_t irq, int8_t reset,
		uint32_t spi_clock_hz);
	void end();

protected:
	size_t printDriverInfo(Print& out) const override {
		return out.print("CH390");
	}

private:
	static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data);
	static void* spi_init(const void* context);
	static esp_err_t spi_deinit(void* context);
	static esp_err_t spi_read(void* context, uint32_t command,
		uint32_t address, void* data, uint32_t length);
	static esp_err_t spi_write(void* context, uint32_t command,
		uint32_t address, const void* data, uint32_t length);

	esp_err_t transfer(bool write, uint32_t command, uint32_t address,
		void* data, uint32_t length);

	SPIClass* _spi = nullptr;
	int8_t _cs = -1;
	uint32_t _spi_clock_hz = 0;
	esp_eth_handle_t _handle = nullptr;
	esp_eth_mac_t* _mac = nullptr;
	esp_eth_phy_t* _phy = nullptr;
	esp_eth_netif_glue_handle_t _glue = nullptr;
	esp_event_handler_instance_t _event_instance = nullptr;
};

CH390NetworkInterface ch390;

void CH390NetworkInterface::event_handler(void* arg, esp_event_base_t event_base,
	int32_t event_id, void* event_data) {
	CH390NetworkInterface* interface = static_cast<CH390NetworkInterface*>(arg);
	if (!interface || event_base != ETH_EVENT || !event_data) return;
	esp_eth_handle_t handle = *static_cast<esp_eth_handle_t*>(event_data);
	if (handle != interface->_handle) return;

	switch (event_id) {
	case ETHERNET_EVENT_START:
		interface->setStatusBits(ESP_NETIF_STARTED_BIT);
		break;
	case ETHERNET_EVENT_CONNECTED:
		interface->setStatusBits(ESP_NETIF_CONNECTED_BIT);
		break;
	case ETHERNET_EVENT_DISCONNECTED:
		interface->clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT |
			ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
		break;
	case ETHERNET_EVENT_STOP:
		interface->clearStatusBits(ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT |
			ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT |
			ESP_NETIF_HAS_GLOBAL_IP6_BIT | ESP_NETIF_HAS_STATIC_IP_BIT);
		break;
	default:
		break;
	}
}

void* CH390NetworkInterface::spi_init(const void* context) {
	return const_cast<void*>(context);
}

esp_err_t CH390NetworkInterface::spi_deinit(void* context) {
	(void)context;
	return ESP_OK;
}

esp_err_t CH390NetworkInterface::spi_read(void* context, uint32_t command,
	uint32_t address, void* data, uint32_t length) {
	return static_cast<CH390NetworkInterface*>(context)->transfer(
		false, command, address, data, length);
}

esp_err_t CH390NetworkInterface::spi_write(void* context, uint32_t command,
	uint32_t address, const void* data, uint32_t length) {
	return static_cast<CH390NetworkInterface*>(context)->transfer(
		true, command, address, const_cast<void*>(data), length);
}

esp_err_t CH390NetworkInterface::transfer(bool write, uint32_t command,
	uint32_t address, void* data, uint32_t length) {
	if (!_spi || _cs < 0 || (!data && length != 0)) return ESP_ERR_INVALID_STATE;
	_spi->beginTransaction(SPISettings(_spi_clock_hz, MSBFIRST, SPI_MODE0));
	digitalWrite(_cs, LOW);
	_spi->write(((command & 0x01) << 7) | (address & 0x7F));
	if (write) {
		_spi->writeBytes(static_cast<const uint8_t*>(data), length);
	} else {
		_spi->transferBytes(nullptr, static_cast<uint8_t*>(data), length);
	}
	digitalWrite(_cs, HIGH);
	_spi->endTransaction();
	return ESP_OK;
}

bool CH390NetworkInterface::begin(SPIClass& spi, int8_t cs, int8_t irq,
	int8_t reset, uint32_t spi_clock_hz) {
	if (_esp_netif) return true;
	_spi = &spi;
	_cs = cs;
	_spi_clock_hz = spi_clock_hz;
	pinMode(_cs, OUTPUT);
	digitalWrite(_cs, HIGH);

	Network.begin();
	esp_err_t result = gpio_install_isr_service(0);
	if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return false;

	eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
	eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
	phy_config.phy_addr = 1;
	phy_config.reset_gpio_num = reset;
	phy_config.hw_reset_assert_time_us = 10000;
	phy_config.post_hw_reset_delay_ms = 25;

	eth_ch390_config_t ch390_config = {};
	ch390_config.int_gpio_num = irq;
	ch390_config.poll_period_ms = irq < 0 ? 10 : 0;
	ch390_config.custom_spi_driver.config = this;
	ch390_config.custom_spi_driver.init = spi_init;
	ch390_config.custom_spi_driver.deinit = spi_deinit;
	ch390_config.custom_spi_driver.read = spi_read;
	ch390_config.custom_spi_driver.write = spi_write;

	_mac = esp_eth_mac_new_ch390(&ch390_config, &mac_config);
	_phy = esp_eth_phy_new_ch390(&phy_config);
	if (!_mac || !_phy) {
		end();
		return false;
	}

	esp_eth_config_t driver_config = ETH_DEFAULT_CONFIG(_mac, _phy);
	if (esp_eth_driver_install(&driver_config, &_handle) != ESP_OK || !_handle) {
		end();
		return false;
	}

	uint8_t base_mac[ETH_ADDR_LEN];
	uint8_t ethernet_mac[ETH_ADDR_LEN];
	if (esp_read_mac(base_mac, ESP_MAC_ETH) != ESP_OK) {
		end();
		return false;
	}
	esp_derive_local_mac(ethernet_mac, base_mac);
	if (esp_eth_ioctl(_handle, ETH_CMD_S_MAC_ADDR, ethernet_mac) != ESP_OK) {
		end();
		return false;
	}

	esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
	_esp_netif = esp_netif_new(&netif_config);
	if (!_esp_netif) {
		end();
		return false;
	}
	_glue = esp_eth_new_netif_glue(_handle);
	if (!_glue || esp_netif_attach(_esp_netif, _glue) != ESP_OK ||
		!initNetif(ESP_NETIF_ID_ETH0)) {
		end();
		return false;
	}
	if (esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID,
		event_handler, this, &_event_instance) != ESP_OK) {
		end();
		return false;
	}
	if (esp_eth_start(_handle) != ESP_OK) {
		end();
		return false;
	}
	return true;
}

void CH390NetworkInterface::end() {
	if (_handle) esp_eth_stop(_handle);
	if (_event_instance) {
		esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID,
			_event_instance);
		_event_instance = nullptr;
	}
	if (_glue) {
		esp_eth_del_netif_glue(_glue);
		_glue = nullptr;
	}
	if (_handle) {
		esp_eth_driver_uninstall(_handle);
		_handle = nullptr;
	}
	if (_mac) {
		_mac->del(_mac);
		_mac = nullptr;
	}
	if (_phy) {
		_phy->del(_phy);
		_phy = nullptr;
	}
	destroyNetif();
	_spi = nullptr;
	_cs = -1;
	_spi_clock_hz = 0;
}

} // namespace

#endif // ESP32

WiredEthernet eth;

void WiredEthernet::select(EthernetController controller) {
	_controller = controller;
	_spi_clock_hz = controller == EthernetController::CH390 ?
		osboard::CH390_SPI_CLOCK_HZ : ETHER_SPI_CLOCK;
}

const char* WiredEthernet::controller_name() const {
	switch (_controller) {
	case EthernetController::ENC28J60: return "enc28j60";
	case EthernetController::W5500: return "w5500";
	case EthernetController::CH390: return "ch390";
	default: return "none";
	}
}

bool WiredEthernet::config(const IPAddress& local_ip, const IPAddress& gateway,
	const IPAddress& subnet, const IPAddress& dns) {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60:
		return enc28j60.config(local_ip, gateway, subnet, dns);
	case EthernetController::W5500:
		return w5500.config(local_ip, gateway, subnet, dns);
	case EthernetController::CH390:
		return ch390.config(local_ip, gateway, subnet, dns);
	default:
		return false;
	}
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ?
		ch390.config(local_ip, gateway, subnet, dns) :
		_controller == EthernetController::W5500 ?
		ETH.config(local_ip, gateway, subnet, dns) : false;
#endif
}

#if defined(ESP8266)
bool WiredEthernet::begin(const uint8_t* mac_address) {
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.begin(mac_address);
	case EthernetController::W5500: return w5500.begin(mac_address);
	case EthernetController::CH390: return ch390.begin(mac_address);
	default: return false;
	}
}
#elif defined(ESP32)
void WiredEthernet::set_spi_drive(EthernetController controller) {
	// CH390 traffic on the OS4 shared-bus path requires slower C6 output edges.
	const gpio_drive_cap_t drive = controller == EthernetController::CH390 ?
		GPIO_DRIVE_CAP_0 : GPIO_DRIVE_CAP_DEFAULT;
	(void)gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_SPI_SCK), drive);
	(void)gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_SPI_MOSI), drive);
}

bool WiredEthernet::begin(SPIClass& spi, int8_t cs, int8_t irq, int8_t reset,
	uint32_t spi_clock_hz) {
	_spi_clock_hz = spi_clock_hz;
	if (_controller == EthernetController::CH390) {
		return ch390.begin(spi, cs, irq, reset, spi_clock_hz);
	}
	if (_controller == EthernetController::W5500) {
		return ETH.begin(ETH_PHY_W5500, 1, cs, irq, reset, spi,
			(uint8_t)(spi_clock_hz / 1000000UL));
	}
	return false;
}
#endif

IPAddress WiredEthernet::localIP() const {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.localIP();
	case EthernetController::W5500: return w5500.localIP();
	case EthernetController::CH390: return ch390.localIP();
	default: return IPAddress();
	}
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.localIP() : ETH.localIP();
#endif
}

IPAddress WiredEthernet::subnetMask() const {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.subnetMask();
	case EthernetController::W5500: return w5500.subnetMask();
	case EthernetController::CH390: return ch390.subnetMask();
	default: return IPAddress();
	}
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.subnetMask() : ETH.subnetMask();
#endif
}

IPAddress WiredEthernet::gatewayIP() const {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.gatewayIP();
	case EthernetController::W5500: return w5500.gatewayIP();
	case EthernetController::CH390: return ch390.gatewayIP();
	default: return IPAddress();
	}
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.gatewayIP() : ETH.gatewayIP();
#endif
}

IPAddress WiredEthernet::dnsIP() const {
#if defined(ESP8266)
	return IPAddress(dns_getserver(0));
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.dnsIP() : ETH.dnsIP();
#endif
}

bool WiredEthernet::setDefault() {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60: enc28j60.setDefault(); return true;
	case EthernetController::W5500: w5500.setDefault(); return true;
	case EthernetController::CH390: ch390.setDefault(); return true;
	default: return false;
	}
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.setDefault() :
		_controller == EthernetController::W5500 ? ETH.setDefault() : false;
#endif
}

bool WiredEthernet::connected() const {
#if defined(ESP8266)
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.connected();
	case EthernetController::W5500: return w5500.connected();
	case EthernetController::CH390: return ch390.connected();
	default: return false;
	}
#elif defined(ESP32)
	return linkUp() && (uint32_t)localIP() != 0;
#endif
}

bool WiredEthernet::linkUp() const {
#if defined(ESP8266)
	return connected();
#elif defined(ESP32)
	return _controller == EthernetController::CH390 ? ch390.linkUp() :
		_controller == EthernetController::W5500 ? ETH.linkUp() : false;
#endif
}

bool WiredEthernet::macAddress(uint8_t* address) const {
	if (!address) return false;
#if defined(ESP8266)
	return false;
#elif defined(ESP32)
	return (_controller == EthernetController::CH390 ? ch390.macAddress(address) :
		_controller == EthernetController::W5500 ? ETH.macAddress(address) : nullptr) != nullptr;
#endif
}

#endif // ARDUINO
