#include "platform/ethernet.h"

#if defined(ARDUINO)

#include "defines.h"
#include "platform/hostname.h"

#if defined(ESP8266)

#include <ENC28J60lwIP.h>
#include <lwip/dns.h>

#include "drivers/ch390_raw.h"
#include "drivers/os_w5500.h"

OSWiznet5500lwIP w5500(PIN_ETHER_CS);

namespace {

// The core ENC28J60 driver has no end(). ECON1 is common to all banks;
// clear RXEN using Bit Field Clear without changing the driver's bank cache.
class StoppableENC28J60 : public ENC28J60 {
public:
	StoppableENC28J60(int8_t cs, SPIClass& spi = SPI, int8_t intr = -1)
		: ENC28J60(cs, spi, intr), cs_(cs), spi_(spi) {}
	void end() {
		spi_.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
		digitalWrite(cs_, LOW);
		spi_.transfer(0xa0 | 0x1f);
		spi_.transfer(0x04);
		digitalWrite(cs_, HIGH);
		spi_.endTransaction();
	}
private:
	int8_t cs_;
	SPIClass& spi_;
};

ManagedEthernet<StoppableENC28J60> enc28j60(PIN_ETHER_CS);
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
#if defined(OS_ETH_DIAGNOSTICS)
#include <esp_netif_net_stack.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <lwip/priv/tcpip_priv.h>
#endif

namespace {

class CH390NetworkInterface : public NetworkInterface {
public:
	bool begin(SPIClass& spi, int8_t cs, int8_t irq, int8_t reset,
		uint32_t spi_clock_hz);
	void end();
#if defined(OS_ETH_DIAGNOSTICS)
	esp_err_t diagnostic_set_running(bool running) {
		if (!_handle) return ESP_ERR_INVALID_STATE;
		return running ? esp_eth_start(_handle) : esp_eth_stop(_handle);
	}
	bool diagnostic_traffic(CH390TrafficDiag& out) const {
		return _mac && esp_eth_mac_ch390_traffic(_mac, &out) == ESP_OK;
	}
	esp_err_t diagnostic_regs(const uint8_t* addrs, uint8_t* values, size_t count) const {
		return _mac ? esp_eth_mac_ch390_read_regs(_mac, addrs, values, count) : ESP_ERR_INVALID_STATE;
	}
	esp_err_t diagnostic_restart_rx() {
		return _mac ? esp_eth_mac_ch390_restart_rx(_mac) : ESP_ERR_INVALID_STATE;
	}
	esp_err_t diagnostic_phy(uint32_t reg, uint32_t* value, bool write) {
		if (!_handle) return ESP_ERR_INVALID_STATE;
		esp_eth_phy_reg_rw_data_t data = { .reg_addr = reg, .reg_value_p = value };
		return esp_eth_ioctl(_handle, write ? ETH_CMD_WRITE_PHY_REG : ETH_CMD_READ_PHY_REG, &data);
	}
	esp_err_t diagnostic_transmit(uint8_t* frame, size_t length) {
		return _handle ? esp_eth_transmit(_handle, frame, length) : ESP_ERR_INVALID_STATE;
	}
#endif
	bool diagnostics(CH390Diagnostics& out) const {
		return _mac && esp_eth_mac_ch390_diagnostics(_mac, &out) == ESP_OK;
	}

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
	mac_config.sw_reset_timeout_ms = 100;
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
	if (!setHostname(hostname_effective())) {
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

bool WiredEthernet::ch390_diagnostics(CH390Diagnostics& out) const {
	if (!is_ch390()) return false;
#if defined(ESP8266)
	out = ch390.diagnostics();
	return true;
#elif defined(ESP32)
	return ch390.diagnostics(out);
#endif
}

bool WiredEthernet::service_ch390() {
#if defined(ESP8266)
	return is_ch390() && ch390.active() && ch390.service();
#else
	return false; // ESP32 services recovery inside the serialized MAC worker.
#endif
}

#if defined(ESP8266)
bool WiredEthernet::raw_begin(const uint8_t* mac_address) {
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.raw_begin(mac_address);
	case EthernetController::W5500: return w5500.raw_begin(mac_address);
	case EthernetController::CH390: return ch390.raw_begin(mac_address);
	default: return false;
	}
}

void WiredEthernet::end() {
	switch (_controller) {
	case EthernetController::ENC28J60: enc28j60.end(); break;
	case EthernetController::W5500: w5500.end(); break;
	case EthernetController::CH390: ch390.end(); break;
	default: break;
	}
}

bool WiredEthernet::begin(const uint8_t* mac_address) {
	// The core's begin() starts DHCP itself; assign its netif name beforehand.
	// getNetIf exposes a const view, but these are our mutable driver instances.
	switch (_controller) {
	case EthernetController::ENC28J60:
		const_cast<netif*>(enc28j60.getNetIf())->hostname = hostname_effective();
		return enc28j60.begin(mac_address);
	case EthernetController::W5500:
		const_cast<netif*>(w5500.getNetIf())->hostname = hostname_effective();
		return w5500.begin(mac_address);
	case EthernetController::CH390:
		const_cast<netif*>(ch390.getNetIf())->hostname = hostname_effective();
		return ch390.begin(mac_address);
	default: return false;
	}
}

bool WiredEthernet::fault_pending() const {
	return is_w5500() && w5500.active() && w5500.faultPending();
}

bool WiredEthernet::health_check() {
	return !is_w5500() || (w5500.active() && w5500.healthCheck());
}

bool WiredEthernet::raw_reinit() {
	return is_w5500() && w5500.active() && w5500.reinitialize();
}
#elif defined(ESP32)
#if defined(OS_ETH_DIAGNOSTICS)
namespace {
struct DhcpSnapshot {
	tcpip_api_call_data call;
	struct netif* interface;
	uint32_t ip, offered, server, xid;
	unsigned flags, state, tries, timeout;
};

// Inspect lwIP only on its own thread; printing happens back in the main loop.
static err_t snapshot_dhcp(tcpip_api_call_data* call) {
	auto* s = reinterpret_cast<DhcpSnapshot*>(call);
	s->ip = ip4_addr_get_u32(netif_ip4_addr(s->interface));
	s->flags = s->interface->flags;
	auto* d = netif_dhcp_data(s->interface);
	if (d) {
		s->state = d->state;
		s->tries = d->tries;
		s->timeout = d->request_timeout;
		s->xid = d->xid;
		s->offered = ip4_addr_get_u32(&d->offered_ip_addr);
		s->server = ip4_addr_get_u32(ip_2_ip4(&d->server_ip_addr));
	}
	return ERR_OK;
}

static volatile bool dump_requested = true;

static void diagnostic_event(void*, esp_event_base_t base, int32_t id, void*) {
	Serial.printf("[ETHDIAG %lu] event %s id=%ld\n",
		(unsigned long)millis(), base == ETH_EVENT ? "ETH" : "IP", (long)id);
	dump_requested = true;
}

// MAC registers worth comparing between healthy and stuck states. ROCR (0x07)
// is read-to-clear: each dump shows overflows since the previous dump.
const uint8_t kDumpRegs[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0F,
	0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x22, 0x23, 0x24, 0x25, 0x2D, 0x31, 0x32, 0x39, 0x4A, 0x50, 0x52, 0x53,
	0x55, 0x57, 0x74, 0x75, 0x7A, 0x7B, 0x7E, 0x7F, 0x70, 0x70,
};

static void dump_registers(const char* reason) {
	uint8_t v[sizeof(kDumpRegs)] = {};
	esp_err_t rc = ch390.diagnostic_regs(kDumpRegs, v, sizeof(v));
	Serial.printf("[ETHDIAG %lu] regs(%s) rc=%d", (unsigned long)millis(), reason, rc);
	for (size_t i = 0; i < sizeof(v); ++i) Serial.printf(" %02x=%02x", kDumpRegs[i], v[i]);
	Serial.println();
	Serial.printf("[ETHDIAG %lu] phy", (unsigned long)millis());
	for (uint32_t r = 0; r < 32; ++r) {
		if (r == 7) r = 16;
		uint32_t value = 0;
		esp_err_t e = ch390.diagnostic_phy(r, &value, false);
		if (e == ESP_OK) Serial.printf(" %02lx=%04lx", (unsigned long)r, (unsigned long)value);
		else Serial.printf(" %02lx=err%d", (unsigned long)r, e);
	}
	Serial.println();
}

static void put32(uint8_t* p, uint32_t v) {
	p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

// Raw IPv4/UDP broadcast 0.0.0.0:40002 -> 255.255.255.255:40002, independent of
// netif/DHCP state, so an observer can tell whether frames leave the module.
static esp_err_t send_probe(const uint8_t* mac, uint32_t seq, const CH390TrafficDiag& t) {
	uint8_t f[14 + 20 + 8 + 20] = {};
	memset(f, 0xff, 6);
	memcpy(f + 6, mac, 6);
	f[12] = 0x08;
	uint8_t* ip = f + 14;
	ip[0] = 0x45; ip[2] = 0; ip[3] = 48; ip[8] = 64; ip[9] = 17;
	memset(ip + 16, 0xff, 4);
	uint32_t sum = 0;
	for (int i = 0; i < 20; i += 2) sum += (ip[i] << 8) | ip[i + 1];
	while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
	ip[10] = ~sum >> 8; ip[11] = ~sum & 0xff;
	uint8_t* udp = ip + 20;
	udp[0] = udp[2] = 40002 >> 8; udp[1] = udp[3] = 40002 & 0xff; udp[5] = 28;
	uint8_t* payload = udp + 8;
	memcpy(payload, "OSDG", 4);
	put32(payload + 4, seq);
	put32(payload + 8, millis());
	put32(payload + 12, t.rx_frames);
	put32(payload + 16, t.rx_probe);
	return ch390.diagnostic_transmit(f, sizeof(f));
}
}

void WiredEthernet::diagnostic_poll() {
	static bool initialized = false;
	static uint32_t last = 0, last_dump = 0, probe_seq = 0;
	static char command[24];
	static size_t command_length = 0;
	static bool overflow = false;
	static bool restart_pending = false;
	static uint32_t stopped_at = 0;
	if (!initialized) {
		Serial.begin(115200);
		esp_err_t e = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, diagnostic_event, nullptr);
		esp_err_t i = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, diagnostic_event, nullptr);
		Serial.printf("[ETHDIAG] enabled; event registration eth=%d ip=%d\n", e, i);
		initialized = true;
	}
	if (restart_pending && (uint32_t)(millis() - stopped_at) >= 1000) {
		restart_pending = false;
		Serial.printf("[ETHDIAG %lu] eth-start rc=%d\n",
			(unsigned long)millis(), ch390.diagnostic_set_running(true));
	}
	// Exact USB-only commands, excluded from production builds. Never auto-recover.
	for (unsigned count = 0; count < 64 && Serial.available(); ++count) {
		char c = (char)Serial.read();
		if (c == '\r') continue;
		if (c != '\n') {
			if (command_length < sizeof(command) - 1) command[command_length++] = c;
			else overflow = true;
			continue;
		}
		command[command_length] = 0;
		bool ch = !overflow && !restart_pending && is_ch390();
		if (!overflow && !restart_pending && !strcmp(command, "diag-dhcp")) {
			auto* netif = is_ch390() ? ch390.netif() : is_w5500() ? ETH.netif() : nullptr;
			if (netif) {
				esp_err_t stop = esp_netif_dhcpc_stop(netif);
				esp_err_t start = (stop == ESP_OK || stop == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
					? esp_netif_dhcpc_start(netif) : ESP_ERR_INVALID_STATE;
				Serial.printf("[ETHDIAG %lu] dhcp-restart stop=%d start=%d\n",
					(unsigned long)millis(), stop, start);
			}
		} else if (ch && !strcmp(command, "diag-eth")) {
			esp_err_t result = ch390.diagnostic_set_running(false);
			Serial.printf("[ETHDIAG %lu] eth-stop rc=%d\n", (unsigned long)millis(), result);
			if (result == ESP_OK) {
				stopped_at = millis();
				restart_pending = true;
			}
		} else if (ch && !strcmp(command, "diag-regs")) {
			dump_requested = true;
		} else if (ch && !strcmp(command, "diag-rx")) {
			// MAC only: RX disable, RX pointer reset, ISR clear, RX enable.
			Serial.printf("[ETHDIAG %lu] rx-restart rc=%d\n",
				(unsigned long)millis(), ch390.diagnostic_restart_rx());
			dump_requested = true;
		} else if (ch && !strcmp(command, "diag-aneg")) {
			// PHY only: restart auto-negotiation; driver link events follow.
			uint32_t bmcr = 0;
			esp_err_t r = ch390.diagnostic_phy(0, &bmcr, false);
			uint32_t next = bmcr | 0x1200;
			esp_err_t w = r == ESP_OK ? ch390.diagnostic_phy(0, &next, true) : r;
			Serial.printf("[ETHDIAG %lu] aneg-restart bmcr=%04lx read=%d write=%d\n",
				(unsigned long)millis(), (unsigned long)bmcr, r, w);
		} else {
			Serial.println("[ETHDIAG] rejected command; use diag-dhcp, diag-eth, diag-regs, diag-rx or diag-aneg");
		}
		command_length = 0;
		overflow = false;
	}
	if (is_ch390() && (dump_requested || (uint32_t)(millis() - last_dump) >= 30000)) {
		dump_requested = false;
		last_dump = millis();
		dump_registers("auto");
	}
	if ((uint32_t)(millis() - last) < 5000) return;
	last = millis();
	auto* n = is_ch390() ? ch390.netif() : is_w5500() ? ETH.netif() : nullptr;
	if (!n) return;
	DhcpSnapshot s = {};
	s.state = 255;
	s.interface = static_cast<struct netif*>(esp_netif_get_netif_impl(n));
	if (!s.interface) return;
	esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
	esp_err_t status_result = esp_netif_dhcpc_get_status(n, &status);
	err_t snapshot_result = tcpip_api_call(snapshot_dhcp, &s.call);
	CH390Diagnostics d = {};
	bool valid = ch390_diagnostics(d);
	Serial.printf("[ETHDIAG %lu] %s link=%u up=%u ip=%s flags=0x%x dhcp_status=%d status_rc=%d state=%u tries=%u timeout=%u xid=%08lx offered=%s server=%s snapshot_rc=%d\n",
		(unsigned long)last, controller_name(), linkUp(), esp_netif_is_netif_up(n),
		IPAddress(s.ip).toString().c_str(), s.flags, status, status_result,
		s.state, s.tries, s.timeout, (unsigned long)s.xid,
		IPAddress(s.offered).toString().c_str(), IPAddress(s.server).toString().c_str(), snapshot_result);
	if (valid) Serial.printf("[ETHDIAG] bad_ready=%lu bad_header=%lu rx_errors=%lu rx_resets=%lu tx_timeouts=%lu health_failures=%lu recoveries=%lu recovery_failures=%lu fault=%u pending=%u\n",
		(unsigned long)d.bad_ready, (unsigned long)d.bad_header,
		(unsigned long)d.rx_errors, (unsigned long)d.rx_resets,
		(unsigned long)d.tx_timeouts, (unsigned long)d.health_failures,
		(unsigned long)d.recoveries, (unsigned long)d.recovery_failures,
		d.last_fault, d.pending);
	CH390TrafficDiag t = {};
	if (is_ch390() && ch390.diagnostic_traffic(t)) {
		uint8_t mac[6] = {};
		macAddress(mac);
		esp_err_t probe = send_probe(mac, ++probe_seq, t);
		uint32_t now = millis();
		Serial.printf("[ETHDIAG] traffic rx=%lu bcast=%lu probe=%lu probe_seq=%lu rx_age=%lu tx_ok=%lu tx_rejected=%lu tx_age=%lu tsr_or=%02x tsra=%02x tsrb=%02x lock_timeouts=%lu active=%u init=%u tx_probe=%lu rc=%d\n",
			(unsigned long)t.rx_frames, (unsigned long)t.rx_bcast, (unsigned long)t.rx_probe,
			(unsigned long)t.rx_probe_seq, (unsigned long)(now - t.rx_last_ms),
			(unsigned long)t.tx_ok, (unsigned long)t.tx_rejected, (unsigned long)(now - t.tx_last_ms),
			t.tsr_or, t.last_tsra, t.last_tsrb, (unsigned long)t.lock_timeouts,
			t.active, t.initialized, (unsigned long)probe_seq, probe);
		Serial.printf("[ETHDIAG] rx_deferred=%lu empty_ready=%lu rx_incomplete=%lu probe_backwards=%lu probe_session=%lu\n",
			(unsigned long)d.rx_deferred, (unsigned long)d.empty_ready,
			(unsigned long)d.rx_incomplete, (unsigned long)t.rx_probe_backwards,
			(unsigned long)t.rx_probe_session);
	}
}
#endif

void WiredEthernet::end() {
	if (is_ch390()) ch390.end();
	else if (is_w5500()) ETH.end();
}

void WiredEthernet::set_spi_drive(EthernetController controller) {
	// CH390 traffic on the OS4 shared-bus path requires slower C6 output edges.
	const gpio_drive_cap_t drive = controller == EthernetController::CH390 ?
		GPIO_DRIVE_CAP_0 : GPIO_DRIVE_CAP_DEFAULT;
	(void)gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_SPI_SCK), drive);
	(void)gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_SPI_MOSI), drive);
}

static void w5500_hostname_on_start(void*, esp_event_base_t, int32_t, void*) {
	// Raw ETH events are serialized: START is handled before CONNECTED starts
	// DHCP. Arduino's forwarded onEvent callback runs on a different task.
	if (!ETH.setHostname(hostname_effective())) {
		log_e("Could not set Ethernet hostname");
	}
}

bool WiredEthernet::begin(SPIClass& spi, int8_t cs, int8_t irq, int8_t reset,
	uint32_t spi_clock_hz) {
	_spi_clock_hz = spi_clock_hz;
	if (_controller == EthernetController::CH390) {
		return ch390.begin(spi, cs, irq, reset, spi_clock_hz);
	}
	if (_controller == EthernetController::W5500) {
		static bool hostname_handler_registered = false;
		if (!hostname_handler_registered) {
			if (esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_START,
				w5500_hostname_on_start, nullptr) != ESP_OK) return false;
			hostname_handler_registered = true;
		}
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
	switch (_controller) {
	case EthernetController::ENC28J60: return enc28j60.isLinked();
	case EthernetController::W5500: return w5500.isLinked();
	case EthernetController::CH390: return ch390.isLinked();
	default: return false;
	}
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
