#if defined(OSPI)
#ifndef OTF_LINUXLOCALSERVER_H
#define OTF_LINUXLOCALSERVER_H

#include "LocalServer.h"
#include "etherport.h"

namespace OTF {
  class LinuxLocalClient : public LocalClient {
    friend class LinuxLocalServer;

  private:
    EthernetClient client;
    LinuxLocalClient(EthernetClient client);
    
  public:
    bool dataAvailable();
    size_t readBytes(char *buffer, size_t length);
    size_t readBytesUntil(char terminator, char *buffer, size_t length);
    void print(const char *data);
    //int peek();
    void setTimeout(int timeout);
    void flush();
    void stop();
  };


  class LinuxLocalServer : public LocalServer {
  private:
    EthernetServer server;
    LinuxLocalClient *activeClient = nullptr;

  public:
    LinuxLocalServer(uint16_t port);
    ~LinuxLocalServer();

    LocalClient *acceptClient();
    void begin();
  };
}// namespace OTF

#endif
#endif