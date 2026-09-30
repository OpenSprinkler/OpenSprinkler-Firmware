/*
 * ESP8266 raw-frame adapter for WCH CH390H/D.
 *
 * The register setup and frame handling follow Espressif's Apache-2.0 CH390
 * driver: https://github.com/espressif/esp-eth-drivers/tree/master/ch390
 */

#if defined(ARDUINO)

#include "drivers/ch390_raw.h"

#include "boards/board_profile.h"
#include "defines.h"

namespace {

constexpr uint32_t kSpiClockHz = osboard::CH390_SPI_CLOCK_HZ;

constexpr uint8_t kNcr = 0x00;
constexpr uint8_t kNsr = 0x01;
constexpr uint8_t kTcr = 0x02;
constexpr uint8_t kRcr = 0x05;
constexpr uint8_t kEpcr = 0x0B;
constexpr uint8_t kEpar = 0x0C;
constexpr uint8_t kEpdrl = 0x0D;
constexpr uint8_t kEpdrh = 0x0E;
constexpr uint8_t kWcr = 0x0F;
constexpr uint8_t kPar = 0x10;
constexpr uint8_t kMar = 0x16;
constexpr uint8_t kGpr = 0x1F;
constexpr uint8_t kVidLow = 0x28;
constexpr uint8_t kVidHigh = 0x29;
constexpr uint8_t kPidLow = 0x2A;
constexpr uint8_t kPidHigh = 0x2B;
constexpr uint8_t kTcr2 = 0x2D;
constexpr uint8_t kTxChecksumControl = 0x31;
constexpr uint8_t kRxChecksumControl = 0x32;
constexpr uint8_t kInterruptControl = 0x39;
constexpr uint8_t kRxLengthControl = 0x52;
constexpr uint8_t kBroadcastControl = 0x53;
constexpr uint8_t kInterruptClockControl = 0x54;
constexpr uint8_t kMemoryPointerControl = 0x55;
constexpr uint8_t kMemoryReadNoIncrement = 0x70;
constexpr uint8_t kMemoryRead = 0x72;
constexpr uint8_t kMemoryWrite = 0x78;
constexpr uint8_t kTxLengthLow = 0x7C;
constexpr uint8_t kTxLengthHigh = 0x7D;
constexpr uint8_t kInterruptStatus = 0x7E;
constexpr uint8_t kInterruptMask = 0x7F;

constexpr uint8_t kNcrReset = 0x01;
constexpr uint8_t kNsrLink = 0x40;
constexpr uint8_t kTcrTxRequest = 0x01;
constexpr uint8_t kRcrDiscardCrcError = 0x10;
constexpr uint8_t kRcrAllMulticast = 0x08;
constexpr uint8_t kRcrReceiveEnable = 0x01;
constexpr uint8_t kTcr2RetryLateCollision = 0x40;
constexpr uint8_t kPointerResetTx = 0x02;
constexpr uint8_t kPointerResetRx = 0x01;
constexpr uint8_t kInterruptPointerAutoReturn = 0x80;
constexpr uint8_t kClearInterruptStatus = 0x2F;
constexpr uint8_t kReceiveErrorMask = 0xBF;

constexpr uint8_t kPhySelect = 0x40;
constexpr uint8_t kPhyBmcr = 0x00;
constexpr uint8_t kPhyId1 = 0x02;
constexpr uint8_t kPhyId2 = 0x03;
constexpr uint16_t kBmcrReset = 0x8000;
constexpr uint16_t kBmcrAutoNegotiation = 0x1000;
constexpr uint16_t kBmcrPowerDown = 0x0800;
constexpr uint16_t kBmcrIsolate = 0x0400;
constexpr uint16_t kBmcrRestartAutoNegotiation = 0x0200;

constexpr uint16_t kEthernetCrcLength = 4;
constexpr uint16_t kMaximumWireFrameLength = 1522;

struct RxHeader {
  uint8_t flag;
  uint8_t status;
  uint8_t lengthLow;
  uint8_t lengthHigh;
};

}  // namespace

