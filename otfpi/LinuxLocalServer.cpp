#include "LinuxLocalServer.h"

using namespace OTF;

LinuxLocalServer::LinuxLocalServer(uint16_t port) : server(port) {}

LinuxLocalServer::~LinuxLocalServer() {
  if (activeClient != nullptr)
    activeClient->stop();
    delete activeClient;
}


LocalClient *LinuxLocalServer::acceptClient() {
  if (activeClient != nullptr) {
    activeClient->stop();
    delete activeClient;
  }

  EthernetClient client = server.available();
  if (client) {
    activeClient = new LinuxLocalClient(client);
  } else {
    activeClient = nullptr;
  }
  return activeClient;
}


void LinuxLocalServer::begin() {
  server.begin();
}


LinuxLocalClient::LinuxLocalClient(EthernetClient client) {
  this->client = client;
}

bool LinuxLocalClient::dataAvailable() {
  return client.available();
}

size_t LinuxLocalClient::readBytes(char *buffer, size_t length) {
  return client.readBytes(buffer, length);
}

size_t LinuxLocalClient::readBytesUntil(char terminator, char *buffer, size_t length) {
  return client.readBytesUntil(terminator, buffer, length);
}

void LinuxLocalClient::print(const char *data) {
  client.write((uint8_t*)data, strlen(data));
}

/*int LinuxLocalClient::peek() {
  return client.peek();
}*/

void LinuxLocalClient::setTimeout(int timeout) {
  //client.setTimeout(timeout);
}

void LinuxLocalClient::flush() {
	client.flush();
}

void LinuxLocalClient::stop() {
	client.stop();
}
