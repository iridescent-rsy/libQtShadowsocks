#pragma once

#include <QList>
#include <QtNetwork/QHostAddress>

#include "server.h"
#include "proxy.h"
#include "address.h"

namespace QSX {

using QSS::Address;

class IChooser {
public:
  virtual ~IChooser() = default;
  virtual Server getServer(Address &destination) = 0;
};

class Configuration {
protected:
  QList<Server> m_servers;
  Proxy m_proxy;
  IChooser *m_chooser = nullptr;
  bool m_shareOverLan;
  uint16_t m_localPort;

public:
  Configuration();
  ~Configuration();

  Server getServer(Address &destination, int index = -1);

  void setServers(QList<Server> &servers) {
    m_servers = servers;
  }

  void setProxy(Proxy &proxy) {
    m_proxy = proxy;
  }

  void setLocalPort(uint16_t port) {
    m_localPort = port;
  }

  void registerChooser(IChooser *chooser) {
    m_chooser = chooser;
  }

  void unregisterChooser() {
    delete m_chooser;
    m_chooser = nullptr;
  }

  void setShareOverLan(bool share) {
    m_shareOverLan = share;
  }

  QList<Server> &getServers() {
    return m_servers;
  }

  Proxy &getProxy() {
    return m_proxy;
  }

  bool getShareOverLan() {
    return m_shareOverLan;
  }

  QHostAddress getLocalAddress() {
    return m_shareOverLan ? QHostAddress::Any : QHostAddress::LocalHost;
  }

  uint16_t getLocalPort() {
    return m_localPort;
  }

};

}
