#pragma once

#include <memory>
#include <QtNetwork/QTcpSocket>
#include <crypto/encryptor.h>
#include <types/configuration.h>

namespace QSX {

class TcpHandler : public QObject {

Q_OBJECT

  enum STATE {
    INIT,                 /* handshake */
    ADDRESS,              /* handle the destination address */
    UDP_ASSOC,            /* handle the udp associate */
    DNS,                  /* look up the ss server address, not used for Qt */
    CONNECTING,           /* connect to ss server */
    STREAM,               /* ss server connected, start data streaming */
    DESTROYED             /* end of this handler */
  };

  enum CMD {
    CONNECT = 1,
    // BIND = 2,          /* command BIND not used */
    UDP_ASSOCIATE = 3
  };

  enum ERRCODE {
    GOOD,                 /* 0 close by manual */
    E_DATA_LENGTH,        /* 1 data length not correct */
    E_READ_HEADER,        /* 2 failed at parse HEADER address */
    E_NO_CMD,             /* 3 no such command */
    E_CLOSE_REMOTE,       /* 4 socket closed by remote */
    E_CLOSE_LOCAL,        /* 5 socket closed by local */
    E_TIMEOUT_REMOTE,     /* 6 timeout when connecting to remote */
    E_TIMEOUT_LOCAL,      /* 7 timeout when connecting to local */
    E_OTHER_REMOTE,       /* 8 other error occurred on remote socket */
    E_OTHER_LOCAL         /* 9 other error occurred on local socket */
  };

  static const char HANDLE_ACCEPT[], HANDLE_REJECT[], HANDLE_RESPONSE[];

  std::unique_ptr<QTcpSocket> m_local, m_remote;
  Configuration *m_config = nullptr;
  std::unique_ptr<QSS::Encryptor> m_encryptor = nullptr;

public:
  TcpHandler(QTcpSocket *socket, Configuration &configuration);
  ~TcpHandler() override;

  void close(int r = 0);

  uint64_t getCountRead() {
    return m_countRead;
  }

  uint64_t getCountWrite() {
    return m_countWrite;
  }

private:
  STATE m_state = INIT;
  uint64_t m_countRead = 0, m_countWrite = 0;
  QByteArray m_wannaWrite;

  void handle(QByteArray &data);
  void createRemote(QSS::Address &destination);
  void loadProxyRemote(Proxy &proxy);
  void sendToRemote(QByteArray &data);
  bool readHeader(QByteArray &data, Address &destination);

protected slots:
  void onRecvLocal();
  void onRecvRemote();
  void onConnectedRemote();
  void onErrorLocal();
  void onErrorRemote();

signals:
  void connecting(QSS::Address &destination);
  void connected();
  void bytesRead(uint64_t s);
  void bytesWrite(uint64_t s);
  void finished(int r);
};

}