bool ch390_probe(SPIClass& spi, int8_t cs, uint32_t clock_hz) {
  SPISettings settings(clock_hz, MSBFIRST, SPI_MODE0);
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);

  auto read_register = [&](uint8_t address) {
    spi.beginTransaction(settings);
    digitalWrite(cs, LOW);
    spi.transfer(address & 0x7F);
    uint8_t value = spi.transfer(0x00);
    digitalWrite(cs, HIGH);
    spi.endTransaction();
    return value;
  };
  // Identification is read-only: no CH390 reset commands on W5500/empty buses.
  const uint8_t vid_low = read_register(kVidLow);
  const uint8_t vid_high = read_register(kVidHigh);
  const uint8_t pid_low = read_register(kPidLow);
  const uint8_t pid_high = read_register(kPidHigh);
  DEBUG_PRINTF("CH390 probe: VID=%02X%02X PID=%02X%02X\n",
    vid_high, vid_low, pid_high, pid_low);
  return vid_low == 0x00 && vid_high == 0x1C &&
    pid_low == 0x51 && pid_high == 0x91;
}

#if defined(ESP8266)

CH390Raw::CH390Raw(int8_t cs, SPIClass& spi, int8_t intr)
    : _spi(spi),
      _settings(kSpiClockHz, MSBFIRST, SPI_MODE0),
      _cs(cs),
      _pendingWireLength(0) {
  (void)intr;
}

uint8_t CH390Raw::readRegister(uint8_t address) {
  _spi.beginTransaction(_settings);
  digitalWrite(_cs, LOW);
  _spi.transfer(address & 0x7F);
  const uint8_t value = _spi.transfer(0x00);
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
  return value;
}

void CH390Raw::writeRegister(uint8_t address, uint8_t value) {
  _spi.beginTransaction(_settings);
  digitalWrite(_cs, LOW);
  _spi.transfer(0x80 | (address & 0x7F));
  _spi.transfer(value);
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
}

void CH390Raw::readMemory(uint8_t* buffer, uint16_t length) {
  _spi.beginTransaction(_settings);
  digitalWrite(_cs, LOW);
  _spi.transfer(kMemoryRead);
  while (length-- != 0) {
    *buffer++ = _spi.transfer(0x00);
  }
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
}

void CH390Raw::writeMemory(const uint8_t* buffer, uint16_t length) {
  _spi.beginTransaction(_settings);
  digitalWrite(_cs, LOW);
  _spi.transfer(0x80 | kMemoryWrite);
  while (length-- != 0) {
    _spi.transfer(*buffer++);
  }
  digitalWrite(_cs, HIGH);
  _spi.endTransaction();
}

void CH390Raw::discardMemory(uint16_t length) {
  uint8_t scratch[32];
  while (length != 0) {
    const uint16_t chunk = length > sizeof(scratch) ? sizeof(scratch) : length;
    readMemory(scratch, chunk);
    length -= chunk;
  }
}

bool CH390Raw::probe() {
  return ch390_probe(_spi, _cs, osboard::CH390_PROBE_SPI_CLOCK_HZ);
}

bool CH390Raw::waitPhyReady(uint32_t timeoutUs) {
  const uint32_t started = micros();
  while ((readRegister(kEpcr) & 0x01) != 0) {
    if (micros() - started >= timeoutUs) {
      return false;
    }
    delayMicroseconds(50);
  }
  return true;
}

bool CH390Raw::readPhy(uint8_t reg, uint16_t& value) {
  if (!waitPhyReady(2000)) {
    return false;
  }
  writeRegister(kEpar, kPhySelect | (reg & 0x1F));
  writeRegister(kEpcr, 0x0C);  // Select PHY and begin a read.
  if (!waitPhyReady(2000)) {
    return false;
  }
  value = (static_cast<uint16_t>(readRegister(kEpdrh)) << 8) |
          readRegister(kEpdrl);
  return true;
}

bool CH390Raw::writePhy(uint8_t reg, uint16_t value) {
  if (!waitPhyReady(2000)) {
    return false;
  }
  writeRegister(kEpar, kPhySelect | (reg & 0x1F));
  writeRegister(kEpdrl, value & 0xFF);
  writeRegister(kEpdrh, value >> 8);
  writeRegister(kEpcr, 0x0A);  // Select PHY and begin a write.
  return waitPhyReady(2000);
}

