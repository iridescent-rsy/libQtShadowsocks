#include <memory>
#include <error.h>
#include <util/common.h>
#include <types/address.h>
#include <QtNetwork/QNetworkProxy>
#include "tcphandler.h"

#define dout (qDebug() << "[" << this << "]")

namespace QSX {

const char TcpHandler::HANDLE_ACCEPT[] = { 5, 0 };
const char TcpHandler::HANDLE_REJECT[] = { 0, 91 };
const char TcpHandler::HANDLE_RESPONSE[] = { 5, 0, 0, 1, 0, 0, 0, 0, 0, 0 };

TcpHandler::TcpHandler(QTcpSocket *socket, Configuration &configuration) {
  m_local.reset(socket);
  m_config = &configuration;
  m_wannaWrite.clear();

  QObject::connect(m_local.get(), &QTcpSocket::readyRead, this, &TcpHandler::onRecvLocal);
  QObject::connect(m_local.get(),
                   static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
                   this,
                   &TcpHandler::onErrorLocal);
}

TcpHandler::~TcpHandler() {
  if (m_state != DESTROYED) {
    close(GOOD);
  }
  dout << "tcp_handler died";
}

void TcpHandler::close(int r) {
  if (m_local) {
    m_local->close();
  }
  if (m_remote) {
    m_remote->close();
  }
  if (m_encryptor) {
    m_encryptor.reset();
  }
  m_state = DESTROYED;
  emit finished(r);
}

/**
 * Handle the client request
 * details in https://www.ietf.org/rfc/rfc1928.txt
 * @param data data from client
 */
void TcpHandler::handle(QByteArray &data) {
  if (m_state == INIT) {
    /** Authentication
     *
     *  client -> server
     *  +----+----------+----------+
     *  |VER | NMETHODS | METHODS  |
     *  +----+----------+----------+
     *  | 1  |    1     | 1 to 255 |
     *  +----+----------+----------+
     *
     *  server -> client
     *  +----+--------+
     *  |VER | METHOD |
     *  +----+--------+
     *  | 1  |   1    |
     *  +----+--------+
     */
    if (data.size() < 2) {
      close(E_DATA_LENGTH);
      return;
    }

    if (data[0] != char(5)) {
      // not a socks5 connection, reject it
      m_local->write(HANDLE_REJECT, sizeof(HANDLE_REJECT));
    } else {
      // ok
      m_local->write(HANDLE_ACCEPT, sizeof(HANDLE_ACCEPT));
      m_state = ADDRESS;
    }
  } else if (m_state == ADDRESS) {
    /**
     *  client -> server
     *  +----+-----+-------+------+----------+----------+
     *  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
     *  +----+-----+-------+------+----------+----------+
     *  | 1  |  1  | X'00' |  1   | Variable |    2     |
     *  +----+-----+-------+------+----------+----------+
     *
     *  server -> client
     *  +----+-----+-------+------+----------+----------+
     *  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
     *  +----+-----+-------+------+----------+----------+
     *  | 1  |  1  | X'00' |  1   | Variable |    2     |
     *  +----+-----+-------+------+----------+----------+
     */
    if (data.size() < 5 || data[0] != char(5)) {
      close(E_DATA_LENGTH);
      return;
    }
    char cmd = data[1];
    switch (cmd) {
      // TODO use enum or definition
    case 0x01: {
      // CONNECT
      data = data.remove(0, 3);
      int length;
      QSS::Address destination;
      std::string header;
      header.resize(data.size());
      memcpy(&header[0], data.data(), data.size());
      QSS::Common::parseHeader(header, destination, length);
      if (0 == length) {
        dout << "parse error";
        // parse error
        close(E_PARSE_ADDRESS);
        return;
      }
      dout << "connecting to" << destination.getAddress().data() << ":" << destination.getPort();

      // reply to client
      m_local->write(HANDLE_RESPONSE, sizeof(HANDLE_RESPONSE));

      createRemote(destination);
      m_state = CONNECTING;

      Q_ASSERT(data.size() != 0);

      handle(data);
      return;
    }

    case 0x03:
      // UDP ASSOCIATE
      Q_ASSERT(false);

    default:
      qWarning() << "unknown socks5 cmd! [" << QString::number(cmd) << "]";
      close(E_NO_CMD);
      return;
    }
  } else if (m_state == CONNECTING) {
    m_wannaWrite += data;
  } else if (m_state == STREAM) {
    m_wannaWrite += data;
    sendToRemote(m_wannaWrite);
    m_wannaWrite.clear();
  } else {
    Q_ASSERT(false);
  }
}

void TcpHandler::createRemote(QSS::Address &destination) {
  // choose a server
  auto remote = m_config->getServer(destination);
  dout << "createRemote: server:" << remote.server << ":" << remote.server_port << "," << remote.method;

  // init the server and encryptor
  m_remote = std::make_unique<QTcpSocket>();

  QObject::connect(m_remote.get(), &QTcpSocket::readyRead, this, &TcpHandler::onRecvRemote);
  QObject::connect(m_remote.get(),
                   static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
                   this,
                   &TcpHandler::onErrorRemote);
  QObject::connect(m_remote.get(), &QTcpSocket::connected, this, &TcpHandler::onConnectedRemote);

  loadProxyRemote(m_config->getProxy());
  m_remote->connectToHost(remote.server, remote.server_port);

  auto method = remote.method.toStdString();
  auto passwd = remote.passwd.toStdString();
  m_encryptor = std::make_unique<QSS::Encryptor>(method, passwd);
}

void TcpHandler::loadProxyRemote(Proxy &proxy) {
  if (proxy.use) {
    QNetworkProxy p;
    QNetworkProxy::ProxyType type;
    if (proxy.type == Proxy::HTTP) {
      type = QNetworkProxy::HttpProxy;
    } else if (proxy.type == Proxy::SOCKS5) {
      type = QNetworkProxy::Socks5Proxy;
    } else {
      // unknown type, do not set
      return;
    }
    p.setType(type);
    p.setHostName(proxy.server);
    p.setPort(proxy.port);
    m_remote->setProxy(p);
  }
}

void TcpHandler::sendToRemote(QByteArray &data) {
  // encrypt the data
  auto enc = m_encryptor->encrypt((const uint8_t *)data.data(), (size_t)data.size());
  m_countWrite += enc.size();
  emit bytesWrite(enc.size());
  m_remote->write(enc.data(), (int)enc.size());
}

void TcpHandler::onRecvLocal() {
  auto data = m_local->readAll();
  if (data.isEmpty()) {
    dout << "no data or maybe error occurred.";
    return;
  }

  handle(data);
}

void TcpHandler::onRecvRemote() {
  auto recv = m_remote->readAll();
  m_countRead += recv.size();
  emit bytesRead((uint64_t)recv.size());
  auto dec = m_encryptor->decrypt((const uint8_t *)recv.data(), (size_t)recv.size());
  m_local->write(dec.data(), (int)dec.length());
}

void TcpHandler::onConnectedRemote() {
  static QByteArray none;
  m_state = STREAM;
  handle(none);
}

void TcpHandler::onErrorLocal() {
  if (m_local->error() == QAbstractSocket::RemoteHostClosedError) {
    close(E_CLOSE_LOCAL);
  } else if (m_local->error() == QAbstractSocket::SocketTimeoutError) {
    close(E_TIMEOUT_LOCAL);
  } else {
    dout << "TcpHandler: local: " << m_local->errorString();
    close(E_OTHER_LOCAL);
  }
}

void TcpHandler::onErrorRemote() {
  if (m_remote->error() == QAbstractSocket::RemoteHostClosedError) {
    close(E_CLOSE_REMOTE);
  } else if (m_remote->error() == QAbstractSocket::SocketTimeoutError) {
    close(E_TIMEOUT_REMOTE);
  } else {
    dout << "TcpHandler: remote: " << m_remote->errorString();
    close(E_OTHER_REMOTE);
  }
}

}
