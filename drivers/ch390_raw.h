#pragma once

#if defined(ARDUINO)

#include <Arduino.h>
#include <SPI.h>

bool ch390_probe(SPIClass& spi, int8_t cs, uint32_t clock_hz);

#if defined(ESP8266)

// Raw Ethernet-frame driver used beneath ESP8266 Arduino's LwipIntfDev.
// IRQ is intentionally unsupported: LwipIntfDev polls readFrameSize().
class CH390Raw {
 public:
  CH390Raw(int8_t cs = SS, SPIClass& spi = SPI, int8_t intr = -1);

  bool begin(const uint8_t* macAddress);
  void end();

  uint16_t sendFrame(const uint8_t* data, uint16_t length);
  uint16_t readFrame(uint8_t* buffer, uint16_t bufferSize);

  bool probe();
  bool isLinked();
  constexpr bool isLinkDetectable() const { return true; }

 protected:
  static constexpr bool interruptIsPossible() { return false; }

  uint16_t readFrameSize();
  void discardFrame(uint16_t frameSize);
  uint16_t readFrameData(uint8_t* frame, uint16_t frameSize);

 private:
  uint8_t readRegister(uint8_t address);
  void writeRegister(uint8_t address, uint8_t value);
  void readMemory(uint8_t* buffer, uint16_t length);
  void writeMemory(const uint8_t* buffer, uint16_t length);
  void discardMemory(uint16_t length);

  bool initializePhy();
  bool readPhy(uint8_t reg, uint16_t& value);
  bool writePhy(uint8_t reg, uint16_t value);
  bool waitPhyReady(uint32_t timeoutUs);
  void resetReceivePath();

  SPIClass& _spi;
  SPISettings _settings;
  int8_t _cs;
  uint16_t _pendingWireLength;
};

#include <LwipIntfDev.h>
using CH390lwIP = LwipIntfDev<CH390Raw>;

#endif // ESP8266
#endif // ARDUINO