bool CH390Raw::initializePhy() {
  if (!writePhy(kPhyBmcr, kBmcrReset)) {
    return false;
  }

  uint16_t bmcr = 0;
  const uint32_t started = millis();
  do {
    if (!readPhy(kPhyBmcr, bmcr)) {
      return false;
    }
    if ((bmcr & kBmcrReset) == 0) {
      break;
    }
    delay(1);
  } while (millis() - started < 500);

  if ((bmcr & kBmcrReset) != 0) {
    return false;
  }

  bmcr &= ~(kBmcrPowerDown | kBmcrIsolate);
  bmcr |= kBmcrAutoNegotiation | kBmcrRestartAutoNegotiation;
  if (!writePhy(kPhyBmcr, bmcr)) {
    return false;
  }

  uint16_t phyId1 = 0;
  uint16_t phyId2 = 0;
  if (!readPhy(kPhyId1, phyId1) || !readPhy(kPhyId2, phyId2)) {
    return false;
  }
  DEBUG_PRINTF("CH390 PHY ID: 0x%04X 0x%04X\n", phyId1, phyId2);
  const uint32_t oui = (static_cast<uint32_t>(phyId1) << 6) |
                       ((phyId2 >> 10) & 0x3F);
  const uint8_t model = (phyId2 >> 4) & 0x3F;
  if (oui != 0x1CDC64 || model != 0x01) {
    DEBUG_PRINTF("CH390 unexpected PHY: 0x%06lX/%u\n",
                 static_cast<unsigned long>(oui), model);
    return false;
  }
  return true;
}

bool CH390Raw::begin(const uint8_t* macAddress) {
  _pendingWireLength = 0;
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  _spi.begin();

  if (!probe()) {
    DEBUG_PRINTLN(F("CH390 identity check failed"));
    return false;
  }

  writeRegister(kNcr, kNcrReset);
  const uint32_t started = millis();
  while (readRegister(kNcr) & kNcrReset) {
    if ((uint32_t)(millis() - started) >= 250) return false;
    delay(1);
  }
  writeRegister(kGpr, 0);
  delay(10);

  // Conservative MAC defaults. lwIP computes checksums, so hardware checksum
  // offload remains disabled. Accept multicast to support common local-network
  // protocols in addition to DHCP broadcast traffic.
  writeRegister(kNcr, 0x00);
  writeRegister(kWcr, 0x00);
  writeRegister(kTcr, 0x00);
  writeRegister(kRcr, kRcrDiscardCrcError | kRcrAllMulticast);
  writeRegister(kTcr2, kTcr2RetryLateCollision);
  writeRegister(kTxChecksumControl, 0x00);
  writeRegister(kRxChecksumControl, 0x00);
  writeRegister(kInterruptControl, 0x00);
  writeRegister(kInterruptClockControl, 0x00);
  writeRegister(kRxLengthControl, 0x98);  // Reject frames longer than 1536 B.
  writeRegister(kNsr, 0x2C);  // Clear wake and completed-transmit status.

  writeRegister(kBroadcastControl, 0x00);
  for (uint8_t i = 0; i < 7; ++i) {
    writeRegister(kMar + i, 0x00);
  }
  writeRegister(kMar + 7, 0x80);  // Accept Ethernet broadcasts.

  for (uint8_t i = 0; i < 6; ++i) {
    writeRegister(kPar + i, macAddress[i]);
  }
  DEBUG_PRINTF("CH390 MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
               macAddress[0], macAddress[1], macAddress[2], macAddress[3],
               macAddress[4], macAddress[5]);

  if (!initializePhy()) {
    DEBUG_PRINTLN(F("CH390 PHY initialization failed"));
    return false;
  }

  writeRegister(kMemoryPointerControl, kPointerResetTx | kPointerResetRx);
  writeRegister(kInterruptStatus, kClearInterruptStatus);
  writeRegister(kInterruptMask, kInterruptPointerAutoReturn);
  writeRegister(kRcr, kRcrDiscardCrcError | kRcrAllMulticast |
                          kRcrReceiveEnable);
  DEBUG_PRINTLN(F("CH390 initialized with RX polling"));
  return true;
}

