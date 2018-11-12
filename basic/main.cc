#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include "../lib/util/listener.h"

int main(int argc, char **argv) {

  QCoreApplication a(argc, argv);

  QSS::Configuration configuration;
  QSS::Server server;
  server.server = "107.182.191.51";
  server.server_port = 26316;
//  server.server = "127.0.0.1";
//  server.server_port = 8388;
  server.method = "aes-256-cfb";
  server.passwd = "rsy0715Z-";
  QList<QSS::Server> servers;
  servers.append(server);
  configuration.setServers(servers);
  configuration.setLocalPort(1080);

  QSS::Listener listener;
  listener.start(configuration);

  QCoreApplication::exec();

  return 0;
}
