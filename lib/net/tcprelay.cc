#include "tcprelay.h"

namespace QSX {

TcpRelay::TcpRelay(QSS::Configuration &configuration) {
  m_local = std::make_unique<QTcpServer>();
  m_config = configuration;
}

TcpRelay::~TcpRelay() {
  close();
}

bool TcpRelay::listen() {

  QObject::connect(m_local.get(), &QTcpServer::newConnection, this, &TcpRelay::accepted);

  auto localAddress = m_config.getLocalAddress();
  auto localPort = m_config.getLocalPort();

  Q_ASSERT(localPort != 0);

  qDebug() << "TcpReply: listen" << localAddress << ":" << localPort;
  bool r = m_local->listen(localAddress, localPort);
  if (!r) {
    qWarning() << "listen error!";
    close();
    return false;
  }

  return true;
}

void TcpRelay::close() {
  for (auto handler : m_cache) {
    delete handler;
  }
  m_cache.clear();
}

void TcpRelay::accepted() {
  auto client = m_local->nextPendingConnection();

  qDebug() << "TcpReply accept a connection:" << client->peerAddress().toString() << ":" << client->peerPort();
  auto handler = new TcpHandler(client, m_config);
  QObject::connect(handler, &TcpHandler::bytesRead, [=](uint64_t s) {
    qDebug() << "r += " << s << "\t\tR:" << handler->getCountRead();
  });
  QObject::connect(handler, &TcpHandler::bytesWrite, [=](uint64_t s) {
    qDebug() << "w += " << s << "\t\tW:" << handler->getCountWrite();
  });
  m_cache.emplace_back(handler);
}

}