void CH390Raw::end() {
  writeRegister(kInterruptMask, 0x00);
  writeRegister(kRcr, readRegister(kRcr) & ~kRcrReceiveEnable);
}

bool CH390Raw::isLinked() {
  return (readRegister(kNsr) & kNsrLink) != 0;
}

uint16_t CH390Raw::sendFrame(const uint8_t* data, uint16_t length) {
  if (length == 0 || length > 1518) {
    return 0;
  }

  const uint32_t started = micros();
  while ((readRegister(kTcr) & kTcrTxRequest) != 0) {
    if (micros() - started > 20000) {
      DEBUG_PRINTLN(F("CH390 TX busy timeout"));
      return 0;
    }
    delayMicroseconds(10);
  }

  writeMemory(data, length);
  writeRegister(kTxLengthLow, length & 0xFF);
  writeRegister(kTxLengthHigh, length >> 8);
  writeRegister(kTcr, readRegister(kTcr) | kTcrTxRequest);
  return length;
}

void CH390Raw::resetReceivePath() {
  const uint8_t rcr = readRegister(kRcr);
  writeRegister(kRcr, rcr & ~kRcrReceiveEnable);
  writeRegister(kMemoryPointerControl, kPointerResetRx);
  writeRegister(kInterruptStatus, kClearInterruptStatus);
  writeRegister(kRcr, rcr | kRcrReceiveEnable);
  _pendingWireLength = 0;
}

uint16_t CH390Raw::readFrameSize() {
  if (_pendingWireLength != 0) {
    return _pendingWireLength - kEthernetCrcLength;
  }

  // The first MRCMDX read initiates prefetch; the second returns fresh state.
  (void)readRegister(kMemoryReadNoIncrement);
  const uint8_t ready = readRegister(kMemoryReadNoIncrement);
  if ((ready & 0xFE) != 0) {
    DEBUG_PRINTF("CH390 invalid RX-ready value 0x%02X\n", ready);
    resetReceivePath();
    return 0;
  }
  if ((ready & 0x01) == 0) {
    return 0;
  }

  RxHeader header{};
  readMemory(reinterpret_cast<uint8_t*>(&header), sizeof(header));
  const uint16_t wireLength =
      (static_cast<uint16_t>(header.lengthHigh) << 8) | header.lengthLow;

  if ((header.status & kReceiveErrorMask) != 0 ||
      wireLength < kEthernetCrcLength ||
      wireLength > kMaximumWireFrameLength) {
    DEBUG_PRINTF("CH390 dropping RX frame: status=0x%02X length=%u\n",
                 header.status, wireLength);
    if (wireLength >= kEthernetCrcLength &&
        wireLength <= kMaximumWireFrameLength) {
      discardMemory(wireLength);
    } else {
      resetReceivePath();
    }
    return 0;
  }

  _pendingWireLength = wireLength;
  return wireLength - kEthernetCrcLength;
}

uint16_t CH390Raw::readFrameData(uint8_t* frame, uint16_t frameSize) {
  if (_pendingWireLength == 0 ||
      frameSize + kEthernetCrcLength != _pendingWireLength) {
    resetReceivePath();
    return 0;
  }

  readMemory(frame, frameSize);
  discardMemory(kEthernetCrcLength);
  _pendingWireLength = 0;
  return frameSize;
}

void CH390Raw::discardFrame(uint16_t frameSize) {
  (void)frameSize;
  if (_pendingWireLength != 0) {
    discardMemory(_pendingWireLength);
    _pendingWireLength = 0;
  }
}

uint16_t CH390Raw::readFrame(uint8_t* buffer, uint16_t bufferSize) {
  const uint16_t frameSize = readFrameSize();
  if (frameSize == 0) {
    return 0;
  }
  if (frameSize > bufferSize) {
    discardFrame(frameSize);
    return 0;
  }
  return readFrameData(buffer, frameSize);
}

#endif // ESP8266
#endif // ARDUINO
